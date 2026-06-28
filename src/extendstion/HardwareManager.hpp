#ifndef HARDWAREMANAGER_HPP
#define HARDWAREMANAGER_HPP

#include "GlobalVar.hpp"
#include <Arduino.h>

// Device status enum
enum class DEVICE_STATUS {
    IDLE,           // ไม่ทำงาน
    CONNECTING,     // กำลังเชื่อมต่อ
    WORKING,        // กำลังทำงาน
    ERROR           // เกิดข้อผิดพลาด
};

// Hardware device structure
struct HardwareDevice {
    String name;
    DEVICE_STATUS status;
    String statusStr;
    String details;
    unsigned long lastUpdateTime;
};

class HardwareManager {
private:
    HardwareDevice devices[8]; // ESP32, Display, Audio, Microphone, SDCard, WiFi, Power, Network
    int deviceCount = 8;
    unsigned long lastCheckTime = 0;
    const unsigned long CHECK_INTERVAL = 1000; // Check every 1 second

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

#endif
