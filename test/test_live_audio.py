"""Run actual queue/gating code with deterministic cross-task interleavings."""
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]
audio = (root / 'src/audio/SoundManager.cpp').read_text(encoding='utf-8')
ai = (root / 'src/ai/AIConversation.cpp').read_text(encoding='utf-8')
def function(source, signature):
    start = source.index(signature)
    brace = source.index('{', start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]

prefix = r'''
#include <atomic>
#include "../src/audio/VoiceEnvelope.hpp"
VoiceEnvelope liveSpeechEnvelope;
#include <deque>
#include <cassert>
#include <cstring>
#include <algorithm>
#include <cstdio>
#include <cstdint>
using std::min;
using TickType_t=int;
constexpr int pdPASS=1;
constexpr size_t LIVE_PCM_BLOCK_BYTES=4;
struct Block{uint16_t length=0;uint32_t epoch=0;uint8_t data[4]{};} storage[4];
using LivePcmBlock=Block;
Block* livePcmBlocks=storage;
struct Queue{std::deque<uint16_t> slots;} ready,freeSlots;
Queue *livePcmReadyQueue=&ready,*livePcmFreeQueue=&freeSlots;
std::atomic<bool> livePcmEnabled{true},livePcmSpeaking{false};
std::atomic<bool> livePcmInputComplete{true},livePcmBuffering{false};
std::atomic<size_t> livePcmQueuedBytes{0};
std::atomic<uint32_t> livePcmEpoch{0},livePcmFirstQueuedAtMs{0};
uint32_t fakeMillis=123; uint32_t millis(){return fakeMillis;}
bool immediateConsumer=false,failPublish=false;
struct{void println(const char*){}} Serial;
Queue micReady,micFree;
Queue *liveMicReadyQueue=&micReady,*liveMicFreeQueue=&micFree;
bool microphoneReady=true;
std::atomic<bool> liveMicStreaming{false};
bool ensureLiveMicStorage(){return true;}
bool ensureLivePcmStorage(){return true;}
void subtractLivePcmQueuedBytes(size_t);
int xQueueReceive(Queue* q,uint16_t* slot,int){if(q->slots.empty())return 0;*slot=q->slots.front();q->slots.pop_front();return pdPASS;}
int xQueueSend(Queue* q,const uint16_t* slot,int){
 if(q==&ready && failPublish)return 0;
 q->slots.push_back(*slot);
 if(q==&ready && immediateConsumer){
  const auto id=q->slots.front();q->slots.pop_front();
  subtractLivePcmQueuedBytes(storage[id].length);
  // Simulate slot reuse on the other core before the producer resumes.
  storage[id].length=99;freeSlots.slots.push_back(id);
 }
 return pdPASS;
}
int xQueueReceive(Queue* q,uint8_t* slot,int timeout){uint16_t id=0;int result=xQueueReceive(q,&id,timeout);*slot=static_cast<uint8_t>(id);return result;}
int xQueueSend(Queue* q,const uint8_t* slot,int timeout){uint16_t id=*slot;return xQueueSend(q,&id,timeout);}
'''
queue_code = '\n'.join(function(audio, sig) for sig in [
    'static void subtractLivePcmQueuedBytes(', 'void clearLivePcmOutput(',
    'bool queueLivePcmAudio(', 'bool livePcmHasBufferedAudio(',
    'bool startLiveMicrophoneStream(', 'void stopLiveMicrophoneStream('])
queue_code = queue_code.replace('static void subtractLivePcmQueuedBytes', 'void subtractLivePcmQueuedBytes')
gate = ai[ai.index('            const bool tailGuard =', ai.index('void AIConversation::runLiveSession')):
          ai.index('            size_t sampleCount = 0;', ai.index('void AIConversation::runLiveSession'))]
