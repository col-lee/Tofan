#pragma once
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cmath>
#include "PetPersonality.hpp"

namespace preferences {
enum ColorRole { Background, Surface, Text, Accent, Muted, Selection, ColorCount };
struct HSV { uint16_t hue; uint8_t saturation, value; };
struct Values {
    HSV colors[ColorCount] = {{160,3,98},{155,9,94},{205,30,22},{160,30,79},{205,17,49},{165,19,88}};
    // Reuse the zero-initialized reserved byte: existing version-1 records retain their layout.
    uint8_t volume = 50, volumeStep = 5, petPersonality = 0;
    uint8_t wifi = 0, admin = 0, voice = 0, autoNext = 1, shuffle = 0;
};
inline int clamp(int value, int low, int high) { return value < low ? low : value > high ? high : value; }
inline int hardwareVolume(int percent) { return (clamp(percent,0,100) * 21 + 50) / 100; }
inline int adjustVolume(int percent, int detents, int step) {
    return clamp(percent + clamp(detents,-100,100) * clamp(step,2,5),0,100);
}
inline bool valid(const Values& v) {
    if (v.volume > 100 || v.volumeStep < 2 || v.volumeStep > 5 || v.petPersonality >= pet::personalityCount) return false;
    if (v.wifi > 1 || v.admin > 1 || v.voice > 1 || v.autoNext > 1 || v.shuffle > 1) return false;
    for (const auto& c : v.colors) if (c.hue >= 360 || c.saturation > 100 || c.value > 100) return false;
    return true;
}
inline uint32_t rgb(HSV c) {
    const float h = (c.hue % 360) / 60.0f, s = c.saturation / 100.0f, v = c.value / 100.0f;
    const float chroma = v*s, x = chroma * (1-std::fabs(std::fmod(h,2.0f)-1)), m = v-chroma;
    float r=0,g=0,b=0;
    switch (static_cast<int>(h)) {
        case 0:r=chroma;g=x;break; case 1:r=x;g=chroma;break;
        case 2:g=chroma;b=x;break; case 3:g=x;b=chroma;break;
        case 4:r=x;b=chroma;break; default:r=chroma;b=x;break;
    }
    return (static_cast<uint32_t>((r+m)*255+0.5f)<<16) |
           (static_cast<uint32_t>((g+m)*255+0.5f)<<8) | static_cast<uint32_t>((b+m)*255+0.5f);
}
inline uint16_t rgb565(uint32_t c) { return ((c>>8)&0xf800) | ((c>>5)&0x07e0) | ((c>>3)&0x001f); }
inline bool dark(uint32_t c) { return (((c>>16)&255)*299 + ((c>>8)&255)*587 + (c&255)*114) < 145000; }
inline int nextTrack(int current, int count, bool shuffle, uint32_t randomValue) {
    if (count <= 0) return -1;
    if (count == 1) return 0;
    if (current < 0 || current >= count) return shuffle ? randomValue % count : 0;
    if (!shuffle) return (current + 1) % count;
    const int candidate = randomValue % (count-1);
    return candidate >= current ? candidate + 1 : candidate;
}
inline void playbackTime(char* out, std::size_t capacity, uint32_t current, uint32_t total) {
    std::snprintf(out,capacity,"%02lu:%02lu / %02lu:%02lu",
        static_cast<unsigned long>(current/60),static_cast<unsigned long>(current%60),
        static_cast<unsigned long>(total/60),static_cast<unsigned long>(total%60));
}
struct Record { uint32_t version = 1; Values values; uint32_t checksum = 0; };
inline uint32_t checksum(const Values& v) {
    uint32_t hash = 2166136261u;
    const auto* bytes = reinterpret_cast<const uint8_t*>(&v);
    for (std::size_t i=0;i<sizeof(v);++i) hash = (hash ^ bytes[i])*16777619u;
    return hash;
}
inline bool decode(const Record& record, Values& out) {
    if (record.version != 1 || !valid(record.values) || record.checksum != checksum(record.values)) return false;
    out = record.values; return true;
}
}

class UserSettings {
public:
    preferences::Values values;
    pet::Custom customPet;
    bool saveFailed = false;
    void begin();
    bool save();
    bool saveCustomPet(const pet::Custom& value);
};
extern UserSettings userSettings;
