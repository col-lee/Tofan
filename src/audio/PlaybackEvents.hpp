#pragma once
#include <atomic>
#include <cstdint>

// Audio task owns command acceptance; UI consumes one-shot completion notifications.
class PlaybackEvents {
    std::atomic<uint32_t> generation{1}, finished{0};
    std::atomic<int> started{-1};
public:
    bool beginCommand(uint32_t expected = 0) {
        if (expected && expected != generation.load()) return false;
        generation.fetch_add(1);
        finished.store(0);
        return true;
    }
    void complete(bool localMusic) { if (localMusic) finished.store(generation.load()); }
    uint32_t takeCompleted() {
        const uint32_t result=finished.exchange(0);
        return result==generation.load()?result:0;
    }
    void markStarted(int index) { if (index>=0) started.store(index); }
    int takeStarted() { return started.exchange(-1); }
};

inline bool shouldRunRecognition(bool enabled, bool microphoneReady, bool recordingMode) {
    return enabled && microphoneReady && !recordingMode;
}
