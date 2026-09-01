#pragma once

#include <Arduino.h>

enum class DEVICE_STATUS {
    CONNECTING,
    WORKING,
    ERROR
};

struct HardwareDevice {
    String name;
    DEVICE_STATUS status;
    String statusStr;
    String details;
    unsigned long lastUpdateTime;
};

class HardwareManager {
private:
    static constexpr int DEVICE_COUNT = 8;
    HardwareDevice devices[DEVICE_COUNT];
    int deviceCount = DEVICE_COUNT;
    unsigned long lastCheckTime = 0;
    const unsigned long CHECK_INTERVAL = 1000;

public:
    HardwareManager();
    ~HardwareManager();

    void initDevices();
    void updateAllStatus();
    void updateESP32Status();
    void updateDisplayStatus();
    void updateAudioStatus();
    void updateMicrophoneStatus();
    void updateSDCardStatus();
    void updateWiFiStatus();
    void updatePowerStatus();
    void updateNetworkStatus();

    HardwareDevice getDevice(int index);
    int getDeviceCount() { return deviceCount; }
    String getStatusColor(DEVICE_STATUS status);
    String getStatusString(DEVICE_STATUS status);
};

extern HardwareManager hwManager;
