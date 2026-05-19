#include "SoundManager.hpp"
#include "DisplayManager.hpp"
#include "FileManager.hpp"
#include "Network.hpp"
#include <ArduinoJson.h>
#include "ToFan-project-1_inferencing.h"

Audio audio;
bool isAudio_install;

// เพิ่มบรรทัดนี้เพื่อจำ Path ไฟล์ที่เล่นล่าสุด
String currentFilePath = ""; 

String currentSongTitle = "Unknown";
uint32_t currentAudioTime = 0;
uint32_t totalAudioDuration = 0;

bool isPlayingAudio = false; 
int currentAudioProgress = 0; 
unsigned long lastProgressUpdate = 0;
bool autoPlayNext = false;


/** Audio buffers, pointers and selectors */
typedef struct {
    signed short *buffers[2];
    unsigned char buf_select;
    unsigned char buf_ready;
    unsigned int buf_count;
    unsigned int n_samples;
} inference_t;

static inference_t inference;
static const uint32_t sample_buffer_size = 2048;
static signed short sampleBuffer[sample_buffer_size];
static bool debug_nn = false; // Set this to true to see e.g. features generated from the raw signal
static int print_results = -(EI_CLASSIFIER_SLICES_PER_MODEL_WINDOW);
static bool record_status = true;


struct wav_header_t {
  char chunkID[4] = {'R', 'I', 'F', 'F'};
  uint32_t chunkSize; // จะถูกเติมทีหลังตอนอัดเสร็จ
  char format[4] = {'W', 'A', 'V', 'E'};
  char subchunk1ID[4] = {'f', 'm', 't', ' '};
  uint32_t subchunk1Size = 16;
  uint16_t audioFormat = 1; // PCM
  uint16_t numChannels = 1; // Mono
  uint32_t sampleRate = 16000;
  uint32_t byteRate = 16000 * 1 * 16 / 8;
  uint16_t blockAlign = 1 * 16 / 8;
  uint16_t bitsPerSample = 16;
  char subchunk2ID[4] = {'d', 'a', 't', 'a'};
  uint32_t subchunk2Size; // จะถูกเติมทีหลังตอนอัดเสร็จ
};

File recordFile;
uint32_t totalSize = 0;

const int MAX_STATIONS = 3;
String onlineStations[MAX_STATIONS] = {
    "http://stream.radioparadise.com/aac-128",      // สถานีที่ 1: Radio Paradise
    "http://ice1.somafm.com/groovesalad-128-mp3",   // สถานีที่ 2: SomaFM (Chill)
    "http://lofi.stream.laut.fm/lofi"
};

int currentStationIndex = 2;

void initAudio(){
  Serial.printf("Audio Task started on Core %d\n", xPortGetCoreID());
  if(audio.setPinout(BLCK_PIN, RLC_PIN, DIN_PIN)) {
    Serial.println("installed audio.");
    isAudio_install = true;
  } else {
    Serial.println("install audio failed.");
  }
  audio.setVolume(50);

  if (xSemaphoreTake(sdSemaphore, pdMS_TO_TICKS(500)) == pdTRUE) {
    audio.connecttoFS(SD, "/main/Musics/ใจรัก.mp3");
    xSemaphoreGive(sdSemaphore);
  }
}

