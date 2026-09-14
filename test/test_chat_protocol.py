"""Compile actual history setup/replay blocks against ArduinoJson and a fake socket."""
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]
src = (root/'src/ai/AIConversation.cpp').read_text(encoding='utf8')
setup = src[src.index('    setup["inputAudioTranscription"]'):src.index('    // Keep server-side VAD')]
replay = src[src.index('        if(liveInitialHistory.length()>2){'):src.index('        liveSetupComplete.store(true);')]
out = root/'.pio/chat-protocol-test'
out.mkdir(parents=True, exist_ok=True)
code = r'''
#include <ArduinoJson.h>
namespace memory {ArduinoJson::Allocator* jsonAllocator(){return ArduinoJson::detail::DefaultAllocator::instance();}}
#include <string>
#include <cassert>
#include <iostream>
class String:public std::string {public:using std::string::string;int indexOf(const char* s)const{auto p=find(s);return p==npos?-1:int(p);}size_t write(uint8_t c){push_back(c);return 1;}size_t write(const uint8_t* p,size_t n){append((const char*)p,n);return n;}};
struct {const char* model="gemini-3.1-flash-live-preview";} config;
String rememberedText="[{\"role\":\"user\",\"parts\":[{\"text\":\"Remember tea\"}]},{\"role\":\"model\",\"parts\":[{\"text\":\"Okay\"}]}]";
struct {String context(){return rememberedText;}} chatHistory;
struct {void println(const char*){}} Serial;
struct {bool okay=true,disconnected=false;String sent;bool sendTXT(String s){sent=s;return okay;}void disconnect(){disconnected=true;}} liveSocket;
String liveInitialHistory,liveSessionHandle;
bool liveHistoryInitial=false,mic=false,error=false;
void setError(const char*){error=true;}
JsonDocument makeSetup(){JsonDocument d;auto setup=d["setup"].to<JsonObject>();
''' + setup + r'''
return d;}
void ready(){
''' + replay + r'''
mic=true;}
int main(){
auto d=makeSetup();assert(d["setup"]["inputAudioTranscription"].is<JsonObject>());assert(d["setup"]["outputAudioTranscription"].is<JsonObject>());assert(d["setup"]["historyConfig"]["initialHistoryInClientContent"]==true);
ready();JsonDocument p;assert(!deserializeJson(p,liveSocket.sent));assert(p["clientContent"]["turnComplete"]==true);assert(p["clientContent"]["turns"][0]["role"]=="user");assert(p["clientContent"]["turns"][1]["role"]=="model");assert(mic&&liveInitialHistory.empty());
liveSessionHandle="resume-handle";d=makeSetup();assert(d["setup"]["historyConfig"].isNull());liveSocket.sent.clear();ready();assert(liveSocket.sent.empty());
liveSessionHandle.clear();config.model="gemini-2.5-flash-native-audio-preview";d=makeSetup();assert(d["setup"]["historyConfig"].isNull());ready();deserializeJson(p,liveSocket.sent);assert(p["clientContent"]["turnComplete"]==false);
makeSetup();mic=false;liveSocket.okay=false;ready();assert(error&&liveSocket.disconnected&&!mic);
rememberedText="[]";d=makeSetup();liveSocket.sent.clear();ready();assert(liveSocket.sent.empty());
std::cout<<"PASS: transcription setup, new/resumed/empty history, 3.1 and 2.5 replay, send failure before microphone starts\n";
}
'''
(out/'main.cpp').write_text(code, encoding='utf8')
subprocess.run(['g++','-std=c++17','-I'+str(root/'.pio/libdeps/esp32-s3-devkitc-1-n16r8v/ArduinoJson/src'),str(out/'main.cpp'),'-o',str(out/'test.exe')],check=True)
subprocess.run([str(out/'test.exe')],check=True)
