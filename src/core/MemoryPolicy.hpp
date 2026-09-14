#pragma once
#include <ArduinoJson.h>
#include <esp_heap_caps.h>
#include <stdint.h>

namespace memory {
// Stateless and thread-safe. ESP-IDF can free either heap with heap_caps_free.
// Retain a working internal-memory fallback when PSRAM is absent/exhausted.
inline void* allocate(size_t size) {
    void* p=heap_caps_malloc(size,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    return p?p:heap_caps_malloc(size,MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
}
inline void release(void* p) { heap_caps_free(p); }
inline void* zeroAllocate(size_t count,size_t size) {
    if(size&&count>SIZE_MAX/size)return nullptr;
    void* p=heap_caps_calloc(count,size,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    return p?p:heap_caps_calloc(count,size,MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
}
class JsonAllocator final:public ArduinoJson::Allocator {
public:
    void* allocate(size_t size) override {return memory::allocate(size);}
    void deallocate(void* p) override {release(p);}
    void* reallocate(void* p,size_t size) override {
        if(!size){release(p);return nullptr;}
        void* next=heap_caps_realloc(p,size,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
        // Failed realloc leaves the original allocation intact.
        return next?next:heap_caps_realloc(p,size,MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
    }
};
inline ArduinoJson::Allocator* jsonAllocator(){static JsonAllocator allocator;return &allocator;}
void begin();
bool tlsUsesPsram();
}
