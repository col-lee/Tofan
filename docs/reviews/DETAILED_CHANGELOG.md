# บันทึกการจัดโครงสร้าง src

## การเปลี่ยนแปลง

- ย้าย extensions ไป ai/audio/display/hardware/network/storage ตามหน้าที่
- เปลี่ยน event.hpp เป็น core/Commands.hpp และ GlobalVar.hpp เป็น core/SharedResources.hpp
- แยก LGFX configuration เป็น hardware/DisplayDevice.hpp
- รวม RTOS handle definitions ใน core/SharedResources.cpp
- ย้าย startup/loop dispatch จาก main.cpp ไป app/Application.*
- แยก JPEG/GIF และ display task เป็น display/MediaPlayback.cpp
- แก้ header guard ของ Network.hpp ให้ครอบ public API ทั้งไฟล์
- ลบ VoiceControl.cpp ที่ว่าง; voice inference ยังอยู่ใน audio/SoundManager.cpp
- ปรับคอมเมนต์ให้บอกหน้าที่ปัจจุบันและแก้เอกสารให้ตรงกับ source/API

รักษาลำดับเริ่มระบบและ logic การทำงานเดิมไว้ รวมถึงงานแก้ไขที่มีอยู่ก่อนใน workspace ดู [ข้อจำกัดที่ยังเหลือ](CODE_REVIEW.md)
