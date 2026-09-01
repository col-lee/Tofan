# 🛠️ ToFan OS - Quick Implementation Guide

## ✅ ที่ปรับปรุงแล้ว

### 1️⃣ Hardware Manager (✨ NEW)
**ไฟล์**: `HardwareManager.hpp` / `HardwareManager.cpp`

ระบบติดตามสถานะ 8 hardware components พร้อม 4 states:
- 🟢 WORKING (Working)
- 🟡 CONNECTING (Connecting...)
- 🔴 ERROR (Error)
- ⚪ IDLE (Not Active)

**Usage**:
```cpp
#include "HardwareManager.hpp"

// In setup()
hwManager.initDevices();

// In your loop/display
hwManager.updateAllStatus();
HardwareDevice dev = hwManager.getDevice(0);  // Get ESP32 status
String status = hwManager.getStatusString(dev.status);  // "Working"
```

---

### 2️⃣ Debug Menu Enhancement
**ไฟล์**: `DisplayManager.cpp` - function `debug()`

แสดงผล:
- 📊 8 Hardware devices พร้อม status indicator
- 📈 System resources (RAM, Stack usage)
- 🔌 Individual device details (IP, file status, etc.)

ตัวอย่างการแสดง:
```
═══════════════════════════════════
HARDWARE DEBUG STATUS
═══════════════════════════════════
● ESP32-S3-R16             Working    65KB
  RAM info displayed here
● Display (ST7789)         Working    320x240 SPI
● Audio (MAX98357A)        Idle       Not Playing
● Microphone (I2S)         Working    I2S Ready
● SDCard Module            Working    Connected
● WiFi Module              Working    MyNetwork
● IP5306 (Power)           Working    Active
● Network                  Idle       Offline

System Resources:
RAM: 456KB | Min: 123KB
Tasks - Audio:256 Dis:512 Net:384
PSRAM: 2048KB avail

[Back to exit debug menu]
═══════════════════════════════════
```

---

### 3️⃣ Config Manager (✨ NEW)
**ไฟล์**: `config.hpp`

ศูนย์รวมทุก pin definitions และ configuration constants:

**Before** ❌ (Magic numbers ทั่วไป):
```cpp
#define ENC_A_PIN 15        // main.cpp
#define SD_MOSI 11          // FileManager.hpp
#define AUDIO_BCLK 4        // SoundManager.hpp
// ... กระจัดกระจาย
```

**After** ✅ (Centralized):
```cpp
#include "config.hpp"       // ที่เดียว!

// ใน config.hpp มีทั้งหมด:
// - DISPLAY pins
// - ENCODER pins
// - AUDIO pins
// - MICROPHONE pins
// - SD CARD pins
// - TASK STACK SIZES
// - MEMORY LIMITS
// - TIMING CONSTANTS
```

**Benefits**:
- ✅ ง่ายต่อการ debug hardware ใหม่
- ✅ ลดการ #define ซ้ำๆ
- ✅ Configuration centralized

---

### 4️⃣ IOManager Implementation
**ไฟล์**: `IOManager.hpp` / `IOManager.cpp`

**Before**: ❌ Empty class
```cpp
class IOManager {
public:
    IOManager();
    ~IOManager();
};  // ว่างเปล่า!
```

**After**: ✅ Complete implementation
```cpp
class IOManager {
public:
    void initPins();
    void setLED(bool state);
    void toggleLED();
    bool readButton(int buttonPin);
    
    // LED diagnostics
    void blinkLED(int times);
    void indicateError();      // Fast blink
    void indicateSuccess();    // Slow blink
    void indicateConnecting(); // Medium blink
};

// Usage
ioManager.initPins();
ioManager.setLED(true);         // Turn LED on
ioManager.indicateSuccess();    // Blink success pattern
```

---

### 5️⃣ Network.cpp Bug Fix
**Fixed**: Typo "usesrname" → "username"

```cpp
// Before ❌
prefs.putString("usesrname", username_obj.username);  // TYPO!

// After ✅
prefs.putString("username", username_obj.username);   // Correct!
```

---

## 🚀 Integration Steps

### Step 1: Include in main.cpp
```cpp
#include "extendstion/HardwareManager.hpp"
#include "extendstion/IOManager.hpp"
#include "extendstion/config.hpp"
```

### Step 2: Initialize in setup()
```cpp
void setup() {
    // ... existing code ...
    
    // Initialize Hardware Manager
    hwManager.initDevices();
    
    // Initialize IO Manager
    ioManager.initPins();
    
    // ... rest of setup ...
}
```

