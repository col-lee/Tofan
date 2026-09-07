// Maps rotary encoder and button input to UI navigation and device actions.
#include "InputController.hpp"
#include "AppCoordinator.hpp"
#include "../core/SharedResources.hpp"
#include "../display/DisplayManager.hpp"
#include "../network/Network.hpp"
#include "../audio/SoundManager.hpp"
#include "../hardware/IOManager.hpp"
#include "../hardware/HardwareManager.hpp"
#include "../ai/AIConversation.hpp"
#include "../core/GlobalState.hpp"
#include <AiEsp32RotaryEncoder.h>
#include "../core/MediaNavigation.hpp"

InputController inputController;

namespace {
AiEsp32RotaryEncoder rotaryEncoder = AiEsp32RotaryEncoder(ENC_A_PIN, ENC_B_PIN, -1, ENC_VCC, ENC_STEPS, false);

void IRAM_ATTR readEncoderISR() {
    rotaryEncoder.readEncoder_ISR();
}

void startAIPetListening() {
    appCoordinator.startAiPetListening();
}

void stopAIPetListening() {
    appCoordinator.stopAiPetListening();
}
} // namespace

void InputController::begin() {
    rotaryEncoder.begin();
    rotaryEncoder.setup(readEncoderISR);
    rotaryEncoder.setBoundaries(-DISM.boundaries_home, DISM.boundaries_home, false);
    rotaryEncoder.setEncoderValue(DISM.animatedMenuIndex_target);
    rotaryEncoder.disableAcceleration();
    ioManager.initPins();
}

void InputController::update() {
    if (DISM.currentState == UI_STATE::APP_DISPLAY_CLOSING) {
        if (!DISM.mediaClearComplete.load()) return;
        DISM.createUISprite();
        rotaryEncoder.setBoundaries(0, media::lastIndex(DISM.imageNames.size()), true);
        rotaryEncoder.setEncoderValue(DISM.imageSelectedIndex);
        DISM.currentState = UI_STATE::APP_DISPLAY_LIST;
        DISM.drawImageList();
    }
    if (DISM.currentState == UI_STATE::HOME_MENU && DISM.isAnimatingMenu) {
        DISM.drawHomeMenu();
        vTaskDelay(1);
    }

    if (DISM.currentState == UI_STATE::GLOBAL_VOLUME) {
        if (millis() - lastVolActivityTime > 2000) {
            DISM.currentState = DISM.previousState;
            if (DISM.currentState == UI_STATE::HOME_MENU) {
                rotaryEncoder.setBoundaries(-DISM.boundaries_home, DISM.boundaries_home, false);
                rotaryEncoder.setEncoderValue(DISM.animatedMenuIndex_target);
                DISM.drawHomeMenu();
            } else if (DISM.currentState == UI_STATE::APP_MUSIC) {
                rotaryEncoder.setBoundaries(0, 4, true);
                rotaryEncoder.setEncoderValue(DISM.currentMusicControlIndex);
                DISM.drawMusicPlayer(currentSongTitle, currentAudioProgress, isPlayingAudio);
            } else if (DISM.currentState == UI_STATE::APP_MUSIC_LIST) {
                rotaryEncoder.setBoundaries(0, media::lastIndex(DISM.playlistNames.size()), true);
                rotaryEncoder.setEncoderValue(DISM.playlistSelectedIndex);
                DISM.drawMusicList();
            } else if (DISM.currentState == UI_STATE::APP_DISPLAY_LIST) {
                rotaryEncoder.setBoundaries(0, media::lastIndex(DISM.imageNames.size()), true);
                rotaryEncoder.setEncoderValue(DISM.imageSelectedIndex);
                DISM.drawImageList();
            } else if (DISM.currentState == UI_STATE::APP_SETTINGS) {
                rotaryEncoder.setBoundaries(0, 1, true);
                rotaryEncoder.setEncoderValue(DISM.settingSelectedIndex);
                DISM.drawSettings();
            } else if (DISM.currentState == UI_STATE::APP_ONLINE_MUSIC) {
                rotaryEncoder.setBoundaries(0, 3, true);
                rotaryEncoder.setEncoderValue(DISM.currentMusicControlIndex);
                DISM.drawOnlineMusicPlayer();
            }
        }
    }

    static unsigned long lastPlayerRefresh = 0;
    if (millis() - lastPlayerRefresh >= 200) {
        lastPlayerRefresh = millis();
        if (DISM.currentState == UI_STATE::APP_MUSIC)
            DISM.drawMusicPlayer(currentSongTitle, currentAudioProgress, isPlayingAudio);
        else if (DISM.currentState == UI_STATE::APP_ONLINE_MUSIC)
            DISM.drawOnlineMusicPlayer();
    }

    if (DISM.currentState == UI_STATE::DEBUG) {
        DISM.debug();
        vTaskDelay(5);
    }

    if (DISM.currentState == UI_STATE::RECORDE) {
        DISM.recorde();
        recordLoop();
        vTaskDelay(1);
    }

    if (!app::runtime.isRecordingMode) {
        detectWord();
    }

    handleInput();
}

