#define ARDUINO_H
#include <Arduino.h>
#include "extendstion/event.hpp"
#include "extendstion/GlobalVar.hpp"
#include "extendstion/DisplayManager.hpp"
#include "extendstion/FileManager.hpp"
#include "extendstion/Network.hpp"
#include "extendstion/SoundManager.hpp"
// #include "extendstion/IOManager.hpp"
#include "extendstion/HardwareManager.hpp"
#include "extendstion/config.hpp"

#ifndef DISPLAYMANAGER_HH
#define DISPLAYMANAGER_HH
  #include "extendstion/DisplayManager.hpp"
#endif

#ifndef NETWORK_LIBRARY
  #include <WiFi.h>
  #include <AsyncTCP.h>
  #include <ESPAsyncWebServer.h>
#endif

#include <AiEsp32RotaryEncoder.h>

SemaphoreHandle_t displaySemaphore = NULL;
// SemaphoreHandle_t variable_audio = NULL;
TaskHandle_t t_handleAudio = NULL;
TaskHandle_t t_handleDisplay = NULL;
TaskHandle_t runnet = NULL;
QueueHandle_t display_command = NULL;
QueueHandle_t audio_command = NULL;
QueueHandle_t api_event_queue = NULL;
NetworkManager nm;
FileManager file_card;

// Recording state control
bool volatile isRecordingMode = false;     // Disable detectWord during recording
bool volatile isRecording = false;          // Track actual recording state

AiEsp32RotaryEncoder rotaryEncoder = AiEsp32RotaryEncoder(ENC_A_PIN, ENC_B_PIN, -1, ENC_VCC, ENC_STEPS, false);
 
unsigned long backBtnPressTime = 0;
bool isBackBtnLongPressed = false;
int lastSyncedVol = -1;
unsigned long lastVolActivityTime = 0;

static unsigned long lastTouchTime = 0;

void IRAM_ATTR readEncoderISR() {
    rotaryEncoder.readEncoder_ISR();
}

