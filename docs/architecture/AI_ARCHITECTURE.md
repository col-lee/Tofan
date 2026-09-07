# AI voice pipeline

This document describes the implemented firmware contract. The backend is external and is not included in this repository.

```text
I2S microphone -> WAV on SD -> HTTP backend -> JSON audioUrl -> audio task -> speaker
```

## Firmware ownership

- `src/app/AppCoordinator.cpp`: starts a capture on entry to AI Pet when enabled/configured, drains recorder frames and stops after about 5000 ms. A background task submits `/main/ai_pet_input.wav`.
- `src/audio/SoundManager.cpp`: captures I2S at `EI_CLASSIFIER_FREQUENCY` and writes mono PCM16 WAV with a 16000 Hz header. The model frequency must match the header.
- `src/ai/AIConversation.cpp`: NVS configuration and backend HTTP request.
- `src/display/DisplayManager.cpp`: Pet animation uses microphone amplitude while listening or playing; it does not analyze TTS output for lip synchronization.

AI initializes in `AppCoordinator::begin()`. Routes register when Admin Mode starts. Returning to Pet can start another capture; there is no continuous conversation loop or wake-word trigger for the backend. Leaving Pet stops active capture but does not cancel a submitted HTTP task.

## Request and response

The device POSTs the WAV file as the raw body to `pipelineUrl` with `Content-Type: audio/wav`, `X-AI-Provider: <provider>` and optional `Authorization: Bearer <apiKey>`.

The backend should perform STT, reply generation and TTS, then return a successful 2xx response with JSON:

```json
{"audioUrl":"http://backend.example/audio/reply.wav"}
```

Only a nonempty `audioUrl` is extracted. Other fields such as transcript/replyText are ignored. Binary audio responses are not supported. The playback URL must be reachable by the device; firmware does not attach the backend bearer token to that playback request.

## Configuration API

GET `/api/ai/config` returns settings, `configured`, and `apiKeySet`, `userSet`, `passwordSet` without secret values. POST `/api/ai/config` accepts JSON; GET `/api/ai/status` returns `enabled`, `configured`, `state`, `lastError`.

| Field | Implemented behavior |
| --- | --- |
| `enabled` | Enables capture/submission; default false |
| `pipelineUrl` | Required nonempty URL when saving; buffer 192 bytes |
| `provider` | Stored and sent as X-AI-Provider; buffer 24 bytes |
| `model` | Stored but not sent to the backend; buffer 48 bytes |
| `apiKey` | Optional backend bearer token; buffer 128 bytes |
| `user`, `password` | Stored but not used for HTTP authentication; buffers 64 bytes each |
| `allowInsecureTLS` | Calls setInsecure() for HTTPS when true; default false |

String limits include the terminating null byte. Omitting or sending an empty `apiKey`/`password` retains the old value. NVS namespace is `AIConfig`. `configured` means enabled plus nonempty URL; it does not verify connectivity.

## Current limitations

AI routes do not check the Admin login token. Admin Mode availability does not automatically authenticate them. HTTPS code creates a secure client but does not configure a CA certificate; setting `allowInsecureTLS` false alone does not implement certificate trust. The HTTP client does not set a project-specific timeout or retry policy. Status `completed` means HTTP submission succeeded, not that returned audio played successfully.

See the [Thai setup guide](../guides/AI_SETUP_GUIDE_TH.md) and [current review](../reviews/CODE_REVIEW.md).
