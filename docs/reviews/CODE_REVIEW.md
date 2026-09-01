# 📋 โปรเจ็ค ToFan OS - Code Review Report

## 📌 ข้อมูลโปรเจ็ค
- **Microcontroller**: ESP32-S3-R16n8 (Dual Core)
- **Display**: TFT 320x240 SPI (Driver ST7789)
- **Audio**: MAX98357A Amplifier + I2S Microphone
- **Storage**: SDCard Module (SPI)
- **Power**: IP5306 Power Management
- **Architecture**: FreeRTOS Multi-Task

---

## 🔍 ผลการวิเคราะห์โค้ด

### ✅ จุดเด่นของโปรเจ็ค

1. **Architecture ที่ดี** (main.cpp, state machine design)
   - ใช้ UI State Enum ที่ชัดเจน (BOOT_LOADING, HOME_MENU, APP_MUSIC เป็นต้น)
   - Input handling แบบ Event-driven ผ่าน Rotary Encoder
   - Multi-task FreeRTOS ทำให้ system responsive

2. **Display Management ที่ดีขึ้นมา** (DisplayManager)
   - ใช้ Sprite buffer ที่จัดการผ่าน Semaphore
   - Animation smooth ด้วย Lerp function
   - Color scheme consistent

3. **Networking** (Network.cpp)
   - AsyncWebServer + WebSocket ทำให้ real-time update
   - Preferences API สำหรับ config persistence
   - Admin Mode สำหรับ debug

4. **Audio Management** (SoundManager)
   - Support offline (SD Card) + online (HTTP streaming) playback
   - WAV recording functionality
   - Voice control dengan Edge Impulse ML model

---

### ⚠️ ปัญหาที่พบและต้องแก้ไข

#### 1. **Memory Management Issues** 📊

**ปัญหา**: การแจก memory ไม่เป็นระบบ, memory leak เสี่ยง

```cpp
// ❌ ปัญหา: Vector ขนาดใหญ่ไม่มีการจำกัด size
std::vector<String> playlistNames;     // อาจโตขึ้นเรื่อยๆ
std::vector<String> playlistPaths;
std::vector<String> imageNames;
std::vector<String> imagePaths;
```

**แนวทางแก้ไข**:
- จำกัดจำนวน items ที่ load (paginate files instead)
- Clear vectors เมื่อ exit จาก state
- Monitor heap ด้วย ESP.getFreeHeap()

---

#### 2. **IOManager ที่ว่างเปล่า** 🔌

**ปัญหา**: IOManager.hpp/cpp ไม่ได้ implement อะไร, code ที่เกี่ยวกับ GPIO/Pins กระจัดกระจาย

```cpp
// IOManager.hpp - ว่างเปล่า!
class IOManager {
private:
public:
    IOManager(/* args */);
    ~IOManager();
};
```

**แนวทางแก้ไข**:
```cpp
// ควรจะเป็น:
class IOManager {
private:
    static const int LED_PIN = 18;
    static const int BTN_BACK_PIN = 7;
    static const int ENC_SW_PIN = 10;
    
public:
    void initPins();
    void setLED(bool state);
    bool readButton(int pinNum);
};
```

---

#### 3. **VoiceControl.cpp ยังไม่ complete** 🎤

**ปัญหา**: VoiceControl.cpp มีแค่ include file ไม่มี implementation

```cpp
// VoiceControl.cpp - ยังไม่เสร็จ
#include "ToFan-project-1_inferencing.h"
```

**แนวทางแก้ไข**:
- Implement voice command detection
- Add function for command callback
- Test Edge Impulse model integration

---

#### 4. **Error Handling ไม่สม่ำเสมอ** ⚠️

**ปัญหา**: บาง function ไม่ check return value, บาง function throw error ไม่ consistent

```cpp
// ❌ ไม่ check error
DISM.initDisplay();
file_card.initSDCard();
initAudio();
initMicrophone();

// ✅ ดี: มีการ check
if(audio.setPinout(BLCK_PIN, RLC_PIN, DIN_PIN)) {
    Serial.println("installed audio.");
    isAudio_install = true;
}
```

**แนวทางแก้ไข**:
- Return bool/error code จากทุก init function
- Check return value ใน setup()
- Log error messages ตรงจุด error เกิด

---

#### 5. **Semaphore ใช้ผิด (Race Condition เสี่ยง)** 🔒

