#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/queue.h>
#include <atomic>
namespace chat {
constexpr size_t TextLimit=1536,RecentLimit=8,ArchiveLimit=1024*1024,RecordLimit=1000,ContextLimit=8192;
struct Entry {uint32_t id=0,time=0,uptime=0;bool model=false,partial=false,truncated=false;uint16_t length=0;char text[TextLimit+1]={};};
// Append only complete UTF-8 codepoints. Never store a broken Thai character.
inline void append(Entry& entry,const char* text){
    if(!text)return;
    for(size_t i=0;text[i];){
        const uint8_t c=text[i];size_t n=c<0x80?1:(c&0xe0)==0xc0?2:(c&0xf0)==0xe0?3:(c&0xf8)==0xf0?4:0;
        if(!n){++i;continue;}bool complete=true;for(size_t j=1;j<n;++j)if(!text[i+j]||(static_cast<uint8_t>(text[i+j])&0xc0)!=0x80){complete=false;break;}
        if(!complete){++i;continue;}
        if(c<32&&c!='\n'&&c!='\t'){i+=n;continue;}
        if(entry.length+n>TextLimit){entry.truncated=true;break;}
        for(size_t j=0;j<n;++j)entry.text[entry.length++]=text[i++];entry.text[entry.length]=0;
    }
}
}
class ChatHistory {
    struct Index {uint32_t offset,length;};
    SemaphoreHandle_t mutex=nullptr;QueueHandle_t queue=nullptr;
    chat::Entry* recent=nullptr;Index* index=nullptr;
    chat::Entry* assembly=nullptr;
    unsigned recentCount=0,recentStart=0,count=0;uint32_t bytes=0,nextId=1,lastAttempt=0,resetRevision=0;
    bool persistent=false,damaged=false,full=false,truncated=false;
    String error;
    std::atomic<unsigned> dropped{0};std::atomic<bool> resetPending{false};
    void remember(const chat::Entry& entry);
    void commit(chat::Entry& entry,bool partial);
    void status(JsonDocument& d) const;
public:
    void begin();
    void transcript(bool model,const char* text);
    void finish(bool partial=false);
    void service();
    String page(unsigned before=0,bool includeMessages=true);
    String context();
    bool hasContext();
    bool requestReset(){return !resetPending.exchange(true);}
    bool resetting() const{return resetPending.load();}
    void reset(); // Called only after Gemini's worker has fully stopped.
};
extern ChatHistory chatHistory;
