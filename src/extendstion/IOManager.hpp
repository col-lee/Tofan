#ifndef IOMANAGER_HPP
#define IOMANAGER_HPP

#include "GlobalVar.hpp"
#include "config.hpp"
#include <Arduino.h>

class IOManager {
private:
    bool ledState = false;
    bool buttonStates[3] = {false, false, false};

public:
    IOManager();
    ~IOManager();

    // Initialization
    void initPins();

    // LED Control
    void setLED(bool state);
    void toggleLED();
    bool getLEDState() { return ledState; }

    // Button Reading
    bool readButton(int buttonPin);
    bool isButtonPressed(int buttonPin);

    // LED Diagnostics
    void blinkLED(int times, int delayMs = 100);
    void indicateError();      // Blink red pattern
    void indicateSuccess();    // Blink green pattern
    void indicateConnecting(); // Blink yellow pattern
};

extern IOManager ioManager;

#endif
