#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <vector>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

struct FoodCategory { int id; String name; };
struct FoodItem { int id, category; String name; };
struct FoodView { std::vector<FoodCategory> categories; String result; int category=0,count=0; bool spinning=false; };
class FoodStore {
    SemaphoreHandle_t mutex=nullptr;
    std::vector<FoodCategory> categories;
    std::vector<FoodItem> items;
    int nextId=1,selectedCategory=0,lastItem=0;
    String result;
    uint32_t spinStarted=0;
    bool spinActive=false;
    void encode(JsonDocument& doc) const;
    bool save();
    void choose(uint32_t random);
public:
    void begin();
    String snapshot();
    String change(JsonDocument& request);
    FoodView view();
    void roll(int categoryIndex,bool draw);
};
extern FoodStore foodStore;
