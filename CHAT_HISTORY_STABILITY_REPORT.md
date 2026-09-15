# ToFan(5) — Chat History & Stability Fix Report

Date: 2026-09-15

## Confirmed Chat History bug

The growing `assemblingBytes` symptom was partly normal and partly a real bug.

- The UI reports UTF-8 bytes, not visible characters. Thai text commonly uses multiple UTF-8 bytes per character, so ~600 bytes can be normal for a moderately sized Thai transcript.
- The real bug was that ordinary transcript fragments and the end-of-turn marker shared the same realtime ingress capacity. Under a burst, a turn boundary could be dropped. When that happened the next turn continued appending to the previous assembly, so `assemblingBytes` could keep growing across turns.
- `generationComplete` was also treated as a history boundary even though input/output transcription arrives independently from model/audio events. This could commit too early and leave late transcript fragments to be attached to the next turn.
- A resumable Gemini WebSocket disconnect used to finalize the history as partial even though the same Gemini session was about to resume. That split a single conversation turn into multiple records.

## Chat History fixes

1. Replaced per-fragment transient allocation with a fixed PSRAM-first ingress pool.
2. Reserved ingress capacity for turn-boundary events so normal transcript bursts cannot consume every slot.
3. Added separate `finishUser()` and `finishModel()` operations for barge-in/interruption handling.
4. Normal turns use one atomic `finish()` boundary instead of multiple finish messages.
5. `generationComplete` no longer commits chat history. `turnComplete` is the normal conversation boundary.
6. Resumable WebSocket reconnects preserve the in-progress history assembly.
7. Interrupted model output is stored as partial without incorrectly committing the next user's utterance.
8. Added diagnostics: `receivedBytes`, `assemblingUserBytes`, `assemblingModelBytes`, and `boundaryDrops`.
9. Updated the web History status so total received bytes are shown separately from the current turn assembly.
10. Fixed/extended the host regression test to cover burst saturation, late fragments, and barge-in splitting.

## Additional stability fixes included

- Audio metadata exposed across cores no longer uses mutable shared Arduino `String` globals; readers get synchronized snapshots.
- AI status uses an atomic state literal and synchronized error-text snapshot.
- Application startup now stops safely if semaphore/queue/task creation fails and tears down partially-created tasks.
- Main/UI command producers no longer wait forever (`portMAX_DELAY`) on queues that can stall the main loop.
- Display-clear command ordering was hardened so a fast display task cannot race the closing state.
- AI config changes are rejected while AI Pet is active instead of silently stopping a live session behind the coordinator.
- Existing Gemini session resumption, PCM jitter buffering, PSRAM queues, MJPEG buffering, and upload slicing are retained.

## Regression results

- `test_chat_history.py`: PASS (burst boundary reservation, late transcript, barge-in split, persistence, UTF-8, reset/failure paths)
- All Python regression scripts in `test/test_*.py`: PASS
- Runtime contract checks: 32/32 PASS
- Web Node tests: 10/10 PASS
- Embedded Portal assets: 10/10 match `web/dist`
- Partition layout: no overlap; 16 MB table ends at `0x1000000`

## Build note

A full ESP32-S3 PlatformIO firmware compile was not run in this environment because the PlatformIO toolchain is not installed here. The clean project intentionally excludes old `.pio` build output and stale firmware binaries. Build once in VS Code/PlatformIO before flashing.

The original `web/node_modules` was installed for Windows and is intentionally omitted from the clean project. `web/dist` and the matching embedded `PortalAssets.hpp` are included, so rebuilding the web portal is not required merely to compile/flash the firmware.