### Step 3: Update Display Manager
```cpp
// In DisplayManager.cpp - already updated!
// debug() function now uses HardwareManager
```

---

## 📊 Devices Tracked in Debug Menu

| # | Device | Monitor | States |
|---|--------|---------|--------|
| 1 | ESP32-S3-R16 | Free Heap | Working |
| 2 | Display (ST7789) | Init Status | Working / Error |
| 3 | Audio (MAX98357A) | Playing State | Working / Idle / Error |
| 4 | Microphone (I2S) | Ready Status | Working / Error |
| 5 | SDCard Module | Mount Status | Working / Error |
| 6 | WiFi Module | SSID/Status | Connected / Connecting / Idle |
| 7 | IP5306 (Power) | Active Status | Working |
| 8 | Network | IP Address | Online / Offline |

---

## 🎨 Status Color Scheme

```cpp
🟢 GREEN    = DEVICE_STATUS::WORKING    (กำลังทำงาน)
🟡 YELLOW   = DEVICE_STATUS::CONNECTING (กำลังเชื่อมต่อ)
🔴 RED      = DEVICE_STATUS::ERROR      (เกิดข้อผิดพลาด)
⚪ GRAY      = DEVICE_STATUS::IDLE       (ไม่ทำงาน)
```

---

## 📝 How to Add New Device Status

Example: Adding Battery Level Monitor

```cpp
// 1. Add in HardwareManager.hpp
struct HardwareDevice {
    // ... existing fields ...
    int batteryLevel;  // New field
};

// 2. Add in HardwareManager.cpp
void HardwareManager::updateBatteryStatus() {
    // Read battery level (example)
    int batteryADC = analogRead(BATTERY_PIN);
    int percent = map(batteryADC, 0, 4095, 0, 100);
    
    devices[8].status = (percent > 20) ? DEVICE_STATUS::WORKING : DEVICE_STATUS::ERROR;
    devices[8].details = String(percent) + "%";
}

// 3. Call in updateAllStatus()
updateBatteryStatus();
```

---

## 🔍 Debugging Tips

### View Debug Menu
1. กดปุ่ม Rotary Encoder เพื่อเข้า Home Menu
2. หมุนไปที่ "Debug" (รายการที่ 5)
3. กดปุ่ม Select
4. ดู hardware status จะ update ทุก 1 วินาที
5. กดปุ่ม Back เพื่อออก

### Monitor via Serial
```cpp
// Add this in main loop
if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    
    if (cmd == "debug") {
        for (int i = 0; i < hwManager.getDeviceCount(); i++) {
            HardwareDevice dev = hwManager.getDevice(i);
            Serial.printf("%s: %s (%s)\n", 
                dev.name.c_str(),
                hwManager.getStatusString(dev.status).c_str(),
                dev.details.c_str());
        }
    }
}
```

---

## 📦 File Changes Summary

| File | Change | Status |
|------|--------|--------|
| HardwareManager.hpp | ✨ NEW | Created |
| HardwareManager.cpp | ✨ NEW | Created |
| config.hpp | ✨ NEW | Created |
| IOManager.hpp | 🔄 Updated | Implemented |
| IOManager.cpp | 🔄 Updated | Implemented |
| DisplayManager.hpp | 🔄 Updated | Added HardwareManager include |
| DisplayManager.cpp | 🔄 Updated | Enhanced debug() function |
| Network.cpp | 🐛 Fixed | Fixed typo (usesrname) |
| GlobalVar.hpp | 🔄 Updated | Added config.hpp include |
| main.cpp | 🔄 Updated | Added HardwareManager init |

---

## ⚠️ Next Steps (To Do)

### HIGH Priority
- [ ] Test compilation with new files
- [ ] Verify HardwareManager.updateAllStatus() works correctly
- [ ] Test Debug Menu display on actual hardware
- [ ] Verify IP5306 integration (battery level reading)

### MEDIUM Priority
- [ ] Implement VoiceControl.cpp properly
- [ ] Add error logging to all hardware status checks
- [ ] Optimize memory usage in device tracking

### LOW Priority
- [ ] Add more device statistics (temperature, etc.)
- [ ] Implement device health scoring
- [ ] Create performance dashboard

---

**Version**: 2.0.1
**Date**: 2025-06-17
**Status**: Ready for Testing ✅
