#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>

// Audio tasks publish whole-frame energy; UI reads without consuming audio.
class VoiceEnvelope {
    std::atomic<uint32_t> amplitude{0}, updated{0};
public:
    void reset() { amplitude.store(0); }
    void push(const int16_t* samples, size_t count, uint32_t now, unsigned gainPercent=100) {
        if (!samples || !count) return;
        uint64_t sum=0;
        for(size_t i=0;i<count;++i) {
            const int32_t sample=samples[i];
            sum+=sample<0?-sample:sample;
        }
        const uint32_t mean=static_cast<uint32_t>((sum/count)*(gainPercent>100?100:gainPercent)/100);
        amplitude.store(mean);
        updated.store(now);
    }
    float level(uint32_t now) const {
        const uint32_t age=now-updated.load();
        if(age>=160) return 0.0f;
        const float fade=age<=60?1.0f:(160-age)/100.0f;
        return amplitude.load()/32768.0f*fade;
    }
};
