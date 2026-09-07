# ข้อจำกัดที่พบจากซอร์สปัจจุบัน

รายการนี้เป็นการตรวจโค้ด ไม่ใช่ผลทดสอบฮาร์ดแวร์ และยังไม่ได้แก้ปัญหาทั้งหมดในงานจัดโครงสร้าง

| ประเด็น | หลักฐานและผลกระทบ |
| --- | --- |
| Queue ownership | Commands.hpp มี String ใน payload แต่ FreeRTOS queue คัดลอก byte อาจเกิด dangling pointer หรือ double free |
| Startup failure | Application.cpp ออกจาก helper เมื่อ mutex/queue/task สร้างไม่สำเร็จ แต่ begin() ยังทำขั้นถัดไป |
| Shared state | UI, recording และ AI state ถูกอ่าน/เขียนหลาย task; volatile ไม่ใช่ synchronization |
| SD access | FileManager ใช้ sdSemaphore หลายจุด แต่ AIConversation เปิดและส่งไฟล์โดยไม่ล็อก mutex นี้ |
| AI authentication/TLS | AI routes ไม่ตรวจ login token และ secure client ไม่ได้ตั้ง CA |
| Backend fields | model/user/password ถูกเก็บแต่ไม่ส่งหรือใช้ใน HTTP request |
| Path validation | FileManager ตรวจ startsWith("/main") ซึ่งยังไม่ใช่ canonical path validation หรือการบังคับ directory boundary |
| Reproducible build | โมเดล inference อยู่ใน local dependency cache แต่ไม่มีแหล่งติดตั้งใน platformio.ini |
| Test coverage | test_main.cpp ตรวจเลขบวกและ boolean เท่านั้น |
| Audio EOF | callback ตั้ง autoPlayNext แต่ยังไม่มี logic อ่านแฟล็กเพื่อเล่นเพลงถัดไป |

การแยกโมดูลช่วยให้หาตำแหน่งแก้ได้ง่ายขึ้น แต่ไม่ถือว่าประเด็นเหล่านี้ผ่านการแก้หรือรับรองแล้ว