void initMicrophone() {
  // i2s_config_t mic_config = {
  //       .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
  //       .sample_rate = SAMPLE_RATE,
  //       .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT, // ไมค์ INMP441 ส่งมาเป็น 32-bit
  //       .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        // .communication_format = I2S_COMM_FORMAT_STAND_I2S,
  //       .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
  //       .dma_buf_count = 8,
  //       .dma_buf_len = 64,
  //       .use_apll = false,
  //       .tx_desc_auto_clear = false,
  //       .fixed_mclk = 0
  //   };

  //   i2s_pin_config_t mic_pins = {
  //       .bck_io_num = MIC_SCK_PIN,
  //       .ws_io_num = MIC_WS_PIN,
  //       .data_out_num = I2S_PIN_NO_CHANGE, 
  //       .data_in_num = MIC_SD_PIN
  //   };

  //   i2s_driver_install(MIC_I2S_PORT, &mic_config, 0, NULL);
  //   i2s_set_pin(MIC_I2S_PORT, &mic_pins);

    // summary of inferencing settings (from model_metadata.h)

    // Test LED
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    Serial.println("Edge Impulse Inferencing Demo");

    // summary of inferencing settings (from model_metadata.h)
    ei_printf("Inferencing settings:\n");
    ei_printf("\tInterval: ");
    ei_printf_float((float)EI_CLASSIFIER_INTERVAL_MS);
    ei_printf(" ms.\n");
    ei_printf("\tFrame size: %d\n", EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE);
    ei_printf("\tSample length: %d ms.\n", EI_CLASSIFIER_RAW_SAMPLE_COUNT / 16);
    ei_printf("\tNo. of classes: %d\n", sizeof(ei_classifier_inferencing_categories) / sizeof(ei_classifier_inferencing_categories[0]));

    run_classifier_init();
    ei_printf("\nStarting continious inference in 2 seconds...\n");
    ei_sleep(2000);

    if (microphone_inference_start(EI_CLASSIFIER_SLICE_SIZE) == false) {
        ei_printf("ERR: Could not allocate audio buffer (size %d), this could be due to the window length of your model\r\n", EI_CLASSIFIER_RAW_SAMPLE_COUNT);
        return;
    }

    ei_printf("Recording...\n");
}

int16_t readMicData() {
    int32_t sample32 = 0;
    int16_t sample16 = 0;
    size_t bytes_read;

    // 1. อ่านข้อมูลเสียงแบบ 32-bit จากพอร์ต 1
    esp_err_t result = i2s_read(MIC_I2S_PORT, &sample32, sizeof(sample32), &bytes_read, portMAX_DELAY);

    if (result == ESP_OK && bytes_read > 0) {
        // 2. แปลงข้อมูลจาก 32-bit ให้เป็น 16-bit (เพื่อตัดบิตขยะทิ้งและลดขนาดตัวเลข)
        // INMP441 มักจะส่งข้อมูลมาที่บิตบนๆ การเลื่อนบิต (Bit Shift) >> 14 จะได้ค่า 16-bit ที่พอดี
        sample16 = sample32 >> 16; 

        // 3. กรองสัญญาณรบกวนจุกจิก (ถ้าไมค์ไม่มีเสียง มันมักจะพ่น 0 หรือ -1 ออกมา)
        if (sample16 != 0 && sample16 != -1) {
            // 4. พ่นค่าออกทาง Serial
            Serial.print(">MicLevel:");
            Serial.println(sample16);
            return sample16;
        }
    }

    return 0;
}

