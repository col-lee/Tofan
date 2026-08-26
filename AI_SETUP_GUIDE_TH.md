# คู่มือใช้งาน Edge Voice Assistant

เอกสารนี้อธิบายการตั้งค่าและการเชื่อมต่อระบบเสียงของ ToFan:

```text
ไมโครโฟน -> ESP32-S3 -> AI Backend -> STT -> LLM -> TTS -> audioUrl -> ESP32-S3 -> ลำโพง
```

ระบบนี้เรียกว่า **Edge Voice Assistant** หรือ **IoT Conversational Voice Pipeline**

## 1. ส่วนประกอบของระบบ

### ESP32-S3

ทำหน้าที่บันทึกเสียง WAV, ส่งไฟล์ไปยัง Backend, รับ URL เสียงตอบกลับ และเล่นผ่าน MAX98357A

เมื่อเปิดหน้า `AIPet` และเปิดใช้งาน AI แล้ว ระบบจะ:

1. บันทึกเสียงอัตโนมัติ 5 วินาที
2. ส่ง WAV ไปที่ `pipelineUrl`
3. รอ Backend ประมวลผล STT -> LLM -> TTS
4. อ่าน `audioUrl` จาก JSON response
5. ส่ง URL ให้ Audio task เล่นเสียง
6. ขยับปาก AI Pet ตาม amplitude จากไมโครโฟน

### AI Backend

ทำหน้าที่เรียกบริการ STT, LLM และ TTS ตัวจริง เช่น OpenAI, Google Gemini, Google Cloud TTS, ElevenLabs หรือโมเดลที่รันในเครื่อง

ไม่ควรให้ ESP32 เรียก provider โดยตรง เพราะ API key อาจรั่ว, firmware ต้องรองรับ API หลายรูปแบบ และการจัดการ TLS/timeout จะซับซ้อนขึ้น

## 2. ค่าที่ต้องตั้งบน ESP32

ตั้งค่าผ่าน Admin WebServer ด้วย `POST /api/ai/config`

```json
{
  "enabled": true,
  "pipelineUrl": "https://your-backend.example/api/v1/voice",
  "provider": "openai",
  "model": "gpt-4o-mini",
  "apiKey": "backend-device-key",
  "user": "",
  "password": "",
  "allowInsecureTLS": false
}
```

| ค่า | จำเป็น | ความหมาย |
|---|---:|---|
| `enabled` | ใช่ | `true` เพื่อเปิดการฟังและส่งเสียงใน AIPet |
| `pipelineUrl` | ใช่ | URL endpoint ของ Backend ที่รับ `POST audio/wav` |
| `provider` | ไม่ | ชื่อ provider ที่ส่งเป็น header `X-AI-Provider` เช่น `openai`, `gemini`, `local` |
| `model` | ไม่ | ชื่อโมเดลที่ Backend จะเลือกใช้ เช่น `gpt-4o-mini` หรือ `gemini-2.0-flash` |
| `apiKey` | แนะนำ | device/backend key; firmware ส่งเป็น `Authorization: Bearer ...` |
| `user` | ไม่ | สำรองไว้สำหรับระบบยืนยันตัวตนในอนาคต ปัจจุบันเก็บใน NVS แต่ยังไม่ส่งไปกับ audio request |
| `password` | ไม่ | สำรองไว้สำหรับระบบยืนยันตัวตนในอนาคต ปัจจุบันเก็บใน NVS แต่ยังไม่ส่งไปกับ audio request |
| `allowInsecureTLS` | ไม่ | ใช้ `true` ได้เฉพาะ development ที่ใช้ certificate self-signed; production ต้องเป็น `false` |

การอ่าน config โดยไม่เปิดเผย secret:

```text
GET /api/ai/config
```

การอ่านสถานะ:

```text
GET /api/ai/status
```

Response จะมี `apiKeySet`, `userSet`, `passwordSet` แทนค่าลับจริง

## 3. ค่า Backend สำหรับ AI แต่ละตัว

ค่าในตารางต่อไปนี้เป็น **environment variables ของ Backend** ไม่ใช่ค่าที่ควรฝังใน ESP32

### OpenAI / ChatGPT

Backend ต้องมีค่าประมาณนี้:

```env
LLM_PROVIDER=openai
OPENAI_API_KEY=ใส่คีย์ของ OpenAI ที่นี่
LLM_MODEL=gpt-4o-mini
STT_PROVIDER=openai
STT_MODEL=whisper-1
TTS_PROVIDER=openai
TTS_MODEL=gpt-4o-mini-tts
TTS_VOICE=alloy
```

ค่า ESP32 ที่แนะนำ:

```json
{
  "enabled": true,
  "pipelineUrl": "https://backend.example/api/v1/voice",
  "provider": "openai",
  "model": "gpt-4o-mini",
  "apiKey": "device-key",
  "allowInsecureTLS": false
}
```

หมายเหตุ: ชื่อโมเดลและ voice ต้องตรวจสอบกับ SDK/API version ที่ Backend ใช้จริง

### Google Gemini

Gemini ใช้สำหรับ LLM เป็นหลัก ส่วน STT/TTS อาจใช้ Google Cloud Speech/TTS หรือ provider อื่น:

```env
LLM_PROVIDER=gemini
GOOGLE_API_KEY=ใส่คีย์ Gemini ที่นี่
LLM_MODEL=gemini-2.0-flash
STT_PROVIDER=google-cloud
GOOGLE_APPLICATION_CREDENTIALS=/run/secrets/google-service-account.json
TTS_PROVIDER=google-cloud
TTS_LANGUAGE_CODE=th-TH
TTS_VOICE=th-TH-Standard-A
```

