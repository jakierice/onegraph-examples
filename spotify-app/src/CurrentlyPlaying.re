open BsReactstrap;
open Utils;
open Emotion;

module Css = AppStyle;
let songImage = requireAssetURI("./img/now-playing-ex.png");
let audioWave = requireAssetURI("./img/audio-wave.png");
let like = requireAssetURI("./img/like.png");
let share = requireAssetURI("./img/share.png");

type action =
  | Toggle;

type state = {isDropdownOpen: bool};

let playerWrapper = [%css
  [
    width(`px(300)),
    position(`relative),
    margin3(`px(0), `auto, `px(64)),
  ]
];

let songNameStyle = [%css
  [fontSize(`px(24)), marginBottom(`px(0)), textAlign(`left)]
];

let artistNameStyle = [%css
  [fontSize(`px(14)), marginBottom(`px(0)), textAlign(`left)]
];

let audioWaveStyle = [%css [height(`px(48))]];

let actionBtnStyle = [%css
  [
    width(`px(32)),
    border(`px(2), `solid, `hex("ffffff")),
    borderRadius(`pct(50.)),
    padding(`px(4)),
    margin(`px(4)),
    select(":hover", [cursor(`pointer), backgroundColor(`hex("525252"))]),
  ]
];

let progressBarStyle = [%css [width(`px(200)), height(`px(4))]];

let albumImage = [%css [width(`pct(100.))]];

let component = ReasonReact.reducerComponent("User");

let make =
    (~songName, ~artistName, ~isPlaying, ~progressPct, ~imageUrl, _children) => {
  ...component,
  initialState: () => {isDropdownOpen: false},
  reducer: (action, state) =>
    switch (action) {
    | Toggle => ReasonReact.Update({isDropdownOpen: !state.isDropdownOpen})
    },
  render: self =>
    ReasonReact.(
      <div className="current-playing">
        <div className=playerWrapper>
          <img className=albumImage src=imageUrl alt="Album Image" />
          <div
            className={
              Cn.make([
                Css.flexWrapper(~justify=`spaceBetween, ~align=`center),
              ])
            }>
            <div>
              <h3 className=songNameStyle> {string(songName)} </h3>
              <p className=artistNameStyle> {string(artistName)} </p>
            </div>
            <img className=audioWaveStyle src=audioWave alt="Audio Wave" />
          </div>
          <div
            className={
              Cn.make([
                Css.flexWrapper(~justify=`spaceBetween, ~align=`center),
              ])
            }>
            <Progress
              color="success"
              className=progressBarStyle
              value=progressPct
            />
            <img className=actionBtnStyle src=share alt="Share Icon" />
            <img className=actionBtnStyle src=like alt="Like Icon" />
          </div>
        </div>
      </div>
    ),
};