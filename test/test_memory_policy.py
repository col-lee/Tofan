"""Exercise real PSRAM allocators and boot hook with deterministic heap failures."""
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[1]
out=root/'.pio/memory-test';out.mkdir(parents=True,exist_ok=True)
(out/'esp_heap_caps.h').write_text(r'''
#pragma once
#include <cstdlib>
#include <cstring>
#include <map>
#include <cassert>
constexpr int MALLOC_CAP_SPIRAM=1,MALLOC_CAP_INTERNAL=2,MALLOC_CAP_8BIT=4;
struct Block{size_t size;int caps;};
static std::map<void*,Block> blocks;
static bool external=true,internal=true;static int allocations=0;
inline bool allowed(int c){return c&MALLOC_CAP_SPIRAM?external:internal;}
inline void* heap_caps_malloc(size_t n,int c){++allocations;if(!allowed(c))return nullptr;void* p=malloc(n?n:1);if(p)blocks[p]={n,c};return p;}
inline void heap_caps_free(void* p){if(!p)return;assert(blocks.erase(p)==1);free(p);}
inline void* heap_caps_calloc(size_t n,size_t s,int c){void* p=heap_caps_malloc(n*s,c);if(p)memset(p,0,n*s);return p;}
inline void* heap_caps_realloc(void* p,size_t n,int c){if(!p)return heap_caps_malloc(n,c);assert(blocks.count(p));if(!allowed(c))return nullptr;void* next=heap_caps_malloc(n,c);if(next){memcpy(next,p,blocks[p].size<n?blocks[p].size:n);heap_caps_free(p);}return next;}
''',encoding='utf8')
(out/'Arduino.h').write_text(r'''
#pragma once
inline bool psramFound(){return external;}
struct {template<class... T>void printf(const char*,T...) {}} Serial;
struct {unsigned getFreeHeap(){return 0;}unsigned getFreePsram(){return 0;}unsigned getMaxAllocHeap(){return 0;}} ESP;
''',encoding='utf8')
(out/'mbedtls').mkdir(exist_ok=True)
(out/'mbedtls/platform.h').write_text(r'''
#pragma once
#ifndef NO_TLS_HOOK
#define MBEDTLS_PLATFORM_MEMORY
#endif
static void* (*tlsCalloc)(size_t,size_t)=nullptr;
static void (*tlsFree)(void*)=nullptr;static int hookCalls=0;
inline int mbedtls_platform_set_calloc_free(void*(*c)(size_t,size_t),void(*f)(void*)){++hookCalls;tlsCalloc=c;tlsFree=f;return 0;}
''',encoding='utf8')
code='#include "'+(root/'src/core/MemoryPolicy.cpp').as_posix()+'"\n'+r'''
#include <iostream>
int main(int argc,char**){
#ifdef NO_TLS_HOOK
memory::begin();assert(!memory::tlsUsesPsram()&&!hookCalls);return 0;
#endif
if(argc>1){external=false;memory::begin();assert(!memory::tlsUsesPsram()&&!hookCalls);return 0;}
memory::begin();memory::begin();assert(memory::tlsUsesPsram()&&hookCalls==1);
auto* p=(unsigned char*)tlsCalloc(4,4096);assert(p&&(blocks[p].caps&MALLOC_CAP_SPIRAM));for(int i=0;i<16384;++i)assert(p[i]==0);tlsFree(p);
int calls=allocations;assert(!tlsCalloc(SIZE_MAX,2));assert(calls==allocations);
auto* allocator=memory::jsonAllocator();p=(unsigned char*)allocator->allocate(8);memset(p,42,8);
external=false;p=(unsigned char*)allocator->reallocate(p,16);assert(p&&(blocks[p].caps&MALLOC_CAP_INTERNAL));for(int i=0;i<8;++i)assert(p[i]==42);
internal=false;assert(!allocator->reallocate(p,32));assert(blocks.count(p)&&p[0]==42);allocator->deallocate(p);
assert(!memory::zeroAllocate(2,16));internal=true;p=(unsigned char*)tlsCalloc(1,8);assert(p&&(blocks[p].caps&MALLOC_CAP_INTERNAL));tlsFree(p);
external=true;
{JsonDocument d(allocator);d["text"]=std::string(3000,'x');d["items"][0]=42;assert(!d.overflowed());for(auto b:blocks)assert(b.second.caps&MALLOC_CAP_SPIRAM);std::string json;serializeJson(d,json);JsonDocument copy(allocator);assert(!deserializeJson(copy,json));assert(copy["items"][0]==42);}
assert(blocks.empty());std::cout<<"PASS: TLS hook once, zeroed PSRAM buffers, overflow, internal fallback, realloc data/failure ownership, JSON round-trip and cleanup\n";
}
'''
(out/'main.cpp').write_text(code,encoding='utf8')
subprocess.run(['g++','-std=c++17','-I'+str(out),'-I'+str(root/'.pio/libdeps/esp32-s3-devkitc-1-n16r8v/ArduinoJson/src'),str(out/'main.cpp'),'-o',str(out/'test.exe')],check=True)
subprocess.run([str(out/'test.exe')],check=True)
subprocess.run([str(out/'test.exe'),'no-psram'],check=True)
subprocess.run(['g++','-std=c++17','-DNO_TLS_HOOK','-I'+str(out),'-I'+str(root/'.pio/libdeps/esp32-s3-devkitc-1-n16r8v/ArduinoJson/src'),str(out/'main.cpp'),'-o',str(out/'unsupported.exe')],check=True)
subprocess.run([str(out/'unsupported.exe')],check=True)
