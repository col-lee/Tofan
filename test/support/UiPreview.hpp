#pragma once
// Desktop drawing adapter: executes production render methods and records geometry.
// Text is approximated with a desktop font; this is not a hardware screenshot.
#include "../../src/core/UserSettings.hpp"
#include <string>
#include <vector>
#include <atomic>
#include <fstream>
#include <sstream>
#include <type_traits>
#include <cmath>
class String : public std::string {
public:
    using std::string::string;
    String()=default;
    String(const std::string& s):std::string(s){}
    template<class N,typename std::enable_if<std::is_arithmetic<N>::value,int>::type=0>
    String(N n):std::string(std::to_string(n)){}
    String substring(int start,int end=99999) const { return substr(start,end-start); }
    void remove(int start) { erase(start); }
};
enum {TL_DATUM,TC_DATUM,TR_DATUM,ML_DATUM,MC_DATUM,MR_DATUM,BL_DATUM,BC_DATUM,BR_DATUM};
constexpr uint16_t TFT_WHITE=65535,TFT_BLACK=0;
constexpr int portMAX_DELAY=1,pdTRUE=1,WL_CONNECTED=1;
int displaySemaphore=0;
int xSemaphoreTake(int,int) { return 1; }
void xSemaphoreGive(int) {}
unsigned long simulatedMillis=1000;
unsigned long millis() { return simulatedMillis; }
int random(int limit) { return limit/2; }
int random(int start,int end) { return (start+end)/2; }
int readMicData() { return 0; }
class LGFX {
public:
    int width() const {return 320;} int height() const {return 240;}
    uint16_t color565(int r,int g,int b) {return preferences::rgb565((r<<16)|(g<<8)|b);}
};
class LGFX_Sprite {
    std::ostringstream frame;
    int font=1,datum=0,color=0;
    void shape(const char* name,std::initializer_list<int> args) {
        frame<<"{\"op\":\""<<name<<"\",\"a\":[";bool first=true;
        for(int a:args) {if(!first) frame<<',';frame<<a;first=false;} frame<<"]}\n";
    }
public:
    void* getBuffer(){return this;}
    void setTextSize(int){} void setTextFont(int f){font=f;}
    void setTextColor(int c){color=c;} void setTextDatum(int d){datum=d;}
    void pushSprite(int,int){}
    int textWidth(const String& text) {return text.length()*(font==1?6:8);}
    void fillSprite(int c){frame.str("");frame.clear();shape("rect",{0,0,320,240,c});}
    void fillRect(int x,int y,int w,int h,int c){shape("rect",{x,y,w,h,c});}
    void drawRect(int x,int y,int w,int h,int c){shape("outline",{x,y,w,h,c});}
    void fillRoundRect(int x,int y,int w,int h,int r,int c){shape("round",{x,y,w,h,r,c});}
    void drawRoundRect(int x,int y,int w,int h,int r,int c){shape("round-outline",{x,y,w,h,r,c});}
    void fillCircle(int x,int y,int r,int c){shape("ellipse",{x-r,y-r,r*2,r*2,c});}
    void drawCircle(int x,int y,int r,int c){shape("ellipse-outline",{x-r,y-r,r*2,r*2,c});}
    void fillEllipse(int x,int y,int rx,int ry,int c){shape("ellipse",{x-rx,y-ry,rx*2,ry*2,c});}
    void fillTriangle(int x1,int y1,int x2,int y2,int x3,int y3,int c){shape("triangle",{x1,y1,x2,y2,x3,y3,c});}
    void drawLine(int x1,int y1,int x2,int y2,int c){shape("line",{x1,y1,x2,y2,c});}
    void drawPixel(int x,int y,int c){shape("rect",{x,y,1,1,c});}
    void drawString(const String& text,int x,int y,int f=0){
        frame<<"{\"op\":\"text\",\"a\":["<<x<<','<<y<<','<<(f?f:font)<<','<<datum<<','<<color<<"],\"text\":\"";
        for(char ch:text){if(ch=='"'||ch=='\\')frame<<'\\';if(ch!='\n' && ch!='\r')frame<<ch;}frame<<"\"}\n";
    }
    void save(const char* name){std::ofstream file(std::string("docs/build/ui-preview/")+name+".jsonl");file<<frame.str();}
};
LGFX tft;LGFX_Sprite spr;UserSettings userSettings;
String currentSongTitle="Morning light";
bool isPlayingAudio=true,isOnlineAudio=false;
int currentAudioProgress=34,currentStationIndex=0;
uint32_t currentAudioTime=83,totalAudioDuration=245;
const char* onlineStationNames[]={"Radio Paradise","SomaFM Groove Salad","Lofi"};
struct {int status(){return WL_CONNECTED;}} WiFi;
bool networkSettingsBusy(){return false;}
namespace app {struct {bool isRecording=false;bool isRecordingMode=false;} runtime;}
String getRecordingName(){return "voice_record_000012.wav";}
struct {unsigned long getFreeHeap(){return 192000;}unsigned long getFreePsram(){return 6000000;}} ESP;
struct Device {String name="Speaker";int status=0;String details="Ready for playback";};
struct {
    void updateAllStatus(){} int getDeviceCount(){return 8;}
    Device getDevice(int){return Device{};} String getStatusString(int){return "Ready";}
} hwManager;
