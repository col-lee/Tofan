# ToFan(2) — Audit & Fix Report

Audit date: 2026-09-12

## Scope

ตรวจ source และ configuration หลักของโปรเจกต์ในหัวข้อ Gemini Live / AI Pet, Audio + PSRAM, MJPEG, Upload/Web Portal, embedded web assets, partition/build configuration และ regression tests

## ส่วนที่ตรวจแล้วอยู่ในสภาพดี

- Gemini Live transport รองรับ TEXT, BIN และ fragmented WebSocket frames แล้ว จึงรับ `setupComplete` แบบ binary ได้
- Gemini Live ปิด client heartbeat watchdog เดิมที่เคยทำให้หลุดประมาณ 4 วินาที
- เมื่อ Barge-in ปิด มี `NO_INTERRUPTION` และฝั่ง client gate ไมค์จน model turn / speaker tail จบ
- ไมค์ Live เป็น PCM16 mono 16 kHz และ output queue เป็น PCM16 mono 24 kHz
- Audio read-ahead ใช้ PSRAM ขนาดใหญ่ และ Live PCM queue / inference / recorder backlog เป็น PSRAM-first
- MJPEG ใช้ PSRAM ping-pong read-ahead 512 KiB x2 + frame buffer และ prefetch แยกจาก display task
- Web upload เขียน SD เป็นช่วงสั้นและปล่อย SD mutex ระหว่าง slice
- ขนาดไฟล์ใหญ่ใน web compression/upload เป็น warning มากกว่า arbitrary hard limit; hard stop เหลือกรณีทางเทคนิค เช่น SD พื้นที่ไม่พอ/ไฟล์เสีย
- Embedded portal assets ใน `src/network/PortalAssets.hpp` ตรงกับ `web/dist` ทุก asset ที่ตรวจ
- Partition table 16 MB ไม่มี overlap และจบที่ 0x1000000 พอดี
- ไม่พบ Gemini API key/password ที่ hard-code ใน text source

## จุดผิด/เสี่ยงที่แก้ในรอบนี้

### 1. PSRAM live queue partial-allocation leak
เดิม `ensureLiveMicStorage()` / `ensureLivePcmStorage()` สามารถ allocate PSRAM สำเร็จบางส่วน แต่สร้าง FreeRTOS queue ไม่ครบแล้ว return false โดยไม่คืน allocation ที่สร้างไปแล้ว ทำให้ retry มีโอกาสรั่ว RAM/PSRAM

แก้โดยเพิ่ม cleanup functions และล้าง partial state ก่อน retry รวมถึง cleanup live mic storage ถ้าสร้าง capture task ไม่สำเร็จ

### 2. Gemini PCM speaker สมมติว่า `i2s_write()` เขียนครบทุกครั้ง
เดิมเพิ่ม `done += batch` แม้ `i2s_write()` จะเขียนได้เพียงบางส่วนหรือ timeout ภายใต้โหลดสูง ทำให้ sample ถูกข้ามและเสียงตอบกลับอาจขาด/ปลายประโยคหาย

แก้เป็น advance ตาม `writtenBytes` จริง, retry แบบ bounded และ log เมื่อ I2S stall จริง

### 3. Gemini speaker queue รับ network burst แบบ zero-wait
เดิม queue PCM ด้วย timeout 0 ทำให้ burst สั้น ๆ สามารถ drop chunk ได้ทันทีแม้ audio task กำลังจะคืน slot

แก้ให้รอแบบ bounded 15 ms เพื่อ absorb jitter โดยไม่ปล่อยให้ latency โตไม่จำกัด

### 4. Gemini session resumption มี parser แต่ไม่ได้ใช้ handle
เดิม log `sessionResumptionUpdate` อย่างเดียว เมื่อ server rotate WebSocket context อาจเริ่มใหม่

แก้ให้ setup ขอ `sessionResumption`, เก็บ `newHandle`, และส่ง handle ล่าสุดเมื่อ reconnect ภายใน AI Pet session เดิม หาก handle ทำให้ setup error จะ clear แล้ว fallback ไป fresh session

### 5. Gemini long-session context management
เพิ่ม `contextWindowCompression.slidingWindow` เพื่อให้ native-audio context ไม่โตจนชน session context limit ง่าย ๆ

### 6. Direct AI config route สามารถหยุด Live session ใต้ AppCoordinator
แม้ Web Portal หลักจะ block อยู่แล้ว แต่ compatibility route `/api/ai/config` เดิม save แล้ว `stopLiveSession()` ขณะที่ UI runtime ยังคิดว่า AI Pet listening อยู่

แก้เป็น HTTP 409: ต้องออกจาก AI Pet ก่อนเปลี่ยน config และเพิ่ม request-body cap 4096 bytes

### 7. GIF Serial output ต่อเฟรม
ลบ debug `>>> DRAWING 1 FRAME! <<<` และ `openedd.` ที่สร้าง Serial I/O ต่อเนื่องและรบกวนความลื่นโดยไม่จำเป็น

## Tests

- Web node tests: 10/10 pass
- Native media tests: 15 pass
- Native MJPEG/video parser tests: pass
- Recording regression: 6 pass
- Web policy checks: pass
- Settings/persistence tests: 12 groups pass
- Runtime contract checks ที่เพิ่มใหม่: 13/13 pass
- Partition layout validation: pass
- Embedded web asset equality check: pass

## Build limitations ของ environment ที่ใช้ audit

- ไม่มี PlatformIO/ESP32-S3 compiler toolchain ใน environment นี้ จึงยังไม่ได้ full firmware compile หลัง source changes รอบนี้
- `web/node_modules` ใน ZIP เป็น dependency ที่ติดตั้งจาก Windows (`@esbuild/win32-x64`) จึง `npm run build` บน Linux ไม่ได้ นี่เป็น dependency-platform mismatch ไม่ใช่ syntax failure ของ web source
- เพราะรอบนี้ไม่ได้แก้ web source และตรวจว่า embedded assets ตรงกับ `web/dist` แล้ว จึงไม่ regenerate `PortalAssets.hpp`
- ใน ZIP ที่แก้แล้วจะไม่รวม `firmware.bin/.elf/.map` เก่าจากก่อน audit และลบ `.o/.d` ของ source ที่แก้ เพื่อบังคับให้ PlatformIO compile ไฟล์เหล่านี้ใหม่ ป้องกันการเผลอ flash/link binary cache ที่ยังไม่มี fixes รอบนี้

## สิ่งที่ควรทำต่อก่อนเป็นสินค้าจริง

- Gemini API key ปัจจุบันเก็บใน NVS ของอุปกรณ์ เหมาะกับ prototype แต่ไม่ควรใช้ master API key แบบถาวรในสินค้าที่แจกผู้ใช้ ควรใช้ backend + ephemeral token/session credential
- ทดสอบ Gemini Live จริงอย่างน้อย: 10+ turns, reconnect หลัง Wi-Fi drop, server GoAway/session rotation, พูดภาษาไทยยาว ๆ, และ upload/file activity พร้อม AI Pet
- เก็บ Serial log ของ `[GEMINI] Session resumption update`, `GoAway`, I2S stall และ dropped chunks เพื่อตรวจ tuning บนฮาร์ดแวร์จริง
