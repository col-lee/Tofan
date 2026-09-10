"""Generate a host harness around the actual input controller and UI renderer."""
from pathlib import Path
import re
import runpy
root=Path(__file__).resolve().parents[1]
runpy.run_path(str(root/'test/render_ui_preview.py'))
preview=(root/'.pio/ui-preview.cpp').read_text(encoding='utf-8')
preview=preview[:preview.index('int main(){')]
controller=(root/'src/app/InputController.cpp').read_text(encoding='utf-8')
controller=re.sub(r'^#include .*$', '', controller, flags=re.M)
stubs=r'''
#include "../src/core/Commands.hpp"
#include "../src/core/MediaNavigation.hpp"
#include "../src/app/InputController.hpp"
#include <cassert>
#define IRAM_ATTR
constexpr int ENC_A_PIN=0,ENC_B_PIN=1,ENC_VCC=2,ENC_STEPS=4,ENC_SW=5,BTN_BACK=6;
constexpr int BUTTON_DEBOUNCE_MS=200,pdPASS=1;
int pdMS_TO_TICKS(int ticks){return ticks;}
void vTaskDelay(int){}
uint32_t esp_random(){return 7;}
int audio_command=1,display_command=2;
DisplayManager DISM;
int saveCount=0,appliedVolume=-1;
bool appliedVoice=false;
void UserSettings::begin(){}
bool UserSettings::save(){++saveCount;return true;}
void requestNetworkSettings(bool,bool){}
void setOutputVolume(int v){appliedVolume=v;}
void setVoiceAssistantEnabled(bool v){appliedVoice=v;}
int consumeStartedTrack(){return -1;}
uint32_t consumeFinishedTrack(){return 0;}
void detectWord(){} void recordLoop(){}
bool enterRecordingMode(){app::runtime.isRecordingMode=true;return true;}
void exitRecordingMode(){app::runtime.isRecordingMode=false;app::runtime.isRecording=false;}
bool startRecording(){app::runtime.isRecording=true;return true;}
void stopRecording(){app::runtime.isRecording=false;}
std::vector<AUDIO_COMMAND> audioSent;
int xQueueSend(int queue,const void* command,int){
    if(queue==audio_command) audioSent.push_back(*static_cast<const AUDIO_COMMAND*>(command));
    else if(static_cast<const DISPLAY_COMMAND*>(command)->display_state==DISPLAY_COMMAND::CLEAR) DISM.mediaClearComplete.store(true);
    return pdPASS;
}
void DisplayManager::createUISprite(){} void DisplayManager::deleteUISprite(){}
void DisplayManager::loadMusicList(){playlistNames={"A.wav","B.mp3"};playlistPaths={"/main/Musics/A.wav","/main/Musics/B.mp3"};}
void DisplayManager::loadImageList(){imageNames={"A.jpg","B.jpg","C.gif"};imagePaths={"/main/Pictures/A.jpg","/main/Pictures/B.jpg","/main/Pictures/C.gif"};imageSelectedIndex=0;}
const int MAX_STATIONS=3;
String onlineStations[]={"http://a","http://b","http://c"};
bool hasPausedAudio=false;
void scheduleIdleMood(unsigned long){}
void DisplayManager::setPetMood(ui::PetMood mood,const char*,unsigned long){petMood=mood;}
struct TestCoordinator {
 int rolled=0,touches=0;
 void startAiPetListening(){}void stopAiPetListening(){}
 void reactToAiPetRotation(int steps);
 void reactToAiPetTouch(){++touches;}
} appCoordinator;
struct {bool keys[7]{};void initPins(){}bool isButtonPressed(int key){return keys[key];}} ioManager;
class AiEsp32RotaryEncoder {
public:
    int position=0,previous=0,maximum=0,minimum=0;
    template<class... Args> AiEsp32RotaryEncoder(Args...){}
    void begin(){} void setup(void(*)()){} void disableAcceleration(){} void readEncoder_ISR(){}
    void setBoundaries(int low,int high,bool){minimum=low;maximum=high;}
    void setEncoderValue(int value){position=previous=value;}
    int readEncoder(){return position;}
    int encoderChanged(){int change=position-previous;previous=position;return change;}
};
'''
tests=r'''
void tick(){simulatedMillis+=250;inputController.update();}
void press(){ioManager.keys[ENC_SW]=true;tick();ioManager.keys[ENC_SW]=false;tick();}
void back(){ioManager.keys[BTN_BACK]=true;tick();ioManager.keys[BTN_BACK]=false;tick();}
void turn(int value){rotaryEncoder.position=value;tick();}
int main(){
    DISM.applyTheme();DISM.currentState=UI_STATE::HOME_MENU;inputController.begin();
    turn(3);press();assert(DISM.currentState==UI_STATE::APP_PET);
    assert(rotaryEncoder.minimum==-2000 && rotaryEncoder.maximum==2000);
    turn(2);assert(DISM.petLookDirection==1 && rotaryEncoder.position==0 && DISM.petMood==ui::PetMood::Playful);
    turn(-3);assert(DISM.petLookDirection==-1 && rotaryEncoder.position==0);
    turn(4);assert(DISM.petMood==ui::PetMood::Excited);
    simulatedMillis+=50;appCoordinator.reactToAiPetRotation(1);
    assert(DISM.petMood==ui::PetMood::Excited);
    auto beforeZero=DISM.petInteractionCount;appCoordinator.reactToAiPetRotation(0);
    assert(DISM.petInteractionCount==beforeZero);
    app::runtime.aiPetProcessing=true;DISM.petMood=ui::PetMood::Thinking;
    turn(-1);assert(DISM.petLookDirection==-1 && DISM.petMood==ui::PetMood::Thinking);
    app::runtime.aiPetProcessing=false;
    auto interactions=DISM.petInteractionCount;
    for(int i=0;i<2100;++i) turn(1);
    assert(DISM.petInteractionCount==interactions+2100 && rotaryEncoder.position==0);
    press();assert(appCoordinator.touches==1);back();
    assert(DISM.currentState==UI_STATE::HOME_MENU && rotaryEncoder.maximum==5);
    auto beforeHome=DISM.petInteractionCount;appCoordinator.reactToAiPetRotation(1);
    assert(DISM.petInteractionCount==beforeHome);
    turn(2);press();assert(DISM.settingsPage==Page::Root && rotaryEncoder.maximum==3);
    press();assert(DISM.settingsPage==Page::Display && rotaryEncoder.maximum==6);
    turn(3);press();assert(DISM.currentState==UI_STATE::APP_COLOR_PICKER);
    turn(250);press();turn(35);press();turn(92);press();
    assert(DISM.currentState==UI_STATE::APP_SETTINGS && userSettings.values.colors[3].hue==250);
    assert(userSettings.values.colors[3].saturation==35 && userSettings.values.colors[3].value==92);
    int saved=saveCount;press();turn(20);back();
    assert(userSettings.values.colors[3].hue==250 && saveCount==saved);
    back();turn(2);press();assert(DISM.settingsPage==Page::Sound);
    turn(1);press();assert(userSettings.values.shuffle==1);
    turn(0);press();assert(userSettings.values.autoNext==0);
    turn(2);press();assert(userSettings.values.volumeStep==2);
    turn(3);press();assert(DISM.currentState==UI_STATE::GLOBAL_VOLUME);
    turn(1);assert(DISM.currentVolLevel==52 && appliedVolume==52);
    back();assert(DISM.currentState==UI_STATE::APP_SETTINGS && DISM.settingsPage==Page::Sound && rotaryEncoder.maximum==3);
    back();turn(3);press();press();assert(userSettings.values.voice==1 && appliedVoice);
    back();back();turn(0);press();
    assert(DISM.currentState==UI_STATE::APP_DISPLAY_LIST && rotaryEncoder.maximum==2);
    turn(2);press();assert(DISM.currentState==UI_STATE::APP_DISPLAY);
    back();tick();assert(DISM.currentState==UI_STATE::APP_DISPLAY_LIST && DISM.imageSelectedIndex==2 && rotaryEncoder.maximum==2);
    back();turn(5);press();assert(DISM.currentState==UI_STATE::RECORDE);
    press();assert(app::runtime.isRecording);press();assert(!app::runtime.isRecording);
    back();assert(!app::runtime.isRecordingMode && DISM.currentState==UI_STATE::HOME_MENU);
    std::puts("PASS: AIpet rotary direction/speed/continuous input/busy state/touch/back, navigation, settings, volume, display and recording");
}
'''
coordinator=(root/'src/app/AppCoordinator.cpp').read_text(encoding='utf-8')
rotation=coordinator[coordinator.index('void AppCoordinator::reactToAiPetRotation'):coordinator.index('void AppCoordinator::reactToAiPetTouch')]
rotation=rotation.replace('AppCoordinator::','TestCoordinator::')
(root/'.pio/ui-flow.cpp').write_text(preview+stubs+rotation+controller+tests,encoding='utf-8')
