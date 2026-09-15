"""Exercise production storage polling and music service diagnostics."""
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[1]
web=(root/'src/network/WebPortal.cpp').read_text(encoding='utf8')
start=web.index('    static uint64_t storageTotal=0,storageUsed=0;')
logic=web[start:web.index('    d["musicSdWaits"]',start)]
audio=(root/'src/audio/SoundManager.cpp').read_text(encoding='utf8')
service=audio[audio.index('static std::atomic<uint32_t> musicSdWaits'):audio.index('static VoiceEnvelope')]
stub=r'''
#include <ArduinoJson.h>
#include <atomic>
#include <cassert>
#include <iostream>
uint32_t now=1;uint32_t millis(){return now;}
struct SerialStub{template<class... T> void printf(const char*,T...){}} Serial;
bool isConnectSDcard=true,isPlayingAudio=false;uint32_t getSdSpiFrequencyHz(){return 20000000;}namespace app{struct {bool isRecordingMode=false;} runtime;}
int sdSemaphore=1;struct Guard{bool held=true;Guard(int){}};
struct {int queries=0;uint64_t totalBytes(){++queries;return 100000;}uint64_t usedBytes(){++queries;return 40000;}} SD;
struct {int calls=0;uint32_t filled=200000;void loop(){++calls;}uint32_t inBufferFilled(){return filled;}} audio;
'''
code=stub+service+'\nJsonDocument poll(){JsonDocument d;'+logic+'return d;}\n'+r'''
int main(){
 isPlayingAudio=true;auto d=poll();assert(SD.queries==0&&d["storageKnown"]==false&&d["storageTotal"].isNull());
 isPlayingAudio=false;d=poll();assert(SD.queries==2&&d["storageTotal"]==100000);
 for(now=2;now<30000;now+=100)poll();assert(SD.queries==2);
 now=31000;isPlayingAudio=true;poll();assert(SD.queries==2);
 isPlayingAudio=false;app::runtime.isRecordingMode=true;poll();assert(SD.queries==2);
 app::runtime.isRecordingMode=false;poll();assert(SD.queries==4);
 isConnectSDcard=false;d=poll();assert(!d["storageKnown"]&&d["storageTotal"].isNull());
 now=100;serviceMusic();now=130;serviceMusic();now=135;serviceMusic();assert(audio.calls==3&&getMusicMaxServiceGapMs()==30);assert(getMusicInputBufferBytes()==200000);musicLastService=0;now=10000;serviceMusic();assert(getMusicMaxServiceGapMs()==30);
 audio.filled=120000;serviceMusic();audio.filled=20000;serviceMusic();assert(getMusicMinInputBufferBytes()==20000&&getMusicLowBufferEvents()==1);audio.filled=18000;serviceMusic();assert(getMusicLowBufferEvents()==1);audio.filled=120000;serviceMusic();audio.filled=16000;serviceMusic();assert(getMusicLowBufferEvents()==2);
 std::cout<<"PASS: no FAT scans during playback/recording, cached/unknown capacity, refresh interval, SD removal and audio loop gap measurement\n";
}
'''
out=root/'.pio/music-service-test';out.mkdir(parents=True,exist_ok=True);(out/'main.cpp').write_text(code,encoding='utf8')
subprocess.run(['g++','-std=c++17','-I'+str(root/'.pio/libdeps/esp32-s3-devkitc-1-n16r8v/ArduinoJson/src'),str(out/'main.cpp'),'-o',str(out/'test.exe')],check=True)
subprocess.run([str(out/'test.exe')],check=True)
