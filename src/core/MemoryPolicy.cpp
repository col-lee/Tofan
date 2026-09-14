#include "MemoryPolicy.hpp"
#include <Arduino.h>
#include <mbedtls/platform.h>

namespace memory {
namespace {bool installed=false;}
bool tlsUsesPsram(){return installed;}
void begin(){
    // Install once, before starting networking/TLS workers. Never switch hooks
    // underneath active sessions. No global malloc policy or SDK ABI changes.
    static bool initialized=false;if(initialized)return;initialized=true;
#if defined(MBEDTLS_PLATFORM_MEMORY) && !defined(MBEDTLS_PLATFORM_CALLOC_MACRO) && !defined(MBEDTLS_PLATFORM_FREE_MACRO)
    if(psramFound())installed=mbedtls_platform_set_calloc_free(zeroAllocate,release)==0;
#endif
    Serial.printf("[MEMORY] TLS allocator=%s | internal=%u PSRAM=%u largest=%u\n",
        installed?"PSRAM preferred":"SDK default",ESP.getFreeHeap(),ESP.getFreePsram(),ESP.getMaxAllocHeap());
}
}
