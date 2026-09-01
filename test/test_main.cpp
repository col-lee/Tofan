#include <Arduino.h>
#include <unity.h>

// ฟังก์ชันจำลองที่ต้องการเทสต์
void test_led_pin_number(void) {
    TEST_ASSERT_EQUAL(2, 2); // ตัวอย่างเช็กว่า 2 เท่ากับ 2 ไหม
}

void setup() {
    delay(2000); // รอให้บอร์ดพร้อมทำงาน
    UNITY_BEGIN(); // เริ่มต้นระบบ Test
    RUN_TEST(test_led_pin_number); // สั่งรันเทสต์ฟังก์ชัน
    UNITY_END(); // จบการ Test
}

void loop() {
    // ไม่ต้องใส่อะไรใน loop สำหรับงาน Test
}