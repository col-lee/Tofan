#include "HardwareManager.hpp"
#include "GlobalVar.hpp"
#include "DisplayManager.hpp"
#include "SoundManager.hpp"
#include "Network.hpp"
#include "FileManager.hpp"
#include "../core/GlobalState.hpp"
#include <SD.h>
#include <WiFi.h>
#include <Wire.h>

HardwareManager hwManager;

namespace {
bool ip5306BusStarted = false;

bool isIP5306ProbeConfigured() {
    return IP5306_I2C_SDA >= 0 && IP5306_I2C_SCL >= 0;
}

bool probeIP5306() {
    if (!isIP5306ProbeConfigured()) return false;
    if (!ip5306BusStarted) {
        Wire.begin(IP5306_I2C_SDA, IP5306_I2C_SCL);
        ip5306BusStarted = true;
    }
    Wire.beginTransmission(IP5306_I2C_ADDRESS);
    return Wire.endTransmission() == 0;
}
}

HardwareManager::HardwareManager() {
    for (int i = 0; i < deviceCount; i++) {
        devices[i].status = DEVICE_STATUS::CONNECTING;
        devices[i].lastUpdateTime = 0;
    }
}

HardwareManager::~HardwareManager() {}

void HardwareManager::initDevices() {
    devices[0].name = "ESP32-S3-R16N8";
    devices[1].name = "Display (ST7789)";
    devices[2].name = "Audio (MAX98357A)";
    devices[3].name = "Microphone (I2S)";
    devices[4].name = "SDCard Module";
    devices[5].name = "WiFi Module";
    devices[6].name = "IP5306 (Power)";
    devices[7].name = "Network";

    for (int i = 0; i < deviceCount; i++) {
        devices[i].status = DEVICE_STATUS::CONNECTING;
        devices[i].details = "-";
        devices[i].lastUpdateTime = millis();
    }
}

void HardwareManager::updateAllStatus() {
    if (millis() - lastCheckTime < CHECK_INTERVAL) return;
    lastCheckTime = millis();

    updateESP32Status();
    updateDisplayStatus();
    updateAudioStatus();
    updateMicrophoneStatus();
    updateSDCardStatus();
    updateWiFiStatus();
    updatePowerStatus();
    updateNetworkStatus();
}

void HardwareManager::updateESP32Status() {
    devices[0].status = DEVICE_STATUS::WORKING;
    devices[0].details = "Heap " + String(ESP.getFreeHeap() / 1024) + "KB";
    if (psramFound()) {
        devices[0].details += " PSRAM " + String(ESP.getFreePsram() / 1024) + "KB";
    }
    devices[0].lastUpdateTime = millis();
}

void HardwareManager::updateDisplayStatus() {
    const bool displayReady = isDisplay_install && tft.width() > 0 && tft.height() > 0;
    devices[1].status = displayReady ? DEVICE_STATUS::WORKING : DEVICE_STATUS::ERROR;
    devices[1].details = displayReady
        ? String(tft.width()) + "x" + String(tft.height()) + " SPI"
        : "Init failed";
    devices[1].lastUpdateTime = millis();
}

void HardwareManager::updateAudioStatus() {
    devices[2].status = isAudio_install ? DEVICE_STATUS::WORKING : DEVICE_STATUS::ERROR;
    devices[2].details = isAudio_install ? (isPlayingAudio ? "Playing" : "Ready") : "Init failed";
    devices[2].lastUpdateTime = millis();
}

void HardwareManager::updateMicrophoneStatus() {
    if (!isMicrophoneReady()) {
        devices[3].status = DEVICE_STATUS::ERROR;
        devices[3].details = "I2S init failed";
    } else if (!isMicrophoneCapturing()) {
        devices[3].status = DEVICE_STATUS::CONNECTING;
        devices[3].details = "Capture task starting";
    } else if (millis() - getMicrophoneLastSampleMillis() <= 2000) {
        devices[3].status = DEVICE_STATUS::WORKING;
        devices[3].details = app::runtime.isRecording ? "Recording" : "Sampling";
    } else {
        devices[3].status = DEVICE_STATUS::CONNECTING;
        devices[3].details = "Waiting samples";
    }

    const uint32_t errors = getMicrophoneReadErrors();
    if (errors > 0) {
        devices[3].details += " E:" + String(errors);
    }
    devices[3].lastUpdateTime = millis();
}