ค่า ESP32 ที่แนะนำ:

```json
{
  "enabled": true,
  "pipelineUrl": "https://backend.example/api/v1/voice",
  "provider": "gemini",
  "model": "gemini-2.0-flash",
  "apiKey": "device-key",
  "allowInsecureTLS": false
}
```

อย่าใส่ `GOOGLE_API_KEY` หรือ service-account private key ลงใน ESP32

### Local AI / Ollama

ใช้กรณีมีเครื่อง server ภายในบ้านหรือ LAN ที่รัน Ollama และมี Backend gateway อยู่หน้า Ollama:

```env
LLM_PROVIDER=ollama
OLLAMA_BASE_URL=http://ollama:11434
LLM_MODEL=llama3.2
STT_PROVIDER=whisper-local
TTS_PROVIDER=piper
```

ค่า ESP32 ต้องชี้ไปที่ Backend gateway ไม่ใช่ Ollama โดยตรง:

```json
{
  "enabled": true,
  "pipelineUrl": "http://192.168.1.50:8000/api/v1/voice",
  "provider": "local",
  "model": "llama3.2",
  "apiKey": "lan-device-key",
  "allowInsecureTLS": false
}
```

ถ้าใช้ HTTPS แบบ self-signed ใน LAN จึงค่อยพิจารณา `allowInsecureTLS: true` ชั่วคราวเท่านั้น

### Provider อื่น

โมดูล ESP32 ไม่ได้ล็อกกับ vendor ใด vendor หนึ่ง ขอเพียง Backend รับสัญญาเดียวกัน:

- STT: Whisper, Google Speech-to-Text, Azure Speech, Deepgram
- LLM: OpenAI, Gemini, Claude, Groq, Ollama
- TTS: OpenAI TTS, Google Cloud TTS, Azure Speech, ElevenLabs, Piper

เปลี่ยน provider ที่ Backend ได้โดยไม่ต้องเปลี่ยน firmware

## 4. Backend API Contract

ESP32 จะส่ง:

```http
POST /api/v1/voice
Content-Type: audio/wav
Authorization: Bearer device-key
X-AI-Provider: openai
```

Body เป็นไฟล์ WAV แบบ:

- PCM 16-bit
- Mono
- 16 kHz

Backend ควรตอบ HTTP 2xx พร้อม JSON:

```json
{
  "requestId": "abc-123",
  "transcript": "สวัสดีครับ",
  "replyText": "สวัสดีครับ มีอะไรให้ช่วยไหม",
  "audioUrl": "https://backend.example/audio/abc-123.wav"
}
```

`audioUrl` ต้องเป็น URL ที่ ESP32 เข้าถึงได้จริง และ Audio library ของโปรเจกต์ต้องรองรับรูปแบบไฟล์/stream นั้น

ถ้าไม่มี `audioUrl`, HTTP status ไม่ใช่ 2xx, Wi-Fi หลุด หรือไฟล์เสียงเปิดไม่ได้ ระบบจะเข้าสถานะ error และจะไม่เล่นเสียง

## 5. ลำดับการตั้งค่าใช้งาน

1. เตรียม Backend ให้รับ WAV และตอบ JSON ตาม contract
2. ตั้งค่า provider key ไว้ใน environment ของ Backend
3. เปิด Admin Mode ของ ToFan
4. ส่ง `POST /api/ai/config` ด้วย `pipelineUrl` ของ Backend
5. ตั้ง `enabled` เป็น `true`
6. ตรวจ `GET /api/ai/status` ต้องได้ `configured: true`
7. เชื่อม ESP32 กับ Wi-Fi เดียวกับ Backend หรือใช้ URL ที่เข้าถึงได้
8. เข้า `AIPet` และพูดในช่วงบันทึก 5 วินาที
9. ตรวจ Serial Monitor หากไม่มีเสียงตอบกลับ

ตัวอย่างคำสั่งทดสอบ config:

```bash
curl -X POST http://ESP32_IP/api/ai/config \
  -H "Content-Type: application/json" \
  -d '{"enabled":true,"pipelineUrl":"https://backend.example/api/v1/voice","provider":"openai","model":"gpt-4o-mini","apiKey":"device-key","allowInsecureTLS":false}'
```

## 6. ความปลอดภัย

- ห้าม commit API key, password หรือ service-account key ลง Git
- ใช้ HTTPS และ certificate validation ใน production
- `allowInsecureTLS: true` ใช้เฉพาะทดสอบชั่วคราว
- จำกัดขนาดไฟล์เสียงและเวลาประมวลผลที่ Backend
- ทำ rate limit ต่อ device
- ลบหรือ mask Authorization header ใน log
- เพิ่ม authentication ให้ `/api/ai/config` ก่อนนำ WebServer ออก Internet
- ใช้ device token แยกจาก provider API key เมื่อทำได้

## 7. ข้อจำกัดของ implementation ปัจจุบัน

- AIPet บันทึกเสียงแบบ fixed duration 5 วินาที ยังไม่มี push-to-talk หรือ wake-word สำหรับ conversation pipeline
- รองรับผลลัพธ์แบบ `audioUrl`; ยังไม่รองรับ audio bytes ใน JSON
- `user/password` ถูกเก็บใน NVS แต่ยังไม่ถูกส่งกับ audio request
- Backend ต้องทำ STT, LLM และ TTS เอง
- ต้องทดสอบว่า audio URL และ codec ที่ Backend ส่งกลับเข้ากันได้กับ `ESP32-audioI2S`

ดูรายละเอียดสถาปัตยกรรมเพิ่มเติมได้ที่ [AI_ARCHITECTURE.md](AI_ARCHITECTURE.md)
