#include "SoundManager.hpp"
#include "DisplayManager.hpp"
#include "FileManager.hpp"
#include "Network.hpp"
#include <ArduinoJson.h>

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
  i2s_config_t mic_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT, // ไมค์ INMP441 ส่งมาเป็น 32-bit
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 64,
        .use_apll = false,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0
    };

    i2s_pin_config_t mic_pins = {
        .bck_io_num = MIC_SCK_PIN,
        .ws_io_num = MIC_WS_PIN,
        .data_out_num = I2S_PIN_NO_CHANGE, 
        .data_in_num = MIC_SD_PIN
    };

    i2s_driver_install(MIC_I2S_PORT, &mic_config, 0, NULL);
    i2s_set_pin(MIC_I2S_PORT, &mic_pins);
}

void readMicData() {
    int32_t sample32 = 0; 
    int16_t sample16 = 0;
    size_t bytes_read;

    // 1. อ่านข้อมูลเสียงแบบ 32-bit จากพอร์ต 1
    esp_err_t result = i2s_read(MIC_I2S_PORT, &sample32, sizeof(sample32), &bytes_read, portMAX_DELAY);

    if (result == ESP_OK && bytes_read > 0) {
        // 2. แปลงข้อมูลจาก 32-bit ให้เป็น 16-bit (เพื่อตัดบิตขยะทิ้งและลดขนาดตัวเลข)
        // INMP441 มักจะส่งข้อมูลมาที่บิตบนๆ การเลื่อนบิต (Bit Shift) >> 14 จะได้ค่า 16-bit ที่พอดี
        sample16 = sample32 >> 14; 

        // 3. กรองสัญญาณรบกวนจุกจิก (ถ้าไมค์ไม่มีเสียง มันมักจะพ่น 0 หรือ -1 ออกมา)
        if (sample16 != 0 && sample16 != -1) {
            // 4. พ่นค่าออกทาง Serial
            Serial.println(sample16);
        }
    }
}

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