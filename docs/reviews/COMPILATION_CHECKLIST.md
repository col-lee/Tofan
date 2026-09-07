# การตรวจ build

Environment: `esp32-s3-devkitc-1-n16r8v` ตาม platformio.ini

```sh
pio run -e esp32-s3-devkitc-1-n16r8v
```

ผลตรวจวันที่ 8 กันยายน 2026:

- Firmware build สำเร็จ และสร้าง `firmware.elf` / `firmware.bin`
- RAM 99,136 / 327,680 bytes (30.3%)
- Flash 1,524,761 / 5,242,880 bytes (29.1% ของ app slot)
- ตรวจ local include 77 จุด และ Markdown link 29 จุด ไม่พบไฟล์ปลายทางหาย
- `git diff --check -- src docs README.md` ผ่าน
- มี warning จาก dependency เช่น Edge Impulse macro redefinition และ attribute ใน ESPAsyncWebServer

ผล build ครั้งนี้อยู่ใน `docs/build/restructure-build.log`; `docs/build/pio-build.log` เป็น log เดิม โมเดล inference ต้องมีในเครื่องก่อน build ดู [คู่มือพัฒนา](../guides/IMPLEMENTATION_GUIDE.md)

ยังไม่ได้ upload หรือทดสอบบนบอร์ด `test/test_main.cpp` เป็น smoke test ตัวอย่าง ไม่ยืนยัน queue ownership, concurrency, WAV, UI หรือเครือข่าย
