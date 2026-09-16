// Owns speaker playback, I2S capture, WAV recording and local voice inference.
#include "SoundManager.hpp"
#include "RecordingPath.hpp"
#include "PlaybackEvents.hpp"
#include "VoiceEnvelope.hpp"
#include "PcmBatch.hpp"
#include "../core/UserSettings.hpp"
#include <atomic>
#include <esp_heap_caps.h>
#include <esp_timer.h>
#include "../display/DisplayManager.hpp"
#include "../storage/FileManager.hpp"
#include "../network/Network.hpp"
#include "../core/GlobalState.hpp"
#include <ArduinoJson.h>
#include "ToFan-project-1_inferencing.h"

Audio audio;
static std::atomic<uint32_t> musicSdWaits{0}, musicMaxServiceGap{0};
static std::atomic<uint32_t> musicInputBufferBytes{0}, musicMinInputBufferBytes{0}, musicLowBufferEvents{0};
static uint32_t musicLastService = 0;
static bool musicBufferPrimed = false;
static constexpr uint32_t MUSIC_REFILL_LOW_WATER_BYTES = 96 * 1024;
static constexpr uint32_t MUSIC_CRITICAL_BUFFER_BYTES = 32 * 1024;

// ESP32-audioI2S 2.x submits one 32-bit stereo frame per i2s_write() call.
// At 44.1 kHz that means 44,100 driver/semaphore calls per second. Intercept
// the library's post-gain callback and submit short DMA-friendly batches.
// 128 frames add at most 2.9 ms latency at 44.1 kHz while reducing driver
// calls by up to 128x. Gemini PCM already arrives in batches and uses the same
// measured write helper directly.
static constexpr size_t SPEAKER_I2S_BATCH_FRAMES = 128;
static constexpr uint8_t SPEAKER_I2S_MAX_STALLS = 10;
static PcmBatch<SPEAKER_I2S_BATCH_FRAMES> musicOutputBatch;
static std::atomic<uint32_t> speakerI2sWrites{0}, speakerI2sStalls{0};
static std::atomic<uint32_t> speakerI2sDroppedFrames{0}, speakerI2sMaxWriteUs{0};

uint32_t getSpeakerI2sWrites(){return speakerI2sWrites.load();}
uint32_t getSpeakerI2sStalls(){return speakerI2sStalls.load();}
uint32_t getSpeakerI2sDroppedFrames(){return speakerI2sDroppedFrames.load();}
uint32_t getSpeakerI2sMaxWriteUs(){return speakerI2sMaxWriteUs.load();}

static void observeSpeakerWriteDuration(uint32_t elapsedUs) {
    uint32_t maximum = speakerI2sMaxWriteUs.load();
    while (elapsedUs > maximum &&
           !speakerI2sMaxWriteUs.compare_exchange_weak(maximum, elapsedUs)) {}
}

