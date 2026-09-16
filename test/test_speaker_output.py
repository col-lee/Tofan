"""Compile the portable PCM batch test and enforce speaker-path contracts."""
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]
out = root / ".pio" / "speaker-output-test"
out.mkdir(parents=True, exist_ok=True)
exe = out / "test.exe"

subprocess.run([
    "g++", "-std=c++17", str(root / "test" / "test_pcm_batch.cpp"),
    "-o", str(exe)
], check=True)
subprocess.run([str(exe)], check=True)

audio = (root / "src" / "audio" / "SoundManager.cpp").read_text(encoding="utf-8")
bridge = (root / "src" / "audio" / "AudioBatchBridge.cpp").read_text(encoding="utf-8")
portal = (root / "src" / "network" / "WebPortal.cpp").read_text(encoding="utf-8")

checks = {
    "decoded music bypasses per-sample driver writes":
        "void audio_process_i2s(uint32_t* sample, bool* continueI2S)" in bridge
        and "*continueI2S = false;" in bridge
        and "batchDecodedMusicFrame(*sample);" in bridge,
    "audio callback bridge avoids inherited weak linkage":
        '#include "SoundManager.hpp"' not in bridge
        and '#include <Audio.h>' not in bridge,
    "music is submitted in bounded batches":
        "SPEAKER_I2S_BATCH_FRAMES = 128" in audio
        and "musicOutputBatch.full()" in audio,
    "Gemini and music share measured I2S writes":
        "writeSpeakerFrames(" in audio
        and "liveStereoScratch + batchDone" in audio,
    "source transitions discard stale staged PCM":
        audio.count("clearMusicOutputBatch();") >= 6,
    "natural EOF flushes the final partial batch":
        "if (wasRunning && !audio.isRunning()) flushMusicOutputBatch();" in audio,
    "I2S stall and drop diagnostics are exposed":
        'd["speakerI2sStalls"]' in portal
        and 'd["speakerI2sDroppedFrames"]' in portal
        and 'd["speakerI2sMaxWriteUs"]' in portal,
}

failed = [name for name, ok in checks.items() if not ok]
for name, ok in checks.items():
    print(f"{'PASS' if ok else 'FAIL'}: {name}")
if failed:
    raise SystemExit(f"{len(failed)} speaker output checks failed")
