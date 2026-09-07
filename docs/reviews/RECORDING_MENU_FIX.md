# การทำงานของหน้า Record

InputController เลือกเมนู index 5 แล้วเรียก enterRecordingMode() ก่อนเปลี่ยนหน้าเป็น RECORDE การเข้าหน้ายังไม่เริ่มเขียนไฟล์ ต้องกด encoder เพื่อเรียก startRecording("/main/Musics/voice_record.wav") กดอีกครั้งเพื่อ stopRecording()

CaptureSamples เป็นผู้เรียก i2s_read() และส่ง RecorderFrame เข้า recorderQueue; recordLoop() เขียนเฟรมลง SD จาก application update การทำเช่นนี้ใช้ capture task ร่วมกับ inference โดยสลับตาม recording state

เมื่อกด Back ออกจาก Record จะเรียก exitRecordingMode() ซึ่งหยุดบันทึก ปิดไฟล์หลังอัปเดต WAV header ล้าง queue/state แล้วให้ inference กลับมาทำงาน ตัวนับหน้าจอใช้ DisplayManager::seconds/previousMillis

AI Pet ใช้ recorder ชุดเดียวกัน แต่บันทึก `/main/ai_pet_input.wav` และหยุดอัตโนมัติประมาณ 5 วินาที การเข้าหน้า volume เป็นอีก transition ที่ควรทดสอบระหว่างบันทึก

ตรวจบนบอร์ด: เริ่ม/หยุดหลายรอบ, Back ขณะบันทึก, เปิด volume, SD หาย/เขียนไม่ได้ และเล่น WAV ที่ได้ ผล build ไม่ทดแทนการทดสอบเหล่านี้
