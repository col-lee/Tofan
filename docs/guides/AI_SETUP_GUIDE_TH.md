# ตั้งค่า AI Pet

## สิ่งที่ต้องมี

- บอร์ดที่จับเสียงและเขียน SD ได้ พร้อมไลบรารีโมเดลตาม [คู่มือพัฒนา](IMPLEMENTATION_GUIDE.md)
- Wi-Fi ที่เข้าถึง backend และ URL ของไฟล์เสียงตอบกลับได้
- Backend ที่รับ raw WAV และตอบ JSON `audioUrl`; โปรเจกต์นี้ไม่มีซอร์ส backend

## ขั้นตอน

1. เข้า Settings เปิด Wi-Fi และ Admin Mode
2. ส่ง JSON ไป `POST http://<device-ip>/api/ai/config` ด้วย `Content-Type: application/json`

```json
{
  "enabled": true,
  "pipelineUrl": "http://backend.example/api/v1/voice",
  "provider": "backend",
  "apiKey": "example-device-token"
}
```

ตัวอย่าง URL/token ต้องเปลี่ยนให้ตรงกับ backend ของคุณ `apiKey` ถูกส่งเป็น Bearer token ส่วน `provider` ถูกส่งใน X-AI-Provider; ตั้งค่า model และ provider SDK ที่ backend เพราะ firmware เก็บ `model` แต่ยังไม่ส่งออกไป

3. อ่าน `GET /api/ai/config` ตรวจ enabled/configured และ `GET /api/ai/status` ดู state/lastError
4. กลับเข้าหน้า AI Pet เพื่อเริ่มบันทึกประมาณ 5 วินาทีไป `/main/ai_pet_input.wav`
5. Backend ตอบ 2xx พร้อม `{"audioUrl":"http://backend.example/audio/reply.wav"}` จากนั้นอุปกรณ์ส่ง URL ไป audio queue

## พฤติกรรมที่ควรทราบ

- การเปิด AI ขณะอยู่หน้า Pet ไม่ได้เริ่มฟังทันที ให้กลับเข้า Pet ใหม่
- บันทึกหนึ่งรอบต่อการเข้า Pet ที่พร้อมทำงาน ไม่มีการสนทนาต่อเนื่องอัตโนมัติ
- คำสั่งเสียงในเครื่องสำหรับเปิด/ปิดไฟเป็นคนละส่วนกับ AI backend
- `user`/`password` เก็บใน NVS แต่ยังไม่ใช้ authenticate HTTP; ค่า apiKey/password ว่างไม่ล้างค่าที่เคยบันทึก
- เสียงตอบกลับต้องเข้าถึงได้โดย audio client ซึ่งไม่ได้แนบ bearer token จาก AI request
- AI routes ยังไม่ตรวจ token login และ HTTPS ยังไม่มีการตั้ง CA certificate; `allowInsecureTLS: true` ปิดการตรวจ certificate ใช้เฉพาะการทดลองในเครือข่ายที่ควบคุม

## ตรวจปัญหา

| อาการ | จุดตรวจ |
| --- | --- |
| ไม่เริ่มฟัง | enabled, pipelineUrl, microphone readiness และ SD |
| WiFi is not connected | สถานะ Wi-Fi STA และ credentials |
| Audio file could not be opened | ไฟล์ `/main/ai_pet_input.wav` และ SD |
| Pipeline HTTP error | statusCode, URL, backend และ bearer token |
| ส่งสำเร็จแต่ไม่เล่น | JSON มี audioUrl และ URL เข้าถึงได้จากบอร์ด |
| WAV เร็ว/ช้าผิดปกติ | EI_CLASSIFIER_FREQUENCY ต้องตรงกับ header 16000 Hz |

รายละเอียดตรงกับ [สัญญา backend](../architecture/AI_ARCHITECTURE.md)
