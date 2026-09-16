#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

#include "../src/audio/PcmBatch.hpp"

int main() {
    PcmBatch<4> batch;
    assert(batch.empty());
    assert(batch.capacity() == 4);

    assert(batch.push(10));
    assert(batch.push(20));
    assert(batch.push(30));
    assert(batch.push(40));
    assert(batch.full());
    assert(!batch.push(50));

    const std::vector<uint32_t> expected{10, 20, 30, 40};
    assert(std::vector<uint32_t>(batch.data(), batch.data() + batch.size()) == expected);

    batch.clear();
    assert(batch.empty());
    assert(batch.push(50));
    assert(batch.size() == 1);
    assert(batch.data()[0] == 50);

    PcmBatch<128> oneSecond;
    size_t dmaWrites = 0;
    for (uint32_t frame = 0; frame < 44100; ++frame) {
        assert(oneSecond.push(frame));
        if (oneSecond.full()) {
            ++dmaWrites;
            oneSecond.clear();
        }
    }
    if (!oneSecond.empty()) ++dmaWrites;  // final transition/EOF flush
    assert(dmaWrites == 345);
    assert(dmaWrites * 100 < 44100);      // >100x fewer driver calls

    // Production capacity must bound added latency below 3 ms at 44.1 kHz.
    constexpr size_t productionFrames = 128;
    constexpr uint32_t latencyUs = productionFrames * 1000000UL / 44100UL;
    static_assert(latencyUs < 3000, "speaker batching adds too much latency");

    std::cout << "PASS: PCM batch boundary, ordering, overflow guard, reset, >100x call reduction and latency bound\n";
}
