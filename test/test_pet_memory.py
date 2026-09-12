"""Exercise the production memory reaction across Gemini connect/disconnect."""
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[1]
source=(root/'src/app/AppCoordinator.cpp').read_text(encoding='utf-8')
start=source.index('    const uint32_t freeHeap = ESP.getFreeHeap();')
end=source.index('    // Highest-priority machine/environment reactions.',start)
logic=source[start:end]
prefix=r'''
#include <cstdint>
#include <cstdio>
uint32_t memoryPressureSince=0;
bool psramAvailable=true;
bool psramFound(){return psramAvailable;}
struct {uint32_t heap=200000,psram=7000000;
 uint32_t getFreeHeap(){return heap;} uint32_t getFreePsram(){return psram;}
} ESP;
bool step(uint32_t now){
'''
tests=r'''
 return memoryPressureSince && static_cast<uint32_t>(now-memoryPressureSince)>2200;
}
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"failed at %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(){
 CHECK(!step(100));
 // More than half the pre-connection heap used by TLS/audio, but ample free RAM.
 ESP.heap=80000;CHECK(!step(1000));CHECK(!step(5000));CHECK(!step(120000));
 ESP.heap=200000;CHECK(!step(121000));ESP.heap=80000;CHECK(!step(130000));
 // A short allocation spike must not replace the listening/speaking face.
 ESP.heap=20000;CHECK(!step(140000));CHECK(!step(141000));
 ESP.heap=80000;CHECK(!step(142000));CHECK(memoryPressureSince==0);
 // Preserve warnings for real sustained low heap or PSRAM.
 ESP.heap=20000;CHECK(!step(150000));CHECK(step(152201));
 ESP.heap=80000;CHECK(!step(152300));ESP.psram=100000;
 CHECK(!step(153000));CHECK(step(155201));
 ESP.psram=7000000;CHECK(!step(155300));
 psramAvailable=false;ESP.psram=0;CHECK(!step(160000));CHECK(!step(170000));
 ESP.heap=20000;CHECK(!step(0xfffffff0u));CHECK(step(2300));
 std::puts("PASS: Gemini RAM allocation/reconnect, transient pressure, genuine low memory, recovery, no PSRAM and timer wrap");
}
'''
path=root/'.pio/pet-memory-test.cpp';exe=root/'.pio/pet-memory-test.exe'
path.write_text(prefix+logic+tests,encoding='utf-8')
subprocess.run(['g++','-std=c++17',str(path),'-o',str(exe)],check=True)
subprocess.run([str(exe)],check=True)
