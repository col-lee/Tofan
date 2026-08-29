#pragma once

#include <Arduino.h>

namespace app {

struct RuntimeState {
    bool isRecordingMode = false;
    bool isRecording = false;
    bool aiPetListening = false;
    volatile bool aiPetProcessing = false;
    unsigned long aiPetRecordingStartedAt = 0;
    unsigned long backBtnPressTime = 0;
    bool isBackBtnLongPressed = false;
    unsigned long lastVolActivityTime = 0;
};

extern RuntimeState runtime;

} // namespace app
