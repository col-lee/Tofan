#pragma once
#include <cstdint>
#include <cstdio>
#include <cstddef>

namespace recording {
// Caller holds the SD mutex through name selection and file creation.
// The hint is only an optimization: existence is checked even after reboot.
template<class Exists>
bool nextPath(uint32_t& hint, Exists exists, char* output, std::size_t capacity) {
    if (!output || capacity == 0) return false;
    output[0] = '\0';
    if (hint == 0) return false; // Exhausted; never wrap and reuse a filename.
    for (;;) {
        const int written = std::snprintf(output, capacity, "/main/Musics/voice_record_%06lu.wav",
                                          static_cast<unsigned long>(hint));
        if (written < 0 || static_cast<std::size_t>(written) >= capacity) {
            output[0] = '\0';
            return false;
        }
        const bool occupied = exists(output);
        hint = hint == UINT32_MAX ? 0 : hint + 1;
        if (!occupied) return true;
        if (hint == 0) {
            output[0] = '\0';
            return false;
        }
    }
}
}
