#ifndef ARDUINO_H
#define ARDUINO_H
    #include <Arduino.h>
    #include "event.hpp"
    #include "config.hpp"
#endif

#ifndef SPI_H
#define SPI_H
    #include <SPI.h>
#endif

#ifndef SD_H
#define SD_H
    #include <SD.h>
#endif

#ifndef LGFX_H
#define LGFX_H
    #include <LovyanGFX.hpp>

class LGFX : public lgfx::LGFX_Device
{
    lgfx::Panel_ST7789 _panel_instance;
    lgfx::Bus_SPI _bus_instance;

public:
    LGFX()
    {
        auto cfg = _bus_instance.config();
        cfg.spi_host = SPI3_HOST; // ใช้ Bus 3 สำหรับจอ
        cfg.pin_sclk = 39;
        cfg.pin_mosi = 38;
        cfg.pin_dc = 41;
        cfg.freq_write = 80000000;

        _bus_instance.config(cfg);
        _panel_instance.setBus(&_bus_instance);
        auto p_cfg = _panel_instance.config();
        p_cfg.pin_cs = 40;
        p_cfg.pin_rst = 42;
        p_cfg.panel_width = 240;
        p_cfg.panel_height = 320;
        p_cfg.invert = true;
        // p_cfg.rgb_order = false;
        _panel_instance.config(p_cfg);
        setPanel(&_panel_instance);
    }
};

    extern class LGFX tft;
    extern LGFX_Sprite spr;
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
    extern bool isConnectSDcard;

    extern volatile bool isRecordingMode;
    extern volatile bool isRecording;
#endif



