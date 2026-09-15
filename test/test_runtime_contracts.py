from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ai = (ROOT / "src/ai/AIConversation.cpp").read_text(encoding="utf-8")
ai_h = (ROOT / "src/ai/AIConversation.hpp").read_text(encoding="utf-8")
audio = (ROOT / "src/audio/SoundManager.cpp").read_text(encoding="utf-8")
media = (ROOT / "src/display/MediaPlayback.cpp").read_text(encoding="utf-8")

chat_h = (ROOT / "src/ai/ChatHistory.hpp").read_text(encoding="utf-8")
chat = (ROOT / "src/ai/ChatHistory.cpp").read_text(encoding="utf-8")
app = (ROOT / "src/app/Application.cpp").read_text(encoding="utf-8")
coordinator = (ROOT / "src/app/AppCoordinator.cpp").read_text(encoding="utf-8")
input_controller = (ROOT / "src/app/InputController.cpp").read_text(encoding="utf-8")
audio_h = (ROOT / "src/audio/SoundManager.hpp").read_text(encoding="utf-8")

checks = {
    "Gemini setup requests session resumption": 'setup["sessionResumption"]' in ai,
    "Gemini reconnect sends stored session handle": 'resumption["handle"] = liveSessionHandle' in ai,
    "Gemini stores new resumption handles": 'update["newHandle"]' in ai and 'liveSessionHandle = newHandle' in ai,
    "Long live sessions enable sliding-window compression": 'compression["slidingWindow"]' in ai,
    "AI config cannot silently stop an active AI Pet session": 'Exit AI Pet before changing AI settings' in ai,
    "AI config request body has a bounded size": 'AI_CONFIG_BODY_MAX_BYTES' in ai,
    "Live mic partial allocation has cleanup": 'releaseLiveMicStorage()' in audio,
    "Live PCM partial allocation has cleanup": 'releaseLivePcmStorage()' in audio,
    "I2S playback accounts for actual bytes written": 'writtenSamples = writtenBytes / sizeof(uint32_t)' in audio,
    "I2S playback no longer blindly advances a full batch": 'done += batch;' not in audio,
    "Gemini speaker queue gets a bounded wait longer than one PCM block": 'pdMS_TO_TICKS(250)' in ai,
    "Per-frame GIF Serial spam removed": 'DRAWING 1 FRAME' not in media,
    "Session handle state exists in AIConversation": 'liveSessionHandle' in ai_h,
    "Gemini mic packets are aggregated near 100 ms": 'LIVE_MIC_SEND_SAMPLES = LIVE_MIC_CAPTURE_SAMPLES * 3' in ai,
    "Gemini mic transport logs packet duration": 'Mic transport packet' in ai,
    "WebSocket TCP frame timeout is raised above 5 seconds": '-DWEBSOCKETS_TCP_TIMEOUT=30000' in (ROOT / "platformio.ini").read_text(encoding="utf-8"),
    "Gemini TX JSON/base64 scratch lives off task stack": 'liveTxScratchBuffer' in ai and 'ensureLiveTxScratchCapacity' in ai,
    "Gemini TX scratch is released with session": 'releaseLiveTxScratchBuffer();' in ai,
    "Gemini speaker uses a startup jitter prebuffer": 'LIVE_PCM_PREBUFFER_BYTES' in audio and 'PCM jitter buffer ready' in audio,
    "Gemini speaker detects queue underruns and re-buffers": 'PCM underrun #' in audio and 'livePlaybackPrimed = false' in audio,
    "Gemini generation completion releases short replies from prebuffer": 'finishLivePcmInput();' in ai and 'generationComplete' in ai,
    "Gemini speaker exposes buffering and underrun diagnostics": 'speakerBuffering' in ai and 'speakerUnderruns' in ai and 'speakerBufferedMs' in ai,

    "Chat history reserves ingress slots for turn boundaries": 'FinishReserve=2' in chat_h and 'uxQueueMessagesWaiting(incomingFree)<=chat::FinishReserve' in chat,
    "Chat history exposes dropped turn-boundary diagnostics": 'boundaryDrops' in chat_h and 'd["boundaryDrops"]' in chat,
    "Normal Gemini turn uses one atomic history boundary": 'chatHistory.finish();' in ai,
    "Gemini generationComplete does not prematurely commit history": 'Do not commit chat history here' in ai and 'if (serverContent["generationComplete"]' in ai,
    "Resumable WebSocket disconnect preserves in-progress history": 'canResumeTurn' in ai and 'if (!canResumeTurn)' in ai,
    "Audio metadata no longer exposes mutable cross-core String globals": 'extern String currentSongTitle' not in audio_h and 'String getCurrentSongTitle()' in audio_h and 'audioMetadataMux' in audio,
    "AI status state is atomic and error text is snapshotted": 'std::atomic<const char*> state' in ai_h and 'errorMux' in ai_h and 'errorSnapshot()' in ai,
    "Application does not start tasks after initialization failure": 'if (!initializeSystem())' in app and 'applicationReady' in app,
    "Application tears down partially-created background tasks": 'vTaskDelete(t_handleAudio)' in app and 'vTaskDelete(runnet)' in app and 'vTaskDelete(t_handleDisplay)' in app,
    "UI/audio command producers avoid unbounded main-loop queue waits": 'xQueueSend(audio_command, &command, portMAX_DELAY)' not in coordinator and 'xQueueSend(display_command,&cmd,portMAX_DELAY)' not in input_controller,
}

failed = [name for name, ok in checks.items() if not ok]
for name, ok in checks.items():
    print(f"{'PASS' if ok else 'FAIL'}: {name}")
if failed:
    raise SystemExit(f"{len(failed)} runtime contract checks failed")
print(f"All {len(checks)} runtime contract checks passed")