void detectWord() {
  bool m = microphone_inference_record();
    if (!m) {
        // ei_printf("ERR: Failed to record audio...\n");
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
        // print the predictions
        ei_printf("Predictions ");
        ei_printf("(DSP: %d ms., Classification: %d ms., Anomaly: %d ms.)",
            result.timing.dsp, result.timing.classification, result.timing.anomaly);
        ei_printf(": \n");
        for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
            ei_printf("    %s: ", result.classification[ix].label);
            ei_printf_float(result.classification[ix].value);
            ei_printf("\n");

            if (strcmp(result.classification[ix].label, "เปิดไฟ") == 0 && result.classification[ix].value > 0.8f)
            {
              Serial.println(">>> 💡 รับทราบ! กำลังเปิดไฟ... <<<");
              digitalWrite(LED_PIN, HIGH);
            }

            // เช็คว่าป้ายกำกับ (label) ตรงกับคำว่า "ปิดไฟ" ไหม? และมั่นใจมากกว่า 80% (0.8) หรือเปล่า?
            else if (strcmp(result.classification[ix].label, "ปิดไฟ") == 0 && result.classification[ix].value > 0.8f)
            {
              Serial.println(">>> 🌑 รับทราบ! กำลังปิดไฟ... <<<");
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

static void audio_inference_callback(uint32_t n_bytes)
{
    for(int i = 0; i < n_bytes>>1; i++) {
        inference.buffers[inference.buf_select][inference.buf_count++] = sampleBuffer[i];

        if(inference.buf_count >= inference.n_samples) {
            inference.buf_select ^= 1;
            inference.buf_count = 0;
            inference.buf_ready = 1;
        }
    }
}

static void capture_samples(void* arg) {

  const int32_t i2s_bytes_to_read = (uint32_t)arg;
  size_t bytes_read = i2s_bytes_to_read;

  // 🌟 แก้ไขตรงนี้: หาร 4 เพื่อหาจำนวนตัวอย่างก่อนสร้างถัง (ประหยัด RAM ไปได้ 4 เท่า!)
  const int32_t samples_to_read = i2s_bytes_to_read / 4; 
  int32_t raw32_buffer[samples_to_read];

  while (record_status) {

    /* read data at once from i2s */
    // จำนวนไบต์ที่อ่าน (i2s_bytes_to_read) จะพอดีกับขนาดถัง (samples_to_read * 4) เป๊ะครับ
    i2s_read((i2s_port_t)1, (void*)raw32_buffer, i2s_bytes_to_read, &bytes_read, portMAX_DELAY);

    if (bytes_read <= 0) {
      ei_printf("Error in I2S read : %d", bytes_read);
    }
    else {
        if (bytes_read < i2s_bytes_to_read) {
            // ไม่ต้องปริ้นเตือนตรงนี้ก็ได้ครับ เพราะบางทีไมค์ส่งมาไม่เต็มก้อน AI ก็ยังทำงานต่อได้
            ei_printf("Partial I2S read"); 
        }

        int samples_read = bytes_read / 4;

        // scale the data (otherwise the sound is too quiet)
        for (int x = 0; x < samples_read; x++) {
            int32_t sample = raw32_buffer[x] >> 16; // หั่นบิต
            
            sample = sample * 16; // 🌟 ปรับความดัง (Gain) ตรงนี้ (แนะนำ 16 หรือ 32)
            
            // กันเสียงแตก (Clipping Protection)
            if (sample > 32767) sample = 32767;
            if (sample < -32768) sample = -32768;
            
            sampleBuffer[x] = (int16_t)sample; // โยนเข้าถังพักของ AI
        }

        if (record_status) {
            audio_inference_callback(samples_read * 2); // คืนค่าเป็นจำนวน "ไบต์" สำหรับ 16-bit
        }
        else {
            break;
        }
    }
  }
  vTaskDelete(NULL);
}

/**
 * @brief      Init inferencing struct and setup/start PDM
 *
 * @param[in]  n_samples  The n samples
 *
 * @return     { description_of_the_return_value }
 */
static bool microphone_inference_start(uint32_t n_samples)
{
    inference.buffers[0] = (signed short *)malloc(n_samples * sizeof(signed short));

    if (inference.buffers[0] == NULL) {
        return false;
    }

    inference.buffers[1] = (signed short *)malloc(n_samples * sizeof(signed short));

    if (inference.buffers[1] == NULL) {
        ei_free(inference.buffers[0]);
        return false;
    }

    inference.buf_select = 0;
    inference.buf_count = 0;
    inference.n_samples = n_samples;
    inference.buf_ready = 0;

    if (i2s_init(EI_CLASSIFIER_FREQUENCY)) {
        ei_printf("Failed to start I2S!");
    }

    ei_sleep(100);

    record_status = true;

    xTaskCreate(capture_samples, "CaptureSamples", 1024 * 32, (void*)sample_buffer_size, 10, NULL);

    return true;
}

/**
 * @brief      Wait on new data
 *
 * @return     True when finished
 */
// 🌟 โค้ดที่แก้ไขแล้ว (Non-blocking: ลื่นไหล 100%)
static bool microphone_inference_record(void)
{
    // ถ้าถังเสียงยังไม่เต็ม ไม่ต้องรอ! คืนค่า false แล้วกลับไปรัน OS ต่อเลย
    if (inference.buf_ready == 0) {
        return false;
    }

    // ถ้าระบบหมุนมาไม่ทันจนถังล้น (Overrun)
    if (inference.buf_ready > 1) {
        // ei_printf("Warning: Buffer overrun\n"); // คอมเมนต์ไว้จะได้ไม่รกจอ
    }

    // ถ้าถังเต็มพอดี (buf_ready == 1) เคลียร์สถานะแล้วคืนค่า true ให้ AI เริ่มคิด
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

/**
 * @brief      Stop PDM and release buffers
 */
static void microphone_inference_end(void)
{
    i2s_deinit();
    ei_free(inference.buffers[0]);
    ei_free(inference.buffers[1]);
}


static int i2s_init(uint32_t sampling_rate) {
  // Start listening for audio: MONO @ 8/16KHz
  i2s_config_t i2s_config = {
      .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
      .sample_rate = sampling_rate,
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
      .bck_io_num = MIC_SCK_PIN,    // IIS_SCLK
      .ws_io_num = MIC_WS_PIN,     // IIS_LCLK
      .data_out_num = I2S_PIN_NO_CHANGE,  // IIS_DSIN
      .data_in_num = MIC_SD_PIN,   // IIS_DOUT
  };
  esp_err_t ret = 0;

  ret = i2s_driver_install((i2s_port_t)1, &i2s_config, 0, NULL);
  if (ret != ESP_OK) {
    ei_printf("Error in i2s_driver_install");
  }

  ret = i2s_set_pin((i2s_port_t)1, &pin_config);
  if (ret != ESP_OK) {
    ei_printf("Error in i2s_set_pin");
  }

  ret = i2s_zero_dma_buffer((i2s_port_t)1);
  if (ret != ESP_OK) {
    ei_printf("Error in initializing dma buffer with 0");
  }

  return int(ret);
}

static int i2s_deinit(void) {
    i2s_driver_uninstall((i2s_port_t)1); //stop & destroy i2s driver
    return 0;
}

#if !defined(EI_CLASSIFIER_SENSOR) || EI_CLASSIFIER_SENSOR != EI_CLASSIFIER_SENSOR_MICROPHONE
#error "Invalid model for current sensor."
#endif


void handleAudio(void *parameter) {
  AUDIO_COMMAND cmd;
  int volume = 50;
  enum AudioState {
    STATE_STOPPED,
    STATE_PLAYING,
    STATE_PAUSED
  } currentState = STATE_STOPPED;
  
  unsigned long lastVolumeUpdate = 0;
  unsigned long lastWsUpdate = 0;
  
  for(;;) {
    // 1. รับคำสั่งจาก Queue (รองรับระบบเดิม)
    if (xQueueReceive(audio_command, &cmd, 0) == pdPASS) {
      if (cmd.module == AUDIO_COMMAND::MODULE::AUDIO) {
        switch (cmd.audio_state) {
          case AUDIO_COMMAND::AUDIO_STATE::PLAY:
            if(cmd.path != "" && cmd.path != "null") {
              currentFilePath = cmd.path; 
              audio.pauseResume();
              audio.stopSong(); 

              if (currentFilePath.startsWith("http://") || currentFilePath.startsWith("https://")) {
                  audio.connecttohost(currentFilePath.c_str());
                  currentSongTitle = "Connecting..."; // ตั้งชื่อรอไว้ก่อน
              } else {
                  // 👉 โหมด SD Card (ทำงานเหมือนเดิมเป๊ะ)
                  if(xSemaphoreTake(sdSemaphore, pdMS_TO_TICKS(500)) == pdTRUE) {
                    audio.connecttoSD(currentFilePath.c_str());
                    xSemaphoreGive(sdSemaphore);
                  }
                  
                  int lastSlashIndex = cmd.path.lastIndexOf('/');
                  if (lastSlashIndex >= 0) {
                      currentSongTitle = cmd.path.substring(lastSlashIndex + 1);
                  } else {
                      currentSongTitle = cmd.path; 
                  }
              }
              
            }
            else {
              if (currentState == STATE_PAUSED) audio.pauseResume();
            }
            currentState = STATE_PLAYING;
            isPlayingAudio = true;
            break;
            
          case AUDIO_COMMAND::AUDIO_STATE::PUASE:
            if(currentState == STATE_PLAYING) {
              audio.pauseResume();
              currentState = STATE_PAUSED;
              isPlayingAudio = false;
            }
            break;

          case AUDIO_COMMAND::AUDIO_STATE::SEEK:
            audio.audioFileSeek(cmd.seek_time);
            currentState = STATE_PLAYING;
            isPlayingAudio = true;
            break;
            
          case AUDIO_COMMAND::AUDIO_STATE::STOP:
            audio.stopSong();
            currentState = STATE_STOPPED;
            isPlayingAudio = false;
            break;
        }
      }
    }
    
    // 2. ลูปการทำงานของ Audio
    if (isConnectSDcard) {
      if (currentState == STATE_PLAYING) {
        if(xSemaphoreTake(sdSemaphore, pdMS_TO_TICKS(5)) == pdTRUE) {
            audio.loop(); 
            xSemaphoreGive(sdSemaphore);
        }
      }
      
      // 3. อัปเดต % ความคืบหน้าเพลงสำหรับ UI

      if (currentState == STATE_PLAYING && (millis() - lastProgressUpdate >= 1000)) {
            totalAudioDuration = audio.getAudioFileDuration();
            currentAudioTime = audio.getAudioCurrentTime(); // 🌟 ดึงเวลาปัจจุบันมาเก็บไว้
            
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
    
    if(currentState == STATE_STOPPED || currentState == STATE_PAUSED) {
      vTaskDelay(pdMS_TO_TICKS(500));
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
        
        // อัปเดตชื่อเพลงกลับไปที่ Web ทันที
    }
}

// จะถูกเรียกอัตโนมัติเมื่อเล่นเพลงจบไฟล์
void audio_eof_mp3(const char *info){
    Serial.print("End of File: ");
    Serial.println(info);
    
    // แจ้งเตือนหน้าเว็บว่าเพลงหยุดแล้ว (หรือคุณสามารถเขียนโค้ด Auto-play เพลงถัดไปตรงนี้ได้)
    autoPlayNext = true;
}

void startRecording(const char* path) {
    if(!isFileManager_install) {
        Serial.println("SD Card Mount Failed");
        return;
    }

    if(SD.exists(path)) SD.remove(path);

    recordFile = SD.open(path, FILE_WRITE);
    if(!recordFile) {
        Serial.println("Failed to open file for recording");
        return;
    }

    // เขียน Header หลอกๆ ไว้ก่อน 44 ไบต์แรก
    wav_header_t header;
    recordFile.write((uint8_t*)&header, sizeof(header));
    totalSize = 0;
    Serial.println("Recording started...");
}

#define BUFFER_SAMPLES 256 

void recordLoop() {
    int32_t i2s_buffer[BUFFER_SAMPLES]; // ถังรับเสียง 32-bit จากไมค์
    int16_t wav_buffer[BUFFER_SAMPLES]; // ถังพักเสียง 16-bit ก่อนลง SD
    size_t bytes_read;

    // 1. ดูดเสียงจากไมค์มารวดเดียว 256 ตัวอย่าง
    i2s_read(I2S_NUM_1, &i2s_buffer, sizeof(i2s_buffer), &bytes_read, portMAX_DELAY);

    if (bytes_read > 0) {
        int samples_read = bytes_read / 4; 
        
        // 🌟 กำหนดตัวคูณขยายเสียง (Gain) ลองตั้งที่ 8, 16 หรือ 32 ดูครับ
        int gain_factor = 16; 

        for (int i = 0; i < samples_read; i++) {
            // 1. ดึงค่า 16-bit ออกมาตามปกติ
            int32_t raw_sample = i2s_buffer[i] >> 16; 
            
            // 2. คูณขยายเสียง!
            int32_t amplified = raw_sample * gain_factor;

            // 3. 🛑 ระบบกันเสียงแตก (Clipping Protection) 🛑
            // สำคัญมาก! ถ้าคูณแล้วเลขทะลุ 32767 ลำโพงจะดังแครกๆ เหมือนวิทยุพัง
            if (amplified > 32767) amplified = 32767;
            if (amplified < -32768) amplified = -32768;

            // 4. โยนลงถังพัก
            wav_buffer[i] = (int16_t)amplified; 
        }

        recordFile.write((uint8_t*)wav_buffer, samples_read * 2);
        totalSize += (samples_read * 2);
    }
}

void stopRecording() {
    // กลับไปอัปเดตขนาดไฟล์ที่ Header (ไบต์ที่ 4 และ 40)
    wav_header_t header;
    header.chunkSize = totalSize + 36;
    header.subchunk2Size = totalSize;

    recordFile.seek(0);
    recordFile.write((uint8_t*)&header, sizeof(header));
    recordFile.close();
    Serial.println("Recording saved!");
}