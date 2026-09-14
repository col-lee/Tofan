# Conversation memory

AI Conversation now includes a history panel, capacity indicator, warnings, and a paginated reader. Reset uses a confirmation modal, stops the current Live session, waits for its worker to exit, and clears both archived text and local context. It leaves personality and other settings intact.

Gemini Live input/output transcriptions are stored as JSON Lines in `/tofan-chat/history.jsonl` on the SD card. This is text history, not audio recording. The card must be present at boot; without it, the UI explicitly reports temporary RAM storage. Insert the card and reboot to enable persistence.

Limits:

- Archive: 1 MiB or 1,000 messages. Old records are retained when full; further archive writes stop until reset.
- New-session context: at most the latest 8 messages and 8 KiB of text, starting with a user message. Older records remain readable on SD.
- Per message: 1,536 UTF-8 bytes, truncated only at character boundaries with a visible warning.
- An unsynchronized clock displays uptime instead of an invented date. Interrupted conversations carry a partial marker.

The panel warns when history exceeds the recent-context window, approaches archive capacity, fills up, or encounters write failures. SD writes run separately from microphone transport through a bounded queue. Queue overflow is reported. Incomplete SD records preserve the readable prefix and prevent further appends until reset.

Authenticated endpoints: `GET /api/history` (four records per page), `GET /api/history?before=N`, `GET /api/history?summary=1`, and `POST /api/history/reset` with `confirm=reset`. Reset is unavailable during firmware updates or an active non-Live AI request.

Memory replay is sent only for a new session, never in addition to a resumed session. Gemini 3.1 uses `historyConfig.initialHistoryInClientContent` and an initial `clientContent` payload before microphone streaming. Gemini 2.5 uses its legacy context-seeding behavior. This feature records Gemini Live transcriptions; the separate HTTP audio provider does not provide this history integration.

Protocol references: [Gemini Live capabilities](https://ai.google.dev/gemini-api/docs/live-api/capabilities) and [Live API reference](https://ai.google.dev/api/live).

Validation: native tests exercise production history code with simulated SD storage, reload, pagination, UTF-8 limits, capacity, write failures, reset, and RAM fallback. Protocol tests compile the production setup/replay blocks with a fake socket. Browser tests cover reading, escaped text, pagination, modal cancellation/confirmation, reset completion, reload, and mobile fit. Existing microphone and audio lifecycle regression tests pass. ESP32-S3 firmware builds successfully. Physical SD power-loss behavior and a real Gemini session still require testing on the device.
