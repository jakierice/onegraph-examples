open PeerJsBinding;
open ReasonReact;
open Utils;
open Emotion;
open SharedCss;

let recordPlayer = requireAssetURI("./img/record-player.png");
let headphone = requireAssetURI("./img/headphone.png");

let userKindWrapper = [%css
  [position(`fixed), top(`px(16)), left(`px(16))]
];

let userKindIcon = [%css [width(`px(24)), margin2(`px(0), `px(4))]];

let userKindSwitchBtn = [%css [fontSize(`px(12)), fontWeight(600)]];

let unactiveStyle = [%css [opacity(0.5)]];

let statusRibon = (~ribonColor) => [%css
  [
    fontSize(`px(32)),
    position(`absolute),
    left(`pct(50.)),
    transform(`translateX(`pct(-50.))),
    zIndex(2),
    top(`px(200)),
    backgroundColor(ribonColor),
    width(`px(400)),
    color(`hex("ffffffc9")),
  ]
];

type connectionStatus =
  | Disconnected
  | Connecting(float)
  | Connected
  | Error;

type followerNum = int;

type userKind =
  | DJ(followerNum)
  | Listener(string);

type state = {
  switchboard: option(PeerJsBinding.switchboard),
  switchboardId: option(BsUuid.Uuid.V4.t),
  userKind,
  connectionToDj: connectionStatus,
  checkMessageIntervalId: option(Js.Global.intervalId),
  lastMessageReceivedAt: float,
  connectionId: int,
  previousTrackIds: array(string),
};

type action =
  | ExamineDJState(SpotifyControls.playerStatus)
  | MaintainConnection
  | NotifyPeers
  | PausePlayer
  | SetDjConnectionStatus(int, connectionStatus)
  | SetSwitchboard(switchboard, BsUuid.Uuid.V4.t)
  | SetUserKind(userKind)
  | SyncPlayer(SpotifyControls.playerStatus)
  | SetPreviouseTrackId(string);

let component = ReasonReact.reducerComponent("App");

let msDiffThreshold = 10000;

let initiateConnection =
    (
      ~switchboard,
      ~peerId,
      ~onConnected,
      ~onConnecting: option(unit => unit)=?,
      ~onDisconnected,
      ~onConnectError,
      ~onReceiveDjPlayerState,
      (),
    ) => {
  let hasEverReceivedMessage = ref(false);

  let onData = data => {
    switch (hasEverReceivedMessage^) {
    | false =>
      hasEverReceivedMessage := true;
      onConnected();
    | true => ()
    };

    switch (Serialize.messageOfString(data)) {
    | Unrecognized(raw) => Js.log2("Unrecognized message from peer: ", raw)
    | Malformed(raw) => Js.log2("Malformed message from peer: ", raw)
    | DjPlayerState(playerStatus) => onReceiveDjPlayerState(playerStatus)
    };
  };

  let onError = data => onConnectError(data);

  let onClose = data => onDisconnected(data);

  let connection =
    connect(
      ~me=switchboard,
      ~toPeerId=peerId,
      ~onData,
      ~onClose,
      ~onError,
      ~onConnecting?,
      (),
    );

  connection;
};

let reestablishConnection = (state, djId) =>
  switch (state.switchboard) {
  | None => NoUpdate
  | Some(switchboard) =>
    let nextConnectionId = state.connectionId + 1;
    let now = Js.Date.now();

    UpdateWithSideEffects(
      {
        ...state,
        connectionToDj: Connecting(now),
        connectionId: nextConnectionId,
      },
      (
        self => {
          let _ignoredNewConnection =
            initiateConnection(
              ~switchboard,
              ~onConnected=
                () => {
                  Js.log2("Connected to DJ", nextConnectionId);
                  self.send(
                    SetDjConnectionStatus(nextConnectionId, Connected),
                  );
                },
              ~onConnectError=
                _data => {
                  Js.log2("Error with connection to DJ", nextConnectionId);
                  self.send(SetDjConnectionStatus(nextConnectionId, Error));
                },
              ~onDisconnected=
                _data => {
                  Js.log2("Disconnected from DJ", nextConnectionId);
                  self.send(
                    SetDjConnectionStatus(nextConnectionId, Disconnected),
                  );
                },
              ~onReceiveDjPlayerState=
                playerStatus => {
                  Js.log2("Received DJ playState", nextConnectionId);
                  self.send(ExamineDJState(playerStatus));
                  self.send(SetPreviouseTrackId(playerStatus.trackId));
                },
              ~peerId=djId,
              (),
            );
          ();
        }
      ),
    );
  };

let make =
    (
      ~auth,
      ~songName,
      ~artistName,
      ~isPlaying,
      ~progressPct,
      ~albumImageUrl,
      ~positionMs,
      ~trackId,
      ~userName,
      ~userIconUrl,
      ~setLogOut,
      _children,
    ) => {
  ...component,
  initialState: () => {
    switchboard: None,
    switchboardId: None,
    userKind: DJ(0),
    connectionToDj: Connecting(0.),
    checkMessageIntervalId: None,
    lastMessageReceivedAt: 0.0,
    connectionId: (-1),
    previousTrackIds: [||],
  },
  didMount: self => {
    let url = ReasonReact.Router.dangerouslyGetInitialUrl();
    let query = QueryString.parseQueryString(url.search);
    let userKind =
      switch (Js.Dict.get(query, "dj")) {
      | Some(Single(djId)) => Listener(djId)
      | _ => DJ(0)
      };
    self.send(SetUserKind(userKind));
    let maybePeerId = Utils.localStorageGetItem("spotDjPeerId");
    let peerId =
      switch (Js.nullToOption(maybePeerId)) {
      | None =>
        let newId = BsUuid.Uuid.V4.create();
        Utils.localStorageSetItem(
          "spotDjPeerId",
          BsUuid.Uuid.V4.toString(newId),
        );
        newId;
      | Some(id) => id
      };

    let me = newSwitchBoard(peerId);
    self.send(SetSwitchboard(me, peerId));

    switch (userKind) {
    | DJ(_) =>
      Js.log("I'm a DJ");

      let intervalId =
        Js.Global.setInterval(() => self.send(NotifyPeers), 1000);
      self.onUnmount(() => Js.Global.clearInterval(intervalId));

    | Listener(djId) =>
      let intervalId =
        Js.Global.setInterval(() => self.send(MaintainConnection), 1000);
      self.onUnmount(() => Js.Global.clearInterval(intervalId));

      Js.log({j|My switchboard $me has a connection to dj $djId|j});
    };
  },
  reducer: (action, state) =>
    switch (action) {
    | NotifyPeers =>
      switch (state.switchboard) {
      | None => NoUpdate
      | Some(switchboard) =>
        let followerNum = PeerJsBinding.getConnectedPeerCount(switchboard);

        UpdateWithSideEffects(
          {...state, userKind: DJ(followerNum)},
          (
            self =>
              switch (self.state.switchboard) {
              | None => ()
              | Some(switchboard) =>
                let rawMessage =
                  Serialize.stringOfMessage(
                    DjPlayerState({isPlaying, trackId, positionMs}),
                  );
                broadcast(switchboard, rawMessage);
              }
          ),
        );
      }
    | SetSwitchboard(switchboard, switchboardId) =>
      Update({
        ...state,
        switchboard: Some(switchboard),
        switchboardId: Some(switchboardId),
      })
    | SetPreviouseTrackId(id) =>
      Js.log("Set Pre");
      let idAlreadyExists = Js.Array.includes(id, state.previousTrackIds);
      idAlreadyExists ?
        NoUpdate :
        {
          let trackIds =
            Array.copy(state.previousTrackIds) |> Js.Array.append(id);
          if (Array.length(trackIds) > 4) {
            let newTrackIds = Js.Array.slice(~start=1, ~end_=5, trackIds);
            Js.log2("Set Pre:", newTrackIds);
            Update({...state, previousTrackIds: newTrackIds});
          } else {
            let newTrackIds = trackIds;
            Js.log2("Set Pre:", newTrackIds);
            Update({...state, previousTrackIds: newTrackIds});
          };
        };

    | ExamineDJState(dj) =>
      Js.log("Examin");
      /* Determine if we should run a OneGraph Spotify mutation to sync players, and if so, which mutation */
      let djAction =
        switch (
          dj.isPlaying,
          isPlaying,
          dj.trackId == trackId,
          abs(dj.positionMs - positionMs) > msDiffThreshold,
        ) {
        | (false, true, _, _) => Some(PausePlayer)
        | (true, false, _, _)
        | (true, true, false, _)
        | (true, true, true, true) => Some(SyncPlayer(dj))
        | (true, true, true, false)
        | (false, false, _, _) => None
        };

      let now = Js.Date.now();

      switch (djAction) {
      | None => Update({...state, lastMessageReceivedAt: now})
      | Some(message) =>
        UpdateWithSideEffects(
          {...state, lastMessageReceivedAt: now},
          (self => self.send(message)),
        )
      };

    /* Listener actions */
    | PausePlayer =>
      SideEffects(
        (
          _ =>
            SpotifyControls.pausePlayer(OneGraphAuth.authToken(auth))
            |> ignore
        ),
      )
    | SyncPlayer(playerStatus) =>
      SideEffects(
        (
          _ =>
            SpotifyControls.startPlayer(
              OneGraphAuth.authToken(auth),
              playerStatus,
            )
            |> ignore
        ),
      )
    | SetUserKind(userKind) => Update({...state, userKind})
    | SetDjConnectionStatus(connectionId, connectionToDj) =>
      switch (connectionId == state.connectionId) {
      | false =>
        Js.log3(
          "Received DJConnectionStatus message from old connection. Old/New connectionIds:",
          connectionId,
          state.connectionId,
        );
        NoUpdate;
      | true => Update({...state, connectionToDj})
      }

    | MaintainConnection =>
      Js.log2("!!!!", state.connectionId);
      switch (state.connectionToDj, state.userKind) {
      | (_, DJ(_)) => NoUpdate
      | (Connected, Listener(_)) =>
        let now = Js.Date.now();
        let elapsedMs = now -. state.lastMessageReceivedAt;
        let djAwayMsThreshold = 1500.0;
        Js.log2("connected:", elapsedMs);
        switch (elapsedMs > djAwayMsThreshold) {
        | false => NoUpdate
        | true =>
          SideEffects(
            (
              ({send}) =>
                send(SetDjConnectionStatus(state.connectionId, Disconnected))
            ),
          )
        };
      | (Connecting(asOfMs), Listener(djId)) =>
        let connectionTimeoutMs = 10000.0;
        let elapsedMs = Js.Date.now() -. asOfMs;
        Js.log2("asOfMs:", asOfMs);
        Js.log2("elapsedMs:", elapsedMs);
        switch (elapsedMs > connectionTimeoutMs) {
        | false => NoUpdate
        | true => reestablishConnection(state, djId)
        };

      | (Error, Listener(djId))
      | (Disconnected, Listener(djId)) => reestablishConnection(state, djId)
      };
    },
  render: self =>
    <div>
      <div className={appearAnimation(~direction=`normal, ~delayMs=0)}>
        {
          switch (self.state.userKind) {
          | DJ(num) =>
            <div className=userKindWrapper>
              <div
                className={flexWrapper(~justify=`flexStart, ~align=`center)}>
                <img src=recordPlayer alt="DJ Icon" className=userKindIcon />
                <p className=marginZero>
                  <strong className=textBold> {string("DJ ")} </strong>
                  <span className=textWeak>
                    {string(" - " ++ string_of_int(num) ++ " followers")}
                  </span>
                </p>
              </div>
            </div>
          | Listener(_) =>
            <div className=userKindWrapper>
              <div
                className={flexWrapper(~justify=`flexStart, ~align=`center)}>
                <img
                  src=headphone
                  alt="Listener Icon"
                  className=userKindIcon
                />
                <p className=marginZero>
                  <strong className=textBold> {string("Audience ")} </strong>
                  <span className=textWeak>
                    {string(" - listening with ")}
                    <i> {string("ABC")} </i>
                  </span>
                </p>
              </div>
              <button
                className={Cn.make([button, userKindSwitchBtn, marginZero])}>
                <a
                  className=textBlack
                  href={"http://" ++ Utils.Window.host ++ "/#"}>
                  {string("I wanna be DJ")}
                </a>
              </button>
            </div>
          }
        }
        <pre> {string(trackId ++ string_of_int(positionMs))} </pre>
        <User auth userName userIconUrl setLogOut />
        {
          switch (self.state.userKind, self.state.connectionToDj) {
          | (Listener(_), Connected) => null
          /*Come back animation*/
          | (Listener(_), Connecting(asOfMs)) =>
            switch (asOfMs, self.state.connectionId < 1) {
            | (0., _)
            | (_, true) =>
              <div
                key="connecting"
                className={
                  Cn.make([
                    statusRibon(~ribonColor=`hex("cfcfcf")),
                    scaleAnimation,
                  ])
                }>
                {string("Connecting ...")}
              </div>
            | _ =>
              <div
                key="offline"
                className={
                  Cn.make([
                    statusRibon(~ribonColor=`hex("1DB954f0")),
                    scaleAnimation,
                  ])
                }>
                {string("DJ is offline")}
              </div>
            }

          | (Listener(_), Disconnected) =>
            <div
              key="offline"
              className={
                Cn.make([
                  statusRibon(~ribonColor=`hex("1DB954f0")),
                  scaleAnimation,
                ])
              }>
              {string("DJ is offline")}
            </div>
          | (Listener(_), Error) =>
            <div
              key="error"
              className={
                Cn.make([
                  statusRibon(~ribonColor=`hex("b50303")),
                  scaleAnimation,
                ])
              }>
              <small>
                {string("Whops.. Something is wrong. Try Refresh")}
              </small>
            </div>
          | (DJ(_), _) => null
          }
        }
        <PreviouslyPlayed trackList={self.state.previousTrackIds} />
        <div
          className={
            switch (self.state.userKind, self.state.connectionToDj) {
            | (DJ(_), _)
            | (Listener(_), Connected) => ""
            | (Listener(_), _) => unactiveStyle
            }
          }>
          <CurrentlyPlaying
            songName
            artistName
            isPlaying
            progressPct
            albumImageUrl
          />
        </div>
        <LinkShare
          peerId={
            switch (self.state.peerId) {
            | Some(peerId) => BsUuid.Uuid.V4.toString(peerId)
            | None => ""
            }
          }
        />
      </div>
    </div>,
};
