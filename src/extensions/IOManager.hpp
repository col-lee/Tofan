#ifndef IOMANAGER_HPP
#define IOMANAGER_HPP

#include "GlobalVar.hpp"
#include "../core/Config.hpp"
#include <Arduino.h>

class IOManager {
private:
    bool ledState = false;
    bool buttonStates[3] = {false, false, false};

public:
    IOManager();
    ~IOManager();

    void initPins();

    void setLED(bool state);
    void toggleLED();
    bool getLEDState() { return ledState; }

    bool readButton(int buttonPin);
    bool isButtonPressed(int buttonPin);

    void blinkLED(int times, int delayMs = 100);
    void indicateError();
    void indicateSuccess();
    void indicateConnecting();
};

extern IOManager ioManager;

#endif
