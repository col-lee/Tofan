#ifndef ARDUINO_H
#define ARDUINO_H
    #include <Arduino.h>
    #include "event.hpp"
#endif

#ifndef SPI_H
#define SPI_H
    #include <SPI.h>
#endif

#ifndef SD_H
#define SD_h
    #include <SD.h>
#endif

#ifndef TFT_eSPI_H
#define TFT_eSPI_H
    #include <TFT_eSPI.h>
#endif

#if defined (TFT_eSPI_H)
    extern TFT_eSPI tft;
#endif

#if defined (ARDUINO_H)
    extern SemaphoreHandle_t sdSemaphore;
    extern SemaphoreHandle_t displaySemaphore;
    extern TaskHandle_t t_handleAudio;
    extern TaskHandle_t t_handleDisplay;
    extern TaskHandle_t runnet;
    extern TaskHandle_t t_uiTask;
    extern QueueHandle_t q_handleMsg;
    extern QueueHandle_t sound_volume;
    extern QueueHandle_t display_command;
    extern QueueHandle_t audio_command;
    extern QueueHandle_t api_event_queue;
    extern QueueHandle_t fileMg;
#endif

#if defined (SPI_H)
    extern SPIClass hspi;
#endif

#ifndef ArduinoJ
#define ArduinoJ
    #include <ArduinoJson.h>
#endif

#ifndef IS_INSTALLATION
#define IS_INSTALLATION
    extern bool isNetwork_install;
    extern bool isDisplay_install;
    extern bool isAudio_install;
    extern bool isFileManager_install;
#endif



