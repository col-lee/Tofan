#pragma once

#include <cstddef>
#include <cstdint>

// Small, allocation-free staging buffer for stereo I2S frames. Keeping this
// independent of Arduino/FreeRTOS makes the ordering and boundary behavior
// host-testable.
template <size_t Capacity>
class PcmBatch {
    static_assert(Capacity > 0, "PCM batch capacity must be positive");

public:
    bool push(uint32_t frame) {
        if (count_ >= Capacity) return false;
        frames_[count_++] = frame;
        return true;
    }

    const uint32_t* data() const { return frames_; }
    size_t size() const { return count_; }
    constexpr size_t capacity() const { return Capacity; }
    bool full() const { return count_ == Capacity; }
    bool empty() const { return count_ == 0; }
    void clear() { count_ = 0; }

private:
    uint32_t frames_[Capacity]{};
    size_t count_ = 0;
};
