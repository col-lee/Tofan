#include <Arduino.h>
#include <unity.h>

void test_addition()
{
    int result = 2 + 3;

    TEST_ASSERT_EQUAL(5, result);
}

void test_boolean()
{
    bool wifiEnabled = true;

    TEST_ASSERT_TRUE(wifiEnabled);
}

void setup()
{
    delay(2000);

    UNITY_BEGIN();

    RUN_TEST(test_addition);
    RUN_TEST(test_boolean);

    UNITY_END();
}

void loop()
{
}