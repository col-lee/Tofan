# Internal RAM optimization

The reported 93 KiB at boot and 23 KiB with AI running refer to internal heap, not total RAM including PSRAM. This change targets dynamic allocations that can use external memory while retaining the existing internal task stacks and hardware buffers.

## Changes

- `MemoryPolicy::begin()` runs before application networking starts. The installed Mbed TLS library exposes `mbedtls_platform_set_calloc_free()` even though its default is `CONFIG_MBEDTLS_INTERNAL_MEM_ALLOC`. The callback now tries PSRAM first and falls back to internal RAM. It is installed once, only when PSRAM exists and the API is supported. Unsupported configurations retain their SDK allocator. No changes to cached SDK files, cipher settings, certificate verification, TLS record size, or global `malloc` thresholds are needed.
- JSON documents in AIConversation, ChatHistory, WebPortal, and Network use a shared, stateless ArduinoJson allocator. Allocation/reallocation prefer PSRAM and preserve the original block on failed reallocation. Free supports either heap.
- The two chat transcription assembly buffers move from static internal RAM to PSRAM-preferred allocation, saving approximately 3 KiB of static internal storage.
- `/api/status` includes `heapMin`, `heapLargest`, and `tlsPsram`, alongside existing `heap` and `psram`. `tlsPsram` means the PSRAM-preferred TLS policy is installed; individual allocations can still fall back internally. Boot and Live-ready serial logs report heap/PSRAM and the largest internal block; Live-ready also reports minimum internal heap and task stack high-water mark.

The largest expected improvement is TLS: this SDK configures a 16 KiB maximum record payload for each of its two I/O buffers, plus protocol state. Actual heap recovery must be measured on hardware; it is not valid to promise a particular final free-RAM value from compilation alone.

Gemini's 18 KiB stack, AsyncTCP's 16 KiB stack, Wi-Fi/network driver memory, and DMA allocations remain as configured. There is no PSRAM stack option enabled for these task creation paths. Reducing stacks without measuring worst-case usage is outside this change. Existing audio and WebSocket payload buffers already prefer PSRAM. Ordinary String/request allocations still follow the SDK's normal policy.

## Validation

`test/test_memory_policy.py` compiles the actual allocator and initialization implementation with a simulated capability heap. It verifies installation once, absent PSRAM, zero initialization, multiplication overflow, fallback, realloc data preservation, failed-realloc ownership, ArduinoJson round-trip, and deallocation. Existing history, Gemini history protocol, microphone packet, and audio lifecycle tests also pass.

On hardware, compare boot, connected Live with web closed, Live with web open, and after closing Live. Confirm `[MEMORY] TLS allocator=PSRAM preferred` at boot. Record internal free heap, PSRAM, and largest free internal block across several reconnects. Historical minimum heap does not rise after memory is freed. A real TLS handshake, audio latency, and repeated connect/disconnect sessions remain hardware checks.

References: [Mbed TLS allocator hooks](https://mbed-tls.readthedocs.io/en/latest/kb/how-to/using-static-memory-instead-of-the-heap/) and [ArduinoJson custom allocators](https://arduinojson.org/v7/api/jsondocument/).
