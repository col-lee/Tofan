"""Run the production food store against in-memory NVS and mutex adapters."""
from pathlib import Path
import re, subprocess
root=Path(__file__).resolve().parents[1]
out=root/'.pio/food-test';out.mkdir(exist_ok=True)
stub=r'''
#include <string>
#include <cstdint>
#include <cassert>
#include <iostream>
class String:public std::string { public: using std::string::string;String(const std::string& s):std::string(s){}String()=default;bool isEmpty()const{return empty();}void trim(){auto a=find_first_not_of(" \t\r\n"),b=find_last_not_of(" \t\r\n");*this=a==npos?"":substr(a,b-a+1);}size_t write(uint8_t c){push_back(c);return 1;}size_t write(const uint8_t* s,size_t n){append((const char*)s,n);return n;} };
using SemaphoreHandle_t=void*;
constexpr int pdTRUE=1;
int pdMS_TO_TICKS(int n){return n;}void* xSemaphoreCreateMutex(){return (void*)1;}int xSemaphoreTake(void*,int){return 1;}void xSemaphoreGive(void*){}
uint32_t esp_random(){static uint32_t seed=1;return seed++;}
uint32_t now=0;uint32_t millis(){return now;}
String saved;bool failSave=false;
class Preferences {public:bool begin(const char*,bool){return true;}void end(){}String getString(const char*,const char*){return saved;}size_t putString(const char*,const String& s){if(failSave)return 0;saved=s;return s.size();}};
'''
strip=lambda s:re.sub(r'^#(?:include ["<](?:Arduino.h|Preferences.h|freertos/[^>]*|FoodStore.hpp)[">]|pragma once).*$', '', s,flags=re.M)
source=stub+strip((root/'src/food/FoodStore.hpp').read_text(encoding='utf8'))+strip((root/'src/food/FoodStore.cpp').read_text(encoding='utf8'))+r'''
String update(FoodStore& s,const char* json){JsonDocument d;assert(!deserializeJson(d,json));return s.change(d);}
int main(){
 FoodStore s;s.begin();assert(s.view().count==7);
 s.roll(1,true);assert(s.view().spinning);auto preview=s.view().result;now=40;assert(s.view().result!=preview);now=2320;assert(!s.view().spinning);auto first=s.view().result;s.roll(1,true);now+=2320;assert(s.view().result!=first);
 assert(update(s,R"({"action":"item","name":"Soup","category":999})").length());
 assert(update(s,R"({"action":"deleteCategory","id":1})").length());
 assert(update(s,R"({"action":"category","name":"Lunch"})").empty());
 assert(update(s,R"({"action":"item","name":"Soup","category":12})").empty());
 assert(update(s,R"({"action":"item","name":"Soup","category":12})").length());
 FoodStore restarted;restarted.begin();assert(restarted.view().count==8);
 assert(update(restarted,R"({"action":"draw","category":12})").empty());assert(restarted.view().result=="Soup");
 assert(update(restarted,R"({"action":"item","id":13,"name":"Noodles","category":2})").empty());
 assert(update(restarted,R"({"action":"draw","category":12})").empty());assert(restarted.view().result.empty());
 auto before=restarted.snapshot();failSave=true;assert(update(restarted,R"({"action":"deleteItem","id":13})").length());assert(restarted.snapshot()==before);failSave=false;
 assert(update(restarted,R"({"action":"deleteItem","id":13})").empty());
 assert(update(restarted,R"({"action":"deleteCategory","id":12})").empty());
 assert(update(restarted,R"({"action":"category","name":"  "})").length());
 assert(update(restarted,R"({"action":"category","name":"bad\nname"})").length());
 FoodStore persisted;persisted.begin();assert(persisted.view().count==7);
 std::cout<<"Food store: persistence, random, CRUD, validation and failed-save rollback passed\n";
}
'''
(out/'main.cpp').write_text(source,encoding='utf8')
subprocess.run(['g++','-std=c++17','-I'+str(root/'.pio/libdeps/esp32-s3-devkitc-1-n16r8v/ArduinoJson/src'),str(out/'main.cpp'),'-o',str(out/'test.exe')],check=True)
subprocess.run([str(out/'test.exe')],check=True)
