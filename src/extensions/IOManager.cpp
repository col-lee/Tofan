#include "IOManager.hpp"

IOManager ioManager;

IOManager::IOManager() : ledState(false) {
    for (int i = 0; i < 3; i++) {
        buttonStates[i] = false;
    }
}

IOManager::~IOManager() {}

void IOManager::initPins() {
    pinMode(LED_PIN, OUTPUT);
    pinMode(BTN_BACK, BUTTON_INPUT_MODE);
    pinMode(ENC_SW, BUTTON_INPUT_MODE);
    digitalWrite(LED_PIN, LOW);
    ledState = false;

    Serial.println("IO Manager initialized");
}

void IOManager::setLED(bool state) {
    digitalWrite(LED_PIN, state ? HIGH : LOW);
    ledState = state;
}

void IOManager::toggleLED() {
    setLED(!ledState);
}

bool IOManager::readButton(int buttonPin) {
    return digitalRead(buttonPin) == BUTTON_ACTIVE_LEVEL;
}

bool IOManager::isButtonPressed(int buttonPin) {
    bool currentState = readButton(buttonPin);
    return currentState;
}

void IOManager::blinkLED(int times, int delayMs) {
    for (int i = 0; i < times; i++) {
        setLED(true);
        delay(delayMs);
        setLED(false);
        delay(delayMs);
    }
}

void IOManager::indicateError() {
    for (int i = 0; i < 5; i++) {
        setLED(true);
        delay(100);
        setLED(false);
        delay(100);
    }
}

void IOManager::indicateSuccess() {
    for (int i = 0; i < 3; i++) {
        setLED(true);
        delay(300);
        setLED(false);
        delay(300);
    }
}

void IOManager::indicateConnecting() {
    for (int i = 0; i < 4; i++) {
        setLED(true);
        delay(200);
        setLED(false);
        delay(200);
    }
}

