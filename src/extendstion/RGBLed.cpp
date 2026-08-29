#include "RGBLed.hpp"

RGBLed rgbLed;

bool RGBLed::begin() {
    pixels.begin();
    pixels.setBrightness(RGB_LED_BRIGHTNESS);
    pixels.clear();
    pixels.show();
    return true;
}

void RGBLed::update() {
    if (!enabled || millis() - lastUpdate < RGB_LED_INTERVAL_MS) return;
    lastUpdate = millis();

    pixels.setPixelColor(0, pixels.ColorHSV(hue));
    pixels.show();
    hue += 256;
}

void RGBLed::setEnabled(bool value) {
    enabled = value;
    if (!enabled) {
        pixels.clear();
        pixels.show();
    }
}

void RGBLed::setBrightness(uint8_t brightness) {
    pixels.setBrightness(brightness);
    pixels.show();
}