void HardwareManager::updateSDCardStatus() {
    if (!isFileManager_install || !isConnectSDcard) {
        devices[4].status = DEVICE_STATUS::ERROR;
        devices[4].details = "Not mounted";
        devices[4].lastUpdateTime = millis();
        return;
    }

    if (xSemaphoreTake(sdSemaphore, pdMS_TO_TICKS(25)) != pdTRUE) {
        devices[4].status = DEVICE_STATUS::CONNECTING;
        devices[4].details = "SD busy";
        devices[4].lastUpdateTime = millis();
        return;
    }

    const uint8_t cardType = SD.cardType();
    const uint64_t cardSizeMB = SD.cardSize() / (1024 * 1024);
    xSemaphoreGive(sdSemaphore);

    devices[4].status = cardType == CARD_NONE ? DEVICE_STATUS::ERROR : DEVICE_STATUS::WORKING;
    devices[4].details = cardType == CARD_NONE ? "No card" : String(cardSizeMB) + "MB mounted";
    devices[4].lastUpdateTime = millis();
}

void HardwareManager::updateWiFiStatus() {
    if (WiFi.status() == WL_CONNECTED) {
        devices[5].status = DEVICE_STATUS::WORKING;
        devices[5].details = WiFi.SSID().substring(0, 10) + " " + String(WiFi.RSSI()) + "dBm";
    } else if (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA) {
        devices[5].status = DEVICE_STATUS::WORKING;
        devices[5].details = "AP " + WiFi.softAPIP().toString();
    } else if (WiFi.getMode() == WIFI_STA) {
        devices[5].status = DEVICE_STATUS::CONNECTING;
        devices[5].details = "STA connecting";
    } else {
        devices[5].status = DEVICE_STATUS::ERROR;
        devices[5].details = "Off";
    }
    devices[5].lastUpdateTime = millis();
}

void HardwareManager::updatePowerStatus() {
    if (!isIP5306ProbeConfigured()) {
        devices[6].status = DEVICE_STATUS::ERROR;
        devices[6].details = "I2C probe unset";
    } else if (probeIP5306()) {
        devices[6].status = DEVICE_STATUS::WORKING;
        devices[6].details = "I2C 0x75 OK";
    } else {
        devices[6].status = DEVICE_STATUS::ERROR;
        devices[6].details = "No ACK 0x75";
    }
    devices[6].lastUpdateTime = millis();
}

void HardwareManager::updateNetworkStatus() {
    if (WiFi.status() == WL_CONNECTED && nm.isConnectWiFi) {
        devices[7].status = DEVICE_STATUS::WORKING;
        devices[7].details = WiFi.localIP().toString().substring(0, 15);
    } else if (isNetwork_install && (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA)) {
        devices[7].status = DEVICE_STATUS::WORKING;
        devices[7].details = "Admin AP active";
    } else if (WiFi.getMode() == WIFI_STA) {
        devices[7].status = DEVICE_STATUS::CONNECTING;
        devices[7].details = "Waiting IP";
    } else {
        devices[7].status = DEVICE_STATUS::CONNECTING;
        devices[7].details = "Offline";
    }
    devices[7].lastUpdateTime = millis();
}

HardwareDevice HardwareManager::getDevice(int index) {
    if (index >= 0 && index < deviceCount) {
        return devices[index];
    }
    HardwareDevice empty;
    empty.name = "Unknown";
    empty.status = DEVICE_STATUS::ERROR;
    return empty;
}

String HardwareManager::getStatusString(DEVICE_STATUS status) {
    switch (status) {
        case DEVICE_STATUS::CONNECTING:
            return "Connecting";
        case DEVICE_STATUS::WORKING:
            return "Working";
        case DEVICE_STATUS::ERROR:
            return "Error";
        default:
            return "Unknown";
    }
}

String HardwareManager::getStatusColor(DEVICE_STATUS status) {
    switch (status) {
        case DEVICE_STATUS::CONNECTING:
            return "YELLOW";
        case DEVICE_STATUS::WORKING:
            return "GREEN";
        case DEVICE_STATUS::ERROR:
            return "RED";
        default:
            return "WHITE";
    }
}
