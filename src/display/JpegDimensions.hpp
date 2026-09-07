#pragma once
#include <cstdint>

namespace media {
template<class Reader> bool readJpegDimensions(Reader& file, uint16_t& width, uint16_t& height) {
    width = height = 0;
    if (file.read() != 0xFF || file.read() != 0xD8) return false;
    while (file.available()) {
        if (file.read() != 0xFF) continue;
        int marker;
        do { marker = file.read(); } while (marker == 0xFF);
        if (marker < 0 || marker == 0xD9 || marker == 0xDA) return false;
        if (marker == 0xD8 || marker == 0x01 || (marker >= 0xD0 && marker <= 0xD7)) continue;
        const int high = file.read();
        const int low = file.read();
        if (high < 0 || low < 0) return false;
        const uint32_t length = (high << 8) | low;
        if (length < 2 || length - 2 > file.size() - file.position()) return false;
        if (marker == 0xC0 || marker == 0xC2) {
            if (length < 8 || file.read() < 0) return false;
            const int hHigh = file.read(), hLow = file.read();
            const int wHigh = file.read(), wLow = file.read();
            if (hHigh < 0 || hLow < 0 || wHigh < 0 || wLow < 0) return false;
            height = (hHigh << 8) | hLow;
            width = (wHigh << 8) | wLow;
            return width > 0 && height > 0;
        }
        if (!file.seek(file.position() + length - 2)) return false;
    }
    return false;
}
}
