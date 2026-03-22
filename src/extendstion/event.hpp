struct DISPLAY_COMMAND {
  enum MODULE {DIS, AUDIO} module;
  enum DISPLAY_STATE {SHOW, CLEAR} display_state;
  String path;
};

struct AUDIO_COMMAND {
  enum MODULE {DIS, AUDIO} module;
  enum AUDIO_STATE {PLAY, PUASE, STOP} audio_state;
  String path;
};