// Owns speaker playback, I2S capture, WAV recording and local voice inference.
#include "SoundManager.hpp"
#include "RecordingPath.hpp"
#include "PlaybackEvents.hpp"
#include "../core/UserSettings.hpp"
#include <atomic>
#include <esp_heap_caps.h>
#include "../display/DisplayManager.hpp"
#include "../storage/FileManager.hpp"
#include "../network/Network.hpp"
#include "../core/GlobalState.hpp"
#include <ArduinoJson.h>
#include "ToFan-project-1_inferencing.h"

Audio audio;
bool isAudio_install;

String currentFilePath = "";

String currentSongTitle = "Choose a track";
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
static String recordingName = "No recording yet";

const String& getRecordingName() { return recordingName; }

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
  setOutputVolume(userSettings.values.volume);
  audio.setVolume(preferences::hardwareVolume(userSettings.values.volume));


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

        if (app::runtime.isRecordingMode) {
            if (!app::runtime.isRecording || !recorderFrames || !recorderReadyQueue || !recorderFreeQueue) continue;

            uint8_t slot = 0;
            if (xQueueReceive(recorderFreeQueue, &slot, 0) != pdPASS) {
                recordingDroppedFrames++;
                continue;
            }
            RecorderFrame& frame = recorderFrames[slot];
            frame.sampleCount = sampleCount;
            for (uint16_t index = 0; index < sampleCount; ++index) {
                frame.samples[index] = convertI2SSample(raw32_buffer[index]);
            }
            microphoneLevel = frame.samples[sampleCount - 1];

            if (!app::runtime.isRecording || xQueueSend(recorderReadyQueue, &slot, 0) != pdPASS) {
                xQueueSend(recorderFreeQueue, &slot, 0);
                recordingDroppedFrames++;
            }
            continue;
        }

        for (uint16_t index = 0; index < sampleCount; ++index) {
            sampleBuffer[index] = convertI2SSample(raw32_buffer[index]);
        }
        microphoneLevel = sampleBuffer[sampleCount - 1];
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

    Serial.printf("[AUDIO] Inference buffers: %u bytes x2 | recorder backlog: %u KiB | PSRAM=%s\n",
                  static_cast<unsigned>(inferenceBytes),
                  static_cast<unsigned>((recorderBytes + 1023) / 1024),
                  psramFound() ? "yes" : "no");

    record_status = true;
    const BaseType_t taskCreated = xTaskCreate(capture_samples, "CaptureSamples", 4096,
                                                nullptr, 10, &microphoneTask);
    if (taskCreated != pdPASS) {
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

  for(;;) {
    const int level=preferences::hardwareVolume(requestedVolume.load());
    if (level != volume) { audio.setVolume(level); volume=level; }
    if (xQueueReceive(audio_command, &cmd, 0) == pdPASS) {
      if (!playbackEvents.beginCommand(cmd.autoAdvanceFrom)) continue;
      if (!cmd.path.valid) { Serial.println("Audio path too long"); continue; }
      const String requestedPath(cmd.path.c_str());
      if (cmd.module == AUDIO_COMMAND::MODULE::AUDIO) {
        switch (cmd.audio_state) {
          case AUDIO_COMMAND::AUDIO_STATE::PLAY: {
            bool connected = false;
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
                  currentSongTitle = "Online radio";
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
                      currentSongTitle = requestedPath.substring(lastSlashIndex + 1);
                  } else {
                      currentSongTitle = requestedPath;
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
            if (!connected) currentSongTitle = "Unable to play - retry";
            break;
          }

          case AUDIO_COMMAND::AUDIO_STATE::PUASE:
            if(currentState == STATE_PLAYING) {
              audio.pauseResume();
              currentState = STATE_PAUSED;
              hasPausedAudio = true;
              isPlayingAudio = false;
              if (currentTrackIsVideoAudio) videoAudioActive.store(false);
            }
            break;

          case AUDIO_COMMAND::AUDIO_STATE::SEEK:
            audio.audioFileSeek(cmd.seek_time);
            currentState = STATE_PLAYING;
            isPlayingAudio = true;
            if (currentTrackIsVideoAudio) videoAudioActive.store(true);
            break;

          case AUDIO_COMMAND::AUDIO_STATE::STOP:
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

    if (isConnectSDcard && !isOnlineAudio) {
      if (currentState == STATE_PLAYING) {
        if(xSemaphoreTake(sdSemaphore, pdMS_TO_TICKS(5)) == pdTRUE) {
            audio.loop();
            xSemaphoreGive(sdSemaphore);
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
        audio.loop();
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
      if (isOnlineAudio) currentSongTitle = "Stream ended - retry";
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
        currentSongTitle = id3.substring(7); // ตัดคำว่า "Title: " ออก

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
        recordingName = basename ? basename + 1 : path;
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
    if (info && *info) currentSongTitle = info;
}
void audio_showstreamtitle(const char *info) {
    if (info && *info) currentSongTitle = info;
}

void audio_eof_wav(const char *info) { audio_eof_mp3(info); }
