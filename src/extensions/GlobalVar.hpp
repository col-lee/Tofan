#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <SD.h>
#include <LovyanGFX.hpp>

#include "event.hpp"
#include "../core/Config.hpp"

class LGFX : public lgfx::LGFX_Device {
private:
    lgfx::Panel_ST7789 _panel_instance;
    lgfx::Bus_SPI _bus_instance;

public:
    LGFX() {
        auto cfg = _bus_instance.config();
        cfg.spi_host = SPI3_HOST;
        cfg.pin_sclk = TFT_SCLK;
        cfg.pin_mosi = TFT_MOSI;
        cfg.pin_dc = TFT_DC;
        cfg.freq_write = TFT_SPI_FREQ;

        _bus_instance.config(cfg);
        _panel_instance.setBus(&_bus_instance);

        auto p_cfg = _panel_instance.config();
        p_cfg.pin_cs = TFT_CS;
        p_cfg.pin_rst = TFT_RST;
        p_cfg.panel_width = TFT_WIDTH;
        p_cfg.panel_height = TFT_HEIGHT;
        p_cfg.invert = true;
        _panel_instance.config(p_cfg);

        setPanel(&_panel_instance);
    }
};

extern LGFX tft;
extern LGFX_Sprite spr;

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