void handleInput() {
  if (rotaryEncoder.encoderChanged()) {
        if (DISM.currentState == UI_STATE::HOME_MENU) {
            int rawValue = rotaryEncoder.readEncoder();

            DISM.animatedMenuIndex_target = (float)rawValue;

            // 🌟 2. คำนวณ Index ฟีเจอร์ (0-4) ให้ตรงกับไอคอน
            // สูตรนี้ช่วยให้ค่าติดลบหมุนวนกลับมาเป็น 0-4 ได้ถูกต้อง
            DISM.currentMenuIndex = (rawValue % DISM.totalItems + DISM.totalItems) % DISM.totalItems;
            Serial.println(DISM.currentMenuIndex);

            DISM.isAnimatingMenu = true;
        } 
        else if (DISM.currentState == UI_STATE::APP_MUSIC) {
            DISM.currentMusicControlIndex = rotaryEncoder.readEncoder();
            DISM.drawMusicPlayer(currentSongTitle, currentAudioProgress, isPlayingAudio);
        }
        // 🌟 เลื่อนรายการเพลงในหน้า Playlist
        else if (DISM.currentState == UI_STATE::APP_MUSIC_LIST) {
            if(!DISM.playlistNames.empty()) {
                DISM.playlistSelectedIndex = rotaryEncoder.readEncoder();
                DISM.drawMusicList();
            }
        }

        // 🌟 เลื่อนรายการรูปภาพในหน้า Display List
        else if (DISM.currentState == UI_STATE::APP_DISPLAY_LIST) {
            if(!DISM.imageNames.empty()) {
                DISM.imageSelectedIndex = rotaryEncoder.readEncoder();
                DISM.drawImageList();
            }
        }

        // 🌟 เลื่อนรายการในหน้า Settings (มีแค่ 0 กับ 1)
        else if (DISM.currentState == UI_STATE::APP_SETTINGS) {
            DISM.settingSelectedIndex = rotaryEncoder.readEncoder();
            DISM.drawSettings();
        }

        // 🌟 ปรับลดเสียง
        else if (DISM.currentState == UI_STATE::GLOBAL_VOLUME) {
            DISM.currentVolLevel = rotaryEncoder.readEncoder();
            audio.setVolume(DISM.currentVolLevel);
            DISM.drawVolumeOverlay();
            
            lastVolActivityTime = millis(); // รีเซ็ตเวลาเมื่อมีการหมุน
        }

        else if (DISM.currentState == UI_STATE::POPUP_NO_MUSIC) {
            DISM.popupSelectedIndex = rotaryEncoder.readEncoder();
            DISM.drawPopupNoMusic();
        }

        else if(DISM.currentState == UI_STATE::APP_ONLINE_MUSIC) {
            DISM.currentMusicControlIndex = rotaryEncoder.readEncoder();
            DISM.drawOnlineMusicPlayer();
        }
    }

    // 2. ตรวจจับปุ่มกด "ตกลง" (SW)
    if (digitalRead(ENC_SW) == HIGH && millis() - lastTouchTime > 300) {
        lastTouchTime = millis();
        
        if (DISM.currentState == UI_STATE::HOME_MENU) {
            DISPLAY_COMMAND cmd;
            switch (DISM.currentMenuIndex) {
                case 0:
                    // 🌟 เข้าสู่หน้าเลือกรูปภาพแทนการเล่นไฟล์โดยตรง
                    rotaryEncoder.setBoundaries(0, (int)DISM.imageNames.size() - 1, true);
                    rotaryEncoder.setEncoderValue(DISM.imageSelectedIndex);
                    DISM.loadImageList();
                    DISM.currentState = UI_STATE::APP_DISPLAY_LIST;
                    DISM.drawImageList();
                    break;
                case 1:

                    DISM.loadMusicList();

                    if(DISM.playlistNames.empty() && WiFi.status() != WL_CONNECTED) {
                        rotaryEncoder.setBoundaries(0, 1, false); // หมุนได้แค่ Yes (0) กับ No (1)
                        rotaryEncoder.setEncoderValue(0);
                        DISM.currentState = UI_STATE::POPUP_NO_MUSIC;
                        DISM.drawPopupNoMusic();
                        break;
                    } else if(DISM.playlistNames.empty() && WiFi.status() == WL_CONNECTED) {
                        rotaryEncoder.setBoundaries(0, 2, false); // หมุนได้แค่ Yes (0) กับ No (1)
                        rotaryEncoder.setEncoderValue(0);
                        DISM.currentState = UI_STATE::APP_ONLINE_MUSIC;
                        DISM.drawOnlineMusicPlayer();
                        break;
                    } else {
                        rotaryEncoder.setBoundaries(0, 3, true);
                        rotaryEncoder.setEncoderValue(DISM.currentMusicControlIndex);
                        DISM.currentState = UI_STATE::APP_MUSIC;
                        DISM.drawMusicPlayer(currentSongTitle, currentAudioProgress, isPlayingAudio);
                        break;
                    }
                    
                case 2:
                    // 🌟 เข้าสู่หน้า Settings
                    rotaryEncoder.setBoundaries(0, 1, true);
                    rotaryEncoder.setEncoderValue(DISM.settingSelectedIndex);
                    DISM.currentState = UI_STATE::APP_SETTINGS;
                    DISM.drawSettings();
                    break;

                case 3:
                    DISM.currentState = UI_STATE::APP_PET;
                    DISM.drawAIPet();
                    break;

                case 4:
                    DISM.currentState = UI_STATE::DEBUG;
                    DISM.debug();
                    break;

                case 5:
                    DISM.currentState = UI_STATE::RECORDE;
                    isRecordingMode = true;  // Disable detectWord
                    isRecording = false;      // Not recording yet, waiting for user to press button
                    DISM.recorde();
                    break;
                    
            }
        }

        // 🌟 เลือกไฟล์ภาพใน List เพื่อเล่น/แสดงผล
        else if (DISM.currentState == UI_STATE::APP_DISPLAY_LIST) {
            if (!DISM.imageNames.empty()) {
                String selectedPath = DISM.imagePaths[DISM.imageSelectedIndex];
                
                DISM.deleteUISprite(); // ลบ UI ออกก่อนแสดงภาพ
                DISM.currentState = UI_STATE::APP_DISPLAY;

                DISPLAY_COMMAND cmd;
                cmd.module = DISPLAY_COMMAND::MODULE::DIS;
                cmd.display_state = DISPLAY_COMMAND::DISPLAY_STATE::SHOW;
                cmd.path = selectedPath;
                xQueueSend(display_command, &cmd, portMAX_DELAY);
            }
        }

        else if (DISM.currentState == UI_STATE::APP_MUSIC) {
            AUDIO_COMMAND cmd;
            cmd.module = AUDIO_COMMAND::MODULE::AUDIO;
            
            if (DISM.currentMusicControlIndex == 0) { 
                // 🌟 ปุ่ม Prev 
                if (!DISM.playlistNames.empty()) {
                    DISM.currentPlayingIndex--;
                    if(DISM.currentPlayingIndex < 0) DISM.currentPlayingIndex = DISM.playlistNames.size() - 1;
                    
                    cmd.audio_state = AUDIO_COMMAND::AUDIO_STATE::PLAY;
                    cmd.path = DISM.playlistPaths[DISM.currentPlayingIndex];
                    xQueueSend(audio_command, &cmd, portMAX_DELAY);
                }
            }
            else if (DISM.currentMusicControlIndex == 1) { 
                // ปุ่ม Play/Pause
                if (isPlayingAudio) cmd.audio_state = AUDIO_COMMAND::AUDIO_STATE::PUASE;
                else { cmd.audio_state = AUDIO_COMMAND::AUDIO_STATE::PLAY; cmd.path = ""; }
                
                xQueueSend(audio_command, &cmd, portMAX_DELAY);
                DISM.drawMusicPlayer(currentSongTitle, currentAudioProgress, !isPlayingAudio); 
            }
            else if (DISM.currentMusicControlIndex == 2) { 
                // 🌟 ปุ่ม Next 
                if (!DISM.playlistNames.empty()) {
                    DISM.currentPlayingIndex++;
                    if(DISM.currentPlayingIndex >= (int)DISM.playlistNames.size()) DISM.currentPlayingIndex = 0;
                    
                    cmd.audio_state = AUDIO_COMMAND::AUDIO_STATE::PLAY;
                    cmd.path = DISM.playlistPaths[DISM.currentPlayingIndex];
                    xQueueSend(audio_command, &cmd, portMAX_DELAY);
                }
            }
            else if (DISM.currentMusicControlIndex == 3) { 
                // 🌟 กดปุ่ม Burger -> เข้าหน้า List เพลง
                DISM.loadMusicList(); // โหลดและเรียง A-Z
                rotaryEncoder.setBoundaries(0, (int)DISM.playlistNames.size(), true);
                rotaryEncoder.setEncoderValue(DISM.playlistSelectedIndex);
                DISM.currentState = UI_STATE::APP_MUSIC_LIST;
                DISM.drawMusicList();
            }
        }
        else if (DISM.currentState == UI_STATE::POPUP_NO_MUSIC) {
            if (DISM.popupSelectedIndex == 0) {
                if (nm.connectoWiFi()) {
                    DISM.isWiFiOn = true;
                    DISM.currentMusicControlIndex = 1; // ชี้ไว้ที่ปุ่ม Play
                    rotaryEncoder.setBoundaries(0, 2, false);
                    rotaryEncoder.setEncoderValue(1);
                    DISM.currentState = UI_STATE::APP_ONLINE_MUSIC;

                    AUDIO_COMMAND cmd;
                    cmd.module = AUDIO_COMMAND::MODULE::AUDIO;
                    cmd.audio_state = AUDIO_COMMAND::AUDIO_STATE::PLAY;
                    // ใส่ URL สถานีวิทยุหรือไฟล์ MP3 ออนไลน์ (ตัวอย่าง: วิทยุ Lo-fi)
                    cmd.path = onlineStations[currentStationIndex];
                    xQueueSend(audio_command, &cmd, portMAX_DELAY);
                    DISM.drawOnlineMusicPlayer();

                    return;
                } else {
                    DISM.isWiFiOn = false;
                    // ถ้ายังไม่ต่อเน็ต อาจจะให้วาดข้อความเตือน หรือเด้งไปหน้า Setting
                    Serial.println("Please connect to WiFi first!");
                    DISM.currentState = UI_STATE::APP_SETTINGS;
                    rotaryEncoder.setBoundaries(0, 1, true);
                    rotaryEncoder.setEncoderValue(1); // ชี้ไปที่เมนู WiFi
                    DISM.settingSelectedIndex = 1;
                    DISM.drawSettings();
                    return;
                }
            } else {
                // 🌟 เลือก No: กลับหน้า Home
                DISM.currentState = UI_STATE::HOME_MENU;
                rotaryEncoder.setBoundaries(-DISM.boundaries_home, DISM.boundaries_home, false);
                rotaryEncoder.setEncoderValue(DISM.animatedMenuIndex_target);
                DISM.drawHomeMenu();
            }
        }
        else if(DISM.currentState == UI_STATE::APP_ONLINE_MUSIC) {
            AUDIO_COMMAND cmd;
            cmd.module = AUDIO_COMMAND::MODULE::AUDIO;

            if (DISM.currentMusicControlIndex == 0) { 
                // 🌟 ปุ่ม Prev (สถานีก่อนหน้า)
                currentStationIndex--;
                if(currentStationIndex < 0) currentStationIndex = MAX_STATIONS - 1; // วนกลับไปท้ายสุด
                
                cmd.audio_state = AUDIO_COMMAND::AUDIO_STATE::PLAY;
                cmd.path = onlineStations[currentStationIndex];
                xQueueSend(audio_command, &cmd, portMAX_DELAY);
            }
            else if (DISM.currentMusicControlIndex == 1) { 
                // 🌟 ปุ่ม Play/Pause
                if (isPlayingAudio) {
                    cmd.audio_state = AUDIO_COMMAND::AUDIO_STATE::PUASE; // (พิมพ์ตาม enum เดิมของคุณ)
                } else { 
                    cmd.audio_state = AUDIO_COMMAND::AUDIO_STATE::PLAY; 
                    cmd.path = ""; // ส่ง path ว่าง เพื่อให้มันแค่ Resume
                }
                
                xQueueSend(audio_command, &cmd, portMAX_DELAY);
                DISM.drawOnlineMusicPlayer(); // วาดจอเพื่ออัปเดตไอคอน Play/Pause ทันที
            }
            else if (DISM.currentMusicControlIndex == 2) { 
                // 🌟 ปุ่ม Next (สถานีถัดไป)
                currentStationIndex++;
                if(currentStationIndex >= MAX_STATIONS) currentStationIndex = 0; // วนกลับมาเริ่มใหม่
                
                cmd.audio_state = AUDIO_COMMAND::AUDIO_STATE::PLAY;
                cmd.path = onlineStations[currentStationIndex];
                xQueueSend(audio_command, &cmd, portMAX_DELAY);
            }
        }
        else if (DISM.currentState == UI_STATE::APP_MUSIC_LIST) {
            // 🌟 เลือกเพลงใน List เพื่อเล่น
            if (!DISM.playlistNames.empty()) {
                DISM.currentPlayingIndex = DISM.playlistSelectedIndex;
                String selectedPath = DISM.playlistPaths[DISM.playlistSelectedIndex];
                currentSongTitle = DISM.playlistNames[DISM.playlistSelectedIndex]; // อัปเดต Global
                
                AUDIO_COMMAND cmd;
                cmd.module = AUDIO_COMMAND::MODULE::AUDIO;
                cmd.audio_state = AUDIO_COMMAND::AUDIO_STATE::PLAY;
                cmd.path = selectedPath;
                xQueueSend(audio_command, &cmd, portMAX_DELAY);

                // เด้งกลับไปหน้าเล่นเพลง
                rotaryEncoder.setBoundaries(0, 3, true);
                rotaryEncoder.setEncoderValue(DISM.currentMusicControlIndex);
                DISM.currentState = UI_STATE::APP_MUSIC;
                DISM.drawMusicPlayer(currentSongTitle, 0, true);
            }
        }

        // 🌟 เมื่อกดตกลงในหน้า Settings เพื่อสลับเปิด/ปิด
        else if (DISM.currentState == UI_STATE::APP_SETTINGS) {

            if (DISM.settingSelectedIndex == 0) {
                // รายการที่ 1: Admin Mode
                DISM.isAdminModeOn = !DISM.isAdminModeOn; // สลับค่า
                DISM.drawSettings(); // รีบวาดจอก่อน (เพราะการเปิด WiFi อาจทำให้ระบบหยุดรอแปปนึง)
                
                if(DISM.isAdminModeOn) {
                    nm.startAdminMode();
                    vTaskDelay(pdMS_TO_TICKS(200));
                    Serial.printf("Freeheap: %lu, Min free: %lu, MaxAllocHeap: %lu\n",
                                  ESP.getFreeHeap(),
                                  ESP.getMinFreeHeap(),
                                  ESP.getMaxAllocHeap());
                } else {
                    nm.stopAdminMode();
                    vTaskDelay(pdMS_TO_TICKS(200));

                    Serial.printf("Freeheap: %lu, Min free: %lu, MaxAllocHeap: %lu\n",
                                  ESP.getFreeHeap(),
                                  ESP.getMinFreeHeap(),
                                  ESP.getMaxAllocHeap());
                }
            } else if(DISM.settingSelectedIndex == 1) {
                DISM.isWiFiOn = !DISM.isWiFiOn;

                DISM.drawSettings();
                if(DISM.isWiFiOn) {
                    nm.connectoWiFi();
                    Serial.printf("Freeheap: %lu, Min free: %lu, MaxAllocHeap: %lu\n",
                      ESP.getFreeHeap(),
                      ESP.getMinFreeHeap(),
                      ESP.getMaxAllocHeap());
                } else {
                    nm.closeWiFiSTA();
                    Serial.printf("Freeheap: %lu, Min free: %lu, MaxAllocHeap: %lu\n",
                      ESP.getFreeHeap(),
                      ESP.getMinFreeHeap(),
                      ESP.getMaxAllocHeap());
                }
            }
        }

        else if(DISM.currentState == UI_STATE::DEBUG) {
            DISM.debug();
        }

        else if(DISM.currentState == UI_STATE::RECORDE) {
            // Toggle recording on/off
            if (isRecording) {
                // Stop recording
                stopRecording();
                isRecording = false;
            } else {
                // Start recording
                startRecording("/main/Musics/voice_record.wav");
                isRecording = true;
            }
            DISM.recorde();
        }

    }

    // 3. ตรวจจับปุ่ม "ย้อนกลับ" (BTN_BACK) แบบกดธรรมดา / กดค้าง
    if (digitalRead(BTN_BACK) == HIGH) {
        if (backBtnPressTime == 0) backBtnPressTime = millis();
        
        // 🌟 กดค้าง 1 วิ เข้าโหมดปรับเสียง (  )
        if (millis() - backBtnPressTime > 1000 && !isBackBtnLongPressed) {
            isBackBtnLongPressed = true;
            DISM.previousState = DISM.currentState; // จำสถานะเดิม
            DISM.currentState = UI_STATE::GLOBAL_VOLUME;
            rotaryEncoder.setBoundaries(0, 100, false);
            rotaryEncoder.setEncoderValue(DISM.currentVolLevel);
            lastVolActivityTime = millis(); // เริ่มจับเวลา
            DISM.drawVolumeOverlay();
        }
    } else {
        if (backBtnPressTime > 0) {
            if (!isBackBtnLongPressed) {
                // ย้อนกลับธรรมดา
                if (DISM.currentState == UI_STATE::APP_DISPLAY) {
                    // หากเล่นภาพ/GIF อยู่ ให้หยุดผ่าน Queue (ปลอดภัยกับ FreeRTOS)
                    DISPLAY_COMMAND cmd;
                    cmd.module = DISPLAY_COMMAND::MODULE::DIS;
                    cmd.display_state = DISPLAY_COMMAND::DISPLAY_STATE::CLEAR;
                    xQueueSend(display_command, &cmd, portMAX_DELAY);
                    
                    vTaskDelay(pdMS_TO_TICKS(50));
                    // สร้าง Sprite ใหม่และกลับมาหน้าเลือกรูป
                    rotaryEncoder.setBoundaries(0, (int)DISM.imageNames.size(), true);
                    rotaryEncoder.setEncoderValue(DISM.imageSelectedIndex);
                    DISM.createUISprite(); 
                    DISM.currentState = UI_STATE::APP_DISPLAY_LIST;
                    DISM.drawImageList();
                }
                
                else if (DISM.currentState == UI_STATE::APP_MUSIC_LIST) {
                    rotaryEncoder.setBoundaries(0, 3, true);
                    rotaryEncoder.setEncoderValue(DISM.currentMusicControlIndex);
                    DISM.currentState = UI_STATE::APP_MUSIC;
                    DISM.drawMusicPlayer(currentSongTitle, currentAudioProgress, isPlayingAudio);
                }

                else if (DISM.currentState == UI_STATE::APP_DISPLAY_LIST) {
                    // ออกจากหน้าเลือกรูป กลับหน้า Home
                    rotaryEncoder.setBoundaries(-DISM.boundaries_home, DISM.boundaries_home, false);
                    rotaryEncoder.setEncoderValue(DISM.animatedMenuIndex_target);
                    DISM.currentState = UI_STATE::HOME_MENU;
                    DISM.drawHomeMenu();
                }
                // 🌟 หน้า Settings
                else if (DISM.currentState == UI_STATE::APP_SETTINGS)
                {
                    rotaryEncoder.setBoundaries(-DISM.boundaries_home, DISM.boundaries_home, false);
                    rotaryEncoder.setEncoderValue(DISM.animatedMenuIndex_target);
                    DISM.currentState = UI_STATE::HOME_MENU;
                    DISM.drawHomeMenu();
                }

                else if(DISM.currentState == UI_STATE::APP_ONLINE_MUSIC) {
                    rotaryEncoder.setBoundaries(-DISM.boundaries_home, DISM.boundaries_home, false);
                    rotaryEncoder.setEncoderValue(DISM.animatedMenuIndex_target);
                    DISM.currentState = UI_STATE::HOME_MENU;
                    DISM.drawHomeMenu();
                }

                else if(DISM.currentState == UI_STATE::POPUP_NO_MUSIC) {
                    rotaryEncoder.setBoundaries(0, 3, true);
                    rotaryEncoder.setEncoderValue(DISM.currentMusicControlIndex);
                    DISM.currentState = UI_STATE::APP_MUSIC;
                    DISM.drawMusicPlayer(currentSongTitle, currentAudioProgress, isPlayingAudio);
                }

                else if(DISM.currentState == UI_STATE::APP_PET) {
                    rotaryEncoder.setBoundaries(-DISM.boundaries_home, DISM.boundaries_home, false);
                    rotaryEncoder.setEncoderValue(DISM.animatedMenuIndex_target);
                    DISM.currentState = UI_STATE::HOME_MENU;
                    DISM.drawHomeMenu();
                }

                else if(DISM.currentState == UI_STATE::DEBUG) {
                    rotaryEncoder.setBoundaries(-DISM.boundaries_home, DISM.boundaries_home, false);
                    rotaryEncoder.setEncoderValue(DISM.animatedMenuIndex_target);
                    DISM.currentState = UI_STATE::HOME_MENU;
                    DISM.drawHomeMenu();
                }

                else if(DISM.currentState == UI_STATE::RECORDE) {
                    stopRecording();
                    isRecordingMode = false;  // Re-enable detectWord
                    isRecording = false;
                    DISM.seconds = 0;  // Reset timer
                    rotaryEncoder.setBoundaries(-DISM.boundaries_home, DISM.boundaries_home, false);
                    rotaryEncoder.setEncoderValue(DISM.animatedMenuIndex_target);
                    DISM.currentState = UI_STATE::HOME_MENU;
                    DISM.drawHomeMenu();
                }

                else
                {
                    rotaryEncoder.setBoundaries(-DISM.boundaries_home, DISM.boundaries_home, false);
                    rotaryEncoder.setEncoderValue(DISM.animatedMenuIndex_target);
                    DISM.currentState = UI_STATE::HOME_MENU;
                    DISM.drawHomeMenu();
                }
            }
            backBtnPressTime = 0;
            isBackBtnLongPressed = false;
        }
    }
}

