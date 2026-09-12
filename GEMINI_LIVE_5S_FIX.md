# Gemini Live ~5 second disconnect follow-up

Observed device log after the prior audit:

- setup/authentication succeeds (`setupComplete`)
- session resumption handle is received
- the socket disconnects around 4.95 seconds

Changes in this follow-up:

1. `WEBSOCKETS_TCP_TIMEOUT` is raised from the Links2004 default 5000 ms to 30000 ms. The library uses this timeout while synchronously completing a WebSocket frame, not only during the initial connection/header.
2. Microphone capture remains 512 samples / 32 ms, but three capture blocks are aggregated into one 1536-sample / 96 ms Gemini realtime-input packet. This is close to the current Gemini Live guidance of ~100 ms audio chunks and reduces JSON/TLS/WebSocket overhead by roughly 3x.
3. The larger Base64 + JSON TX scratch (~8.5 KiB) is allocated in PSRAM first instead of living on the Gemini task stack.
4. Partial capture blocks are preserved by the aggregator; stale/speaker-suppressed samples are discarded before the next user turn.
5. Disconnect diagnostics now print TX/RX/drop counters plus free heap/PSRAM.

Expected new startup diagnostic:

`[GEMINI] Mic transport packet = 1536 samples (~96 ms)`

If the connection still drops, the new disconnect line contains enough counters and memory telemetry to separate transport timeout, memory pressure, and server/session behavior.
