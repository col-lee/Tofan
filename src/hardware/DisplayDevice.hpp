#pragma once

// ST7789 panel and SPI bus configuration, with the shared display instances.
// Include SD before LovyanGFX so its filesystem image decoder adapters are enabled.
#include <SD.h>
#include <LovyanGFX.hpp>
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
