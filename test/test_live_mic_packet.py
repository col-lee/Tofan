"""Exercise the actual sender with upstream mbedTLS's encoder and a fake socket.

The fixture is Mbed-TLS v2.28.3 library/base64.c from the upstream repository.
Only its constant-time character lookup is adapted to a host lookup table;
capacity checks, terminator handling and encoding code run unmodified.
"""
from pathlib import Path
import base64
import json
import re
import struct
import subprocess

root = Path(__file__).resolve().parents[1]
source = (root / 'src/ai/AIConversation.cpp').read_text(encoding='utf-8')
codec = (root / 'test/support/mbedtls-base64-2.28.3.c').read_text(encoding='utf-8')
encoder = codec[codec.index('int mbedtls_base64_encode('):codec.index('/*\n * Decode a base64-formatted buffer')]
sender = source[source.index('bool AIConversation::sendLiveMicFrame('):source.index('void AIConversation::runLiveSession(')]
constants = '\n'.join(re.findall(r'^constexpr size_t LIVE_MIC_.*$', source, re.M))
prefix = r'''
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <cstdlib>
#define BASE64_SIZE_T_MAX ((size_t)-1)
#define MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL -0x002A
unsigned char mbedtls_ct_base64_enc_char(unsigned char value){return "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[value];}
struct {template<class... T>void printf(const char*,T...){}void println(const char*){}} Serial;
struct Socket{bool succeeds=true;std::vector<std::string> messages;
 bool sendTXT(uint8_t* data,size_t size){if(!succeeds)return false;messages.emplace_back(reinterpret_cast<char*>(data),size);return true;}
};
class AIConversation{public:
 std::atomic<bool> liveSetupComplete{true};std::atomic<unsigned> liveTxAudioChunks{0};
 Socket liveSocket;uint8_t* liveTxScratchBuffer=nullptr;std::vector<uint8_t> buffer;bool allocate=true;
 bool ensureLiveTxScratchCapacity(size_t n){if(!allocate)return false;buffer.assign(n+8,0xa5);liveTxScratchBuffer=buffer.data();return true;}
 void setError(const char*){}
 bool sendLiveMicFrame(const int16_t*,size_t);
};
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"check failed at %d: %s\n",__LINE__,#x);return 1;}}while(0)
'''
tests = r'''
int main(){
 int16_t samples[LIVE_MIC_SEND_SAMPLES+1];for(size_t i=0;i<LIVE_MIC_SEND_SAMPLES+1;++i)samples[i]=static_cast<int16_t>(i*53);
 unsigned char encoded[LIVE_MIC_BASE64_BYTES];size_t needed=0;
 CHECK(mbedtls_base64_encode(encoded,LIVE_MIC_BASE64_BYTES-1,&needed,reinterpret_cast<unsigned char*>(samples),LIVE_MIC_SEND_SAMPLES*2)==MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL);
 CHECK(needed==4097); // Reproduce the old sender's failure before any socket call.
 AIConversation ai;
 for(size_t count:{1,511,512,1024,1535,1536}){
  CHECK(ai.sendLiveMicFrame(samples,count));
  for(size_t i=ai.buffer.size()-8;i<ai.buffer.size();++i)CHECK(ai.buffer[i]==0xa5);
 }
 CHECK(ai.liveTxAudioChunks==6);
 CHECK(!ai.sendLiveMicFrame(samples,1537));CHECK(!ai.sendLiveMicFrame(nullptr,512));CHECK(!ai.sendLiveMicFrame(samples,0));
 ai.liveSetupComplete=false;CHECK(!ai.sendLiveMicFrame(samples,512));ai.liveSetupComplete=true;
 ai.allocate=false;CHECK(!ai.sendLiveMicFrame(samples,1536));ai.allocate=true;
 ai.liveSocket.succeeds=false;CHECK(!ai.sendLiveMicFrame(samples,1536));CHECK(ai.liveTxAudioChunks==6);
 ai.liveSocket.succeeds=true;CHECK(ai.sendLiveMicFrame(samples,1536));CHECK(ai.liveTxAudioChunks==7);
 for(const auto& message:ai.liveSocket.messages)std::puts(message.c_str());
}
'''
path = root / '.pio/live-mic-packet-test.cpp'
path.write_text(prefix + constants + '\n' + encoder + sender + tests, encoding='utf-8')
exe = root / '.pio/live-mic-packet-test.exe'
subprocess.run(['g++', '-std=c++17', str(path), '-o', str(exe)], check=True)
result = subprocess.run([str(exe)], capture_output=True, text=True, check=True)
packets = result.stdout.splitlines()
counts = [1, 511, 512, 1024, 1535, 1536, 1536]
assert len(packets) == len(counts)
for packet, count in zip(packets, counts):
    audio = json.loads(packet)['realtimeInput']['audio']
    assert audio['mimeType'] == 'audio/pcm;rate=16000'
    pcm = b''.join(struct.pack('<H', (i * 53) & 65535) for i in range(count))
    assert base64.b64decode(audio['data'], validate=True) == pcm
print('PASS: reproduced old -42 buffer error; full/partial PCM packets round-trip exactly, bounds, allocation/send failure and retry verified')
