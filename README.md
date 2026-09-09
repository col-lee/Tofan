# ToFan

เฟิร์มแวร์ Arduino / PlatformIO สำหรับ ESP32-S3 พร้อมจอ ST7789, rotary encoder, SD card, ไมโครโฟน I2S, ลำโพง, Wi-Fi และ RGB LED

## ความสามารถปัจจุบัน

- ดูภาพ JPEG/PNG/GIF จาก `/main/Pictures` และเล่นวิดีโอ raw MJPEG (`.mjpeg/.mjpg`) จาก `/main/Videos` บน SD card
- เล่นเพลงจาก `/main/Musics` และสถานีออนไลน์ที่กำหนดในโค้ด
- บันทึกเสียง WAV mono 16-bit โดย header ระบุ 16 kHz ไปที่ `/main/Musics/voice_record.wav`
- จำแนกคำสั่งเสียงเปิด/ปิดไฟด้วยโมเดล Edge Impulse ในเครื่อง เมื่อไม่ได้อยู่ในโหมดบันทึก
- AI Pet บันทึกประมาณ 5 วินาทีเมื่อเข้าหน้าและเปิดใช้ AI แล้ว จากนั้นส่ง WAV ไป backend และเล่น URL ที่ตอบกลับ
- Settings เปิด/ปิด Admin Mode และ Wi-Fi; หน้า Debug แสดงสถานะอุปกรณ์

## โครงสร้างซอร์ส

```text
src/
  main.cpp                 Arduino setup() / loop()
  app/                     เริ่มระบบ, AI Pet lifecycle และรับอินพุต
  core/                    Config, Commands, GlobalState, SharedResources
  ai/                      ตั้งค่า NVS และ HTTP client สำหรับ AI backend
  audio/                   เล่นเสียง, จับเสียง I2S, WAV และ voice inference
  display/                 UI และการแสดง JPEG/GIF
  hardware/                GPIO, RGB LED, การตั้งค่าจอและสถานะฮาร์ดแวร์
  network/                 Wi-Fi, HTTP Admin Mode และ WebSocket
  storage/                 SD card และจัดการไฟล์
```

## Build และใช้งาน

ใช้ environment `esp32-s3-devkitc-1-n16r8v` ใน [platformio.ini](platformio.ini): flash 16 MB, PSRAM 8 MB และ [partition table](partitions_16MB.csv) ที่มี OTA app slot ขนาด 5 MiB สองชุด

```sh
pio run -e esp32-s3-devkitc-1-n16r8v
pio run -e esp32-s3-devkitc-1-n16r8v -t upload
pio device monitor -b 115200
```

ต้องมีไลบรารีโมเดล `ToFan-project-1_inferencing` ที่ให้ header `ToFan-project-1_inferencing.h` ด้วย ปัจจุบันพบใน `.pio/libdeps` ของ workspace แต่ไม่ได้ประกาศแหล่งติดตั้งใน `lib_deps` จึงต้องจัดเตรียมเองสำหรับเครื่องใหม่ รายละเอียดอยู่ใน [คู่มือพัฒนา](docs/guides/IMPLEMENTATION_GUIDE.md)

SD card เก็บสื่อและเว็บแอดมิน โดยเฟิร์มแวร์สร้าง `/main`, `/main/Pictures`, `/main/Musics` และ `/WEB_Source` เมื่อ mount สำเร็จ ต้องเตรียมไฟล์เว็บใน `/WEB_Source` เอง; `data/data.json` ไม่ใช่ชุดเว็บแอดมิน

## เอกสาร

- [โครงสร้างและขอบเขตโมดูล](docs/architecture/PROJECT_STRUCTURE_IMPROVEMENT.md)
- [คู่มือพัฒนาและตรวจสอบ](docs/guides/IMPLEMENTATION_GUIDE.md)
- [คำสั่งและพินอ้างอิง](docs/guides/QUICK_REFERENCE.md)
- [สัญญาการเชื่อมต่อ AI backend](docs/architecture/AI_ARCHITECTURE.md)
- [ตั้งค่า AI ภาษาไทย](docs/guides/AI_SETUP_GUIDE_TH.md)
- [ผลตรวจและข้อจำกัดปัจจุบัน](docs/reviews/README_REVIEW_SUMMARY.md)

## การเล่นวิดีโอบน TFT (MJPEG)

ToFan สามารถเล่นไฟล์ **raw Motion-JPEG** จาก SD card ได้โดยแยก JPEG ทีละเฟรมแล้วส่งเข้า LovyanGFX `drawJpg()` โดยใช้ PSRAM เป็น frame buffer

- รองรับ `.mjpeg` และ `.mjpg`
- ไฟล์ต้องเป็น raw MJPEG (JPEG ต่อกันหลายเฟรม) ไม่ใช่ MP4/AVI/WebM
- JPEG ควรเป็น Baseline JPEG
- แนะนำ 320x240 @ 12 FPS สำหรับจอปัจจุบัน (panel 240x320 หมุนเป็น 320x240)
- จำกัดขนาดภาพต่อเฟรมไม่เกิน 640x480
- เก็บไว้ที่ `/main/Videos/`

การใช้งาน: **Home -> Media -> เลือกไฟล์ `.mjpeg/.mjpg` -> กด encoder** และกด Back เพื่อหยุดวิดีโอและกลับรายการสื่อ

ดูรายละเอียดและคำสั่ง FFmpeg ที่ [docs/guides/VIDEO_PLAYBACK.md](docs/guides/VIDEO_PLAYBACK.md)
