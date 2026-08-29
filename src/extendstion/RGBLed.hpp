#ifndef RGB_LED_HPP
#define RGB_LED_HPP

#include "config.hpp"
#include <Adafruit_NeoPixel.h>

class RGBLed {
public:
    bool begin();
    void update();
    void setEnabled(bool enabled);
    void setBrightness(uint8_t brightness);

private:
    Adafruit_NeoPixel pixels{RGB_LED_COUNT, RGB_LED_PIN, NEO_GRB + NEO_KHZ800};
    uint16_t hue = 0;
    unsigned long lastUpdate = 0;
    bool enabled = true;
};

extern RGBLed rgbLed;

#endif