void setup() {
  Serial.begin(115200);
  Serial.println("start...");

  rotaryEncoder.begin();
  rotaryEncoder.setup(readEncoderISR);

  rotaryEncoder.setBoundaries(-DISM.boundaries_home, DISM.boundaries_home, false);
  rotaryEncoder.setEncoderValue(DISM.animatedMenuIndex_target);
  rotaryEncoder.disableAcceleration();

  Serial.println("\n#############################");
  Serial.println("#  ESP32 System v2.0        #");
  Serial.println("#############################\n");

  Serial.printf("Freeheap: %lu, Min free: %lu, MaxAllocHeap: %lu\n", 
      ESP.getFreeHeap(),
      ESP.getMinFreeHeap(),
      ESP.getMaxAllocHeap());

  if (psramFound())
  {
      Serial.printf("PSRAM ใช้งานได้: %d bytes\n", ESP.getPsramSize());
      Serial.printf("PSRAM ใช้งานจริง: %d bytes\n", ESP.getFreePsram());
  }
  else
  {
      Serial.println("❌ ไม่พบ PSRAM!");
  }

    pinMode(BTN_BACK, INPUT);
    pinMode(ENC_SW, INPUT);

  sdSemaphore = xSemaphoreCreateMutex();
  displaySemaphore = xSemaphoreCreateMutex();
  if (!sdSemaphore || !displaySemaphore) {
    Serial.println("✗ Failed to create semaphores!");
    return;
  }

  display_command = xQueueCreate(10, sizeof(DISPLAY_COMMAND));
  audio_command = xQueueCreate(10, sizeof(AUDIO_COMMAND));
  if(display_command == NULL && audio_command == NULL) {
    Serial.println("creat queue error.");
  }

  DISM.initDisplay();

  file_card.initSDCard();
  vTaskDelay(pdMS_TO_TICKS(200));

  initAudio();
  vTaskDelay(pdMS_TO_TICKS(500));
  initMicrophone();
  vTaskDelay(pdMS_TO_TICKS(500));

  // Initialize Hardware Manager
  hwManager.initDevices();

  BaseType_t task1 = xTaskCreatePinnedToCore(handleAudio, "handleAudio", 4 * 1024, NULL, 4, &t_handleAudio, 0);
  BaseType_t netWorkTask = xTaskCreatePinnedToCore(runNet, "runNet",  4 * 1024, NULL, 3, &runnet, 0);
  BaseType_t disPTask = xTaskCreatePinnedToCore(handleDisplay, "handleDisplay", 3 * 1024, NULL, 2, &t_handleDisplay, 1);

  if(task1 && netWorkTask && disPTask != pdPASS) {
    Serial.println("Create Task Error.");
    if(xSemaphoreTake(displaySemaphore ,pdMS_TO_TICKS(100)) == pdTRUE) {
      tft.setTextColor(TFT_RED);
      tft.println("Create Task Error.");
      tft.setTextColor(TFT_WHITE);
      xSemaphoreGive(displaySemaphore);
    }
    return;
  }

  if(xSemaphoreTake(displaySemaphore, pdMS_TO_TICKS(100)) == pdTRUE) {
    tft.setTextColor(TFT_GREEN);
    tft.println("Create Task Successfull.");
    tft.println("System Ready.");
    tft.setTextColor(TFT_WHITE);
    xSemaphoreGive(displaySemaphore);
  }

  Serial.println("Create Task Successfull.");
  Serial.println("System Ready.");

  vTaskDelay(pdMS_TO_TICKS(1000));

  DISM.createUISprite();
  vTaskDelay(pdMS_TO_TICKS(200));

  DISM.drawLoading(20, "Mounting SD Card...");
  delay(100);
  DISM.drawLoading(70, "Init Display & Audio...");
  delay(100);
  DISM.drawLoading(100, "Done.");
  delay(100);

  DISM.currentState = UI_STATE::APP_PET;
  DISM.drawAIPet();

//   if(isDisplay_install && isFileManager_install && isNetwork_install && isAudio_install) {
    
//   } else {
//     tft.fillScreen(TFT_BLACK);
//     tft.setCursor(0,0);
//     tft.println("error.");
//   }

}
   
