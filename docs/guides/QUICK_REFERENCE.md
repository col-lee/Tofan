# อ้างอิงด่วน

## คำสั่ง

```sh
pio run -e esp32-s3-devkitc-1-n16r8v
pio run -e esp32-s3-devkitc-1-n16r8v -t upload
pio device monitor -b 115200
```

## พินจาก Config.hpp

| อุปกรณ์ | GPIO |
| --- | --- |
| TFT SCLK / MOSI / DC / CS / RST | 39 / 38 / 41 / 40 / 42 |
| Encoder A / B / SW | 21 / 16 / 10 |
| Back | 7 |
| Speaker BCLK / LRCLK / DIN | 4 / 5 / 6 |
| Microphone WS / SCK / SD | 2 / 17 / 1 |
| SD MOSI / MISO / SCK / CS | 11 / 13 / 12 / 14 |
| Voice command LED / NeoPixel | 18 / 48 |
| IP5306 SDA / SCL | -1 / -1 (ยังไม่กำหนด) |

ปุ่มใช้ INPUT, active HIGH และ debounce 300 ms จอมี panel 240×320 และตั้ง rotation 3; microphone ใช้ I2S port 1

## การควบคุม

- หมุน encoder เพื่อเลือก กด encoder เพื่อยืนยันหรือสั่งงานหน้าปัจจุบัน
- Back สั้นกลับหน้าเดิม/เมนู; Back ค้างเกิน 1000 ms เปิด volume
- Volume กลับหน้าก่อนหน้าเมื่อไม่มีการปรับเกิน 2000 ms
- Record กด encoder เพื่อเริ่ม/หยุด บันทึกทับ `/main/Musics/voice_record.wav`
- Settings มี Admin Mode และ Wi-Fi

## SD และเครือข่าย

| รายการ | ค่า |
| --- | --- |
| ภาพ | `/main/Pictures` (.jpg, .jpeg, .gif) |
| เพลง | `/main/Musics` |
| ไฟล์ AI input | `/main/ai_pet_input.wav` |
| เว็บแอดมิน | `/WEB_Source` |
| AI config | GET/POST `/api/ai/config` |
| AI status | GET `/api/ai/status` |
| Wi-Fi config | POST `/wifi` |
| Login / เปลี่ยนบัญชี / ตรวจ token | POST `/api/signin`, `/api/changeuser`, `/api/checkToken` |
| Upload | POST `/uploadfile` |

รายละเอียดการเริ่มระบบและ task อยู่ใน [โครงสร้าง](../architecture/PROJECT_STRUCTURE_IMPROVEMENT.md) และข้อมูล AI อยู่ใน [สัญญา backend](../architecture/AI_ARCHITECTURE.md)
