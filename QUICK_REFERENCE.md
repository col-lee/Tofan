# 🎨 ToFan OS - Visual Quick Reference Guide

## 📊 Debug Menu Display (Actual Screen)

```
╔════════════════════════════════════════════════════════════════╗
║                 HARDWARE DEBUG STATUS                          ║
╚════════════════════════════════════════════════════════════════╝

● ESP32-S3-R16                    Working              65KB
  Memory available in kilobytes

● Display (ST7789)                Working              320x240 SPI
  SPI interface 320x240 pixels

● Audio (MAX98357A)               Working              Idle
  Not currently playing audio

● Microphone (I2S)                Working              I2S Ready
  I2S interface ready for input

● SDCard Module                   Working              Connected
  SD card mounted successfully

● WiFi Module                     Connecting          Connecting...
  WiFi connection in progress

● IP5306 (Power)                  Working              Active
  Power supply management active

● Network                         Idle                 -
  No internet connectivity

═══════════════════════════════════════════════════════════════════

System Resources:
RAM: 456KB | Min: 123KB
Tasks - Audio:256 Dis:512 Net:384
PSRAM: 2048KB avail

                  [Back to exit debug menu]

═══════════════════════════════════════════════════════════════════
```

---

## 🎯 Status Color Legend

```
┌─────────────────────────────────────────┐
│ Status Indicators & Color Mapping       │
├─────────────────────────────────────────┤
│                                         │
│  🟢 GREEN   = WORKING                   │
│     Status: Device functioning normally │
│     Example: Display connected          │
│                                         │
│  🟡 YELLOW  = CONNECTING                │
│     Status: Initializing/In Progress    │
│     Example: WiFi connecting...         │
│                                         │
│  🔴 RED     = ERROR                     │
│     Status: Problem detected            │
│     Example: SD Card not found          │
│                                         │
│  ⚪ GRAY     = IDLE                      │
│     Status: Not in use/Waiting          │
│     Example: WiFi disabled              │
│                                         │
└─────────────────────────────────────────┘
```

---

## 🔌 Devices Monitored

```
┌─────────────────────────────────────────────────────┐
│ DEVICE                    MONITORS                  │
├─────────────────────────────────────────────────────┤
│ 1. ESP32-S3-R16          CPU/Memory/Heap           │
│ 2. Display (ST7789)      SPI Connection/Init       │
│ 3. Audio (MAX98357A)     Audio State/Playback      │
│ 4. Microphone (I2S)      I2S Ready/Input Stream    │
│ 5. SDCard Module         Mount Status/File Access  │
│ 6. WiFi Module           Connection/SSID/Signal    │
│ 7. IP5306 (Power)        Supply/Battery Level      │
│ 8. Network               IP Address/Connectivity   │
└─────────────────────────────────────────────────────┘
```

---

## 🎮 How to Access Debug Menu

```
Step 1: Power On Device
   └─► System boots and shows ToFan OS splash

Step 2: Press Rotary Encoder Button
   └─► Enter Home Menu

Step 3: Rotate Encoder to Select Item
   └─► Items: [Display] [Music] [Settings] [Pet] [Debug] [Record]
   └─► Navigate to "Debug" (5th item)

Step 4: Press Select Button
   └─► Enter Debug Menu
   └─► See all 8 device statuses

Step 5: View Live Updates
   └─► Status updates every 1 second
   └─► Colors change as devices connect/disconnect

Step 6: Exit Debug Menu
   └─► Press Back button
   └─► Return to Home Menu
```

---

## 💡 LED Patterns (IO Manager)

```
┌──────────────────────────────────────┐
│ LED Diagnostic Patterns              │
├──────────────────────────────────────┤
│                                      │
│ SUCCESS Pattern (Slow):              │
│ [ON 300ms]─[OFF 300ms] × 3           │
│ ████░░░ ████░░░ ████░░░             │
│ Indicates: Operation successful      │
│                                      │
│ CONNECTING Pattern (Medium):         │
│ [ON 200ms]─[OFF 200ms] × 4           │
│ ██░░ ██░░ ██░░ ██░░                 │
│ Indicates: System initializing       │
│                                      │
│ ERROR Pattern (Fast):                │
│ [ON 100ms]─[OFF 100ms] × 5           │
│ █░ █░ █░ █░ █░                      │
│ Indicates: Error detected            │
│                                      │
└──────────────────────────────────────┘
```

---

## 📱 Hardware Configuration

```
                    ESP32-S3-R16n8
                    ┌──────────────┐
                    │              │
         ┌──────────┤  SPI3_HOST   ├──────────┐
         │          │              │          │
    SCLK-39      Display      MOSI-38    DC-41
    RST-42        (TFT)        CS-40
    ST7789
    320×240

                    I2S Interface
         ┌──────────────────────────┐
         │                          │
    SD-1    WS-2    SCK-17    (I2S NUM_1)
         │                          │
     Microphone          (Recording)
```

---

## 🔧 Configuration Locations

```
📍 Pin Definitions
   └─► src/extendstion/config.hpp
   └─► All GPIO pins centralized

📍 Hardware Monitoring
   └─► src/extendstion/HardwareManager.hpp
   └─► src/extendstion/HardwareManager.cpp
   └─► Auto-update every 1 second

📍 Debug Display
   └─► src/extendstion/DisplayManager.cpp
   └─► Function: debug() (lines 892+)
   └─► Shows 8 devices + system info

📍 LED Control
   └─► src/extendstion/IOManager.cpp
   └─► LED patterns & diagnostic functions

📍 Global Variables
   └─► src/extendstion/GlobalVar.hpp
   └─► Extern declarations for all managers
```