void InputController::handleInput() {
    if (DISM.currentState == UI_STATE::APP_DISPLAY_CLOSING) return;
    if (rotaryEncoder.encoderChanged()) {
        if (DISM.currentState == UI_STATE::HOME_MENU) {
            int rawValue = rotaryEncoder.readEncoder();
            DISM.animatedMenuIndex_target = (float)rawValue;
            DISM.currentMenuIndex = (rawValue % DISM.totalItems + DISM.totalItems) % DISM.totalItems;
            DISM.isAnimatingMenu = true;
        } else if (DISM.currentState == UI_STATE::DEBUG) {
            DISM.debugSelectedIndex = rotaryEncoder.readEncoder();
            DISM.debug();
        } else if (DISM.currentState == UI_STATE::APP_MUSIC) {
            DISM.currentMusicControlIndex = rotaryEncoder.readEncoder();
            DISM.drawMusicPlayer(currentSongTitle, currentAudioProgress, isPlayingAudio);
        } else if (DISM.currentState == UI_STATE::APP_MUSIC_LIST) {
            if (!DISM.playlistNames.empty()) {
                DISM.playlistSelectedIndex = rotaryEncoder.readEncoder();
                DISM.drawMusicList();
            }
        } else if (DISM.currentState == UI_STATE::APP_DISPLAY_LIST) {
            if (!DISM.imageNames.empty()) {
                DISM.imageSelectedIndex = rotaryEncoder.readEncoder();
                DISM.drawImageList();
            }
        } else if (DISM.currentState == UI_STATE::APP_SETTINGS) {
            DISM.settingSelectedIndex = rotaryEncoder.readEncoder();
            DISM.drawSettings();
        } else if (DISM.currentState == UI_STATE::GLOBAL_VOLUME) {
            DISM.currentVolLevel = rotaryEncoder.readEncoder();
            audio.setVolume(DISM.currentVolLevel);
            DISM.drawVolumeOverlay();
            lastVolActivityTime = millis();
        } else if (DISM.currentState == UI_STATE::POPUP_NO_MUSIC) {
            DISM.popupSelectedIndex = rotaryEncoder.readEncoder();
            DISM.drawPopupNoMusic();
        } else if (DISM.currentState == UI_STATE::APP_ONLINE_MUSIC) {
            DISM.currentMusicControlIndex = rotaryEncoder.readEncoder();
            DISM.drawOnlineMusicPlayer();
        }
    }

    if (ioManager.isButtonPressed(ENC_SW) && millis() - lastTouchTime > BUTTON_DEBOUNCE_MS) {
        lastTouchTime = millis();

        if (DISM.currentState == UI_STATE::HOME_MENU) {
            DISPLAY_COMMAND cmd;
            switch (DISM.currentMenuIndex) {
                case 0:
                    DISM.loadImageList();
                    rotaryEncoder.setBoundaries(0, media::lastIndex(DISM.imageNames.size()), true);
                    rotaryEncoder.setEncoderValue(DISM.imageSelectedIndex);
                    DISM.currentState = UI_STATE::APP_DISPLAY_LIST;
                    DISM.drawImageList();
                    break;
                case 1:
                    DISM.loadMusicList();
                    rotaryEncoder.setBoundaries(0, 4, true);
                    DISM.currentMusicControlIndex = 1;
                    rotaryEncoder.setEncoderValue(1);
                    DISM.currentState = UI_STATE::APP_MUSIC;
                    DISM.drawMusicPlayer(currentSongTitle, currentAudioProgress, isPlayingAudio);
                    break;
                case 2:
                    rotaryEncoder.setBoundaries(0, 1, true);
                    rotaryEncoder.setEncoderValue(DISM.settingSelectedIndex);
                    DISM.currentState = UI_STATE::APP_SETTINGS;
                    DISM.drawSettings();
                    break;
                case 3:
                    DISM.currentState = UI_STATE::APP_PET;
                    startAIPetListening();
                    DISM.drawAIPet();
                    break;
                case 4:
                    DISM.debugSelectedIndex = 0;
                    DISM.debugScrollOffset = 0;
                    rotaryEncoder.setBoundaries(0, hwManager.getDeviceCount() - 1, false);
                    rotaryEncoder.setEncoderValue(DISM.debugSelectedIndex);
                    DISM.currentState = UI_STATE::DEBUG;
                    DISM.debug();
                    break;
                case 5:
                    if (enterRecordingMode()) {
                        DISM.seconds = 0;
                        DISM.previousMillis = millis();
                        DISM.currentState = UI_STATE::RECORDE;
                        DISM.recorde();
                    } else {
                        Serial.println("Unable to enter recording mode");
                    }
                    break;
            }
        } else if (DISM.currentState == UI_STATE::APP_DISPLAY_LIST) {
            if (media::validIndex(DISM.imageSelectedIndex, DISM.imagePaths.size())) {
                String selectedPath = DISM.imagePaths[DISM.imageSelectedIndex];
                DISM.deleteUISprite();
                DISM.currentState = UI_STATE::APP_DISPLAY;
                DISPLAY_COMMAND cmd;
                cmd.module = DISPLAY_COMMAND::MODULE::DIS;
                cmd.display_state = DISPLAY_COMMAND::DISPLAY_STATE::SHOW;
                cmd.path = selectedPath;
                xQueueSend(display_command, &cmd, portMAX_DELAY);
            }
        } else if (DISM.currentState == UI_STATE::APP_MUSIC) {
            AUDIO_COMMAND cmd;
            cmd.module = AUDIO_COMMAND::MODULE::AUDIO;

            if (DISM.currentMusicControlIndex == 0) {
                if (!DISM.playlistNames.empty()) {
                    DISM.currentPlayingIndex = media::wrapIndex(DISM.currentPlayingIndex < 0 ? -1 : DISM.currentPlayingIndex - 1, DISM.playlistPaths.size());
                    cmd.audio_state = AUDIO_COMMAND::AUDIO_STATE::PLAY;
                    cmd.path = DISM.playlistPaths[DISM.currentPlayingIndex];
                    xQueueSend(audio_command, &cmd, portMAX_DELAY);
                }
            } else if (DISM.currentMusicControlIndex == 1) {
                if (isPlayingAudio && !isOnlineAudio) cmd.audio_state = AUDIO_COMMAND::AUDIO_STATE::PUASE;
                else {
                    if (DISM.playlistPaths.empty()) return;
                    cmd.audio_state = AUDIO_COMMAND::AUDIO_STATE::PLAY;
                    if (!isOnlineAudio && hasPausedAudio) cmd.path = "";
                    else {
                        DISM.currentPlayingIndex = media::wrapIndex(DISM.currentPlayingIndex < 0 ? 0 : DISM.currentPlayingIndex, DISM.playlistPaths.size());
                        cmd.path = DISM.playlistPaths[DISM.currentPlayingIndex];
                    }
                }
                xQueueSend(audio_command, &cmd, portMAX_DELAY);
                DISM.drawMusicPlayer(currentSongTitle, currentAudioProgress, isPlayingAudio);
            } else if (DISM.currentMusicControlIndex == 2) {
                if (!DISM.playlistNames.empty()) {
                    DISM.currentPlayingIndex++;
                    if (DISM.currentPlayingIndex >= (int)DISM.playlistNames.size()) DISM.currentPlayingIndex = 0;
                    cmd.audio_state = AUDIO_COMMAND::AUDIO_STATE::PLAY;
                    cmd.path = DISM.playlistPaths[DISM.currentPlayingIndex];
                    xQueueSend(audio_command, &cmd, portMAX_DELAY);
                }
            } else if (DISM.currentMusicControlIndex == 4) {
                DISM.currentMusicControlIndex = 1;
                rotaryEncoder.setBoundaries(0, 3, true);
                rotaryEncoder.setEncoderValue(1);
                DISM.currentState = UI_STATE::APP_ONLINE_MUSIC;
                DISM.drawOnlineMusicPlayer();
            } else if (DISM.currentMusicControlIndex == 3) {
                DISM.loadMusicList();
                rotaryEncoder.setBoundaries(0, media::lastIndex(DISM.playlistNames.size()), true);
                rotaryEncoder.setEncoderValue(DISM.playlistSelectedIndex);
                DISM.currentState = UI_STATE::APP_MUSIC_LIST;
                DISM.drawMusicList();
            }
        } else if (DISM.currentState == UI_STATE::POPUP_NO_MUSIC) {
            if (DISM.popupSelectedIndex == 0) {
                if (nm.connectoWiFi()) {
                    DISM.isWiFiOn = true;
                    DISM.currentMusicControlIndex = 1;
                    rotaryEncoder.setBoundaries(0, 3, true);
                    rotaryEncoder.setEncoderValue(1);
                    DISM.currentState = UI_STATE::APP_ONLINE_MUSIC;
                    AUDIO_COMMAND cmd;
                    cmd.module = AUDIO_COMMAND::MODULE::AUDIO;
                    cmd.audio_state = AUDIO_COMMAND::AUDIO_STATE::PLAY;
                    cmd.path = onlineStations[currentStationIndex];
                    xQueueSend(audio_command, &cmd, portMAX_DELAY);
                    DISM.drawOnlineMusicPlayer();
                    return;
                } else {
                    DISM.isWiFiOn = false;
                    DISM.currentState = UI_STATE::APP_SETTINGS;
                    rotaryEncoder.setBoundaries(0, 1, true);
                    rotaryEncoder.setEncoderValue(1);
                    DISM.settingSelectedIndex = 1;
                    DISM.drawSettings();
                    return;
                }
            } else {
                DISM.currentState = UI_STATE::HOME_MENU;
                rotaryEncoder.setBoundaries(-DISM.boundaries_home, DISM.boundaries_home, false);
                rotaryEncoder.setEncoderValue(DISM.animatedMenuIndex_target);
                DISM.drawHomeMenu();
            }
        } else if (DISM.currentState == UI_STATE::APP_ONLINE_MUSIC) {
            if (DISM.currentMusicControlIndex == 3) {
                DISM.currentMusicControlIndex = 1;
                rotaryEncoder.setBoundaries(0, 4, true);
                rotaryEncoder.setEncoderValue(1);
                DISM.currentState = UI_STATE::APP_MUSIC;
                DISM.drawMusicPlayer(currentSongTitle, currentAudioProgress, isPlayingAudio);
                return;
            }
            if (WiFi.status() != WL_CONNECTED) {
                DISM.popupSelectedIndex = 0;
                rotaryEncoder.setBoundaries(0, 1, false);
                rotaryEncoder.setEncoderValue(0);
                DISM.currentState = UI_STATE::POPUP_NO_MUSIC;
                DISM.drawPopupNoMusic();
                return;
            }
            AUDIO_COMMAND cmd;
            cmd.module = AUDIO_COMMAND::MODULE::AUDIO;

            if (DISM.currentMusicControlIndex == 0) {
                currentStationIndex = media::wrapIndex(currentStationIndex - 1, MAX_STATIONS);
                cmd.audio_state = AUDIO_COMMAND::AUDIO_STATE::PLAY;
                cmd.path = onlineStations[currentStationIndex];
                xQueueSend(audio_command, &cmd, portMAX_DELAY);
            } else if (DISM.currentMusicControlIndex == 1) {
                if (isPlayingAudio && isOnlineAudio) cmd.audio_state = AUDIO_COMMAND::AUDIO_STATE::PUASE;
                else { cmd.audio_state = AUDIO_COMMAND::AUDIO_STATE::PLAY; cmd.path = onlineStations[currentStationIndex]; }
                xQueueSend(audio_command, &cmd, portMAX_DELAY);
                DISM.drawOnlineMusicPlayer();
            } else if (DISM.currentMusicControlIndex == 2) {
                currentStationIndex = media::wrapIndex(currentStationIndex + 1, MAX_STATIONS);
                cmd.audio_state = AUDIO_COMMAND::AUDIO_STATE::PLAY;
                cmd.path = onlineStations[currentStationIndex];
                xQueueSend(audio_command, &cmd, portMAX_DELAY);
            }
        } else if (DISM.currentState == UI_STATE::APP_MUSIC_LIST) {
            if (media::validIndex(DISM.playlistSelectedIndex, DISM.playlistPaths.size())) {
                DISM.currentPlayingIndex = DISM.playlistSelectedIndex;
                String selectedPath = DISM.playlistPaths[DISM.playlistSelectedIndex];
                currentSongTitle = DISM.playlistNames[DISM.playlistSelectedIndex];
                AUDIO_COMMAND cmd;
                cmd.module = AUDIO_COMMAND::MODULE::AUDIO;
                cmd.audio_state = AUDIO_COMMAND::AUDIO_STATE::PLAY;
                cmd.path = selectedPath;
                xQueueSend(audio_command, &cmd, portMAX_DELAY);
                rotaryEncoder.setBoundaries(0, 4, true);
                rotaryEncoder.setEncoderValue(DISM.currentMusicControlIndex);
                DISM.currentState = UI_STATE::APP_MUSIC;
                DISM.drawMusicPlayer(currentSongTitle, 0, true);
            }
        } else if (DISM.currentState == UI_STATE::APP_SETTINGS) {
            if (DISM.settingSelectedIndex == 0) {
                DISM.isAdminModeOn = !DISM.isAdminModeOn;
                DISM.drawSettings();
                if (DISM.isAdminModeOn) nm.startAdminMode();
                else nm.stopAdminMode();
            } else if (DISM.settingSelectedIndex == 1) {
                DISM.isWiFiOn = !DISM.isWiFiOn;
                DISM.drawSettings();
                if (DISM.isWiFiOn) nm.connectoWiFi();
                else nm.closeWiFiSTA();
            }
        } else if (DISM.currentState == UI_STATE::DEBUG) {
            DISM.debug();
        } else if (DISM.currentState == UI_STATE::RECORDE) {
            if (app::runtime.isRecording) {
                stopRecording();
            } else {
                DISM.seconds = 0;
                DISM.previousMillis = millis();
                if (!startRecording()) {
                    Serial.println("Unable to start recording");
                }
            }
            DISM.recorde();
        }
    }

    if (ioManager.isButtonPressed(BTN_BACK)) {
        if (backBtnPressTime == 0) backBtnPressTime = millis();

        if (millis() - backBtnPressTime > 1000 && !isBackBtnLongPressed && DISM.currentState != UI_STATE::APP_DISPLAY) {
            isBackBtnLongPressed = true;
            DISM.previousState = DISM.currentState;
            DISM.currentState = UI_STATE::GLOBAL_VOLUME;
            rotaryEncoder.setBoundaries(0, 100, false);
            rotaryEncoder.setEncoderValue(DISM.currentVolLevel);
            lastVolActivityTime = millis();
            DISM.drawVolumeOverlay();
        }
    } else {
        if (backBtnPressTime > 0) {
            if (!isBackBtnLongPressed) {
                if (DISM.currentState == UI_STATE::APP_DISPLAY) {
                    DISPLAY_COMMAND cmd;
                    cmd.module = DISPLAY_COMMAND::MODULE::DIS;
                    cmd.display_state = DISPLAY_COMMAND::DISPLAY_STATE::CLEAR;
                    DISM.mediaClearComplete.store(false);
                    xQueueSend(display_command, &cmd, portMAX_DELAY);
                    DISM.currentState = UI_STATE::APP_DISPLAY_CLOSING;
                } else if (DISM.currentState == UI_STATE::APP_MUSIC_LIST) {
                    rotaryEncoder.setBoundaries(0, 4, true);
                    rotaryEncoder.setEncoderValue(DISM.currentMusicControlIndex);
                    DISM.currentState = UI_STATE::APP_MUSIC;
                    DISM.drawMusicPlayer(currentSongTitle, currentAudioProgress, isPlayingAudio);
                } else if (DISM.currentState == UI_STATE::APP_DISPLAY_LIST) {
                    rotaryEncoder.setBoundaries(-DISM.boundaries_home, DISM.boundaries_home, false);
                    rotaryEncoder.setEncoderValue(DISM.animatedMenuIndex_target);
                    DISM.currentState = UI_STATE::HOME_MENU;
                    DISM.drawHomeMenu();
                } else if (DISM.currentState == UI_STATE::APP_SETTINGS) {
                    rotaryEncoder.setBoundaries(-DISM.boundaries_home, DISM.boundaries_home, false);
                    rotaryEncoder.setEncoderValue(DISM.animatedMenuIndex_target);
                    DISM.currentState = UI_STATE::HOME_MENU;
                    DISM.drawHomeMenu();
                } else if (DISM.currentState == UI_STATE::APP_ONLINE_MUSIC) {
                    rotaryEncoder.setBoundaries(-DISM.boundaries_home, DISM.boundaries_home, false);
                    rotaryEncoder.setEncoderValue(DISM.animatedMenuIndex_target);
                    DISM.currentState = UI_STATE::HOME_MENU;
                    DISM.drawHomeMenu();
                } else if (DISM.currentState == UI_STATE::POPUP_NO_MUSIC) {
                    rotaryEncoder.setBoundaries(0, 4, true);
                    rotaryEncoder.setEncoderValue(DISM.currentMusicControlIndex);
                    DISM.currentState = UI_STATE::APP_MUSIC;
                    DISM.drawMusicPlayer(currentSongTitle, currentAudioProgress, isPlayingAudio);
                } else if (DISM.currentState == UI_STATE::APP_PET) {
                    stopAIPetListening();
                    rotaryEncoder.setBoundaries(-DISM.boundaries_home, DISM.boundaries_home, false);
                    rotaryEncoder.setEncoderValue(DISM.animatedMenuIndex_target);
                    DISM.currentState = UI_STATE::HOME_MENU;
                    DISM.drawHomeMenu();
                } else if (DISM.currentState == UI_STATE::DEBUG) {
                    rotaryEncoder.setBoundaries(-DISM.boundaries_home, DISM.boundaries_home, false);
                    rotaryEncoder.setEncoderValue(DISM.animatedMenuIndex_target);
                    DISM.currentState = UI_STATE::HOME_MENU;
                    DISM.drawHomeMenu();
                } else if (DISM.currentState == UI_STATE::RECORDE) {
                    exitRecordingMode();
                    DISM.seconds = 0;
                    rotaryEncoder.setBoundaries(-DISM.boundaries_home, DISM.boundaries_home, false);
                    rotaryEncoder.setEncoderValue(DISM.animatedMenuIndex_target);
                    DISM.currentState = UI_STATE::HOME_MENU;
                    DISM.drawHomeMenu();
                } else {
                    rotaryEncoder.setBoundaries(-DISM.boundaries_home, DISM.boundaries_home, false);
                    rotaryEncoder.setEncoderValue(DISM.animatedMenuIndex_target);
                    DISM.currentState = UI_STATE::HOME_MENU;
                    DISM.drawHomeMenu();
                }
            }
            backBtnPressTime = 0;
            isBackBtnLongPressed = false;
        }
    }
}
