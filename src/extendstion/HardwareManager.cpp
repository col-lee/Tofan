#include "HardwareManager.hpp"
#include "DisplayManager.hpp"
#include "SoundManager.hpp"
#include "Network.hpp"
#include "FileManager.hpp"

HardwareManager hwManager;

HardwareManager::HardwareManager() {
    for (int i = 0; i < deviceCount; i++) {
        devices[i].status = DEVICE_STATUS::IDLE;
        devices[i].lastUpdateTime = 0;
    }
}

HardwareManager::~HardwareManager() {}

void HardwareManager::initDevices() {
    // Initialize device names
    devices[0].name = "ESP32-S3-R16";
    devices[1].name = "Display (ST7789)";
    devices[2].name = "Audio (MAX98357A)";
    devices[3].name = "Microphone (I2S)";
    devices[4].name = "SDCard Module";
    devices[5].name = "WiFi Module";
    devices[6].name = "IP5306 (Power)";
    devices[7].name = "Network";

    for (int i = 0; i < deviceCount; i++) {
        devices[i].status = DEVICE_STATUS::IDLE;
        devices[i].details = "-";
        devices[i].lastUpdateTime = millis();
    }
}

void HardwareManager::updateAllStatus() {
    // Check every CHECK_INTERVAL
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
    devices[0].details = String(ESP.getFreeHeap() / 1024) + "KB";
}

void HardwareManager::updateDisplayStatus() {
    // Check if display is working by checking if sprite buffer exists
    devices[1].status = isDisplay_install ? DEVICE_STATUS::WORKING : DEVICE_STATUS::ERROR;
    if (devices[1].status == DEVICE_STATUS::WORKING) {
        devices[1].details = "320x240 SPI";
    } else {
        devices[1].details = "Init Failed";
    }
}

void HardwareManager::updateAudioStatus() {
    devices[2].status = isAudio_install ? DEVICE_STATUS::WORKING : DEVICE_STATUS::ERROR;
    if (devices[2].status == DEVICE_STATUS::WORKING) {
        devices[2].details = isPlayingAudio ? "Playing" : "Idle";
    } else {
        devices[2].details = "Init Failed";
    }
}

void HardwareManager::updateMicrophoneStatus() {
    // You can add microphone status check here
    devices[3].status = DEVICE_STATUS::WORKING; // Default to working if not error
    devices[3].details = "I2S Ready";
}

void HardwareManager::updateSDCardStatus() {
    devices[4].status = isConnectSDcard ? DEVICE_STATUS::WORKING : DEVICE_STATUS::ERROR;
    if (devices[4].status == DEVICE_STATUS::WORKING) {
        // Get SD card info
        devices[4].details = "Connected";
    } else {
        devices[4].details = "Not Mounted";
    }
}

void HardwareManager::updateWiFiStatus() {
    if (WiFi.status() == WL_CONNECTED) {
        devices[5].status = DEVICE_STATUS::WORKING;
        devices[5].details = WiFi.SSID().substring(0, 15);
    } else if (WiFi.status() == WL_IDLE_STATUS || WiFi.status() == WL_DISCONNECTED) {
        devices[5].status = DEVICE_STATUS::IDLE;
        devices[5].details = "Disconnected";
    } else {
        devices[5].status = DEVICE_STATUS::ERROR;
        devices[5].details = "Failed";
    }
}

void HardwareManager::updatePowerStatus() {
    // IP5306 status - could be enhanced with actual battery level reading
    devices[6].status = DEVICE_STATUS::WORKING;
    devices[6].details = "Active";
}

void HardwareManager::updateNetworkStatus() {
    if (WiFi.status() == WL_CONNECTED && nm.isConnectWiFi) {
        devices[7].status = DEVICE_STATUS::WORKING;
        devices[7].details = WiFi.localIP().toString().substring(0, 15);
    } else {
        devices[7].status = DEVICE_STATUS::IDLE;
        devices[7].details = "Offline";
    }
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
        case DEVICE_STATUS::IDLE:
            return "Idle";
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
        case DEVICE_STATUS::IDLE:
            return "GRAY";
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