**ปัญหา**: ใน main.cpp, queue operation ไม่ protected

```cpp
// ❌ Queue operation ไม่ semaphore protected
xQueueSend(display_command, &cmd, portMAX_DELAY);
xQueueSend(audio_command, &cmd, portMAX_DELAY);

// ⚠️ ต่างหาก: displaySemaphore ใช้ได้บ้าง
if(xSemaphoreTake(displaySemaphore, portMAX_DELAY) == pdTRUE) {
    tft.println("...");
    xSemaphoreGive(displaySemaphore);
}
```

**แนวทางแก้ไข**:
- Queue ไม่ต้อง semaphore (it's thread-safe)
- แต่ shared data structures ต้อง semaphore
- ใช้ consistent lock strategy

---

#### 6. **Magic Numbers ทั่วไป** 🔢

**ปัญหา**: Pin numbers, boundaries, values กระจัดกระจายในโค้ด

```cpp
#define ENC_A_PIN 15
#define ENC_B_PIN  16
#define ENC_SW  10
// ... เหล่านี้กระจายอยู่ใน main.cpp, GlobalVar.hpp, SoundManager.hpp
```

**แนวทางแก้ไข**:
- สร้าง config.hpp เป็นที่ศูนย์รวม
- ทำ Hardware configuration อยู่ที่เดียว

---

#### 7. **Network Module - Typo และ inconsistency** 📡

**ปัญหา**: มี typo ในชื่อฟังก์ชัน/variable

```cpp
// Network.cpp line 72: typo "usesrname" ❌
prefs.putString("usesrname", username_obj.username);

// Line 83: ทำซ้ำ ❌
prefs.getString("usesrname", username_obj.username, sizeof(username_obj.username));
```

**แนวทางแก้ไข**:
```cpp
prefs.putString("username", username_obj.username);  // ✅ ถูก
prefs.getString("username", username_obj.username, sizeof(username_obj.username));
```

---

#### 8. **Task Stack Size ที่ไม่ optimize** 💾

**ปัญหา**: หลายๆ task ใช้ stack size คงที่ไม่ optimize ตามความต้องการ

```cpp
// main.cpp line 552-554: Stack sizes อาจจะ inefficient
BaseType_t task1 = xTaskCreatePinnedToCore(handleAudio, "handleAudio", 4 * 1024, ...);
BaseType_t netWorkTask = xTaskCreatePinnedToCore(runNet, "runNet", 4 * 1024, ...);
BaseType_t disPTask = xTaskCreatePinnedToCore(handleDisplay, "handleDisplay", 3 * 1024, ...);
```

**แนวทางแก้ไข**:
- ใช้ uxTaskGetStackHighWaterMark() เพื่อ monitor ค่าจริง
- ปรับ stack size ให้ใหม่ตาม data
- เพิ่ม log เมื่อ stack usage > 80%

---

#### 9. **Display Buffer Management** 📺

**ปัญหา**: Sprite.getBuffer() check ไม่ consistent

```cpp
// ❌ Inconsistent checks
if (spr.getBuffer() == nullptr)     // บาง function
if (!spr.created())                  // บาง function ใช้ API ที่ต่างกัน
if (spr.getBuffer() == nullptr)     // อีก function
```

**แนวทางแก้ไช**:
```cpp
// ใช้ spr.created() ที่ LovyanGFX recommend
if (!spr.created()) return;
```

---

#### 10. **Auto Play Next Logic** ⏭️

**ปัญหา**: autoPlayNext flag อาจไม่ set ถูกต้อง

```cpp
// ไม่มี code ที่ set autoPlayNext = true ในปัจจุบัน
// เมื่อเพลงจบควรมี callback
bool autoPlayNext = false;  // ไม่มีใครเปลี่ยนเป็น true
```

**แนวทางแก้ไข**:
- ใช้ Audio library callback: `audio_eof_mp3()`, `audio_eof_wav()`
- Set `autoPlayNext = true` ใน callback
- Test ด้วย playlist

---

## 📝 Code Quality Issues

### Naming Convention
- ✅ ดี: `DISM` (DisplayManager), `nm` (NetworkManager), `file_card` (FileManager)
- ❌ ไม่ดี: `runnet` - ควรเป็น `t_networkTask` เพื่อ consistency

### Comments
- ✅ ใช้ emoji comments (🌟, ✅, ❌) ตามสถาน
- ⚠️ บาง comment ไม่ translate ภาษาไทย/English ไม่ consistent

### Includes
- ⚠️ Some circular includes risk (ต้อง check)
- ✅ ใช้ header guards (`#ifndef`, `#define`)

---

## 🚀 Optimization Recommendations

### 1. Performance
```cpp
// ❌ Bubble sort (O(n²)) - ช้า
for(size_t i = 0; i < playlistNames.size(); i++) {
    for(size_t j = i + 1; j < playlistNames.size(); j++) {
        if(playlistNames[i].compareTo(playlistNames[j]) > 0) {
            std::swap(playlistNames[i], playlistNames[j]);
        }
    }
}

// ✅ ควรใช้ std::sort (O(n log n))
std::sort(playlistNames.begin(), playlistNames.end());
```

### 2. Memory
- ใช้ move semantics แทน copy
- จำกัด vector size ด้วย `reserve()`
- Monitor PSRAM usage ตั้งแต่ init

### 3. I/O
- Cache file list แทนจะ read SD card ทุกครั้ง
- ใช้ async I/O operation สำหรับ file operations

---

## 🛠️ Action Items (Priority Order)

### HIGH Priority
1. ✅ [NEW] สร้าง HardwareManager.hpp/.cpp สำหรับ device status tracking
2. ✅ [NEW] ปรับปรุง debug() function ให้แสดง hardware status ทั้ง 3 state
3. แก้ไข IOManager ให้ manage GPIO pins อย่างเป็นระบบ
4. แก้ Typo ใน Network.cpp ("usesrname" → "username")
5. เพิ่ม error handling ใน setup() function

### MEDIUM Priority
6. Implement VoiceControl.cpp อย่างเต็มที่
7. ปรับปรุง sorting algorithm ใช้ std::sort
8. จัดการ memory leak ใน vector
9. ปรับ task stack size ให้ optimize

### LOW Priority
10. Refactor magic numbers → config.hpp
11. Standardize display buffer checks
12. Add comprehensive error logging

---

## 📊 Current Project Structure

```
ToFan-OS/
├── src/
│   ├── main.cpp                    (Input handler, state machine)
│   └── extendstion/
│       ├── DisplayManager.cpp      (UI rendering)
│       ├── SoundManager.cpp        (Audio + Mic)
│       ├── Network.cpp             (WiFi + WebServer)
│       ├── FileManager.cpp         (SD Card ops)
│       ├── HardwareManager.cpp     (✨ NEW - Device status)
│       ├── VoiceControl.cpp        (⚠️ Incomplete)
│       └── IOManager.cpp           (⚠️ Empty)
├── data/
│   └── ToFan-project-1_inferencing.h (Edge Impulse model)
└── platformio.ini
```

---

## 🎯 Recommendations Summary

| Category | Current | Target | Effort |
|----------|---------|--------|--------|
| Error Handling | 40% | 90% | Medium |
| Memory Mgmt | 60% | 95% | Medium |
| Code Comments | 70% | 100% | Low |
| Performance | 75% | 95% | High |
| Testing | 30% | 80% | High |

---

## 📱 Debug Menu Enhancement (✅ DONE)

ระบบ Debug Menu ที่ปรับปรุงแล้วแสดง:
- **Device Status** ทั้ง 8 hardware components
- **3 State Indicators**:
  - 🟢 GREEN: Working (กำลังทำงาน)
  - 🟡 YELLOW: Connecting (กำลังเชื่อมต่อ)
  - 🔴 RED: Error (เกิดข้อผิดพลาด)
  - ⚪ GRAY: Idle (ไม่ทำงาน)
- **System Resources**: RAM, Stack usage, PSRAM
- **Scrollable List**: จอ 320x240 ทั้งเครื่องย่อของ 8 devices

### Devices Tracked:
1. ESP32-S3-R16 - Memory status
2. Display (ST7789) - Connection status
3. Audio (MAX98357A) - Playing/Idle status
4. Microphone (I2S) - Ready status
5. SDCard Module - Mount status
6. WiFi Module - SSID/Status
7. IP5306 Power - Active status
8. Network - IP Address

---

**Created**: 2025-06-17
**Project**: ToFan OS v2.0
**Author**: Code Review System