gate_prefix = r'''
struct {bool liveBargeIn=false;} config;
struct Socket{int boundaries=0,disconnects=0;bool succeed=true;
 bool sendTXT(const char* text){assert(std::strstr(text,"audioStreamEnd"));++boundaries;return succeed;}
 void disconnect(){++disconnects;}
} liveSocket;
std::atomic<bool> liveModelTurnActive{false};
bool liveMicSuspended=false;
uint32_t liveMicResumeAfterMs=0,liveLastModelEventMs=0;
size_t micSendSamples=100;
constexpr size_t LIVE_MIC_CAPTURE_SAMPLES=512;
int16_t micFrame[512];
int pendingMicFrames=0;
bool readLiveMicrophoneFrame(int16_t*,size_t,size_t&,int){if(!pendingMicFrames)return false;--pendingMicFrames;return true;}
void gateStep(uint32_t nowMs,bool speakerBusy){for(int iteration=0;iteration<1;++iteration){
'''
tests = r'''
}}
int main(){
 micFree.slots={1};micReady.slots={2}; // capture task still owns slot 0
 assert(startLiveMicrophoneStream());assert(micFree.slots.size()==2 && micReady.slots.empty());
 stopLiveMicrophoneStream();assert(!liveMicStreaming && micFree.slots.size()==2);
 uint8_t owned=0;xQueueSend(&micFree,&owned,0);assert(startLiveMicrophoneStream());
 std::sort(micFree.slots.begin(),micFree.slots.end());assert((micFree.slots==std::deque<uint16_t>{0,1,2}));
 uint8_t data[8]={1,2,3,4,5,6,7,8};freeSlots.slots={0,1};
 immediateConsumer=true;assert(queueLivePcmAudio(data,8,250));
 assert(livePcmQueuedBytes==0 && ready.slots.empty());
 assert(!livePcmHasBufferedAudio());
 immediateConsumer=false;failPublish=true;assert(!queueLivePcmAudio(data,4,250));assert(livePcmQueuedBytes==0);
 failPublish=false;freeSlots.slots={1,2};storage[0].length=4;
 livePcmQueuedBytes=4;livePcmSpeaking=true; // one block already in flight
 assert(queueLivePcmAudio(data,8,250));assert(livePcmQueuedBytes==12);
 auto epoch=livePcmEpoch.load();clearLivePcmOutput();
 assert(livePcmEpoch==epoch+1 && livePcmQueuedBytes==4 && livePcmHasBufferedAudio());
 subtractLivePcmQueuedBytes(4);livePcmSpeaking=false;assert(!livePcmHasBufferedAudio());
 assert(freeSlots.slots.size()==2); // the consumer still owns slot 0
 gateStep(100,false);assert(liveSocket.boundaries==0);
 liveModelTurnActive=true;gateStep(200,true);assert(liveMicSuspended&&micSendSamples==0&&liveSocket.boundaries==1);
 gateStep(250,true);assert(liveSocket.boundaries==1);
 liveModelTurnActive=false;gateStep(300,true);assert(liveMicSuspended);
 liveMicResumeAfterMs=600;gateStep(500,false);assert(liveMicSuspended);
 pendingMicFrames=16;gateStep(601,false);assert(!liveMicSuspended&&pendingMicFrames==0);
 liveModelTurnActive=true;liveLastModelEventMs=1000;gateStep(32000,false);assert(liveSocket.disconnects==1);
 liveSocket.disconnects=0;liveMicSuspended=false;config.liveBargeIn=true;gateStep(2000,true);assert(!liveMicSuspended);
 config.liveBargeIn=false;liveSocket.succeed=false;gateStep(2100,true);assert(liveSocket.disconnects==1);
 std::puts("PASS: immediate-consumer race, publication rollback, interruption in-flight ownership, next-turn microphone, echo drain, barge-in, stalled-turn recovery");
}
'''
source = root / '.pio/live-audio-test.cpp'
source.write_text(prefix + queue_code + gate_prefix + gate + tests, encoding='utf-8')
exe = root / '.pio/live-audio-test.exe'
subprocess.run(['g++', '-std=c++17', str(source), '-o', str(exe)], check=True)
subprocess.run([str(exe)], check=True)
