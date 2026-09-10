// Encoder navigation, categorized settings and playback actions.
#include "InputController.hpp"
#include "AppCoordinator.hpp"
#include "../display/DisplayManager.hpp"
#include "../audio/SoundManager.hpp"
#include "../network/Network.hpp"
#include "../hardware/IOManager.hpp"
#include "../hardware/HardwareManager.hpp"
#include "../core/GlobalState.hpp"
#include "../core/MediaNavigation.hpp"
#include "../core/UserSettings.hpp"
#include <AiEsp32RotaryEncoder.h>

InputController inputController;
namespace {
AiEsp32RotaryEncoder rotaryEncoder(ENC_A_PIN,ENC_B_PIN,-1,ENC_VCC,ENC_STEPS,false);
void IRAM_ATTR readEncoderISR() { rotaryEncoder.readEncoder_ISR(); }
int volumeEncoder = 0;
bool selectHeld = false;
unsigned long lastRefresh = 0, lastVolumeChange = 0;
bool volumeDirty = false;
using Page=ui::SettingsPage;

void saveSettings() { userSettings.save(); }
void drawCurrent() {
    switch(DISM.currentState) {
        case UI_STATE::HOME_MENU: DISM.drawHomeMenu(); break;
        case UI_STATE::APP_MUSIC: DISM.drawMusicPlayer(currentSongTitle,currentAudioProgress,isPlayingAudio); break;
        case UI_STATE::APP_ONLINE_MUSIC: DISM.drawOnlineMusicPlayer(); break;
        case UI_STATE::APP_MUSIC_LIST: DISM.drawMusicList(); break;
        case UI_STATE::APP_DISPLAY_LIST: DISM.drawImageList(); break;
        case UI_STATE::APP_SETTINGS: DISM.drawSettings(); break;
        case UI_STATE::APP_COLOR_PICKER: DISM.drawColorPicker(); break;
        case UI_STATE::GLOBAL_VOLUME: DISM.drawVolumeOverlay(); break;
        case UI_STATE::POPUP_NO_MUSIC: DISM.drawPopupNoMusic(); break;
        case UI_STATE::APP_PET: DISM.drawAIPet(); break;
        case UI_STATE::RECORDE: DISM.recorde(); break;
        case UI_STATE::DEBUG: DISM.debug(); break;
        default: break;
    }
}
void configureEncoder() {
    int maximum=0,value=0; bool wrap=true;
    switch(DISM.currentState) {
        case UI_STATE::HOME_MENU: maximum=5;value=DISM.currentMenuIndex;break;
        case UI_STATE::APP_MUSIC: maximum=4;value=DISM.currentMusicControlIndex;break;
        case UI_STATE::APP_ONLINE_MUSIC: maximum=3;value=DISM.currentMusicControlIndex;break;
        case UI_STATE::APP_MUSIC_LIST: maximum=media::lastIndex(DISM.playlistPaths.size());value=DISM.playlistSelectedIndex;break;
        case UI_STATE::APP_DISPLAY_LIST: maximum=media::lastIndex(DISM.imagePaths.size());value=DISM.imageSelectedIndex;break;
        case UI_STATE::APP_SETTINGS: maximum=DISM.settingsCount()-1;value=DISM.settingSelectedIndex;break;
        case UI_STATE::POPUP_NO_MUSIC: maximum=1;value=DISM.popupSelectedIndex;break;
        case UI_STATE::DEBUG: maximum=media::lastIndex(hwManager.getDeviceCount());value=DISM.debugSelectedIndex;break;
        case UI_STATE::APP_COLOR_PICKER: {
            auto c=userSettings.values.colors[DISM.colorRole];
            maximum=DISM.colorPhase==0?359:100;
            value=DISM.colorPhase==0?c.hue:DISM.colorPhase==1?c.saturation:c.value;
            wrap=DISM.colorPhase==0; break;
        }
        case UI_STATE::GLOBAL_VOLUME:
            volumeEncoder=0;
            rotaryEncoder.setBoundaries(-2000,2000,false); rotaryEncoder.setEncoderValue(0); return;
        case UI_STATE::APP_PET:
            rotaryEncoder.setBoundaries(-2000,2000,false); rotaryEncoder.setEncoderValue(0); return;
        default:break;
    }
    rotaryEncoder.setBoundaries(0,maximum,wrap);
    rotaryEncoder.setEncoderValue(preferences::clamp(value,0,maximum));
}
void enter(UI_STATE state) { DISM.currentState=state; configureEncoder(); drawCurrent(); }
void settingsPage(Page page) {
    DISM.settingsPage=page; DISM.settingSelectedIndex=0; DISM.settingsScroll=0;
    enter(UI_STATE::APP_SETTINGS);
}
bool sendAudio(AUDIO_COMMAND::AUDIO_STATE action,const String& path="",uint32_t expected=0,int trackIndex=-1) {
    AUDIO_COMMAND cmd{}; cmd.module=AUDIO_COMMAND::AUDIO; cmd.audio_state=action;
    cmd.path=path; cmd.autoAdvanceFrom=expected; cmd.trackIndex=trackIndex;
    return xQueueSend(audio_command,&cmd,pdMS_TO_TICKS(100))==pdPASS;
}
void playTrack(int index,uint32_t expected=0) {
    if(!media::validIndex(index,DISM.playlistPaths.size())) return;
    if(sendAudio(AUDIO_COMMAND::PLAY,DISM.playlistPaths[index],expected,index) && !expected) DISM.currentPlayingIndex=index;
}
void nextTrack(uint32_t expected=0) {
    playTrack(preferences::nextTrack(DISM.currentPlayingIndex,DISM.playlistPaths.size(),userSettings.values.shuffle,esp_random()),expected);
}
void applyNetwork() {
    DISM.isWiFiOn=userSettings.values.wifi; DISM.isAdminModeOn=userSettings.values.admin;
    requestNetworkSettings(DISM.isWiFiOn,DISM.isAdminModeOn);
}
void settingClick() {
    auto& v=userSettings.values;
    if(userSettings.saveFailed) { saveSettings(); drawCurrent(); return; }
    const int i=DISM.settingSelectedIndex;
    switch(DISM.settingsPage) {
        case Page::Root: settingsPage(static_cast<Page>(i+1)); return;
        case Page::Display:
            if(i<6) {
                DISM.colorRole=i;DISM.colorPhase=0;DISM.colorBackup=v.colors[i];
                enter(UI_STATE::APP_COLOR_PICKER);return;
            }
            if(i==6) { preferences::Values defaults; for(int j=0;j<6;++j) v.colors[j]=defaults.colors[j]; DISM.applyTheme(); }
            break;
        case Page::Network:
            if(i==0) v.wifi=!v.wifi; else v.admin=!v.admin;
            applyNetwork(); break;
        case Page::Sound:
            if(i==0) v.autoNext=!v.autoNext;
            else if(i==1) v.shuffle=!v.shuffle;
            else if(i==2) v.volumeStep=v.volumeStep>=5?2:v.volumeStep+1;
            else { DISM.previousState=UI_STATE::APP_SETTINGS; enter(UI_STATE::GLOBAL_VOLUME); return; }
            break;
        case Page::Voice: v.voice=!v.voice;setVoiceAssistantEnabled(v.voice);break;
    }
    saveSettings(); drawCurrent();
}
void finishVolume() {
    if(volumeDirty || userSettings.saveFailed) { saveSettings(); volumeDirty=false; }
    enter(DISM.previousState);
}
}

