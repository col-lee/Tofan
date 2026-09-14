// Initializes devices and background tasks, then dispatches application updates.
#include "Application.hpp"
#include "../core/MemoryPolicy.hpp"
#include "../network/WebPortal.hpp"
#include "../core/UserSettings.hpp"
#include "../food/FoodStore.hpp"
#include <Arduino.h>
#include "../core/Commands.hpp"
#include "../core/SharedResources.hpp"
#include "../display/DisplayManager.hpp"
#include "../storage/FileManager.hpp"
#include "../network/Network.hpp"
#include "../audio/SoundManager.hpp"
#include "../hardware/IOManager.hpp"
#include "../hardware/HardwareManager.hpp"
#include "../ai/AIConversation.hpp"
#include "../ai/ChatHistory.hpp"
#include "../hardware/RGBLed.hpp"
#include "AppCoordinator.hpp"
#include "InputController.hpp"
#include "../core/GlobalState.hpp"

static void initializeSystem() {
    Serial.begin(115200);
    Serial.println("start...");
    memory::begin();

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

    userSettings.begin();
    foodStore.begin();
    DISM.applyTheme();
    DISM.initDisplay();
    ioManager.initPins();
    file_card.initSDCard();
    chatHistory.begin();
    vTaskDelay(pdMS_TO_TICKS(200));

    initAudio();
    vTaskDelay(pdMS_TO_TICKS(500));
    initMicrophone();
    vTaskDelay(pdMS_TO_TICKS(500));

    hwManager.initDevices();
}

static void startBackgroundTasks() {
    BaseType_t task1 = xTaskCreatePinnedToCore(handleAudio, "handleAudio", TASK_STACK_AUDIO, NULL, 4, &t_handleAudio, 0);
    BaseType_t netWorkTask = xTaskCreatePinnedToCore(runNet, "runNet", TASK_STACK_NETWORK, NULL, 3, &runnet, 0);
    BaseType_t disPTask = xTaskCreatePinnedToCore(handleDisplay, "handleDisplay", TASK_STACK_DISPLAY, NULL, 2, &t_handleDisplay, 1);

    if (task1 != pdPASS || netWorkTask != pdPASS || disPTask != pdPASS) {
        Serial.println("Create Task Error.");
        return;
    }
}

static void showBootSequence() {
    DISM.createUISprite();
    vTaskDelay(pdMS_TO_TICKS(200));

    DISM.drawLoading(20, "Mounting SD Card...");
    delay(100);
    DISM.drawLoading(70, "Init Display & Audio...");
    delay(100);
    DISM.drawLoading(100, "Done.");
    delay(100);
}

void app::begin() {
    initializeSystem();
    startBackgroundTasks();
    showBootSequence();
    appCoordinator.begin();
    inputController.begin();
}

void app::update() {
    serviceWebPortal();
    if (webFirmwareUpdating()) { delay(1); return; }
    chatHistory.service();
    appCoordinator.update();
    inputController.update();
}
