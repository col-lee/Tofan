#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
namespace portal {
constexpr size_t MaxUpload = 256u * 1024 * 1024;
inline bool filename(const char* s) {
    const size_t n = s ? std::strlen(s) : 0;
    if (!n || n > 120 || s[0] == '.' || s[n-1] == ' ' || s[n-1] == '.') return false;
    for (size_t i=0;i<n;i++) if (static_cast<unsigned char>(s[i]) < 32 || std::strchr("/\\:*?\"<>|",s[i])) return false;
    return true;
}
inline bool directory(const char* s) {
    return s && (!std::strcmp(s,"Pictures") || !std::strcmp(s,"Musics") || !std::strcmp(s,"Videos"));
}
inline bool imageHeader(const uint8_t* bytes, size_t n) {
    // esp_image_header_t: magic, segment count, chip_id (ESP32-S3 = 9).
    return n >= 24 && bytes[0] == 0xe9 && bytes[1] > 0 && bytes[1] <= 16 && bytes[12] == 9 && bytes[13] == 0;
}
inline bool firmwareSize(size_t n, size_t partition) { return n >= 24 && n <= partition; }
inline bool uploadChunkValid(size_t expected,size_t received,size_t index,size_t length,bool finished) {
    return !finished && received<=expected && index==received && length<=expected-received;
}
}
