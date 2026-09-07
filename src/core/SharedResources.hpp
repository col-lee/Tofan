// Shared RTOS handles and module readiness flags; devices own their readiness flags.
#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <SD.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include "Commands.hpp"
#include "Config.hpp"

// Shared runtime resources used by display/audio/network modules.
extern SemaphoreHandle_t sdSemaphore;
extern SemaphoreHandle_t displaySemaphore;
extern TaskHandle_t t_handleAudio;
extern TaskHandle_t t_handleDisplay;
extern TaskHandle_t runnet;
extern QueueHandle_t display_command;
extern QueueHandle_t audio_command;
extern QueueHandle_t api_event_queue;

extern bool isNetwork_install;
extern bool isDisplay_install;
extern bool isAudio_install;
extern bool isFileManager_install;
extern bool isConnectSDcard;
