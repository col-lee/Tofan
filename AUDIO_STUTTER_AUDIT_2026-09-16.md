# รายงานวิเคราะห์และแก้เสียงกระตุก (SD / Online stream / Gemini Live)

วันที่: 2026-09-16

## ขอบเขตที่ตรวจ

- เส้นทางเพลงจาก SD และ online stream ผ่าน `ESP32-audioI2S`
- เส้นทาง PCM 24 kHz จาก Gemini Live ไปยัง I2S โดยตรง
- queue, jitter buffer, SD mutex, task priority/core, microphone capture และ video prefetch
- regression ของ chat history, AI session, recording, UI, ESP-NOW, memory policy และ web portal

## สาเหตุที่พบ

บัฟเฟอร์ต้นทางเดิมได้รับการปรับปรุงไว้แล้ว (SD read-ahead 1 MiB และ Gemini jitter buffer 16 KiB) แต่ปลายทางเพลง SD/stream ยังมีคอขวดร่วมอยู่ใน `ESP32-audioI2S 2.3.0`: ไลบรารีเรียก `i2s_write()` ทีละ stereo frame หลัง decode/filter/gain

ที่ 44.1 kHz เท่ากับประมาณ 44,100 driver calls ต่อวินาที แต่ละ call มีงานตรวจสอบและ synchronization ของ driver ทำให้ Core 0 เสียเวลาไปกับ overhead และเหลือ headroom น้อยเมื่อ Wi-Fi, TLS, SD, WebSocket หรือ video prefetch ทำงานพร้อมกัน อาการจึงเกิดได้ทั้ง SD และ stream ส่วน Gemini แม้เขียนแบบ batch อยู่แล้ว แต่ยังไม่มี telemetry ปลายทางชุดเดียวกันเพื่อแยก I2S stall ออกจาก network underrun

## การแก้ไข

1. เพิ่ม PCM staging buffer คงที่ 128 stereo frames (512 bytes) ไม่ใช้ heap
2. รับ sample หลัง decoder, filter และ volume gain แล้วเขียน I2S เป็น batch
3. ลดจำนวน driver calls เชิงทฤษฎีจาก 44,100 เหลือ 345 calls/วินาทีที่ 44.1 kHz (>100 เท่า) โดยเพิ่ม latency สูงสุดประมาณ 2.9 ms
4. รองรับ partial write และ retry เมื่อ DMA/I2S หยุดชั่วคราว ไม่เลื่อน pointer เกินจำนวนที่เขียนจริง
5. flush batch สุดท้ายเมื่อจบเพลงตามธรรมชาติ และล้าง staged PCM เมื่อ pause/seek/stop/เปลี่ยนเพลง/สลับเข้า Gemini เพื่อไม่ให้เสียงเก่าหลุดไปยัง source ใหม่
6. ให้ Gemini ใช้ measured I2S writer ตัวเดียวกัน โดยคง jitter prebuffer และ queue ownership เดิม
7. แยก callback bridge เป็น translation unit ที่ไม่ include `Audio.h` เพื่อให้ symbol เป็น strong; แก้ความเสี่ยงจาก declaration แบบ weak ของ dependency
8. เพิ่มสถานะใน `/api/status`:
   - `speakerI2sWrites`
   - `speakerI2sStalls`
   - `speakerI2sDroppedFrames`
   - `speakerI2sMaxWriteUs`

## สิ่งที่ตั้งใจไม่เปลี่ยน

- ไม่ย้าย task ไปคนละ core และไม่เปลี่ยน priority ของ task อื่น
- ไม่เปลี่ยน SD mutex policy, video prefetch, microphone capture หรือ chat-history flow
- ไม่อัปเกรด major version ของ ESP32-audioI2S/Arduino core เพราะ API และ I2S driver ต่างกันมากและมี regression risk สูง
- ไม่เพิ่ม dynamic allocation ใน real-time speaker path

## Tests

- Python/native regression scripts ทั้งหมด 15 ไฟล์: ผ่าน
- Runtime contract checks: 32/32 ผ่าน
- Speaker batching checks: boundary, ordering, overflow, reset, EOF, source transition, latency และ call reduction: ผ่าน
- Web Node tests: 10/10 ผ่าน
- `git diff --check`: ผ่าน (มีเพียงคำเตือน line-ending เดิมของ Windows)

## วิธีตรวจบนอุปกรณ์จริง

หลัง flash ให้เล่นตามลำดับ: SD MP3/AAC/WAV, online radio, Gemini Live และทดลองพร้อมเปิดหน้าเว็บ/เล่น animation

ค่าปกติที่คาดหวัง:

- `speakerI2sWrites` เพิ่มต่อเนื่อง
- `speakerI2sStalls` อาจเพิ่มเล็กน้อยเมื่อ DMA เต็ม แต่ไม่ควรเพิ่มเร็วต่อเนื่อง
- `speakerI2sDroppedFrames` ควรเป็น 0
- Gemini `speakerUnderruns` ควรเป็น 0 หรือเพิ่มเฉพาะช่วงเครือข่ายสะดุดจริง
- SD `musicLowBufferEvents` ควรเป็น 0 หลัง buffer ถูก prime

การแปลผล:

- `speakerI2sDroppedFrames > 0`: ปัญหาอยู่ที่ I2S/DMA scheduling หรือ driver
- `speakerUnderruns > 0` แต่ I2S dropped เป็น 0: Gemini/network ส่งข้อมูลมาไม่ทัน
- `musicLowBufferEvents > 0` แต่ I2S dropped เป็น 0: SD/network input starvation
- ทุก counter เป็น 0 แต่ยังได้ยินกระตุก: ตรวจ BCLK/LRCLK/DOUT ด้วย logic analyzer, ไฟเลี้ยง amplifier, ground, สาย I2S และ speaker/amplifier hardware ต่อ

หมายเหตุ: host tests และ firmware build ยืนยัน logic/ABI ได้ แต่คุณภาพเสียงสุดท้ายต้องยืนยันบนบอร์ดจริง เพราะ power integrity และ I2S signal integrity จำลองบน host ไม่ได้
