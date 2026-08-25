#include "IOManager.hpp"

IOManager ioManager;

IOManager::IOManager() : ledState(false) {
    for (int i = 0; i < 3; i++) {
        buttonStates[i] = false;
    }
}

IOManager::~IOManager() {}

void IOManager::initPins() {
    // Initialize LED
    pinMode(LED_PIN, OUTPUT);
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
    return digitalRead(buttonPin) == HIGH;
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
    // Blink fast (error pattern)
    for (int i = 0; i < 5; i++) {
        setLED(true);
        delay(100);
        setLED(false);
        delay(100);
    }
}

void IOManager::indicateSuccess() {
    // Slow blink (success pattern)
    for (int i = 0; i < 3; i++) {
        setLED(true);
        delay(300);
        setLED(false);
        delay(300);
    }
}

void IOManager::indicateConnecting() {
    // Medium blink (connecting pattern)
    for (int i = 0; i < 4; i++) {
        setLED(true);
        delay(200);
        setLED(false);
        delay(200);
    }
}