void loop() {

    if (autoPlayNext) {
        autoPlayNext = false; // ปิดธงสัญญาณ
        
        // เช็คว่ามีรายการเพลงใน Playlist หรือไม่
        if (!DISM.playlistNames.empty()) {
            DISM.currentPlayingIndex++; // เลื่อนไปเพลงถัดไป
            
            // ถ้าเล่นถึงเพลงสุดท้ายแล้ว ให้วนกลับมาเพลงแรก
            if (DISM.currentPlayingIndex >= (int)DISM.playlistNames.size()) {
                DISM.currentPlayingIndex = 0;
            }
            
            currentSongTitle = DISM.playlistNames[DISM.currentPlayingIndex];
            
            // ส่งคำสั่งไปที่ Queue เสียงเพื่อเล่นเพลง
            AUDIO_COMMAND cmd;
            cmd.module = AUDIO_COMMAND::MODULE::AUDIO;
            cmd.audio_state = AUDIO_COMMAND::AUDIO_STATE::PLAY;
            cmd.path = DISM.playlistPaths[DISM.currentPlayingIndex];
            xQueueSend(audio_command, &cmd, portMAX_DELAY);

            // บังคับหน้าจอให้อัปเดตชื่อเพลงทันที (ถ้าผู้ใช้เปิดหน้า Music อยู่)
            if (DISM.currentState == UI_STATE::APP_MUSIC) {
                DISM.drawMusicPlayer(currentSongTitle, 0, true);
            }
        }
    }

    handleInput();

    if (DISM.currentState == UI_STATE::HOME_MENU && DISM.isAnimatingMenu) {
        DISM.drawHomeMenu();
        vTaskDelay(1);
    }

    if (DISM.currentState == UI_STATE::APP_PET)
    {
        // สุ่มเปลี่ยนอารมณ์ทุกๆ 8 วินาที
        if (millis() - DISM.lastMoodChange > 3000)
        {
            DISM.petMood = random(0, 3);
            DISM.lastMoodChange = millis();
        }
        DISM.drawAIPet();
        vTaskDelay(10);
    }

    // 🌟 ระบบซ่อนหลอดระดับเสียงอัตโนมัติ (หลังไม่หมุน 2 วินาที)
    if (DISM.currentState == UI_STATE::GLOBAL_VOLUME) {
        if (millis() - lastVolActivityTime > 2000) {
            DISM.currentState = DISM.previousState; // กลับไปหน้าเดิม
            
            // วาดหน้าเดิมทับหลอดเสียง
            if(DISM.currentState == UI_STATE::HOME_MENU) {
                rotaryEncoder.setBoundaries(-DISM.boundaries_home, DISM.boundaries_home, false);
                rotaryEncoder.setEncoderValue(DISM.animatedMenuIndex_target);
                DISM.drawHomeMenu();
            }
            
            else if(DISM.currentState == UI_STATE::APP_MUSIC) {
                rotaryEncoder.setBoundaries(0, 3, true);
                rotaryEncoder.setEncoderValue(DISM.currentMusicControlIndex);
                DISM.drawMusicPlayer(currentSongTitle, currentAudioProgress, isPlayingAudio);
            }
            
            else if(DISM.currentState == UI_STATE::APP_MUSIC_LIST) {
                rotaryEncoder.setBoundaries(0, DISM.playlistNames.size(), true);
                rotaryEncoder.setEncoderValue(DISM.playlistSelectedIndex);
                DISM.drawMusicList();
            }
            
            else if(DISM.currentState == UI_STATE::APP_DISPLAY_LIST)  {
                rotaryEncoder.setBoundaries(0, DISM.imageNames.size(), true);
                rotaryEncoder.setEncoderValue(DISM.imageSelectedIndex);
                DISM.drawImageList();
            }
            else if(DISM.currentState == UI_STATE::APP_SETTINGS) {
                rotaryEncoder.setBoundaries(0, 1, true);
                rotaryEncoder.setEncoderValue(DISM.settingSelectedIndex);
                DISM.drawSettings();
            }
            else if(DISM.currentState == UI_STATE::APP_ONLINE_MUSIC) {
                rotaryEncoder.setBoundaries(0, 2, false);
                rotaryEncoder.setEncoderValue(DISM.currentMusicControlIndex);
                DISM.drawOnlineMusicPlayer();
            }
        }
    }

    // 🌟 อัปเดต UI หน้า Music อัตโนมัติ (เพื่อให้หลอดเพลงขยับ)
    static unsigned long lastMusicUIRefresh = 0;
    if (DISM.currentState == UI_STATE::APP_MUSIC && isPlayingAudio && (millis() - lastMusicUIRefresh > 1000)) {
        DISM.drawMusicPlayer(currentSongTitle, currentAudioProgress, isPlayingAudio);
        lastMusicUIRefresh = millis();
    }

    if(DISM.currentState == UI_STATE::APP_ONLINE_MUSIC) {
        DISM.drawOnlineMusicPlayer();  
        vTaskDelay(50);
    }

    if(DISM.currentState == UI_STATE::DEBUG) {
        DISM.debug();
        vTaskDelay(5);
    }

    if(DISM.currentState == UI_STATE::RECORDE) {
        DISM.recorde();
        recordLoop();
        vTaskDelay(1);
    }

    // Only detect voice commands when NOT in recording mode
    if (!isRecordingMode) {
        detectWord();
    }

    // if (rotaryEncoder.encoderChanged()) {
    //     Serial.print("Encoder Value: ");
    //     Serial.println(rotaryEncoder.readEncoder());
    //     Serial.printf("Current Menu Index: %d\n", DISM.currentMenuIndex);
    // }

}
