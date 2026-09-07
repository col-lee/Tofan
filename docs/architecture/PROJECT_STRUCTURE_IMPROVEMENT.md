# โครงสร้างโปรแกรม

เอกสารนี้อธิบายซอร์สปัจจุบันหลังแยกไฟล์ตามหน้าที่

| ตำแหน่ง | หน้าที่ |
| --- | --- |
| `src/main.cpp` | ส่งต่อ Arduino lifecycle ไปยัง `app::begin()` และ `app::update()` |
| `src/app/Application.cpp` | สร้าง mutex/queue, เริ่มอุปกรณ์และ task, แสดง boot UI |
| `src/app/AppCoordinator.cpp` | เริ่ม RGB/AI, อัปเดต AI Pet และส่งเสียงให้ backend ใน task |
| `src/app/InputController.cpp` | encoder ISR, ปุ่ม, เปลี่ยนหน้าและส่งคำสั่งควบคุม |
| `src/core/Config.hpp` | พินและค่าคงที่; บางค่ามีไว้แต่ยังไม่ได้ใช้ทุกจุด |
| `src/core/Commands.hpp` | `DISPLAY_COMMAND` และ `AUDIO_COMMAND` |
| `src/core/GlobalState.*` | `app::runtime` สำหรับ recording/AI Pet |
| `src/core/SharedResources.*` | ประกาศ readiness flags และกำหนด RTOS handles |
| `src/hardware/DisplayDevice.hpp` | คลาส `LGFX` ตั้งค่า ST7789/SPI และประกาศ `tft`, `spr` |
| `src/hardware/HardwareManager.*` | ตรวจสถานะอุปกรณ์สำหรับหน้า Debug |
| `src/hardware/IOManager.*`, `RGBLed.*` | GPIO และ NeoPixel |
| `src/display/DisplayManager.*` | สถานะ UI, sprite, รายการสื่อและหน้าจอ |
| `src/display/MediaPlayback.cpp` | JPEG/GIF decoder callbacks และ `handleDisplay` |
| `src/audio/SoundManager.*` | speaker, I2S capture, recorder และ inference |
| `src/ai/AIConversation.*` | NVS, AI HTTP routes และส่ง WAV |
| `src/network/Network.*` | Wi-Fi, Admin Mode, upload และ WebSocket |
| `src/storage/FileManager.*` | mount SD และ file operations |

## ลำดับการทำงาน

`setup()` → `app::begin()` → เริ่ม Serial, mutex, queues, จอ, GPIO, SD, เสียง, ไมโครโฟนและข้อมูลอุปกรณ์ → เริ่ม task เสียง/เครือข่าย/แสดงภาพ → boot UI → coordinator และ input controller

`loop()` → `app::update()` → coordinator → input controller โดยเริ่มต้นที่หน้า AI Pet การฟังอัตโนมัติต้องเปิดใช้ AI และตั้ง pipeline URL แล้ว

| Task | Core | Priority | Stack bytes |
| --- | --- | --- | --- |
| `handleAudio` | 0 | 4 | 4096 |
| `runNet` | 0 | 3 | 4096 |
| `handleDisplay` | 1 | 2 | 3072 |
| `CaptureSamples` | ไม่ pin | 10 | 4096 |
| `AIPetVoice` | 0 | 3 | 8192 |

`CaptureSamples` เริ่มระหว่าง initMicrophone; `AIPetVoice` สร้างเมื่อจบรอบบันทึกและลบตัวเองเมื่อเสร็จ

## แนวทางเพิ่มโค้ด

- วาง `.hpp` คู่กับ `.cpp` ในโมดูลเจ้าของหน้าที่ ใช้ include แบบ relative ตามไฟล์จริง
- ให้ `main.cpp` เป็น entry point และให้ `Application.cpp` ดูแลลำดับเริ่มระบบ
- เพิ่ม UI/input ที่ display/app, เพิ่ม network route ที่ network หรือ AI ตามหน้าที่
- อธิบายข้อจำกัด, หน่วย, ownership และเงื่อนไขของโค้ดในคอมเมนต์ หลีกเลี่ยงบันทึกว่าเคยแก้อะไรในอดีต
- อัปเดตเอกสารและ build ทุกครั้งที่ย้ายไฟล์หรือเปลี่ยน public header

โครงสร้างนี้ยังใช้ singleton, shared globals และมี dependency ข้ามโมดูล ไม่ใช่ระบบที่แยกชั้นได้อย่างสมบูรณ์ `SoundManager.cpp` ยังรวม capture/recording/inference เพราะใช้ buffer และ task ร่วมกัน ดู [ข้อจำกัด](../reviews/CODE_REVIEW.md) ก่อนแยกเพิ่ม
