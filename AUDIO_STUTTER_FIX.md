# Gemini Live Audio Stutter Audit & Fix

Date: 2026-09-14

## Symptom
Gemini Live can answer successfully, but its speech sometimes sounds chopped or discontinuous even though the WebSocket session remains connected.

## Root cause found in this project
The project already had a large Gemini output queue in PSRAM:

- `LIVE_PCM_BLOCK_BYTES = 4096`
- `LIVE_PCM_BLOCK_COUNT = 128`
- total capacity: about 512 KiB

Gemini output is raw 16-bit mono PCM at 24 kHz, so the playback data rate is about 48,000 bytes/second. A 4096-byte block therefore contains only about 85 ms of sound.

Before this fix, `handleAudio()` started I2S playback as soon as the first ready PCM block arrived. There was no startup jitter prebuffer. Gemini Live delivers output as network chunks, and Wi-Fi/TLS/WebSocket scheduling is not perfectly periodic. If the next chunk arrived slightly later than the current 85 ms block finished, the ready queue became empty and I2S temporarily had no new samples to play. The next chunk then resumed playback. This produces the audible pattern:

`audio -> tiny silence -> audio -> tiny silence`

The 512 KiB queue therefore had plenty of *capacity*, but it usually contained very little data because playback consumed data immediately instead of first building a safety margin.

A secondary source of gaps was the I2S retry policy. A batch could be abandoned after only three zero-progress writes. Under temporary DMA/I2S pressure that could discard samples and create another audible hole.

## Changes made

### 1. Real output jitter buffer
Added a startup/rebuffer threshold:

- `LIVE_PCM_PREBUFFER_BYTES = 16 KiB`
- at 24 kHz PCM16 mono this is about 341 ms of buffered speech
- maximum initial wait is 650 ms, so a stalled/tiny response cannot wait forever

Playback now waits for the safety margin before starting instead of starting on the first network packet.

### 2. Automatic rebuffer after an underrun
If the speaker queue becomes empty while Gemini is still producing audio, playback now switches back to buffering mode. It waits for the safety margin again before resuming. This avoids repeated tiny play/silence/play gaps.

### 3. Short replies are not delayed unnecessarily
`generationComplete` and `turnComplete` now call `finishLivePcmInput()`. If Gemini has already finished producing a short reply, the audio task is allowed to drain whatever is queued even when it is smaller than 16 KiB.

### 4. More tolerant I2S transient stalls
The zero-progress I2S retry limit was increased from 3 to 10 attempts before samples are dropped. Partial writes are still respected exactly.

### 5. Diagnostics
AI status now exposes:

- `speakerBufferedBytes`
- `speakerBufferedMs`
- `speakerBuffering`
- `speakerUnderruns`

Serial also reports:

- `PCM jitter buffer ready ...`
- `PCM underrun #...; rebuffering before resume`

Underrun logging is rate-limited so Serial output itself does not worsen playback.

## What was checked and found to be OK

- Gemini output format is handled as raw PCM16 at 24 kHz.
- Audio task is pinned to Core 0 at priority 4, above Gemini/network tasks at priority 3.
- I2S code already accounts for partial writes using the actual `writtenBytes` value.
- The PSRAM queue capacity itself is already generous and did not need to be enlarged.
- Gemini receive/decode runs separately from speaker consumption.

## Tests run after the change

Passed:

- runtime contract checks: 22/22
- live PCM queue/gating regression
- Gemini live response state regression
- live microphone packet regression
- AI Pet memory/reconnect regression
- AI Pet voice renderer regression

A full PlatformIO firmware build was not run in this environment because the ESP32 PlatformIO toolchain is not installed here. Build once on the development PC before flashing.

## What to watch on the real device

Healthy startup of a response should show something like:

```
[GEMINI] PCM jitter buffer ready | queued=16384 bytes (~341 ms) | source=streaming
```

If Wi-Fi/network delivery temporarily falls behind, you may see:

```
[GEMINI] PCM underrun #1; rebuffering before resume
```

If `speakerUnderruns` remains 0 while audible stutter still occurs, the next likely bottleneck is below the network jitter buffer (I2S/DMA, amplifier clocking, power integrity, or speaker hardware), not Gemini packet delivery.
