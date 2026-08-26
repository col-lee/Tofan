# AI Voice Conversation Architecture

## What this system is called

This design is an **edge voice assistant** or **IoT conversational voice pipeline**. The ESP32-S3 is the edge device: it captures audio, sends it to a backend, and plays the returned audio. The backend is an AI gateway/orchestrator.

## Processing pipeline

```text
Microphone
  -> ESP32-S3 WAV recorder
  -> AI backend /api/v1/voice
  -> Speech-to-Text (STT)
  -> Large Language Model (LLM)
  -> Text-to-Speech (TTS)
  -> audio URL or audio bytes
  -> ESP32-S3
  -> MAX98357A speaker
```

The backend should return JSON like this:

```json
{
  "requestId": "abc-123",
  "transcript": "สวัสดี",
  "replyText": "สวัสดีครับ มีอะไรให้ช่วยไหม",
  "audioUrl": "https://backend.example/audio/abc-123.wav"
}
```

The ESP32 must not contain provider-specific OpenAI, Gemini, or other vendor logic. Keeping provider keys on the device increases the impact of a leaked key and makes TLS, JSON, retries, and audio decoding expensive. The backend can select STT, LLM, and TTS providers without changing firmware.

## Firmware module

Files:

- `src/extendstion/AIConversation.hpp`
- `src/extendstion/AIConversation.cpp`

`AIConversation` currently provides:

- Persistent configuration in ESP32 NVS namespace `AIConfig`.
- `GET /api/ai/config` for non-secret settings and secret-presence flags.
- `POST /api/ai/config` for updating settings.
- `GET /api/ai/status` for connection/configuration state.
- `submitAudioFile(path, responseBody)` for posting a WAV file to the configured backend.
- Automatic AIPet capture records 5 seconds after entering the AIPet screen, then submits the file from a FreeRTOS task.
- AIPet playback expects `audioUrl` in the backend response and queues that URL into the existing audio task.
- HTTPS support. Certificate validation should be added before production; `allowInsecureTLS` is intended only for local development.

The module is initialized in `setup()` and its routes are registered when Admin Mode starts.

## Configuration request

Send JSON to `POST /api/ai/config`:

```json
{
  "enabled": true,
  "pipelineUrl": "https://backend.example/api/v1/voice",
  "provider": "openai",
  "model": "gpt-4o-mini",
  "apiKey": "provider-or-backend-key",
  "user": "optional-user",
  "password": "optional-password",
  "allowInsecureTLS": false
}
```

`apiKey` and `password` are write-only in the normal config response. Omitting either field during an update keeps the existing value. The current web routes do not implement user authentication themselves; expose them only through the existing authenticated Admin Mode or add authentication before deploying outside a trusted local network.

## Backend contract

Recommended backend responsibilities:

1. Authenticate the device with a device token or mutual TLS.
2. Accept `POST /api/v1/voice` with `Content-Type: audio/wav`.
3. Run STT on 16 kHz, mono, 16-bit PCM audio.
4. Send the transcript to the selected LLM with a system prompt and conversation/session ID.
5. Run TTS on the LLM response.
6. Return `transcript`, `replyText`, and either `audioUrl` or encoded audio data.
7. Apply timeouts, rate limits, size limits, logging redaction, and provider error handling.

A practical backend may be built with FastAPI, Node.js, or another HTTP framework. It should own provider SDKs and environment variables such as `OPENAI_API_KEY` or `GEMINI_API_KEY`; do not commit those values to this repository.

## Current AIPet behavior and remaining integration work

The AIPet screen now has an automatic 5-second capture trigger and submits the recording without blocking the display loop. The Pet mouth animation uses live microphone amplitude while recording or playing. The backend response must contain an `audioUrl` that the ESP32 can reach; the existing `Audio` task then streams that URL to the speaker.

The next implementation steps are:

- Start/stop recording from a push-to-talk action or wake word.
- Replace the fixed 5-second capture with push-to-talk or wake-word detection if required.
- Support a response containing audio bytes as an alternative to `audioUrl`.
- Add a cancel/timeout state and a user-visible error message.
- Add authentication to the AI configuration endpoints.

This separation keeps the first module testable and avoids coupling the microphone recorder to one cloud provider.
