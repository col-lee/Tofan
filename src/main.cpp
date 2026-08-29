#define ARDUINO_H
#include <Arduino.h>
#include "extensions/event.hpp"
#include "extensions/GlobalVar.hpp"
#include "extensions/DisplayManager.hpp"
#include "extensions/FileManager.hpp"
#include "extensions/Network.hpp"
#include "extensions/SoundManager.hpp"
#include "extensions/IOManager.hpp"
#include "extensions/HardwareManager.hpp"
#include "extensions/AIConversation.hpp"
#include "extensions/RGBLed.hpp"
#include "app/AppCoordinator.hpp"
#include "app/InputController.hpp"
#include "core/GlobalState.hpp"

#ifndef DISPLAYMANAGER_HH
#define DISPLAYMANAGER_HH
#include "extensions/DisplayManager.hpp"
#endif

#ifndef NETWORK_LIBRARY
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#endif

SemaphoreHandle_t displaySemaphore = NULL;
TaskHandle_t t_handleAudio = NULL;
TaskHandle_t t_handleDisplay = NULL;
TaskHandle_t runnet = NULL;
QueueHandle_t display_command = NULL;
QueueHandle_t audio_command = NULL;
QueueHandle_t api_event_queue = NULL;
NetworkManager nm;
FileManager file_card;

int lastSyncedVol = -1;

static void startAIPetListening() {
    appCoordinator.startAiPetListening();
}

static void updateAIPetListening() {
    appCoordinator.updateAiPetListening();
}

static void stopAIPetListening() {
    appCoordinator.stopAiPetListening();
}

void setup() {
    Serial.begin(115200);
    Serial.println("start...");

    sdSemaphore = xSemaphoreCreateMutex();
    displaySemaphore = xSemaphoreCreateMutex();
    if (!sdSemaphore || !displaySemaphore) {
        Serial.println("Failed to create semaphores!");
        return;
    }

    display_command = xQueueCreate(10, sizeof(DISPLAY_COMMAND));
    audio_command = xQueueCreate(10, sizeof(AUDIO_COMMAND));
    if (display_command == NULL || audio_command == NULL) {
        Serial.println("Create queue error.");
        return;
    }

    DISM.initDisplay();
    file_card.initSDCard();
    vTaskDelay(pdMS_TO_TICKS(200));

    initAudio();
    vTaskDelay(pdMS_TO_TICKS(500));
    initMicrophone();
    vTaskDelay(pdMS_TO_TICKS(500));

    hwManager.initDevices();

    BaseType_t task1 = xTaskCreatePinnedToCore(handleAudio, "handleAudio", 4 * 1024, NULL, 4, &t_handleAudio, 0);
    BaseType_t netWorkTask = xTaskCreatePinnedToCore(runNet, "runNet", 4 * 1024, NULL, 3, &runnet, 0);
    BaseType_t disPTask = xTaskCreatePinnedToCore(handleDisplay, "handleDisplay", 3 * 1024, NULL, 2, &t_handleDisplay, 1);

    if (task1 != pdPASS || netWorkTask != pdPASS || disPTask != pdPASS) {
        Serial.println("Create Task Error.");
        return;
    }

    DISM.createUISprite();
    vTaskDelay(pdMS_TO_TICKS(200));

    DISM.drawLoading(20, "Mounting SD Card...");
    delay(100);
    DISM.drawLoading(70, "Init Display & Audio...");
    delay(100);
    DISM.drawLoading(100, "Done.");
    delay(100);

    appCoordinator.begin();
    inputController.begin();
}

void loop() {
    appCoordinator.update();
    inputController.update();
}
