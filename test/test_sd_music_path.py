"""Static regression checks for SD music starvation fixes."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
config=(root/'src/core/Config.hpp').read_text(encoding='utf8')
fm=(root/'src/storage/FileManager.cpp').read_text(encoding='utf8')
audio=(root/'src/audio/SoundManager.cpp').read_text(encoding='utf8')
web=(root/'src/network/WebPortal.cpp').read_text(encoding='utf8')
checks={
    'SD target clock above legacy 4 MHz':'SD_SPI_TARGET_HZ   20000000U' in config,
    'SD has 10 MHz fallback':'SD_SPI_FALLBACK_HZ 10000000U' in config,
    'SD retains 4 MHz safe fallback':'SD_SPI_SAFE_HZ      4000000U' in config,
    'Mount iterates fallback clocks':'const uint32_t frequencies[]' in fm and 'SD.begin(SD_CS, vspi, frequency)' in fm,
    'Music observes audioI2S input buffer':'audio.inBufferFilled()' in audio,
    'Music has low-water refill policy':'MUSIC_REFILL_LOW_WATER_BYTES' in audio and 'refillPasses' in audio,
    'Music releases SD mutex between refill passes':'xSemaphoreGive(sdSemaphore);' in audio and 'taskYIELD();' in audio,
    'Mic capture pinned away from audio core':'xTaskCreatePinnedToCore(' in audio and 'capture_samples, "CaptureSamples"' in audio and '&microphoneTask, 1' in audio,
    'Mic capture priority no longer 10':'nullptr, 5, &microphoneTask, 1' in audio,
    'Web status exposes SD/music telemetry':'musicLowBufferEvents' in web and 'musicInputBufferBytes' in web and 'sdSpiHz' in web,
}
failed=[name for name,ok in checks.items() if not ok]
for name,ok in checks.items(): print(('PASS' if ok else 'FAIL')+': '+name)
if failed: raise SystemExit('Failed: '+', '.join(failed))
print(f'All {len(checks)} SD music path checks passed')
