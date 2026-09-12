# AIpet reacts to speech

- ตอน Gemini Live ตอบ ปากขยับตามระดับเสียง PCM ที่ส่งให้ลำโพงจริง แทนการอ้างอิงความเร็วที่ข้อมูลเสียงมาถึงจากเครือข่าย
- ตอนรับฟัง ตาและคลื่นเสียงตอบสนองตามพลังงานเฉลี่ยของเฟรมเสียงไมค์ จึงลดการกระพริบจากการอ่าน sample เดียว
- เมื่อเว้นจังหวะหรือไม่มีเสียง ระดับจะลดลงและปากหุบ มีการเกลี่ยขนาดปากเพื่อให้เคลื่อนไหวต่อเนื่อง
- เมื่อปิดเสียงหรือยกเลิกคำตอบ ระดับเสียงสำหรับภาพเคลื่อนไหวจะลดเป็นศูนย์ การเล่นกับลูกกลิ้งและสีหน้าเดิมยังใช้งานได้

เป็นภาพเคลื่อนไหวตามความดัง ไม่ใช่การจับรูปปากตามคำหรือหน่วยเสียง การแสดงผลอาจคลาดจากเสียงจริงเล็กน้อยตามบัฟเฟอร์ I2S และรอบวาดจอ จึงยังต้องตรวจบนบอร์ด

Validation: native `test_voice_envelope.cpp` checks whole-frame energy, mute,
decay, extreme samples and clock wrap. `test_pet_voice_render.py` runs actual
DisplayManager rendering and checks open/closed mouth and listening waves.
Gemini queue and full microphone-packet regressions also pass. No board flashed.
