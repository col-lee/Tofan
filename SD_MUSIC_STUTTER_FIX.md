# SD music stutter audit / fix

## Root causes found

1. The SD card was forced to 4 MHz (`SD.begin(..., 4000000)`). This is a conservative clock but leaves little headroom for high-bitrate local audio and concurrent SD activity.
2. Local music only services `audio.loop()` after acquiring the shared SD mutex. If another task owns the mutex for more than 5 ms, the old code skipped that service cycle. Repeated misses can drain the audioI2S read-ahead buffer.
3. The existing 1 MiB PSRAM audio buffer was useful, but there was no runtime low-water telemetry and no accelerated refill policy when that buffer began to drain.
4. `CaptureSamples` was an unpinned FreeRTOS task at priority 10. The speaker/local-music task is pinned to Core 0 at priority 4, so microphone DMA/copy work could preempt it on Core 0 roughly every 32 ms even when Gemini was not active.

## Changes

- SD mount now tries 20 MHz, then 10 MHz, then 4 MHz for perfboard/card compatibility.
- Local playback observes `audio.inBufferFilled()` from the audio-owning task.
- Below 96 KiB the audio task performs 2 short refill passes; below 32 KiB it performs up to 4 passes. The SD mutex is released between every pass.
- SD wait timeout for one audio service pass is 8 ms instead of 5 ms.
- Microphone capture is now pinned to Core 1 at priority 5, isolating Core 0 speaker/SD audio service without disabling microphone capture or Gemini Live.
- Added diagnostics to `/api/status`:
  - `sdSpiHz`
  - `musicSdWaits`
  - `musicMaxServiceGapMs`
  - `musicInputBufferBytes`
  - `musicMinInputBufferBytes`
  - `musicLowBufferEvents`

## Interpreting diagnostics

- `musicLowBufferEvents == 0`: SD/read-ahead starvation is unlikely; investigate codec/I2S/power next.
- `musicLowBufferEvents > 0`: the compressed input buffer reached a critical level; SD contention/throughput is confirmed.
- High `musicSdWaits`: other tasks are holding the SD mutex too often.
- High `musicMaxServiceGapMs`: the audio service task is not getting serviced frequently enough.
- `sdSpiHz == 4000000`: the board/card fell back to the old safe speed; wiring/card quality may be limiting the faster clock.

## Compatibility

The audio queue, Gemini PCM path, MJPEG double buffer, upload slicing, chat history and recording flow were not structurally changed. Adaptive refill only runs for local SD audio.
