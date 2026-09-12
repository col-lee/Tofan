# Gemini Live reply cutoff and microphone recovery

## Confirmed source defects

- Follow-up device log: `setupComplete` succeeded, capture increased, but TX/RX
  remained zero and every first microphone packet triggered a reconnect. The
  sender gave mbedTLS 4096 bytes for a 4096-character Base64 string, omitting
  capacity for the required NUL terminator. mbedTLS returned -42 (buffer too
  small) before WebSocket send. Passing the full 4097-byte capacity fixes this
  deterministic failure. Bounds checks and distinct Base64/JSON/socket logs
  now identify the failure stage. The previous queue tests did not cover the
  encoder; `test/test_live_mic_packet.py` closes that gap using the upstream
  mbedTLS encoder and checks every decoded PCM byte for full/partial packets.
  Upstream source: https://github.com/Mbed-TLS/mbedtls/blob/v2.28.3/library/base64.c

- `WebSockets 2.7.3` overwrote the configured 131072-byte receive limit with
  15360 bytes. Its frame handler closes with code 1009 above that limit. The
  local library now honors the configured limit, allocates receive data in
  PSRAM first, and the application asserts the effective limit during build.
- PCM byte accounting ran after publishing a queue slot. The audio task could
  finish the block before the producer increments the counter, leaving phantom
  buffered bytes. Half-duplex mic gating then stayed enabled forever. A
  deterministic replay reproduced four phantom bytes after playing eight bytes.
  The producer now accounts before publication and rolls back a failed send.
- Clearing/restarting queues reset ownership while a capture/playback task could
  still own a slot. Restart now drains pending slots without duplicating owned
  slots. Interruption uses a generation number on each PCM block and lets the
  audio task release its own in-flight count.

## Conversation behavior

- Speaker enqueue waits up to 250 ms per block. One 4096-byte PCM16/24 kHz block
  takes about 85 ms, so the previous 15 ms wait could discard ordinary bursts
  when the queue was full. Persistent I2S stalls can still fail and are logged.
- With barge-in disabled, pausing the microphone sends `audioStreamEnd` once;
  new audio reopens the stream. Buffered speaker echo is discarded before the
  next user turn. Barge-in remains an explicit user setting.
- Playback drains before the microphone reopens, followed by the existing
  250 ms acoustic guard. `generationComplete` does not flush or prematurely
  finish playback; `turnComplete` retains its separate meaning.
- If a model turn has no further audio/completion for 30 seconds and playback
  has drained, reconnect instead of keeping the microphone muted forever.
- VAD silence duration is 1000 ms instead of 500 ms, allowing longer pauses in
  a user's sentence at the cost of a slightly slower response.

Protocol reference: [Google Live API WebSockets reference](https://ai.google.dev/api/live),
especially realtimeInput.audioStreamEnd, generationComplete, turnComplete and
automaticActivityDetection.silenceDurationMs.

## Validation and device follow-up

`python test/test_live_audio.py` compiles actual queue and microphone gate code
against deterministic task/queue stubs. It covers immediate consumption, slot
reuse, failed publication rollback, clearing during playback, microphone resume,
echo discard, barge-in and stalled-turn recovery. Runtime contract checks are
in `test/test_runtime_contracts.py`.

No live Gemini session or physical microphone/speaker test was performed here.
Source defects explain possible failures but do not prove which one occurred
on the user's board without serial logs. Check the startup line reports
`Effective WebSocket frame limit = 131072 bytes`, then look for microphone
paused/resumed, interrupted, queue-full, I2S stalled, and disconnected messages.
An `Audio flow` diagnostic every 10 seconds reports captured frames, TX/RX
chunks, queued bytes and microphone/turn state without printing audio content.
If captured increases but TX does not while micPaused is zero, inspect transport;
if captured stops, inspect the microphone capture path. A failed microphone send
now reconnects instead of leaving the UI in a misleading ready state.
Do not include API keys in shared logs.
