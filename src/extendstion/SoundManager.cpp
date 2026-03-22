#include "SoundManager.hpp"
#include "FileManager.hpp"
#include "Network.hpp"

Audio audio;
bool isAudio_install;

void initAudio(){
  Serial.printf("Audio Task started on Core %d\n", xPortGetCoreID());
  audio.setPinout(BLCK_PIN, RLC_PIN, DIN_PIN);
  audio.setVolume(50);
  isAudio_install = true;
}

void handleAudio(void *parameter) {
  AUDIO_COMMAND cmd;
  int volume = 10;
  enum AudioState {
    STATE_STOPPED,
    STATE_PLAYING,
    STATE_PAUSED
  } currentState = STATE_STOPPED, previousState = STATE_STOPPED;
  
  unsigned long lastVolumeUpdate = 0;
  unsigned long lastAudioLoop = 0;
  const unsigned long VOLUME_UPDATE_INTERVAL = 100;
  const unsigned long AUDIO_LOOP_INTERVAL = 5;  // เรียก audio.loop() ทุก 5ms

  if(xSemaphoreTake(sdSemaphore, pdMS_TO_TICKS(100)) == pdTRUE) {
    SD.exists(mp3File) ? audio.connecttoFS(SD, mp3File) : Serial.println("File not found.");
    xSemaphoreGive(sdSemaphore);
  }
  
  for(;;) {
    unsigned long now = millis();
    bool commandProcessed = false;
    
    // เช็ค Queue หลายครั้งต่อ cycle
      if (xQueueReceive(audio_command, &cmd, 0) == pdPASS) {
        if (cmd.module == AUDIO_COMMAND::MODULE::AUDIO) {
          commandProcessed = true;
          
          switch (cmd.audio_state) {
            case AUDIO_COMMAND::AUDIO_STATE::PLAY:
              currentState = STATE_PLAYING;
              break;
            case AUDIO_COMMAND::AUDIO_STATE::PUASE:
              currentState = STATE_PAUSED;
              break;
            case AUDIO_COMMAND::AUDIO_STATE::STOP:
              currentState = STATE_STOPPED;
              break;
          }
        }
      }
    
    if (commandProcessed) {
      Serial.printf("[AUDIO] State: %d\n", currentState);
    }
    
    if (isConnectSDcard) {
      // Handle state changes
      if (currentState != previousState) {
        switch (currentState) {
          case STATE_PLAYING:
            if (previousState == STATE_PAUSED) {
              audio.pauseResume();
            }
            break;
          case STATE_PAUSED:
            audio.pauseResume();
            break;
          case STATE_STOPPED:
            audio.stopSong();
            break;
          default:
            break;
        }
        previousState = currentState;
      }
      
      // Audio loop (throttled)
      if (currentState == STATE_PLAYING) {
        if (now - lastAudioLoop >= AUDIO_LOOP_INTERVAL) {
          audio.loop();
          lastAudioLoop = now;
        }
      }
      
      // Volume control
      if (now - lastVolumeUpdate >= VOLUME_UPDATE_INTERVAL) {
        int newVolume = map(analogRead(LEVEL_READ), 0, 4095, 0, 20);
        if (abs(newVolume - volume) >= 1) {
          volume = newVolume;
          audio.setVolume(volume);
        }
        lastVolumeUpdate = now;
      }
    }
    
    if(currentState == STATE_STOPPED || currentState == STATE_PAUSED) {
      vTaskDelay(pdMS_TO_TICKS(20));
    }
    // Minimal yield
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}