static size_t writeSpeakerFrames(const uint32_t* frames, size_t frameCount, TickType_t timeout) {
    if (!frames || frameCount == 0 || !isAudio_install) return 0;

    size_t completed = 0;
    uint8_t stalledWrites = 0;
    while (completed < frameCount) {
        size_t writtenBytes = 0;
        const int64_t startedUs = esp_timer_get_time();
        const esp_err_t result = i2s_write(
            static_cast<i2s_port_t>(audio.getI2sPort()),
            frames + completed,
            (frameCount - completed) * sizeof(uint32_t),
            &writtenBytes,
            timeout);
        const int64_t elapsedUs = esp_timer_get_time() - startedUs;
        observeSpeakerWriteDuration(elapsedUs > 0 ? static_cast<uint32_t>(elapsedUs) : 0);
        speakerI2sWrites.fetch_add(1);

        const size_t writtenFrames = writtenBytes / sizeof(uint32_t);
        if (writtenFrames > 0) {
            completed += min(writtenFrames, frameCount - completed);
            stalledWrites = 0;
            continue;
        }

        speakerI2sStalls.fetch_add(1);
        // A transient timeout/driver error can clear on the next DMA slot. Keep
        // the established bounded retry behavior for both ESP_OK/zero-progress
        // and explicit errors; only the retry ceiling is allowed to drop PCM.
        (void)result;
        if (++stalledWrites >= SPEAKER_I2S_MAX_STALLS) break;
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    return completed;
}

static void clearMusicOutputBatch() {
    musicOutputBatch.clear();
}

static void flushMusicOutputBatch() {
    if (musicOutputBatch.empty()) return;
    const size_t requested = musicOutputBatch.size();
    const size_t written = writeSpeakerFrames(musicOutputBatch.data(), requested, pdMS_TO_TICKS(20));
    if (written < requested) {
        speakerI2sDroppedFrames.fetch_add(static_cast<uint32_t>(requested - written));
    }
    musicOutputBatch.clear();
}

// AudioBatchBridge calls this after ESP32-audioI2S has decoded, filtered and
// applied gain. It only runs from handleAudio, so the fixed buffer needs no
// cross-task lock.
void batchDecodedMusicFrame(uint32_t sample) {
    if (!musicOutputBatch.push(sample)) {
        speakerI2sDroppedFrames.fetch_add(1);
        return;
    }
    if (musicOutputBatch.full()) {
        flushMusicOutputBatch();
    }
}

uint32_t getMusicSdWaits(){return musicSdWaits.load();}
uint32_t getMusicMaxServiceGapMs(){return musicMaxServiceGap.load();}
uint32_t getMusicInputBufferBytes(){return musicInputBufferBytes.load();}
uint32_t getMusicMinInputBufferBytes(){return musicMinInputBufferBytes.load();}
uint32_t getMusicLowBufferEvents(){return musicLowBufferEvents.load();}

static void observeMusicBuffer() {
    const uint32_t filled = audio.inBufferFilled();
    musicInputBufferBytes.store(filled);
    if (filled >= MUSIC_REFILL_LOW_WATER_BYTES) musicBufferPrimed = true;
    if (!musicBufferPrimed) return;

    uint32_t minimum = musicMinInputBufferBytes.load();
    while ((minimum == 0 || filled < minimum) &&
           !musicMinInputBufferBytes.compare_exchange_weak(minimum, filled)) {}

    static bool belowCritical = false;
    if (filled < MUSIC_CRITICAL_BUFFER_BYTES) {
        if (!belowCritical) {
            const uint32_t event = musicLowBufferEvents.fetch_add(1) + 1;
            Serial.printf("[AUDIO] Music buffer low #%u | filled=%u KiB | sdWaits=%u | serviceGapMax=%u ms | SD=%lu MHz\n",
                          static_cast<unsigned>(event),
                          static_cast<unsigned>(filled / 1024),
                          static_cast<unsigned>(musicSdWaits.load()),
                          static_cast<unsigned>(musicMaxServiceGap.load()),
                          static_cast<unsigned long>(getSdSpiFrequencyHz() / 1000000UL));
        }
        belowCritical = true;
    } else if (filled >= MUSIC_REFILL_LOW_WATER_BYTES) {
        belowCritical = false;
    }
}

static void serviceMusic(){
    const uint32_t now=millis();
    if(musicLastService){
        const uint32_t gap=now-musicLastService;
        uint32_t maximum=musicMaxServiceGap.load();
        while(gap>maximum && !musicMaxServiceGap.compare_exchange_weak(maximum,gap)){}
    }
    musicLastService=now;
    const bool wasRunning = audio.isRunning();
    audio.loop();
    // Codec frame sizes are not guaranteed to be a multiple of our DMA batch.
    // Preserve the final <=127 frames on a natural EOF; explicit pause/stop/
    // seek transitions clear the staging buffer before changing source state.
    if (wasRunning && !audio.isRunning()) flushMusicOutputBatch();
    observeMusicBuffer();
}
static VoiceEnvelope microphoneEnvelope, liveSpeechEnvelope;
float getMicrophoneVoiceLevel() { return microphoneEnvelope.level(millis()); }
float getLiveSpeechLevel() { return liveSpeechEnvelope.level(millis()); }
bool isAudio_install;

String currentFilePath = "";

uint32_t currentAudioTime = 0;
uint32_t totalAudioDuration = 0;

bool isPlayingAudio = false;
int currentAudioProgress = 0;
unsigned long lastProgressUpdate = 0;
static PlaybackEvents playbackEvents;
static std::atomic<int> requestedVolume{50};

// A video owns its companion audio only while this flag is set.  The clock
// is updated by the audio task from Audio::getTotalPlayingTime(), so the
// display task can pace MJPEG frames against the sound without touching the
// Audio object from another core.
static std::atomic<bool> videoAudioPending{false};
static std::atomic<bool> videoAudioActive{false};
static std::atomic<uint32_t> videoAudioClock{0};

int consumeStartedTrack() { return playbackEvents.takeStarted(); }
static std::atomic<bool> voiceAssistantEnabled{false};
static bool classifierNeedsReset = true;
void setOutputVolume(int percent) { requestedVolume.store(preferences::clamp(percent,0,100)); }

bool startVideoCompanionAudio(const String& path) {
    if (!audio_command || !path.length()) return false;

    AUDIO_COMMAND cmd{};
    cmd.module = AUDIO_COMMAND::AUDIO;
    cmd.audio_state = AUDIO_COMMAND::PLAY;
    cmd.path = path;
    cmd.videoSync = true;

    videoAudioPending.store(true);
    videoAudioActive.store(false);
    videoAudioClock.store(0);

    if (xQueueSend(audio_command, &cmd, pdMS_TO_TICKS(50)) != pdPASS) {
        videoAudioPending.store(false);
        return false;
    }
    return true;
}

void stopVideoCompanionAudio() {
    if (!videoAudioPending.load() && !videoAudioActive.load()) return;
    if (!audio_command) {
        videoAudioPending.store(false);
        videoAudioActive.store(false);
        videoAudioClock.store(0);
        return;
    }

    AUDIO_COMMAND cmd{};
    cmd.module = AUDIO_COMMAND::AUDIO;
    cmd.audio_state = AUDIO_COMMAND::STOP;
    cmd.videoSync = true;
    xQueueSend(audio_command, &cmd, pdMS_TO_TICKS(50));
}

bool videoCompanionAudioActive() {
    return videoAudioActive.load();
}

uint32_t videoCompanionAudioClockMs() {
    return videoAudioClock.load();
}
void setVoiceAssistantEnabled(bool enabled) { voiceAssistantEnabled.store(enabled); classifierNeedsReset=true; }
uint32_t consumeFinishedTrack() {
    return playbackEvents.takeCompleted();
}


/** Audio buffers, pointers and selectors */
typedef struct {
    signed short *buffers[2];
    volatile unsigned char buf_select;
    volatile unsigned char buf_ready;
    volatile unsigned int buf_count;
    unsigned int n_samples;
} inference_t;

static inference_t inference;
static constexpr int AUDIO_SRAM_FALLBACK_BUFFER_BYTES = 24 * 1024;
static constexpr int AUDIO_PSRAM_BUFFER_BYTES = 1024 * 1024;
static const uint32_t sample_buffer_size = 2048;
// Small capture/DMA-adjacent scratch buffers stay internal; large queues/read-ahead live in PSRAM.
static signed short sampleBuffer[sample_buffer_size];
static bool debug_nn = false; // Set this to true to see e.g. features generated from the raw signal
static int print_results = -(EI_CLASSIFIER_SLICES_PER_MODEL_WINDOW);
static volatile bool record_status = true;

static int32_t raw32_buffer[sample_buffer_size / 4];

static constexpr uint16_t RECORDER_FRAME_SAMPLES = sample_buffer_size / 4;
static constexpr uint8_t RECORDER_QUEUE_DEPTH = 32;

struct RecorderFrame {
    uint16_t sampleCount;
    int16_t samples[RECORDER_FRAME_SAMPLES];
};

// Keep the ~32 KiB recorder backlog in PSRAM. FreeRTOS queues only move 1-byte slot IDs,
// which keeps internal SRAM pressure low while allowing SD writes to briefly yield to playback/uploads.
static RecorderFrame* recorderFrames = nullptr;
static QueueHandle_t recorderReadyQueue = nullptr;
static QueueHandle_t recorderFreeQueue = nullptr;
static TaskHandle_t microphoneTask = nullptr;
static volatile bool microphoneReady = false;
static volatile bool microphoneCapturing = false;
static volatile int16_t microphoneLevel = 0;
static volatile unsigned long microphoneLastSampleMillis = 0;
static volatile uint32_t microphoneReadErrors = 0;
static volatile uint32_t recordingDroppedFrames = 0;

// Realtime Gemini Live microphone transport. 512 samples at 16 kHz = 32 ms,
// which keeps latency low without flooding the WebSocket with tiny packets.
static constexpr uint16_t LIVE_MIC_FRAME_SAMPLES = sample_buffer_size / 4;
static constexpr uint8_t LIVE_MIC_QUEUE_DEPTH = 16;
struct LiveMicFrame {
    uint16_t sampleCount;
    int16_t samples[LIVE_MIC_FRAME_SAMPLES];
};
static LiveMicFrame* liveMicFrames = nullptr;
static QueueHandle_t liveMicReadyQueue = nullptr;
static QueueHandle_t liveMicFreeQueue = nullptr;
static std::atomic<bool> liveMicStreaming{false};
static std::atomic<uint32_t> liveMicDroppedFrames{0};

// Gemini native audio arrives as mono 16-bit PCM at 24 kHz. Keep a generous
// ~512 KiB queue in PSRAM so network bursts are decoupled from I2S playback.
static constexpr size_t LIVE_PCM_BLOCK_BYTES = 4096;
static constexpr uint16_t LIVE_PCM_BLOCK_COUNT = 128;
struct LivePcmBlock {
    uint16_t length;
    uint32_t epoch;
    uint8_t data[LIVE_PCM_BLOCK_BYTES];
};
static LivePcmBlock* livePcmBlocks = nullptr;
static QueueHandle_t livePcmReadyQueue = nullptr;
static QueueHandle_t livePcmFreeQueue = nullptr;
static std::atomic<bool> livePcmEnabled{false};
static std::atomic<bool> livePcmSpeaking{false};
static std::atomic<bool> livePcmInputComplete{true};
static std::atomic<bool> livePcmBuffering{false};
static std::atomic<size_t> livePcmQueuedBytes{0};
static std::atomic<uint32_t> livePcmEpoch{0};
static std::atomic<uint32_t> livePcmFirstQueuedAtMs{0};
static std::atomic<uint32_t> livePcmUnderruns{0};

// Gemini output is 24 kHz mono PCM16 => 48,000 bytes/sec.  The API delivers
// output in network chunks, not at perfectly periodic intervals.  Starting I2S
// on the first chunk makes even a short Wi-Fi/TLS scheduling gap audible.  Hold
// roughly one third of a second before playback so the large PSRAM queue acts as
// a real jitter buffer rather than just unused capacity.
static constexpr size_t LIVE_PCM_PREBUFFER_BYTES = 16 * 1024; // ~341 ms @ 24 kHz PCM16
static constexpr uint32_t LIVE_PCM_MAX_PREBUFFER_MS = 650;    // never wait forever on a tiny/stalled turn

static void subtractLivePcmQueuedBytes(size_t amount) {
    size_t current = livePcmQueuedBytes.load();
    while (true) {
        const size_t next = current > amount ? current - amount : 0;
        if (livePcmQueuedBytes.compare_exchange_weak(current, next)) return;
    }
}

static void audio_inference_callback(uint32_t nSamples);
static void capture_samples(void* arg);
static bool microphone_inference_start(uint32_t nSamples);
static bool microphone_inference_record(void);
static int microphone_audio_signal_get_data(size_t offset, size_t length, float *outPtr);
static int i2s_init(uint32_t samplingRate);
static bool writeRecorderFrame(const RecorderFrame& frame);

static void freeInferenceBuffers() {
    if (inference.buffers[0]) heap_caps_free(inference.buffers[0]);
    if (inference.buffers[1]) heap_caps_free(inference.buffers[1]);
    inference.buffers[0] = nullptr;
    inference.buffers[1] = nullptr;
}

static void resetRecorderQueues() {
    if (!recorderReadyQueue || !recorderFreeQueue || !recorderFrames) return;
    xQueueReset(recorderReadyQueue);
    xQueueReset(recorderFreeQueue);
    for (uint8_t slot = 0; slot < RECORDER_QUEUE_DEPTH; ++slot) {
        xQueueSend(recorderFreeQueue, &slot, 0);
    }
}

static void releaseRecorderStorage() {
    if (recorderReadyQueue) { vQueueDelete(recorderReadyQueue); recorderReadyQueue = nullptr; }
    if (recorderFreeQueue) { vQueueDelete(recorderFreeQueue); recorderFreeQueue = nullptr; }
    if (recorderFrames) { heap_caps_free(recorderFrames); recorderFrames = nullptr; }
}

static void resetLiveMicQueues() {
    if (!liveMicReadyQueue || !liveMicFreeQueue || !liveMicFrames) return;
    xQueueReset(liveMicReadyQueue);
    xQueueReset(liveMicFreeQueue);
    for (uint8_t slot = 0; slot < LIVE_MIC_QUEUE_DEPTH; ++slot) xQueueSend(liveMicFreeQueue, &slot, 0);
    liveMicDroppedFrames.store(0);
}

static void releaseLiveMicStorage() {
    liveMicStreaming.store(false);
    if (liveMicReadyQueue) { vQueueDelete(liveMicReadyQueue); liveMicReadyQueue = nullptr; }
    if (liveMicFreeQueue) { vQueueDelete(liveMicFreeQueue); liveMicFreeQueue = nullptr; }
    if (liveMicFrames) { heap_caps_free(liveMicFrames); liveMicFrames = nullptr; }
}

static bool ensureLiveMicStorage() {
    if (liveMicFrames && liveMicReadyQueue && liveMicFreeQueue) return true;
    // A previous partial allocation must not be overwritten on retry. This can
    // happen when PSRAM allocation succeeds but one of the small FreeRTOS queues
    // cannot be created because internal SRAM is fragmented.
    if (liveMicFrames || liveMicReadyQueue || liveMicFreeQueue) releaseLiveMicStorage();

    const uint32_t caps = psramFound() ? (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) : (MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    liveMicFrames = static_cast<LiveMicFrame*>(heap_caps_calloc(LIVE_MIC_QUEUE_DEPTH, sizeof(LiveMicFrame), caps));
    if (!liveMicFrames && psramFound()) liveMicFrames = static_cast<LiveMicFrame*>(heap_caps_calloc(LIVE_MIC_QUEUE_DEPTH, sizeof(LiveMicFrame), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    liveMicReadyQueue = xQueueCreate(LIVE_MIC_QUEUE_DEPTH, sizeof(uint8_t));
    liveMicFreeQueue = xQueueCreate(LIVE_MIC_QUEUE_DEPTH, sizeof(uint8_t));
    if (!liveMicFrames || !liveMicReadyQueue || !liveMicFreeQueue) {
        releaseLiveMicStorage();
        return false;
    }
    resetLiveMicQueues();
    return true;
}

static void resetLivePcmQueues() {
    if (!livePcmReadyQueue || !livePcmFreeQueue || !livePcmBlocks) return;
    xQueueReset(livePcmReadyQueue);
    xQueueReset(livePcmFreeQueue);
    for (uint16_t slot = 0; slot < LIVE_PCM_BLOCK_COUNT; ++slot) xQueueSend(livePcmFreeQueue, &slot, 0);
    livePcmQueuedBytes.store(0);
    livePcmSpeaking.store(false);
    livePcmInputComplete.store(true);
    livePcmBuffering.store(false);
    livePcmFirstQueuedAtMs.store(0);
    livePcmUnderruns.store(0);
}

static void releaseLivePcmStorage() {
    livePcmEnabled.store(false);
    livePcmSpeaking.store(false);
    livePcmInputComplete.store(true);
    livePcmBuffering.store(false);
    livePcmQueuedBytes.store(0);
    livePcmFirstQueuedAtMs.store(0);
    if (livePcmReadyQueue) { vQueueDelete(livePcmReadyQueue); livePcmReadyQueue = nullptr; }
    if (livePcmFreeQueue) { vQueueDelete(livePcmFreeQueue); livePcmFreeQueue = nullptr; }
    if (livePcmBlocks) { heap_caps_free(livePcmBlocks); livePcmBlocks = nullptr; }
}

static bool ensureLivePcmStorage() {
    if (livePcmBlocks && livePcmReadyQueue && livePcmFreeQueue) return true;
    if (livePcmBlocks || livePcmReadyQueue || livePcmFreeQueue) releaseLivePcmStorage();

    const uint32_t caps = psramFound() ? (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) : (MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    livePcmBlocks = static_cast<LivePcmBlock*>(heap_caps_calloc(LIVE_PCM_BLOCK_COUNT, sizeof(LivePcmBlock), caps));
    if (!livePcmBlocks && psramFound()) livePcmBlocks = static_cast<LivePcmBlock*>(heap_caps_calloc(LIVE_PCM_BLOCK_COUNT, sizeof(LivePcmBlock), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    livePcmReadyQueue = xQueueCreate(LIVE_PCM_BLOCK_COUNT, sizeof(uint16_t));
    livePcmFreeQueue = xQueueCreate(LIVE_PCM_BLOCK_COUNT, sizeof(uint16_t));
    if (!livePcmBlocks || !livePcmReadyQueue || !livePcmFreeQueue) {
        releaseLivePcmStorage();
        return false;
    }
    resetLivePcmQueues();
    return true;
}

static int16_t convertI2SSample(int32_t rawSample) {
    int32_t sample = (rawSample >> 16) * 16;
    if (sample > 32767) sample = 32767;
    if (sample < -32768) sample = -32768;
    return static_cast<int16_t>(sample);
}

struct wav_header_t {
  char chunkID[4] = {'R', 'I', 'F', 'F'};
  uint32_t chunkSize;
  char format[4] = {'W', 'A', 'V', 'E'};
  char subchunk1ID[4] = {'f', 'm', 't', ' '};
  uint32_t subchunk1Size = 16;
  uint16_t audioFormat = 1;
  uint16_t numChannels = 1;
  uint32_t sampleRate = 16000;
  uint32_t byteRate = 16000 * 1 * 16 / 8;
  uint16_t blockAlign = 1 * 16 / 8;
  uint16_t bitsPerSample = 16;
  char subchunk2ID[4] = {'d', 'a', 't', 'a'};
  uint32_t subchunk2Size;
};

File recordFile;
uint32_t totalSize = 0;
static uint32_t nextRecordingNumber = 1;
namespace {
portMUX_TYPE audioMetadataMux = portMUX_INITIALIZER_UNLOCKED;
constexpr size_t SongTitleCapacity = 256;
constexpr size_t RecordingNameCapacity = 96;
char currentSongTitleText[SongTitleCapacity] = "Choose a track";
char recordingNameText[RecordingNameCapacity] = "No recording yet";
void setMetadata(char* destination,size_t capacity,const char* value){
    if(!value)value="";
    portENTER_CRITICAL(&audioMetadataMux);
    strlcpy(destination,value,capacity);
    portEXIT_CRITICAL(&audioMetadataMux);
}
String snapshotMetadata(const char* source,size_t capacity){
    char copy[SongTitleCapacity];
    const size_t copyCapacity=capacity>sizeof(copy)?sizeof(copy):capacity;
    portENTER_CRITICAL(&audioMetadataMux);
    strlcpy(copy,source,copyCapacity);
    portEXIT_CRITICAL(&audioMetadataMux);
    return String(copy);
}
void setCurrentSongTitle(const char* value){setMetadata(currentSongTitleText,sizeof(currentSongTitleText),value);}
void setCurrentSongTitle(const String& value){setCurrentSongTitle(value.c_str());}
void setRecordingName(const char* value){setMetadata(recordingNameText,sizeof(recordingNameText),value);}
}
String getCurrentSongTitle(){return snapshotMetadata(currentSongTitleText,sizeof(currentSongTitleText));}
String getRecordingName(){return snapshotMetadata(recordingNameText,sizeof(recordingNameText));}

bool isOnlineAudio = false;
bool hasPausedAudio = false;
const char* onlineStationNames[] = {"Radio Paradise", "SomaFM Groove Salad", "Lofi"};
const int MAX_STATIONS = 3;
String onlineStations[MAX_STATIONS] = {
    "http://stream.radioparadise.com/aac-128",      // สถานีที่ 1: Radio Paradise
    "http://ice1.somafm.com/groovesalad-128-mp3",   // สถานีที่ 2: SomaFM (Chill)
    "http://lofi.stream.laut.fm/lofi"
};

int currentStationIndex = 2;

void initAudio(){
  Serial.printf("Audio Task started on Core %d\n", xPortGetCoreID());
  // ESP32-audioI2S defaults to ~300 KiB in PSRAM. A larger read-ahead buffer makes
  // local/online playback much more tolerant of SD and Wi-Fi upload bursts.
  audio.setBufsize(AUDIO_SRAM_FALLBACK_BUFFER_BYTES, AUDIO_PSRAM_BUFFER_BYTES);
  if(audio.setPinout(AUDIO_BCLK, AUDIO_LRCLK, AUDIO_DIN)) {
    Serial.println("installed audio.");
    isAudio_install = true;
  } else {
    Serial.println("install audio failed.");
  }
  clearMusicOutputBatch();
  setOutputVolume(userSettings.values.volume);
  audio.setVolume(preferences::hardwareVolume(userSettings.values.volume));
  if (!ensureLivePcmStorage()) Serial.println("[AUDIO] Unable to allocate Gemini Live PCM queue");
  else Serial.printf("[AUDIO] Gemini Live output queue: %u KiB (%s)\n",
                     static_cast<unsigned>((sizeof(LivePcmBlock) * LIVE_PCM_BLOCK_COUNT) / 1024),
                     psramFound() ? "PSRAM preferred" : "internal RAM");

}

void initMicrophone() {
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    setVoiceAssistantEnabled(userSettings.values.voice != 0);
    microphoneReady = microphone_inference_start(EI_CLASSIFIER_SLICE_SIZE);
    Serial.println(microphoneReady ? "Microphone ready" : "Microphone initialization failed");
}

int16_t readMicData() {
    return microphoneLevel;
}

void detectWord() {
    if (!shouldRunRecognition(voiceAssistantEnabled.load(),microphoneReady,app::runtime.isRecordingMode)) {
        classifierNeedsReset = true;
        return;
    }
    if (classifierNeedsReset) { run_classifier_init(); classifierNeedsReset=false; return; }

    bool m = microphone_inference_record();
    if (!m) {
        return;
    }

    signal_t signal;
    signal.total_length = EI_CLASSIFIER_SLICE_SIZE;
    signal.get_data = &microphone_audio_signal_get_data;
    ei_impulse_result_t result = {0};

    EI_IMPULSE_ERROR r = run_classifier_continuous(&signal, &result, debug_nn);
    if (r != EI_IMPULSE_OK) {
        ei_printf("ERR: Failed to run classifier (%d)\n", r);
        return;
    }

    if (++print_results >= (EI_CLASSIFIER_SLICES_PER_MODEL_WINDOW)) {
        ei_printf("Predictions ");
        ei_printf("(DSP: %d ms., Classification: %d ms., Anomaly: %d ms.)",
            result.timing.dsp, result.timing.classification, result.timing.anomaly);
        ei_printf(": \n");

        for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
            ei_printf("    %s: ", result.classification[ix].label);
            ei_printf_float(result.classification[ix].value);
            ei_printf("\n");

            if (strcmp(result.classification[ix].label, "เปิดไฟ") == 0 && result.classification[ix].value > 0.8f) {
                Serial.println("Voice command: light on");
                digitalWrite(LED_PIN, HIGH);
            }
            else if (strcmp(result.classification[ix].label, "ปิดไฟ") == 0 && result.classification[ix].value > 0.8f) {
                Serial.println("Voice command: light off");
                digitalWrite(LED_PIN, LOW);
            }
        }

#if EI_CLASSIFIER_HAS_ANOMALY == 1
        ei_printf("    anomaly score: ");
        ei_printf_float(result.anomaly);
        ei_printf("\n");
#endif

        print_results = 0;
    }
}

static void audio_inference_callback(uint32_t n_samples)
{
    for (uint32_t i = 0; i < n_samples; i++) {
        inference.buffers[inference.buf_select][inference.buf_count++] = sampleBuffer[i];

        if (inference.buf_count >= inference.n_samples) {
            inference.buf_select ^= 1;
            inference.buf_count = 0;
            inference.buf_ready = 1;
        }
    }
}

static void capture_samples(void* arg) {
    const uint32_t i2sBytesToRead = sizeof(raw32_buffer);
    size_t bytesRead = 0;
    microphoneCapturing = true;

    while (record_status) {
        const esp_err_t result = i2s_read(MIC_I2S_PORT, raw32_buffer, i2sBytesToRead,
                                          &bytesRead, pdMS_TO_TICKS(100));
        if (result != ESP_OK || bytesRead == 0) {
            microphoneReadErrors++;
            continue;
        }

        const uint16_t sampleCount = bytesRead / sizeof(int32_t);
        if (sampleCount == 0) continue;
        microphoneLastSampleMillis = millis();

        for (uint16_t index = 0; index < sampleCount; ++index) {
            sampleBuffer[index] = convertI2SSample(raw32_buffer[index]);
        }
        microphoneLevel = sampleBuffer[sampleCount - 1];
        microphoneEnvelope.push(sampleBuffer, sampleCount, millis());

        if (app::runtime.isRecordingMode) {
            if (!app::runtime.isRecording || !recorderFrames || !recorderReadyQueue || !recorderFreeQueue) continue;
            uint8_t slot = 0;
            if (xQueueReceive(recorderFreeQueue, &slot, 0) != pdPASS) { recordingDroppedFrames++; continue; }
            RecorderFrame& frame = recorderFrames[slot];
            frame.sampleCount = sampleCount;
            memcpy(frame.samples, sampleBuffer, sampleCount * sizeof(int16_t));
            if (!app::runtime.isRecording || xQueueSend(recorderReadyQueue, &slot, 0) != pdPASS) {
                xQueueSend(recorderFreeQueue, &slot, 0);
                recordingDroppedFrames++;
            }
            continue;
        }

        if (liveMicStreaming.load() && liveMicFrames && liveMicReadyQueue && liveMicFreeQueue) {
            uint8_t slot = 0;
            if (xQueueReceive(liveMicFreeQueue, &slot, 0) == pdPASS) {
                LiveMicFrame& frame = liveMicFrames[slot];
                frame.sampleCount = sampleCount;
                memcpy(frame.samples, sampleBuffer, sampleCount * sizeof(int16_t));
                if (xQueueSend(liveMicReadyQueue, &slot, 0) != pdPASS) {
                    xQueueSend(liveMicFreeQueue, &slot, 0);
                    liveMicDroppedFrames.fetch_add(1);
                }
            } else liveMicDroppedFrames.fetch_add(1);
            // Local wake-word inference is unnecessary while a cloud live session owns the mic.
            inference.buf_count = 0; inference.buf_ready = 0;
            continue;
        }

        if (voiceAssistantEnabled.load()) audio_inference_callback(sampleCount);
        else { inference.buf_count=0; inference.buf_ready=0; }
    }

    microphoneCapturing = false;
    microphoneTask = nullptr;
    vTaskDelete(nullptr);
}

/**
 * @brief      Allocate inference buffers, initialize I2S and start the capture task.
 *
 * @param[in]  n_samples  Number of samples in each inference buffer.
 *
 * @return     True if buffers, I2S, recorder queue and capture task were created.
 */
static bool microphone_inference_start(uint32_t n_samples)
{
    const size_t inferenceBytes = n_samples * sizeof(signed short);
    const uint32_t preferredCaps = psramFound() ? (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) : (MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

    inference.buffers[0] = static_cast<signed short*>(heap_caps_malloc(inferenceBytes, preferredCaps));
    if (!inference.buffers[0] && psramFound()) {
        inference.buffers[0] = static_cast<signed short*>(heap_caps_malloc(inferenceBytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    }
    if (!inference.buffers[0]) return false;

    inference.buffers[1] = static_cast<signed short*>(heap_caps_malloc(inferenceBytes, preferredCaps));
    if (!inference.buffers[1] && psramFound()) {
        inference.buffers[1] = static_cast<signed short*>(heap_caps_malloc(inferenceBytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    }
    if (!inference.buffers[1]) {
        freeInferenceBuffers();
        return false;
    }

    inference.buf_select = 0;
    inference.buf_count  = 0;
    inference.n_samples  = n_samples;
    inference.buf_ready  = 0;

    if (i2s_init(EI_CLASSIFIER_FREQUENCY) != 0) {
        freeInferenceBuffers();
        return false;
    }

    const size_t recorderBytes = sizeof(RecorderFrame) * RECORDER_QUEUE_DEPTH;
    recorderFrames = static_cast<RecorderFrame*>(heap_caps_calloc(
        RECORDER_QUEUE_DEPTH, sizeof(RecorderFrame),
        psramFound() ? (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) : (MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)));
    if (!recorderFrames && psramFound()) {
        recorderFrames = static_cast<RecorderFrame*>(heap_caps_calloc(
            RECORDER_QUEUE_DEPTH, sizeof(RecorderFrame), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    }
    recorderReadyQueue = xQueueCreate(RECORDER_QUEUE_DEPTH, sizeof(uint8_t));
    recorderFreeQueue = xQueueCreate(RECORDER_QUEUE_DEPTH, sizeof(uint8_t));
    if (!recorderFrames || !recorderReadyQueue || !recorderFreeQueue) {
        releaseRecorderStorage();
        i2s_driver_uninstall(MIC_I2S_PORT);
        freeInferenceBuffers();
        return false;
    }
    resetRecorderQueues();
    if (!ensureLiveMicStorage()) {
        Serial.println("[AUDIO] Unable to allocate Gemini Live microphone queue");
        releaseRecorderStorage();
        i2s_driver_uninstall(MIC_I2S_PORT);
        freeInferenceBuffers();
        return false;
    }

    Serial.printf("[AUDIO] Inference buffers: %u bytes x2 | recorder backlog: %u KiB | live mic: %u KiB | PSRAM=%s\n",
                  static_cast<unsigned>(inferenceBytes),
                  static_cast<unsigned>((recorderBytes + 1023) / 1024),
                  static_cast<unsigned>((sizeof(LiveMicFrame) * LIVE_MIC_QUEUE_DEPTH + 1023) / 1024),
                  psramFound() ? "yes" : "no");

    record_status = true;
    // Keep microphone DMA/copy work off Core 0, which owns SD music service,
    // Gemini transport and speaker timing. The old unpinned priority-10 task
    // could preempt local playback every microphone frame even when no voice
    // command was being recognized. Core 1 has enough headroom for this short
    // ~32 ms cadence capture job; priority 5 stays above display/UI work.
    const BaseType_t taskCreated = xTaskCreatePinnedToCore(
        capture_samples, "CaptureSamples", 4096, nullptr, 5, &microphoneTask, 1);
    if (taskCreated != pdPASS) {
        releaseLiveMicStorage();
        releaseRecorderStorage();
        i2s_driver_uninstall(MIC_I2S_PORT);
        freeInferenceBuffers();
        return false;
    }

    return true;
}

/**
 * @brief      Consume the ready flag without waiting for new microphone data.
 *
 * @return     True when a completed inference buffer is available.
 */
static bool microphone_inference_record(void)
{
    if (inference.buf_ready == 0) {
        return false;
    }

    if (inference.buf_ready > 1) {
        ei_printf("Warn: Buffer overrun (%d)\n", inference.buf_ready);
    }

    inference.buf_ready = 0;
    return true;
}

/**
 * Get raw audio signal data
 */
static int microphone_audio_signal_get_data(size_t offset, size_t length, float *out_ptr)
{
    numpy::int16_to_float(&inference.buffers[inference.buf_select ^ 1][offset], out_ptr, length);
    return 0;
}

static int i2s_init(uint32_t samplingRate) {
  i2s_config_t i2s_config = {
      .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
      .sample_rate = samplingRate,
      .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
      .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
      .communication_format = I2S_COMM_FORMAT_STAND_I2S,
      .intr_alloc_flags = 0,
      .dma_buf_count = 8,
      .dma_buf_len = 512,
      .use_apll = false,
      .tx_desc_auto_clear = false,
      .fixed_mclk = -1,
  };
  i2s_pin_config_t pin_config = {
      .bck_io_num = MIC_SCK_PIN,
      .ws_io_num = MIC_WS_PIN,
      .data_out_num = I2S_PIN_NO_CHANGE,
      .data_in_num = MIC_SD_PIN,
  };

  if (i2s_driver_install(MIC_I2S_PORT, &i2s_config, 0, nullptr) != ESP_OK) {
    return -1;
  }
  if (i2s_set_pin(MIC_I2S_PORT, &pin_config) != ESP_OK) {
    i2s_driver_uninstall(MIC_I2S_PORT);
    return -1;
  }
  if (i2s_zero_dma_buffer(MIC_I2S_PORT) != ESP_OK) {
    i2s_driver_uninstall(MIC_I2S_PORT);
    return -1;
  }
  return 0;
}

#if !defined(EI_CLASSIFIER_SENSOR) || EI_CLASSIFIER_SENSOR != EI_CLASSIFIER_SENSOR_MICROPHONE
#error "Invalid model for current sensor."
#endif


bool startLiveMicrophoneStream() {
    if (!microphoneReady || !ensureLiveMicStorage()) return false;
    // Capture may still own a slot from the previous session. Rebuilding the
    // free queue would publish that slot twice when capture returns it.
    liveMicStreaming.store(false);
    uint8_t slot = 0;
    while (xQueueReceive(liveMicReadyQueue, &slot, 0) == pdPASS) xQueueSend(liveMicFreeQueue, &slot, 0);
    liveMicStreaming.store(true);
    Serial.println("[GEMINI] Live microphone stream enabled (16 kHz mono PCM)");
    return true;
}

void stopLiveMicrophoneStream() {
    liveMicStreaming.store(false);
    // Leave ownership intact; the next start drains pending frames.
}

bool readLiveMicrophoneFrame(int16_t* output, size_t capacitySamples, size_t& sampleCount, TickType_t timeout) {
    sampleCount = 0;
    if (!output || !liveMicStreaming.load() || !liveMicFrames || !liveMicReadyQueue || !liveMicFreeQueue) return false;
    uint8_t slot = 0;
    if (xQueueReceive(liveMicReadyQueue, &slot, timeout) != pdPASS) return false;
    const LiveMicFrame& frame = liveMicFrames[slot];
    sampleCount = min(capacitySamples, static_cast<size_t>(frame.sampleCount));
    memcpy(output, frame.samples, sampleCount * sizeof(int16_t));
    xQueueSend(liveMicFreeQueue, &slot, 0);
    return sampleCount > 0;
}

bool startLivePcmOutput() {
    if (!ensureLivePcmStorage()) return false;
    clearLivePcmOutput();
    livePcmInputComplete.store(true);
    livePcmBuffering.store(false);
    livePcmUnderruns.store(0);
    livePcmEnabled.store(true);
    return true;
}

void clearLivePcmOutput() {
    liveSpeechEnvelope.reset();
    if (!livePcmReadyQueue || !livePcmFreeQueue || !livePcmBlocks) return;
    livePcmEpoch.fetch_add(1);
    livePcmInputComplete.store(true);
    livePcmBuffering.store(false);
    livePcmFirstQueuedAtMs.store(0);
    uint16_t slot = 0;
    while (xQueueReceive(livePcmReadyQueue, &slot, 0) == pdPASS) {
        subtractLivePcmQueuedBytes(livePcmBlocks[slot].length);
        xQueueSend(livePcmFreeQueue, &slot, 0);
    }
    // The audio task still owns its in-flight block. It will stop on the epoch
    // change and release its own byte count; never zero that count from here.
}

void stopLivePcmOutput() {
    livePcmEnabled.store(false);
    clearLivePcmOutput();
}

bool queueLivePcmAudio(const uint8_t* data, size_t length, TickType_t timeout) {
    if (!data || !length || !livePcmEnabled.load() || !ensureLivePcmStorage()) return false;
    // Receiving any PCM means the current model response is still producing
    // audio. generationComplete/turnComplete will mark the producer finished.
    livePcmInputComplete.store(false);
    // PCM16 must remain sample aligned.
    length &= ~static_cast<size_t>(1);
    size_t offset = 0;
    while (offset < length) {
        uint16_t slot = 0;
        if (xQueueReceive(livePcmFreeQueue, &slot, timeout) != pdPASS) return false;
        LivePcmBlock& block = livePcmBlocks[slot];
        block.epoch = livePcmEpoch.load();
        block.length = static_cast<uint16_t>(min(LIVE_PCM_BLOCK_BYTES, length - offset));
        block.length &= ~static_cast<uint16_t>(1);
        memcpy(block.data, data + offset, block.length);
        const size_t publishedLength = block.length;
        // Account BEFORE publishing: the other core may consume/recycle this
        // slot immediately inside xQueueSend, before this task runs again.
        const size_t previousQueued = livePcmQueuedBytes.fetch_add(publishedLength);
        if (previousQueued == 0) livePcmFirstQueuedAtMs.store(millis());
        if (xQueueSend(livePcmReadyQueue, &slot, timeout) != pdPASS) {
            subtractLivePcmQueuedBytes(publishedLength);
            xQueueSend(livePcmFreeQueue, &slot, 0);
            return false;
        }
        offset += publishedLength;
    }
    return true;
}

void finishLivePcmInput() {
    livePcmInputComplete.store(true);
}

bool livePcmIsBuffering() { return livePcmBuffering.load(); }
uint32_t getLivePcmUnderruns() { return livePcmUnderruns.load(); }

bool livePcmHasBufferedAudio() {
    return livePcmSpeaking.load() || livePcmQueuedBytes.load() > 0;
}

size_t livePcmBufferedBytes() { return livePcmQueuedBytes.load(); }

void handleAudio(void *parameter) {
  // Keep the 1 KiB+ queue payload out of this task's stack.
  // ESP32-audioI2S also uses sizeable temporary stack buffers while opening files.
  static AUDIO_COMMAND cmd;
  int volume = -1;
  enum AudioState {
    STATE_STOPPED,
    STATE_PLAYING,
    STATE_PAUSED
  } currentState = STATE_STOPPED;

  unsigned long lastVolumeUpdate = 0;
  unsigned long lastWsUpdate = 0;
  bool currentTrackIsVideoAudio = false;
  bool liveOutputConfigured = false;
  bool livePlaybackPrimed = false;
  uint32_t liveStereoScratch[512];

  for(;;) {
    const int level=preferences::hardwareVolume(requestedVolume.load());
    if (level != volume) { audio.setVolume(level); volume=level; }

    if (livePcmEnabled.load()) {
      if (!liveOutputConfigured) {
        clearMusicOutputBatch();
        audio.stopSong();
        currentState = STATE_STOPPED;
        isPlayingAudio = false;
        hasPausedAudio = false;
        currentTrackIsVideoAudio = false;
        videoAudioPending.store(false); videoAudioActive.store(false); videoAudioClock.store(0);
        i2s_set_sample_rates(static_cast<i2s_port_t>(audio.getI2sPort()), 24000);
        liveOutputConfigured = true;
        Serial.println("[GEMINI] Speaker switched to 24 kHz live PCM");
      }

      const size_t queuedBytes = livePcmQueuedBytes.load();
      const bool producerFinished = livePcmInputComplete.load();
      if (!livePlaybackPrimed) {
        if (queuedBytes == 0) {
          livePcmBuffering.store(false);
          livePcmSpeaking.store(false);
          isPlayingAudio = false;
          vTaskDelay(pdMS_TO_TICKS(1));
          continue;
        }
        const uint32_t firstQueuedAt = livePcmFirstQueuedAtMs.load();
        const bool waitedLongEnough = firstQueuedAt != 0 &&
            static_cast<uint32_t>(millis() - firstQueuedAt) >= LIVE_PCM_MAX_PREBUFFER_MS;
        if (!producerFinished && queuedBytes < LIVE_PCM_PREBUFFER_BYTES && !waitedLongEnough) {
          livePcmBuffering.store(true);
          livePcmSpeaking.store(false);
          isPlayingAudio = false;
          vTaskDelay(pdMS_TO_TICKS(2));
          continue;
        }
        livePlaybackPrimed = true;
        livePcmBuffering.store(false);
        Serial.printf("[GEMINI] PCM jitter buffer ready | queued=%u bytes (~%u ms) | source=%s\n",
                      static_cast<unsigned>(queuedBytes),
                      static_cast<unsigned>((queuedBytes * 1000UL) / 48000UL),
                      producerFinished ? "complete" : "streaming");
      }

      uint16_t liveSlot = 0;
      if (livePcmReadyQueue && xQueueReceive(livePcmReadyQueue, &liveSlot, pdMS_TO_TICKS(2)) == pdPASS) {
        LivePcmBlock& block = livePcmBlocks[liveSlot];
        const uint32_t playbackEpoch = block.epoch;
        livePcmSpeaking.store(true);
        isPlayingAudio = true;
        const int percent = preferences::clamp(requestedVolume.load(), 0, 100);
        const int16_t* mono = reinterpret_cast<const int16_t*>(block.data);
        size_t samples = block.length / sizeof(int16_t);
        size_t done = 0;
        while (done < samples && livePcmEnabled.load() && playbackEpoch == livePcmEpoch.load()) {
          const size_t batch = min(static_cast<size_t>(512), samples - done);
          for (size_t i = 0; i < batch; ++i) {
            // Match ESP32-audioI2S headroom so Gemini speech does not clip at 100% volume.
            int32_t scaled = (static_cast<int32_t>(mono[done + i]) * percent) / 200;
            const uint16_t sample = static_cast<uint16_t>(static_cast<int16_t>(scaled));
            liveStereoScratch[i] = (static_cast<uint32_t>(sample) << 16) | sample;
          }
          size_t batchDone = 0;
          while (batchDone < batch && livePcmEnabled.load() && playbackEpoch == livePcmEpoch.load()) {
            const size_t requestedFrames = batch - batchDone;
            const size_t writtenSamples = writeSpeakerFrames(
                liveStereoScratch + batchDone, requestedFrames, pdMS_TO_TICKS(50));
            if (writtenSamples > 0) {
              // Meter only samples accepted by I2S, not fast-arriving network
              // chunks. This keeps the face aligned with local playback.
              liveSpeechEnvelope.push(mono + done + batchDone, writtenSamples, millis(), percent / 2);
              batchDone += writtenSamples;
            }
            if (writtenSamples < requestedFrames) {
              const size_t dropped = requestedFrames - writtenSamples;
              speakerI2sDroppedFrames.fetch_add(static_cast<uint32_t>(dropped));
              Serial.printf("[GEMINI] I2S write stalled; dropping %u samples from current block\n",
                            static_cast<unsigned>(dropped));
              break;
            }
          }
          done += batchDone;
          if (batchDone < batch) break;
        }
        subtractLivePcmQueuedBytes(block.length);
        xQueueSend(livePcmFreeQueue, &liveSlot, 0);
      } else {
        livePcmSpeaking.store(false);
        isPlayingAudio = false;
        // If Gemini is still producing audio, an empty queue is a network jitter
        // underrun. Re-prime before resuming instead of emitting many tiny
        // play/silence gaps. When generation is complete, simply finish draining.
        if (!livePcmInputComplete.load()) {
          livePlaybackPrimed = false;
          livePcmBuffering.store(true);
          livePcmFirstQueuedAtMs.store(0);
          const uint32_t underruns = livePcmUnderruns.fetch_add(1) + 1;
          // Avoid making a poor connection worse by flooding the 115200-baud
          // Serial port on every underrun. The counter remains available in
          // /api/ai/status; log only the first few and then every tenth event.
          if (underruns <= 3 || (underruns % 10) == 0) {
            Serial.printf("[GEMINI] PCM underrun #%u; rebuffering before resume\n",
                          static_cast<unsigned>(underruns));
          }
        } else {
          livePlaybackPrimed = false;
          livePcmBuffering.store(false);
          livePcmFirstQueuedAtMs.store(0);
        }
        vTaskDelay(pdMS_TO_TICKS(1));
      }
      continue;
    } else if (liveOutputConfigured) {
      liveOutputConfigured = false;
      livePlaybackPrimed = false;
      livePcmSpeaking.store(false);
      isPlayingAudio = false;
      Serial.println("[GEMINI] Live PCM speaker released");
    }

    if (xQueueReceive(audio_command, &cmd, 0) == pdPASS) {
      if (!playbackEvents.beginCommand(cmd.autoAdvanceFrom)) continue;
      if (!cmd.path.valid) { Serial.println("Audio path too long"); continue; }
      const String requestedPath(cmd.path.c_str());
      if (cmd.module == AUDIO_COMMAND::MODULE::AUDIO) {
        switch (cmd.audio_state) {
          case AUDIO_COMMAND::AUDIO_STATE::PLAY: {
            bool connected = false;
            clearMusicOutputBatch();
            musicLastService=0;
            musicSdWaits.store(0);
            musicMaxServiceGap.store(0);
            musicInputBufferBytes.store(0);
            musicMinInputBufferBytes.store(0);
            musicLowBufferEvents.store(0);
            musicBufferPrimed=false;
            if(requestedPath != "" && requestedPath != "null") {
              currentFilePath = requestedPath;
              currentAudioProgress = 0;
              currentAudioTime = 0;
              totalAudioDuration = 0;
              isOnlineAudio = currentFilePath.startsWith("http://") || currentFilePath.startsWith("https://");

              // connecttohost()/connecttoFS() already call Audio::setDefaults(),
              // which stops/closes the previous source. Calling stopSong() here as
              // well caused the Audio object to be torn down twice during fast
              // music -> video transitions.
              if (isOnlineAudio) {
                  setCurrentSongTitle("Online radio");
                  connected = audio.connecttohost(currentFilePath.c_str());
              } else {
                  if(xSemaphoreTake(sdSemaphore, pdMS_TO_TICKS(1000)) == pdTRUE) {
                    const bool exists = SD.exists(currentFilePath.c_str());
                    if (exists) {
                      Serial.printf("[AUDIO] Opening %s | stack watermark=%u\n",
                                    currentFilePath.c_str(),
                                    static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)));
                      // Use the generic FS entry point explicitly. It is the
                      // library's documented path for SD/SD_MMC/SPIFFS.
                      connected = audio.connecttoFS(SD, currentFilePath.c_str());
                    } else {
                      Serial.printf("[AUDIO] File not found: %s\n", currentFilePath.c_str());
                    }
                    xSemaphoreGive(sdSemaphore);
                  }

                  int lastSlashIndex = requestedPath.lastIndexOf('/');
                  if (lastSlashIndex >= 0) {
                      setCurrentSongTitle(requestedPath.substring(lastSlashIndex + 1));
                  } else {
                      setCurrentSongTitle(requestedPath);
                  }
              }

            }
            else {
              if (currentState == STATE_PAUSED) connected = audio.pauseResume();
            }
            currentState = connected ? STATE_PLAYING : STATE_STOPPED;
            isPlayingAudio = connected;
            currentTrackIsVideoAudio = connected && cmd.videoSync;
            videoAudioPending.store(false);
            videoAudioActive.store(currentTrackIsVideoAudio);
            videoAudioClock.store(0);
            if (connected && cmd.trackIndex >= 0) playbackEvents.markStarted(cmd.trackIndex);
            hasPausedAudio = false;
            if (!connected) setCurrentSongTitle("Unable to play - retry");
            break;
          }

          case AUDIO_COMMAND::AUDIO_STATE::PUASE:
            if(currentState == STATE_PLAYING) {
              clearMusicOutputBatch();
              audio.pauseResume();
              currentState = STATE_PAUSED;
              hasPausedAudio = true;
              isPlayingAudio = false;
              if (currentTrackIsVideoAudio) videoAudioActive.store(false);
            }
            break;

          case AUDIO_COMMAND::AUDIO_STATE::SEEK:
            clearMusicOutputBatch();
            audio.audioFileSeek(cmd.seek_time);
            currentState = STATE_PLAYING;
            isPlayingAudio = true;
            if (currentTrackIsVideoAudio) videoAudioActive.store(true);
            break;

          case AUDIO_COMMAND::AUDIO_STATE::STOP:
            clearMusicOutputBatch();
            audio.stopSong();
            hasPausedAudio = false;
            currentState = STATE_STOPPED;
            isPlayingAudio = false;
            currentTrackIsVideoAudio = false;
            videoAudioPending.store(false);
            videoAudioActive.store(false);
            videoAudioClock.store(0);
            break;
        }
      }
    }

    if(currentState!=STATE_PLAYING)musicLastService=0;
    if (isConnectSDcard && !isOnlineAudio) {
      if (currentState == STATE_PLAYING) {
        // Keep SD ownership short, but when the audioI2S input buffer is getting
        // low allow a few cooperative refill passes. Releasing the mutex after
        // every pass preserves fairness for uploads/GIF/MJPEG/history writes.
        const uint32_t before = musicInputBufferBytes.load();
        const uint8_t refillPasses =
            (musicBufferPrimed && before < MUSIC_CRITICAL_BUFFER_BYTES) ? 4 :
            (musicBufferPrimed && before < MUSIC_REFILL_LOW_WATER_BYTES) ? 2 : 1;
        for (uint8_t pass = 0; pass < refillPasses; ++pass) {
          if(xSemaphoreTake(sdSemaphore, pdMS_TO_TICKS(8)) == pdTRUE) {
              serviceMusic();
              xSemaphoreGive(sdSemaphore);
          } else {
              musicSdWaits.fetch_add(1);
              break;
          }
          if (pass + 1 < refillPasses) taskYIELD();
        }
      }

      if (currentState == STATE_PLAYING && (millis() - lastProgressUpdate >= 1000)) {
            totalAudioDuration = audio.getAudioFileDuration();
            currentAudioTime = audio.getAudioCurrentTime();

            if(totalAudioDuration > 0) {
                currentAudioProgress = (currentAudioTime * 100) / totalAudioDuration;
            }
            lastProgressUpdate = millis();
        }
    } else {
      if(currentState == STATE_PLAYING) {
        serviceMusic();
      }
    }

    if (currentState == STATE_PLAYING && currentTrackIsVideoAudio) {
      videoAudioClock.store(audio.getTotalPlayingTime());
      videoAudioActive.store(audio.isRunning());
    }

    if (currentState == STATE_PLAYING && !audio.isRunning()) {
      currentState = STATE_STOPPED;
      isPlayingAudio = false;
      hasPausedAudio = false;
      if (currentTrackIsVideoAudio) {
        currentTrackIsVideoAudio = false;
        videoAudioActive.store(false);
      }
      if (isOnlineAudio) setCurrentSongTitle("Stream ended - retry");
    }
    if(currentState == STATE_STOPPED || currentState == STATE_PAUSED) {
      vTaskDelay(pdMS_TO_TICKS(10));
    } else {
      vTaskDelay(pdMS_TO_TICKS(1));
    }
  }
}

void audio_info(const char *info){
    Serial.print("Audio Info: ");
    Serial.println(info);
}

// จะถูกเรียกอัตโนมัติเมื่ออ่านข้อมูล ID3 Tag (ชื่อเพลง, ศิลปิน) ได้
void audio_id3data(const char *info){
    Serial.print("ID3 Data: ");
    Serial.println(info);

    String id3 = String(info);
    // ไลบรารีจะส่งข้อความมาในรูปแบบ "Title: ชื่อเพลง"
    if(id3.startsWith("Title: ")){
        setCurrentSongTitle(id3.substring(7)); // ตัดคำว่า "Title: " ออก

    }
}

// จะถูกเรียกอัตโนมัติเมื่อเล่นเพลงจบไฟล์
void audio_eof_mp3(const char *info){
    Serial.print("End of File: ");
    Serial.println(info);

    // ตั้งแฟล็กเมื่อจบไฟล์; ยังไม่มีตัวอ่านแฟล็กเพื่อเล่นเพลงถัดไป
    playbackEvents.complete(!isOnlineAudio && currentFilePath.startsWith("/main/Musics/"));
}

bool enterRecordingMode() {
    if (!microphoneReady || !recorderFrames || !recorderReadyQueue || !recorderFreeQueue) return false;

    app::runtime.isRecording = false;
    app::runtime.isRecordingMode = true;
    inference.buf_count = 0;
    inference.buf_ready = 0;
    resetRecorderQueues();
    return true;
}

void exitRecordingMode() {
    stopRecording();
    app::runtime.isRecording = false;
    app::runtime.isRecordingMode = false;
    inference.buf_count = 0;
    inference.buf_ready = 0;
    resetRecorderQueues();
}

bool startRecording(const char* path) {
    if (!app::runtime.isRecordingMode || !isFileManager_install || recordFile ||
        !recorderFrames || !recorderReadyQueue || !recorderFreeQueue) {
        return false;
    }
    app::runtime.isRecording = false;

    if (xSemaphoreTake(sdSemaphore, pdMS_TO_TICKS(500)) != pdTRUE) {
        Serial.println("SD busy");
        return false;
    }

    char uniquePath[64];
    const bool numbered = path == nullptr;
    if (numbered) {
        if (!recording::nextPath(nextRecordingNumber,
                [](const char* candidate) { return SD.exists(candidate); }, uniquePath, sizeof(uniquePath))) {
            xSemaphoreGive(sdSemaphore);
            Serial.println("Unable to allocate a new recording filename");
            return false;
        }
        path = uniquePath;
    }

    // Only explicit scratch paths (AI Pet) may replace an existing file.
    if (!numbered && SD.exists(path) && !SD.remove(path)) {
        xSemaphoreGive(sdSemaphore);
        Serial.println("Unable to replace recording");
        return false;
    }

    recordFile = SD.open(path, FILE_WRITE);
    if (!recordFile) {
        xSemaphoreGive(sdSemaphore);
        Serial.println("Unable to create recording");
        return false;
    }

    wav_header_t header{};
    header.chunkSize = 36;
    header.subchunk2Size = 0;
    const bool headerWritten = recordFile.write(reinterpret_cast<const uint8_t*>(&header), sizeof(header)) == sizeof(header);

    if (!headerWritten) {
        recordFile.close();
        xSemaphoreGive(sdSemaphore);
        Serial.println("Unable to write WAV header");
        return false;
    }
    xSemaphoreGive(sdSemaphore);

    if (numbered) {
        const char* basename = strrchr(path, '/');
        setRecordingName(basename ? basename + 1 : path);
    }
    totalSize = 0;
    recordingDroppedFrames = 0;
    resetRecorderQueues();
    app::runtime.isRecording = true;
    Serial.println("Recording started");
    return true;
}

static bool writeRecorderFrame(const RecorderFrame& frame) {
    if (!recordFile || frame.sampleCount == 0) return false;
    if (xSemaphoreTake(sdSemaphore, pdMS_TO_TICKS(50)) != pdTRUE) return false;

    const size_t bytesToWrite = frame.sampleCount * sizeof(int16_t);
    const size_t bytesWritten = recordFile.write(reinterpret_cast<const uint8_t*>(frame.samples), bytesToWrite);
    xSemaphoreGive(sdSemaphore);

    if (bytesWritten != bytesToWrite) return false;
    totalSize += bytesWritten;
    return true;
}

void recordLoop() {
    if (!app::runtime.isRecording || !recorderFrames || !recorderReadyQueue || !recorderFreeQueue) return;

    uint8_t slot = 0;
    for (uint8_t index = 0; index < 4 && xQueueReceive(recorderReadyQueue, &slot, 0) == pdPASS; ++index) {
        if (!writeRecorderFrame(recorderFrames[slot])) recordingDroppedFrames++;
        xQueueSend(recorderFreeQueue, &slot, 0);
    }
}

void stopRecording() {
    if (!recordFile) return;

    app::runtime.isRecording = false;
    uint8_t slot = 0;
    while (recorderFrames && recorderReadyQueue && recorderFreeQueue && xQueueReceive(recorderReadyQueue, &slot, 0) == pdPASS) {
        if (!writeRecorderFrame(recorderFrames[slot])) recordingDroppedFrames++;
        xQueueSend(recorderFreeQueue, &slot, 0);
    }

    if (xSemaphoreTake(sdSemaphore, pdMS_TO_TICKS(500)) != pdTRUE) {
        recordFile.close();
        Serial.println("Unable to finalize recording");
        return;
    }

    wav_header_t header{};
    header.chunkSize = totalSize + 36;
    header.subchunk2Size = totalSize;
    recordFile.flush();
    recordFile.seek(0);
    recordFile.write(reinterpret_cast<const uint8_t*>(&header), sizeof(header));
    recordFile.close();
    xSemaphoreGive(sdSemaphore);
    Serial.println("Recording saved");
}

bool isMicrophoneReady() {
    return microphoneReady;
}

bool isMicrophoneCapturing() {
    return microphoneCapturing;
}

unsigned long getMicrophoneLastSampleMillis() {
    return microphoneLastSampleMillis;
}

uint32_t getMicrophoneReadErrors() {
    return microphoneReadErrors;
}

uint32_t getRecordingDroppedFrames() {
    return recordingDroppedFrames;
}

void audio_showstation(const char *info) {
    if (info && *info) setCurrentSongTitle(info);
}
void audio_showstreamtitle(const char *info) {
    if (info && *info) setCurrentSongTitle(info);
}

void audio_eof_wav(const char *info) { audio_eof_mp3(info); }
