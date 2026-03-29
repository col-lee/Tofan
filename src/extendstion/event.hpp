struct DISPLAY_COMMAND {
  enum MODULE {DIS, AUDIO} module;
  enum DISPLAY_STATE {SHOW, CLEAR} display_state;
  String path;
};

struct AUDIO_COMMAND {
  enum MODULE {DIS, AUDIO} module;
  // เพิ่ม SEEK เข้าไปใน enum
  enum AUDIO_STATE {PLAY, PUASE, STOP, SEEK} audio_state; 
  String path;         // ใช้สำหรับเปลี่ยนเพลง
  uint32_t seek_time;  // ใช้สำหรับระบุวินาทีที่จะกรอเพลงไป (เฉพาะตอนสั่ง SEEK)
};