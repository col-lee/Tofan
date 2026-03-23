#define ARDUINO_H
#include <Arduino.h>
#include "extendstion/event.hpp"
#include "extendstion/GlobalVar.hpp"
#include "extendstion/DisplayManager.hpp"
#include "extendstion/FileManager.hpp"
#include "extendstion/Network.hpp"
#include "extendstion/SoundManager.hpp"

#ifndef DISPLAYMANAGER_HH
#define DISPLAYMANAGER_HH
  #include "extendstion/DisplayManager.hpp"
#endif

#ifndef NETWORK_LIBRARY
  #include <WiFi.h>
  #include <AsyncTCP.h>
  #include <ESPAsyncWebServer.h>
#endif

SemaphoreHandle_t displaySemaphore = NULL;
TaskHandle_t t_handleAudio = NULL;
TaskHandle_t t_handleDisplay = NULL;
QueueHandle_t sound_volume = NULL;
QueueHandle_t display_command = NULL;
QueueHandle_t audio_command = NULL;
QueueHandle_t api_event_queue = NULL;
TaskHandle_t runnet;
NetworkManager nm;

#define INPUT_SWITCH 32
// #define LED 32
int state = 0;

#define CLK_PIN 34 // Input Only
#define DT_PIN 35  // Input Only
#define SW_PIN 21  // ปลอดภัย มี Pull-up
 
int lastEncoderValue = 50;

void IRAM_ATTR readEncoderISR() {
  static unsigned long lastInterruptTime = 0;
  unsigned long interruptTime = millis();

  // Debounce 5ms
  if (interruptTime - lastInterruptTime > 5) {
    // เช็คทิศทาง
    if (digitalRead(CLK_PIN) != digitalRead(DT_PIN)) {
      encoderValue++;
      if (encoderValue > 100) encoderValue = 100;
    } else {
      encoderValue--;
      if (encoderValue < 0) encoderValue = 0;
    }
  }
  lastInterruptTime = interruptTime;
}

void setup() {
  Serial.begin(115200);
  Serial.println("start...");

  Serial.println("\n#############################");
  Serial.println("#  ESP32 System v2.0        #");
  Serial.println("#############################\n");

  pinMode(CLK_PIN, INPUT);
  pinMode(DT_PIN, INPUT);
  
  // ขา 21 สั่ง PULLUP ได้ปกติ
  pinMode(SW_PIN, INPUT_PULLUP);

  // ผูก Interrupt
  attachInterrupt(digitalPinToInterrupt(CLK_PIN), readEncoderISR, CHANGE);

  // pinMode(LEVEL_READ, INPUT);
  pinMode(INPUT_SWITCH, INPUT);

  // pinMode(LED, OUTPUT);

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
  vTaskDelay(pdMS_TO_TICKS(200));
  file_card.initSDCard();
  vTaskDelay(pdMS_TO_TICKS(500));
  
  nm.initWiFiManager();
  vTaskDelay(pdMS_TO_TICKS(500));
  nm.initWebServer();
  vTaskDelay(pdMS_TO_TICKS(500));

  websocket.onEvent([](AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
    nm.onEvent(server, client, type, arg, data, len);
  });
  server.addHandler(&websocket);

  initAudio();
  if(xSemaphoreTake(sdSemaphore, pdMS_TO_TICKS(100)) == pdTRUE) {
    SD.exists(mp3File) ? audio.connecttoFS(SD, mp3File) : Serial.println("File not found.");
    xSemaphoreGive(sdSemaphore);
  }

  BaseType_t task1 = xTaskCreatePinnedToCore(handleAudio, "handleAudio", 4 * 1024, NULL, 3, &t_handleAudio, 1);
  BaseType_t netWorkTask = xTaskCreatePinnedToCore(runNet, "runNet", 3 * 1024, NULL, 3, &runnet, tskNO_AFFINITY);
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
  file_card.listFileAll();

  vTaskDelay(pdMS_TO_TICKS(1000));

  if(isDisplay_install && isFileManager_install && isNetwork_install && isAudio_install) {
    tft.fillScreen(TFT_BLACK);
    DISM.drawJpeg("/main/Pictures/test.jpg", 0, 40);
  } else {
    tft.fillScreen(TFT_BLACK);
    tft.setCursor(0,0);
    tft.println("error.");
  }
}

void loop() {
    state = digitalRead(INPUT_SWITCH);
    Serial.println(state);

    if (encoderValue != lastEncoderValue) {
      Serial.printf("Volume: %d\n", encoderValue);
      lastEncoderValue = encoderValue;
    }

    // Serial.printf("Freeheap: %lu\n", ESP.getFreeHeap());
    vTaskDelay(100);
}
