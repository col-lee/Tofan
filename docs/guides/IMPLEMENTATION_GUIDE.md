# คู่มือพัฒนา

## เตรียมเครื่อง

1. ติดตั้ง PlatformIO Core หรือส่วนขยาย PlatformIO ใน VS Code
2. ใช้ environment ใน `platformio.ini` และบอร์ด ESP32-S3 N16R8V
3. เตรียมไลบรารี Arduino export ของโมเดล `ToFan-project-1_inferencing` ใน `lib/ToFan-project-1_inferencing/` ให้มี `src/ToFan-project-1_inferencing.h` และไฟล์โมเดล/SDK ครบ
4. ตรวจอัตราสุ่มของโมเดล `EI_CLASSIFIER_FREQUENCY` ให้ตรงกับ WAV header 16000 Hz เพราะ capture ใช้อัตราจากโมเดล
5. เตรียม SD card และไฟล์เว็บใน `/WEB_Source`; เฟิร์มแวร์ไม่ได้ฝังเว็บเหล่านี้

workspace ปัจจุบันมีโมเดลอยู่ใน `.pio/libdeps/esp32-s3-devkitc-1-n16r8v/ToFan-project-1_inferencing` แต่ `.pio` ถูก ignore และไม่ใช่แหล่ง dependency ที่ทำซ้ำได้บนเครื่องใหม่ อย่าลบ cache นี้ก่อนมีสำเนาโมเดลต้นฉบับ

## Build

```sh
pio run -e esp32-s3-devkitc-1-n16r8v
pio run -e esp32-s3-devkitc-1-n16r8v -t upload
pio device monitor -b 115200
```

บน Windows หาก `pio` ไม่อยู่ใน PATH สามารถใช้ `C:\Users\nonam\.platformio\penv\Scripts\pio.exe` สำหรับเครื่องนี้ หรือเลือก Build ใน PlatformIO IDE

## แก้ไขตามหน้าที่

ดู [แผนผังซอร์ส](../architecture/PROJECT_STRUCTURE_IMPROVEMENT.md) ก่อนเพิ่มไฟล์ การค้นหาด้วย `rg --files src` และ `rg -n 'ชื่อสัญลักษณ์' src` ช่วยตรวจจุดใช้งานก่อนย้ายหรือเปลี่ยนชื่อ

คำสั่ง queue เดิมใช้ Arduino `String` ภายใน struct; FreeRTOS queue คัดลอกเป็น byte จึงยังมีปัญหา ownership ที่ต้องแก้แยกงานนี้ ไม่ควรใช้เป็นต้นแบบ payload สำหรับ queue ใหม่

## ตรวจบนบอร์ด

1. Boot พร้อม SD: ตรวจจอ, เสียง, RGB และหน้า Debug
2. หมุน encoder, กดเลือกและกด Back; กด Back ค้างเกิน 1 วินาทีเพื่อเปิด volume
3. เล่น JPEG/GIF แล้วกลับไปหน้ารายการ; เล่นเพลงจาก SD และทดสอบ pause/seek
4. เข้า Record, กดเริ่ม/หยุด แล้วตรวจ WAV; ออกจาก Record แล้วทดสอบ voice inference
5. เปิด Settings → Admin Mode/Wi-Fi และทดสอบเว็บกับ SD assets ที่เตรียมไว้
6. ตั้ง AI backend, เข้าหน้า Pet และตรวจรอบบันทึก → HTTP POST → เล่น `audioUrl`

`test/test_main.cpp` เป็น Unity smoke test ตัวอย่าง (บวกเลขและ boolean) ไม่ได้ตรวจพฤติกรรมเฟิร์มแวร์ การ build ผ่านยังไม่ยืนยันการทำงานจริงบนอุปกรณ์
