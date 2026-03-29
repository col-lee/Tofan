#include "SoundManager.hpp"
#include "FileManager.hpp"
#include "Network.hpp"
#include <ArduinoJson.h>

Audio audio;
bool isAudio_install;
volatile int encoderValue = 50;

// เพิ่มบรรทัดนี้เพื่อจำ Path ไฟล์ที่เล่นล่าสุด
String currentFilePath = ""; 

String currentSongTitle = "Unknown";
uint32_t currentAudioTime = 0;
uint32_t totalAudioDuration = 0;

void initAudio(){
  Serial.printf("Audio Task started on Core %d\n", xPortGetCoreID());
  audio.setPinout(BLCK_PIN, RLC_PIN, DIN_PIN);
  audio.setVolume(50);
  isAudio_install = true;
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
    unsigned long now = millis();
    
    // 1. รับคำสั่งจาก Queue (WebSocket)
    if (xQueueReceive(audio_command, &cmd, 0) == pdPASS) {
      if (cmd.module == AUDIO_COMMAND::MODULE::AUDIO) {
        
        switch (cmd.audio_state) {
          case AUDIO_COMMAND::AUDIO_STATE::PLAY:
            if(cmd.path != "" && cmd.path != "null") {
              currentFilePath = cmd.path; // จำ Path ไว้
              if(xSemaphoreTake(sdSemaphore, pdMS_TO_TICKS(500)) == pdTRUE) {
                  audio.stopSong(); 
                  audio.connecttoFS(SD, currentFilePath.c_str()); 
                  xSemaphoreGive(sdSemaphore);
              }
              currentSongTitle = cmd.path; 
            } 
            else {
              if (currentState == STATE_PAUSED) {
                  audio.pauseResume(); // เล่นต่อจากเดิม
              } 
              else if (currentState == STATE_STOPPED && currentFilePath != "") {
                  if(xSemaphoreTake(sdSemaphore, pdMS_TO_TICKS(500)) == pdTRUE) {
                      audio.connecttoFS(SD, currentFilePath.c_str()); 
                      xSemaphoreGive(sdSemaphore);
                  }
              }
            }
            currentState = STATE_PLAYING;
            
            // ✅ ส่งบอกเว็บทันทีว่า "กำลังเล่นนะ" (ไอคอนจะได้เปลี่ยนทันที)
            {
                JsonDocument doc;
                doc["type"] = "audio_status";
                doc["state"] = "playing";
                doc["title"] = currentSongTitle;
                String jsonStr;
                serializeJson(doc, jsonStr);
                websocket.textAll(jsonStr);
            }
            break;
            
          case AUDIO_COMMAND::AUDIO_STATE::PUASE:
            if(currentState == STATE_PLAYING) {
              audio.pauseResume();
              currentState = STATE_PAUSED;
              
              // 🚨 พระเอกของเราอยู่ตรงนี้: ส่งบอกเว็บว่า "พักเพลงแล้วนะ"
              JsonDocument doc;
              doc["type"] = "audio_status";
              doc["state"] = "paused";
              doc["currentTime"] = audio.getAudioCurrentTime();
              String jsonStr;
              serializeJson(doc, jsonStr);
              websocket.textAll(jsonStr);
            }
            break;

          case AUDIO_COMMAND::AUDIO_STATE::SEEK:
            audio.setAudioPlayPosition(cmd.seek_time);
            currentState = STATE_PLAYING;
            break;
        }
      }
    }
    
    // 2. ลูปการทำงานของ Audio
    if (isConnectSDcard) {
      
      if (currentState == STATE_PLAYING) {
        // เอาดีเลย์ 5ms ออกแล้ว ให้ audio.loop() ทำงานตลอดเวลาเพื่อป้องกันเพลงกระตุก
        audio.loop(); 
      }
      
      // 3. จัดการ Volume ผ่าน Rotary Encoder
      if (now - lastVolumeUpdate >= 100) {
        int newVolume = encoderValue;
        if (abs(newVolume - volume) >= 1) {
          volume = newVolume;
          audio.setVolume(volume);
        }
        lastVolumeUpdate = now;
      }

      // 4. ส่งข้อมูลสถานะปัจจุบันกลับไปยัง WebSocket (ทุกๆ 1 วินาที เฉพาะตอนเล่นเพลง)
      if (currentState == STATE_PLAYING && (now - lastWsUpdate >= 1000)) {
          currentAudioTime = audio.getAudioCurrentTime();
          totalAudioDuration = audio.getAudioFileDuration();

          JsonDocument doc;
          doc["type"] = "audio_status";
          doc["state"] = "playing";
          doc["title"] = currentSongTitle;
          doc["currentTime"] = currentAudioTime;
          doc["duration"] = totalAudioDuration;
          doc["volume"] = volume;

          String jsonStr;
          serializeJson(doc, jsonStr);
          websocket.textAll(jsonStr); // บรอดแคสต์ข้อมูลให้หน้าเว็บทั้งหมด
          
          lastWsUpdate = now;
      }
    }
    
    // พัก Task ตามสถานะ (ถ้าเล่นอยู่พักน้อยมาก ถ้าหยุดแล้วให้พักเยอะได้)
    if(currentState == STATE_STOPPED || currentState == STATE_PAUSED) {
      vTaskDelay(pdMS_TO_TICKS(20));
    } else {
      vTaskDelay(pdMS_TO_TICKS(1)); 
    }
  }
}

// =========================================================
// Audio Callback Functions ( ฟังก์ชันอัตโนมัติจากไลบรารีI2S)
// =========================================================

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
        JsonDocument doc;
        doc["type"] = "audio_meta";
        doc["title"] = currentSongTitle;
        String jsonStr;
        serializeJson(doc, jsonStr);
        websocket.textAll(jsonStr);
    }
}

// จะถูกเรียกอัตโนมัติเมื่อเล่นเพลงจบไฟล์
void audio_eof_mp3(const char *info){
    Serial.print("End of File: ");
    Serial.println(info);
    
    // แจ้งเตือนหน้าเว็บว่าเพลงหยุดแล้ว (หรือคุณสามารถเขียนโค้ด Auto-play เพลงถัดไปตรงนี้ได้)
    JsonDocument doc;
    doc["type"] = "audio_status";
    doc["state"] = "stopped";
    String jsonStr;
    serializeJson(doc, jsonStr);
    websocket.textAll(jsonStr);
}