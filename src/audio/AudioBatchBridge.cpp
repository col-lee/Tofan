#include <cstdint>

// Deliberately do not include Audio.h here. ESP32-audioI2S 2.x marks the user
// callback declaration itself as weak, which would also make our definition
// weak and link-order dependent. This translation unit provides one strong,
// ABI-compatible bridge into the application-owned batcher.
void batchDecodedMusicFrame(uint32_t frame);

void audio_process_i2s(uint32_t* sample, bool* continueI2S) {
    if (!sample || !continueI2S) return;
    *continueI2S = false;
    batchDecodedMusicFrame(*sample);
}
