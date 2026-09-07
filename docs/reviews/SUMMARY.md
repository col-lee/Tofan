# สรุปโครงสร้างปัจจุบัน

Arduino entry point เรียก application lifecycle; app ประสาน input/AI Pet กับโมดูล audio, display, hardware, network, storage และ ai ส่วน core เก็บ configuration, command payloads และ shared state/resources

DisplayManager ดูแล UI และ MediaPlayback ดูแล JPEG/GIF/task แสดงภาพ โมดูลยังพึ่ง shared globals และ SoundManager ยังรวม audio capture/recording/inference

อ่าน [รายละเอียดการเปลี่ยนแปลง](DETAILED_CHANGELOG.md), [โครงสร้าง](../architecture/PROJECT_STRUCTURE_IMPROVEMENT.md) และ [ข้อจำกัด](CODE_REVIEW.md)