---

## 🚀 Compilation Flow

```
┌─────────────────────────┐
│  PlatformIO Build       │
├─────────────────────────┤
│                         │
│  1. Verify includes     │  ✓ All headers found
│  2. Check config.hpp    │  ✓ Pins defined
│  3. Compile .cpp files  │  ✓ New files included
│  4. Link objects        │  ✓ No conflicts
│  5. Generate .bin       │  ✓ Firmware ready
│  6. Display size        │  ✓ ~700 KB
│                         │
└─────────────────────────┘
        │
        ▼
┌─────────────────────────┐
│  Upload to ESP32-S3     │
├─────────────────────────┤
│  1. Clear flash         │
│  2. Upload firmware     │
│  3. Verify checksum     │
│  4. Reboot device       │
│  5. Watch boot sequence │
│                         │
└─────────────────────────┘
        │
        ▼
┌─────────────────────────┐
│  Serial Monitor (Verify)│
├─────────────────────────┤
│  ✓ Baud: 115200        │
│  ✓ Watch: Boot messages │
│  ✓ Test: Access debug   │
│  ✓ Verify: All devices  │
│                         │
└─────────────────────────┘
```

---

## 📈 System Architecture (Updated)

```
┌─────────────────────────────────────────────────────────┐
│                    ToFan OS v2.0                        │
├─────────────────────────────────────────────────────────┤
│                                                         │
│  ┌─────────────────────────────────────────────────┐  │
│  │  main.cpp - Input Handler & State Machine     │  │
│  └─────────────────────────────────────────────────┘  │
│         │              │              │                │
│         ▼              ▼              ▼                │
│  ┌───────────┐  ┌────────────┐  ┌──────────────┐     │
│  │ Display   │  │   Audio    │  │   Network    │     │
│  │ Manager   │  │ Manager    │  │  Manager     │     │
│  └─────┬─────┘  └────────────┘  └──────────────┘     │
│        │                                              │
│        └────────────┬─────────────────────────────┐   │
│                     ▼                             │   │
│          ┌──────────────────────┐               │   │
│          │ HardwareManager (NEW)│               │   │
│          │                      │               │   │
│          │ 8 Device Tracking    │               │   │
│          │ Status Monitoring    │               │   │
│          └──────────────────────┘               │   │
│                     │                             │   │
│                     ▼                             │   │
│          ┌──────────────────────┐               │   │
│          │ Debug Menu Display   │               │   │
│          │ (Enhanced)           │               │   │
│          │                      │               │   │
│          │ - 8 Devices         │               │   │
│          │ - Color Status      │               │   │
│          │ - System Info       │               │   │
│          └──────────────────────┘               │   │
│                                                 │   │
│  ┌────────────────────────────────────────────┘   │
│  │                                                │
│  ▼                                                │
│  ┌──────────────────────────────────┐            │
│  │ IOManager (Enhanced)             │            │
│  │ - LED Control                    │            │
│  │ - Button Input                   │            │
│  │ - Diagnostic Patterns            │            │
│  └──────────────────────────────────┘            │
│                                                  │
└──────────────────────────────────────────────────┘
```

---

## 🎯 Key Improvements at a Glance

```
BEFORE                          AFTER
──────────────────────────────────────────────────

No device monitoring      →     HardwareManager tracks all
Minimal debug info       →     8 devices + details shown
No LED control          →     Full LED diagnostics
Scattered config        →     Centralized config.hpp
Empty IOManager         →     Complete implementation
Manual status checks    →     Automatic monitoring
Text-only debug         →     Color-coded visual display
```

---

## 🔍 Quick Troubleshooting

```
Issue: Debug menu doesn't display
└─► Check: hwManager.initDevices() called in setup()
└─► Verify: DisplayManager.hpp includes HardwareManager.hpp
└─► Solution: Check serial output for error messages

Issue: Device status shows ERROR
└─► Check: Is device connected?
└─► Verify: Correct pins in config.hpp
└─► Solution: Test device independently

Issue: Compilation fails
└─► Check: All new files in src/extendstion/
└─► Verify: No circular includes
└─► Solution: See COMPILATION_CHECKLIST.md

Issue: Status doesn't update
└─► Check: hwManager.updateAllStatus() being called
└─► Verify: Correct semaphore access
└─► Solution: Monitor Serial for timing issues
```

---

## 📞 Command Reference

```bash
# Build project
pio run -e esp32-s3-devkit

# Build & Upload
pio run -e esp32-s3-devkit -t upload

# Monitor Serial
pio device monitor -b 115200

# Clean rebuild
pio run -e esp32-s3-devkit -t clean
pio run -e esp32-s3-devkit

# Monitor with output
pio device monitor -b 115200 -f send_on_enter
```

---

## ✨ Feature Highlights

```
✅ 8-Device Real-Time Monitoring
   └─► Updates every 1 second
   └─► Non-blocking operation

✅ 4 Status States with Colors
   └─► Working (Green)
   └─► Connecting (Yellow)
   └─► Error (Red)
   └─► Idle (Gray)

✅ System Resource Display
   └─► RAM Usage
   └─► Stack Watermarks
   └─► PSRAM Available

✅ Device-Specific Details
   └─► IP Addresses
   └─► File Status
   └─► Memory Info

✅ LED Diagnostic Patterns
   └─► Success/Error/Connecting
   └─► Visual feedback

✅ Centralized Configuration
   └─► All pins in one file
   └─► Easy to port/modify
```

---

**Status**: ✅ Complete & Ready to Deploy
**Last Updated**: 2025-06-17
**Version**: 2.0.1