void InputController::begin() {
    rotaryEncoder.begin(); rotaryEncoder.setup(readEncoderISR); rotaryEncoder.disableAcceleration();
    ioManager.initPins(); DISM.currentVolLevel=userSettings.values.volume;
    applyNetwork(); configureEncoder();
}
void InputController::update() {
    if(DISM.currentState==UI_STATE::APP_DISPLAY_CLOSING) {
        if(DISM.mediaClearComplete.load()) { DISM.createUISprite(); enter(UI_STATE::APP_DISPLAY_LIST); }
    }
    if(app::runtime.isRecordingMode) recordLoop();
    detectWord(); // The recognizer itself is gated by the saved Voice Assistant switch.
    const int started=consumeStartedTrack();
    if(started>=0) DISM.currentPlayingIndex=started;
    handleInput();
    const uint32_t finished=consumeFinishedTrack();
    if(finished && userSettings.values.autoNext) nextTrack(finished);
    if(volumeDirty && millis()-lastVolumeChange>500) { saveSettings(); volumeDirty=false; }
    if(DISM.currentState==UI_STATE::GLOBAL_VOLUME && millis()-lastVolActivityTime>2000) finishVolume();
    if(millis()-lastRefresh>=200) {
        lastRefresh=millis();
        switch(DISM.currentState) {
            case UI_STATE::APP_MUSIC: case UI_STATE::APP_ONLINE_MUSIC:
            case UI_STATE::APP_SETTINGS: case UI_STATE::DEBUG: case UI_STATE::RECORDE: drawCurrent(); break;
            default:break;
        }
    }
    vTaskDelay(pdMS_TO_TICKS(1));
}
void InputController::handleInput() {
    if(DISM.currentState==UI_STATE::APP_DISPLAY_CLOSING) return;
    if(rotaryEncoder.encoderChanged()) {
        const int value=rotaryEncoder.readEncoder();
        switch(DISM.currentState) {
            case UI_STATE::APP_PET:
                appCoordinator.reactToAiPetRotation(value);
                rotaryEncoder.setEncoderValue(0);
                break;
            case UI_STATE::HOME_MENU: DISM.currentMenuIndex=value;break;
            case UI_STATE::APP_MUSIC: case UI_STATE::APP_ONLINE_MUSIC: DISM.currentMusicControlIndex=value;break;
            case UI_STATE::APP_MUSIC_LIST: DISM.playlistSelectedIndex=value;break;
            case UI_STATE::APP_DISPLAY_LIST: DISM.imageSelectedIndex=value;break;
            case UI_STATE::APP_SETTINGS: DISM.settingSelectedIndex=value;break;
            case UI_STATE::DEBUG: DISM.debugSelectedIndex=value;break;
            case UI_STATE::POPUP_NO_MUSIC: DISM.popupSelectedIndex=value;break;
            case UI_STATE::APP_COLOR_PICKER: {
                auto& c=userSettings.values.colors[DISM.colorRole];
                if(DISM.colorPhase==0) c.hue=value;else if(DISM.colorPhase==1) c.saturation=value;else c.value=value;
                DISM.applyTheme();break;
            }
            case UI_STATE::GLOBAL_VOLUME:
                DISM.currentVolLevel=preferences::adjustVolume(DISM.currentVolLevel,value-volumeEncoder,userSettings.values.volumeStep);
                volumeEncoder=value;
                if(value<=-1900 || value>=1900) { rotaryEncoder.setEncoderValue(0);volumeEncoder=0; }
                userSettings.values.volume=DISM.currentVolLevel;
                setOutputVolume(DISM.currentVolLevel);
                lastVolActivityTime=lastVolumeChange=millis();volumeDirty=true;break;
            default:break;
        }
        drawCurrent();
    }
    const bool pressed=ioManager.isButtonPressed(ENC_SW);
    const bool click=pressed && !selectHeld && millis()-lastTouchTime>BUTTON_DEBOUNCE_MS;
    selectHeld=pressed;
    if(click) {
        lastTouchTime=millis();
        if(DISM.currentState==UI_STATE::HOME_MENU) {
            switch(DISM.currentMenuIndex) {
                case 0: DISM.loadImageList();enter(UI_STATE::APP_DISPLAY_LIST);break;
                case 1: DISM.loadMusicList();DISM.currentMusicControlIndex=1;enter(UI_STATE::APP_MUSIC);break;
                case 2: settingsPage(Page::Root);break;
                case 3: DISM.setPetMood(ui::PetMood::Happy,"hi~",1600);enter(UI_STATE::APP_PET);appCoordinator.startAiPetListening();break;
                case 4: DISM.debugSelectedIndex=0;DISM.debugScrollOffset=0;enter(UI_STATE::DEBUG);break;
                case 5:
                    if(enterRecordingMode()) { DISM.seconds=0;DISM.previousMillis=millis();enter(UI_STATE::RECORDE); }
                    break;
            }
        } else if(DISM.currentState==UI_STATE::APP_DISPLAY_LIST) {
            if(media::validIndex(DISM.imageSelectedIndex,DISM.imagePaths.size())) {
                DISPLAY_COMMAND cmd{};cmd.module=DISPLAY_COMMAND::DIS;cmd.display_state=DISPLAY_COMMAND::SHOW;
                cmd.path=DISM.imagePaths[DISM.imageSelectedIndex];
                DISM.deleteUISprite();DISM.currentState=UI_STATE::APP_DISPLAY;
                xQueueSend(display_command,&cmd,portMAX_DELAY);
            }
        } else if(DISM.currentState==UI_STATE::APP_MUSIC) {
            int i=DISM.currentMusicControlIndex;
            if(i==0) playTrack(media::wrapIndex(DISM.currentPlayingIndex<0?-1:DISM.currentPlayingIndex-1,DISM.playlistPaths.size()));
            else if(i==1) {
                if(isPlayingAudio && !isOnlineAudio) sendAudio(AUDIO_COMMAND::PUASE);
                else if(hasPausedAudio && !isOnlineAudio) sendAudio(AUDIO_COMMAND::PLAY);
                else if(media::validIndex(DISM.currentPlayingIndex,DISM.playlistPaths.size())) playTrack(DISM.currentPlayingIndex);
                else nextTrack();
            } else if(i==2) nextTrack();
            else if(i==3) { DISM.loadMusicList();enter(UI_STATE::APP_MUSIC_LIST); }
            else { DISM.currentMusicControlIndex=1;enter(UI_STATE::APP_ONLINE_MUSIC); }
        } else if(DISM.currentState==UI_STATE::APP_MUSIC_LIST) {
            playTrack(DISM.playlistSelectedIndex);DISM.currentMusicControlIndex=1;enter(UI_STATE::APP_MUSIC);
        } else if(DISM.currentState==UI_STATE::APP_ONLINE_MUSIC) {
            int i=DISM.currentMusicControlIndex;
            if(i==3) { DISM.currentMusicControlIndex=1;enter(UI_STATE::APP_MUSIC); }
            else if(i==1 && isPlayingAudio && isOnlineAudio) sendAudio(AUDIO_COMMAND::PUASE);
            else if(WiFi.status()!=WL_CONNECTED) {
                if(!networkSettingsBusy()) { DISM.popupSelectedIndex=0;enter(UI_STATE::POPUP_NO_MUSIC); }
            } else {
                if(i!=1) currentStationIndex=media::wrapIndex(currentStationIndex+(i==0?-1:1),MAX_STATIONS);
                sendAudio(AUDIO_COMMAND::PLAY,onlineStations[currentStationIndex]);
            }
        } else if(DISM.currentState==UI_STATE::POPUP_NO_MUSIC) {
            if(DISM.popupSelectedIndex==0) { userSettings.values.wifi=1;saveSettings();applyNetwork(); }
            DISM.currentMusicControlIndex=1;enter(UI_STATE::APP_ONLINE_MUSIC);
        } else if(DISM.currentState==UI_STATE::APP_SETTINGS) {
            settingClick();
            if(DISM.currentState==UI_STATE::GLOBAL_VOLUME) lastVolActivityTime=millis();
        } else if(DISM.currentState==UI_STATE::APP_COLOR_PICKER) {
            if(DISM.colorPhase<2) { ++DISM.colorPhase;configureEncoder();drawCurrent(); }
            else { saveSettings();enter(UI_STATE::APP_SETTINGS); }
        } else if(DISM.currentState==UI_STATE::APP_PET) {
            appCoordinator.reactToAiPetTouch();
            drawCurrent();
        } else if(DISM.currentState==UI_STATE::GLOBAL_VOLUME) finishVolume();
        else if(DISM.currentState==UI_STATE::RECORDE) {
            if(app::runtime.isRecording) stopRecording();
            else { DISM.seconds=0;DISM.previousMillis=millis();startRecording(); }
            drawCurrent();
        }
    }
    if(ioManager.isButtonPressed(BTN_BACK)) {
        if(backBtnPressTime==0) backBtnPressTime=millis();
        if(millis()-backBtnPressTime>1000 && !isBackBtnLongPressed &&
           DISM.currentState!=UI_STATE::APP_DISPLAY && DISM.currentState!=UI_STATE::APP_COLOR_PICKER &&
           DISM.currentState!=UI_STATE::GLOBAL_VOLUME) {
            isBackBtnLongPressed=true;DISM.previousState=DISM.currentState;
            lastVolActivityTime=millis();enter(UI_STATE::GLOBAL_VOLUME);
        }
    } else if(backBtnPressTime>0) {
        if(!isBackBtnLongPressed) {
            if(DISM.currentState==UI_STATE::APP_DISPLAY) {
                DISPLAY_COMMAND cmd{};cmd.module=DISPLAY_COMMAND::DIS;cmd.display_state=DISPLAY_COMMAND::CLEAR;
                DISM.mediaClearComplete.store(false);xQueueSend(display_command,&cmd,portMAX_DELAY);
                DISM.currentState=UI_STATE::APP_DISPLAY_CLOSING;
            } else if(DISM.currentState==UI_STATE::APP_COLOR_PICKER) {
                userSettings.values.colors[DISM.colorRole]=DISM.colorBackup;DISM.applyTheme();enter(UI_STATE::APP_SETTINGS);
            } else if(DISM.currentState==UI_STATE::APP_SETTINGS && DISM.settingsPage!=Page::Root) {
                const int parent=static_cast<int>(DISM.settingsPage)-1;
                settingsPage(Page::Root);DISM.settingSelectedIndex=parent;configureEncoder();drawCurrent();
            } else if(DISM.currentState==UI_STATE::APP_MUSIC_LIST) enter(UI_STATE::APP_MUSIC);
            else if(DISM.currentState==UI_STATE::GLOBAL_VOLUME) finishVolume();
            else {
                if(DISM.currentState==UI_STATE::APP_PET) appCoordinator.stopAiPetListening();
                if(DISM.currentState==UI_STATE::RECORDE) exitRecordingMode();
                enter(UI_STATE::HOME_MENU);
            }
        }
        backBtnPressTime=0;isBackBtnLongPressed=false;
    }
}
