"""Exercise production chat storage with an in-memory SD/FreeRTOS adapter."""
from pathlib import Path
import re, subprocess
root=Path(__file__).resolve().parents[1];out=root/'.pio/chat-test';out.mkdir(exist_ok=True)
stub=r'''
#include <string>
#include <map>
#include <set>
#include <deque>
#include <cassert>
#include <cstring>
#include <cstdlib>
#include <iostream>
class String:public std::string {public:using std::string::string;String()=default;String(const std::string& s):std::string(s){}size_t write(uint8_t c){push_back(c);return 1;}size_t write(const uint8_t* p,size_t n){append((const char*)p,n);return n;}};
uint32_t now=1000;uint32_t millis(){return now;}
using SemaphoreHandle_t=void*;void* xSemaphoreCreateMutex(){return (void*)1;}int pdMS_TO_TICKS(int n){return n;}constexpr int pdTRUE=1;
bool historyLocked=false;int xSemaphoreTake(void*,int){return !historyLocked;}void xSemaphoreGive(void*){}SemaphoreHandle_t sdSemaphore=(void*)1;bool isConnectSDcard=true;
struct Queue {std::deque<void*> values;size_t capacity;};using QueueHandle_t=Queue*;
QueueHandle_t xQueueCreate(size_t n,size_t){return new Queue{{},n};}
int xQueueSend(Queue* q,void* p,int){if(q->values.size()==q->capacity)return 0;q->values.push_back(*(void**)p);return 1;}
int xQueuePeek(Queue* q,void* p,int){if(q->values.empty())return 0;*(void**)p=q->values.front();return 1;}
int xQueueReceive(Queue* q,void* p,int){if(!xQueuePeek(q,p,0))return 0;q->values.pop_front();return 1;}
unsigned uxQueueMessagesWaiting(Queue* q){return q->values.size();}
constexpr int MALLOC_CAP_SPIRAM=1,MALLOC_CAP_8BIT=2;void* heap_caps_malloc(size_t n,int){return malloc(n);}void* heap_caps_calloc(size_t n,size_t size,int){return calloc(n,size);}
std::map<std::string,std::string> disk;std::set<std::string> dirs;bool failWrite=false,failRemove=false,failOpen=false,diskFull=false;
constexpr int FILE_READ=0,FILE_APPEND=1;
struct File {std::string path;size_t pos=0;bool opened=false;operator bool()const{return opened;}size_t size()const{return disk[path].size();}size_t position()const{return pos;}bool available()const{return pos<size();}int read(){return available()?(unsigned char)disk[path][pos++]:-1;}bool seek(size_t n){pos=n;return n<=size();}size_t readBytes(char* p,size_t n){n=std::min(n,size()-pos);memcpy(p,disk[path].data()+pos,n);pos+=n;return n;}size_t print(const String& s){size_t n=failWrite?s.size()/2:s.size();disk[path].append(s.data(),n);pos+=n;return n;}void flush(){}void close(){opened=false;}};
struct SDMock {bool exists(const char* p){return disk.count(p)||dirs.count(p);}bool mkdir(const char* p){dirs.insert(p);return true;}File open(const char* p,int mode){if(failOpen||(!disk.count(p)&&mode==FILE_READ))return {};auto& data=disk[p];return {p,mode==FILE_APPEND?data.size():0,true};}bool remove(const char* p){if(failRemove)return false;disk.erase(p);return true;}size_t totalBytes(){return 16*1024*1024;}size_t usedBytes(){if(diskFull)return totalBytes();size_t sum=0;for(auto& p:disk)sum+=p.second.size();return sum;}} SD;
'''
strip=lambda s:re.sub(r'^#(?:pragma once|include ["<](?:Arduino.h|freertos/[^>]*|ChatHistory.hpp|../core/MemoryPolicy.hpp|../core/SharedResources.hpp|SD.h|esp_heap_caps.h)[">]).*$', '',s,flags=re.M)
stub+='\n#include <ArduinoJson.h>\nnamespace memory {ArduinoJson::Allocator* jsonAllocator(){return ArduinoJson::detail::DefaultAllocator::instance();}void* zeroAllocate(size_t n,size_t s){return calloc(n,s);}}\n'
ai=(root/'src/ai/AIConversation.cpp').read_text(encoding='utf8')
start=ai.index('    if (serverContent["generationComplete"] | false) {')
boundary=ai[start:ai.index('\n}\n',start)]
transcripts=ai[ai.index('    chatHistory.transcript(false,serverContent'):ai.index('    if (serverContent["interrupted"]')]
boundarySupport=r"""
struct {void println(const char*){}} Serial;
uint32_t liveLastModelEventMs=0,liveMicResumeAfterMs=0;
std::atomic<bool> liveGenerationComplete{false},liveModelTurnActive{false};String state;
void finishLivePcmInput(){}bool livePcmHasBufferedAudio(){return false;}
void serverEvent(ChatHistory& chatHistory,const char* json){JsonDocument d;assert(!deserializeJson(d,json));JsonObjectConst serverContent=d["serverContent"];
"""+transcripts+boundary+'\n}\n'
source=stub+strip((root/'src/ai/ChatHistory.hpp').read_text())+strip((root/'src/ai/ChatHistory.cpp').read_text())+boundarySupport+r'''
JsonDocument page(ChatHistory& h,unsigned before=0){JsonDocument d;assert(!deserializeJson(d,h.page(before)));return d;}
void service(ChatHistory& h,int n=3){for(int i=0;i<n;++i){now+=101;h.service();}}
void say(ChatHistory& h,const char* user,const char* model){h.transcript(false,user);h.transcript(true,model);h.finish();service(h);}
int main(){
 ChatHistory history;history.begin();assert(page(history)["storage"]=="sd");
 history.transcript(false,"Hello ");history.transcript(false,"world");history.transcript(true,"Hi!");history.finish();service(history);
 auto d=page(history);assert(d["count"]==2);assert(d["messages"][0]["text"]=="Hello world");assert(d["messages"][1]["role"]=="model");
 // SD reader holds the history mutex while new transcript/finish events arrive.
 historyLocked=true;history.transcript(false,"captured during SD read");history.finish();historyLocked=false;
 assert(page(history)["incoming"]==2&&page(history)["dropped"]==0);service(history);assert(page(history)["count"]==3);
 assert(history.requestReset());history.reset();say(history,"Hello world","Hi!");
 ChatHistory reboot;reboot.begin();assert(page(reboot)["count"]==2);assert(reboot.context().find("Hello world")!=std::string::npos);
 for(int i=0;i<6;++i)say(reboot,"Next question","Next answer");
 d=page(reboot);assert(d["long"]==true&&d["hasOlder"]==true);assert(d["messages"].size()==4);assert(page(reboot,d["before"].as<unsigned>())["messages"][0]["id"].as<int>()<d["messages"][0]["id"].as<int>());
 JsonDocument context;assert(!deserializeJson(context,reboot.context()));assert(context.size()==chat::RecentLimit);
 // Ending a connection flushes partial speech exactly once.
 reboot.transcript(true,"Interrupted reply");reboot.finish(true);reboot.finish(true);service(reboot);d=page(reboot);assert(d["count"]==15);assert(d["messages"][3]["partial"]==true);
 // UTF-8 truncation is bounded and never splits a Thai codepoint.
 std::string thai;for(int i=0;i<600;++i)thai+="\xe0\xb8\x81";reboot.transcript(false,thai.c_str());reboot.finish();service(reboot);d=page(reboot);assert(d["truncated"]==true);assert(strlen(d["messages"][3]["text"].as<const char*>())==1536);
 // An SD write failure is visible; no false saved record count.
 failWrite=true;say(reboot,"lost tail","reply");failWrite=false;assert(page(reboot)["damaged"]==true);
 ChatHistory partialBoot;partialBoot.begin();assert(page(partialBoot)["damaged"]==true);assert(page(partialBoot)["count"]==16);
 failRemove=true;assert(partialBoot.requestReset());partialBoot.reset();assert(page(partialBoot)["count"]==16);assert(!partialBoot.resetting());failRemove=false;
 assert(partialBoot.requestReset());partialBoot.transcript(false,"must not survive reset");partialBoot.finish();partialBoot.reset();assert(page(partialBoot)["count"]==0&&partialBoot.context()=="[]");
 ChatHistory clearedBoot;clearedBoot.begin();assert(page(clearedBoot)["count"]==0);
 // Once archive capacity is reached, retain old records and expose a warning.
 for(unsigned i=0;i<chat::RecordLimit/2;++i)say(clearedBoot,"q","a");say(clearedBoot,"overflow","overflow");d=page(clearedBoot);assert(d["count"]==chat::RecordLimit&&d["full"]==true);
 assert(clearedBoot.requestReset());clearedBoot.reset();
 diskFull=true;say(clearedBoot,"q","a");d=page(clearedBoot);assert(d["pending"].as<int>()==2&&d["count"]==0);diskFull=false;service(clearedBoot);assert(page(clearedBoot)["count"]==2);
 // No SD is explicitly volatile and bounded; queue saturation reports drops.
 isConnectSDcard=false;ChatHistory ram;ram.begin();for(int i=0;i<12;++i){ram.transcript(false,"RAM only");ram.finish();}service(ram);d=page(ram);assert(d["storage"]=="ram"&&d["count"]==8&&d["dropped"].as<int>()>0);assert(ram.requestReset());ram.reset();assert(page(ram)["count"]==0);
 isConnectSDcard=true;ChatHistory boundaryHistory;boundaryHistory.begin();boundaryHistory.requestReset();boundaryHistory.reset();
 serverEvent(boundaryHistory,R"({"serverContent":{"inputTranscription":{"text":"question"},"outputTranscription":{"text":"answer"},"generationComplete":true}})");service(boundaryHistory);assert(page(boundaryHistory)["count"]==2);
 serverEvent(boundaryHistory,R"({"serverContent":{"turnComplete":true}})");service(boundaryHistory);assert(page(boundaryHistory)["count"]==2);
 std::cout<<"PASS: transcript assembly, SD persistence, history seeding, pagination, long/full warnings, partials, UTF-8, disk failures, reset and bounded RAM\n";
}
'''
(out/'main.cpp').write_text(source,encoding='utf8')
subprocess.run(['g++','-std=c++17','-I'+str(root/'.pio/libdeps/esp32-s3-devkitc-1-n16r8v/ArduinoJson/src'),str(out/'main.cpp'),'-o',str(out/'test.exe')],check=True)
subprocess.run([str(out/'test.exe')],check=True)
