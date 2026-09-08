// Display and audio command payloads used by the existing FreeRTOS queues.
#pragma once

#include <cstdint>
#include <type_traits>
#include "CommandPath.hpp"

struct DISPLAY_COMMAND {
  enum MODULE {DIS, AUDIO} module;
  enum DISPLAY_STATE {SHOW, CLEAR} display_state;
  CommandPath path;
};

struct AUDIO_COMMAND {
  enum MODULE {DIS, AUDIO} module;
  enum AUDIO_STATE {PLAY, PUASE, STOP, SEEK} audio_state;
  CommandPath path;    // Owned path, safe for byte-copy queues.
  int32_t trackIndex = -1;
  uint32_t autoAdvanceFrom = 0; // Reject stale end-of-track requests.
  uint32_t seek_time;  // ใช้สำหรับระบุวินาทีที่จะกรอเพลงไป (เฉพาะตอนสั่ง SEEK)
};
static_assert(std::is_trivially_copyable<DISPLAY_COMMAND>::value, "Display queue payload must own its data");
static_assert(std::is_trivially_copyable<AUDIO_COMMAND>::value, "Audio queue payload must own its data");
