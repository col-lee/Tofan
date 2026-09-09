# ToFan Video Playback (ESP32-S3 + LovyanGFX)

ฟีเจอร์นี้เล่นวิดีโอจาก SD card โดยใช้ **raw MJPEG** ซึ่งประกอบด้วย JPEG หลายเฟรมเรียงต่อกัน ESP32-S3 จะอ่านทีละเฟรมเข้า PSRAM แล้ว decode ด้วย LovyanGFX

## รูปแบบไฟล์ที่รองรับ

- `.mjpeg`
- `.mjpg`
- Raw Motion-JPEG stream เท่านั้น
- Baseline JPEG (SOF0)
- แนะนำ 320x240 @ 12 FPS
- จำกัด resolution ที่ตรวจไว้สูงสุด 640x480
- frame buffer ใน PSRAM: 512 KiB

ไฟล์ MP4, WebM, MOV, AVI สามารถเก็บบน SD ผ่าน Web Portal ได้ แต่ **ไม่ได้ decode บน TFT โดยตรง** ต้องแปลงเป็น raw MJPEG ก่อน

## ตำแหน่งไฟล์

```text
SD Card/
└── main/
    ├── Pictures/
    ├── Musics/
    └── Videos/
        └── demo.mjpeg
```

`FileManager` จะสร้าง `/main/Videos` ให้อัตโนมัติเมื่อเริ่มระบบถ้ายังไม่มี

## วิธีแปลง MP4 เป็น MJPEG

ติดตั้ง FFmpeg บนคอมพิวเตอร์ แล้วใช้:

```bash
ffmpeg -i input.mp4 -vf "scale=320:240:force_original_aspect_ratio=decrease,pad=320:240:(ow-iw)/2:(oh-ih)/2:black" -r 12 -c:v mjpeg -q:v 7 -pix_fmt yuvj420p -f mjpeg output.mjpeg
```

ความหมายหลัก:

- `scale=320:240...` ลดขนาดให้เหมาะกับจอ
- `pad=...` เติมขอบดำเพื่อรักษาอัตราส่วนภาพ
- `-r 12` ทำเป็น 12 FPS
- `-c:v mjpeg` encode ทุกเฟรมเป็น JPEG
- `-q:v 7` คุณภาพ JPEG (เลขต่ำคุณภาพสูงแต่ไฟล์ใหญ่)
- `-f mjpeg` บังคับให้เป็น raw MJPEG ไม่ใช่ AVI container

## วิธีใช้งานบน ToFan

1. นำ `output.mjpeg` ไปไว้ `/main/Videos/` หรืออัปโหลดผ่าน Web Portal
2. เข้า `Home -> Media`
3. รายการจะรวมไฟล์จาก `/main/Pictures` และ `/main/Videos`
4. หมุน encoder เลือกไฟล์ `.mjpeg`/`.mjpg`
5. กด encoder
6. UI sprite ถูกลบเพื่อคืน PSRAM จากนั้น display task จะเริ่มวิดีโอ
7. กด Back เพื่อส่ง `DISPLAY_COMMAND::CLEAR`, หยุดวิดีโอ และสร้าง UI sprite กลับมา

## Flow ในโค้ด

```text
InputController
    |
    | DISPLAY_COMMAND::SHOW + path
    v
handleDisplay()  [MediaPlayback.cpp]
    |
    | extension .mjpeg/.mjpg
    v
openVideo()      [VideoPlayback.cpp]
    |
    v
PLAYING_VIDEO state
    |
    +--> advanceVideo()
            |
            +--> readMjpegFrame()
            |      [MjpegFrame.hpp]
            |
            +--> JPEG frame -> PSRAM
            |
            +--> readJpegDimensions()
            |
            +--> tft.drawJpg()
            |
            +--> 12 FPS pacing
```

## API

ประกาศอยู่ใน `src/display/VideoPlayback.hpp`

```cpp
bool openVideo(const String& path);
bool advanceVideo();
void closeVideo();
bool videoIsPlaying();
uint32_t videoFramesDecoded();
```

### `openVideo(path)`

เปิดไฟล์จาก SD, จอง frame buffer 512 KiB ใน PSRAM และเตรียม reader

### `advanceVideo()`

เรียกซ้ำจาก display task ฟังก์ชันจะเช็กเวลาเอง ถ้ายังไม่ถึงเฟรมถัดไปจะ return ทันที ถ้าถึงเวลาจะอ่าน JPEG ถัดไปและวาดลงจอ

- `true` = ยังเล่นต่อได้
- `false` = เกิด fatal error / เปิดหรือ decode ต่อไม่ได้

เมื่อถึง EOF หลังเคยเล่นอย่างน้อยหนึ่งเฟรม ระบบจะ seek กลับ byte 0 และเล่นวนอัตโนมัติ

### `closeVideo()`

ปิดไฟล์ SD, คืน PSRAM, reset read buffer และสถานะวิดีโอ

### `videoIsPlaying()`

ใช้เช็กว่ามี file + frame buffer ที่กำลังใช้งานอยู่หรือไม่

### `videoFramesDecoded()`

คืนจำนวนเฟรมที่ decode สำเร็จตั้งแต่เปิดวิดีโอครั้งล่าสุด ใช้ debug/diagnostic ได้

## Error ที่อาจเห็น

```text
[VIDEO] ERROR: PSRAM not detected
[VIDEO] ERROR: Not enough PSRAM for video
[VIDEO] ERROR: Cannot open video
[VIDEO] ERROR: Video frame too large
[VIDEO] ERROR: Progressive JPEG unsupported
[VIDEO] ERROR: Invalid MJPEG frame
[VIDEO] ERROR: Cannot read JPEG dimensions
[VIDEO] ERROR: Cannot decode JPEG frame
```

ถ้าขึ้น `Invalid MJPEG frame` ให้เช็กว่าไฟล์สร้างด้วย `-f mjpeg` ไม่ใช่แค่เปลี่ยนนามสกุล MP4/AVI เป็น `.mjpeg`

## Test

`test/test_media_native.cpp` มี test เพิ่มสำหรับ MJPEG parser:

- หา SOI (`FF D8`) หลัง multipart/boundary bytes
- อ่านสอง JPEG frame ต่อกัน
- ปฏิเสธ frame ที่ใหญ่กว่า buffer

ในชุดที่แก้แล้ว native test ผ่านทั้งหมด 15 cases
