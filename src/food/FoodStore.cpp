#include "FoodStore.hpp"
#include <Preferences.h>
#include <algorithm>
FoodStore foodStore;
namespace {
struct Lock { SemaphoreHandle_t m; bool ok; Lock(SemaphoreHandle_t m):m(m),ok(m&&xSemaphoreTake(m,pdMS_TO_TICKS(1500))==pdTRUE){} ~Lock(){if(ok)xSemaphoreGive(m);} };
bool validName(const String& name,size_t limit) {
    if(name.isEmpty()||name.length()>limit)return false;
    for(size_t i=0;i<name.length();++i)if(static_cast<uint8_t>(name[i])<32)return false;
    return true;
}
}
void FoodStore::encode(JsonDocument& d) const {
    d["nextId"]=nextId;
    auto cats=d["categories"].to<JsonArray>();for(const auto& c:categories){auto o=cats.add<JsonObject>();o["id"]=c.id;o["name"]=c.name;}
    auto rows=d["items"].to<JsonArray>();for(const auto& f:items){auto o=rows.add<JsonObject>();o["id"]=f.id;o["category"]=f.category;o["name"]=f.name;}
}
bool FoodStore::save() {
    JsonDocument d;encode(d);String data;serializeJson(d,data);
    if(data.length()>6000)return false;
    Preferences p;if(!p.begin("tofan-foods",false))return false;
    bool ok=p.putString("catalog",data)==data.length();p.end();return ok;
}
void FoodStore::begin() {
    mutex=xSemaphoreCreateMutex();
    categories={{1,"อาหารจานเดียว"},{2,"เส้น"},{3,"ของหวาน"},{4,"เครื่องดื่ม"}};
    items={{5,1,"ข้าวกะเพรา"},{6,1,"ข้าวผัด"},{7,1,"ข้าวมันไก่"},{8,2,"ผัดไทย"},{9,2,"ก๋วยเตี๋ยว"},{10,3,"ไอศกรีม"},{11,4,"ชาไทย"}};nextId=12;
    Preferences p;if(!p.begin("tofan-foods",true))return;String data=p.getString("catalog","");p.end();
    JsonDocument d;if(data.length()>6000||deserializeJson(d,data)||!d["categories"].is<JsonArray>()||!d["items"].is<JsonArray>())return;
    auto cats=d["categories"].as<JsonArray>(),rows=d["items"].as<JsonArray>();if(cats.size()>12||rows.size()>60)return;
    std::vector<FoodCategory> loadedCats;std::vector<FoodItem> loadedItems;int maxId=0;
    for(auto c:cats){int id=c["id"]|0;String name=c["name"]|"";if(id<=0||!validName(name,60))return;
        for(auto old:loadedCats)if(old.id==id)return;loadedCats.push_back({id,name});maxId=std::max(maxId,id);}
    for(auto f:rows){int id=f["id"]|0,cat=f["category"]|0;String name=f["name"]|"";bool found=false;
        for(auto c:loadedCats){if(c.id==cat)found=true;if(c.id==id)return;}for(auto old:loadedItems)if(old.id==id)return;
        if(id<=0||!found||!validName(name,96))return;loadedItems.push_back({id,cat,name});maxId=std::max(maxId,id);}
    if(maxId>=1000000)return;categories=loadedCats;items=loadedItems;nextId=std::max(maxId+1,d["nextId"]|1);if(nextId>1000000)nextId=maxId+1;
}
void FoodStore::choose(uint32_t random) {
    std::vector<const FoodItem*> candidates;
    for(const auto& f:items)if(!selectedCategory||f.category==selectedCategory)candidates.push_back(&f);
    const bool animate=candidates.size()>1;
    if(candidates.size()>1)candidates.erase(std::remove_if(candidates.begin(),candidates.end(),[&](const FoodItem* f){return f->id==lastItem;}),candidates.end());
    if(candidates.empty()){result="";lastItem=0;spinActive=false;return;}
    const auto* picked=candidates[random%candidates.size()];result=picked->name;lastItem=picked->id;
    spinStarted=millis();spinActive=animate;
}
String FoodStore::snapshot() {
    Lock lock(mutex);if(!lock.ok)return "{}";JsonDocument d;encode(d);d["result"]=result;d["category"]=selectedCategory;String out;serializeJson(d,out);return out;
}
FoodView FoodStore::view() {
    Lock lock(mutex);FoodView v;if(!lock.ok)return v;v.categories=categories;v.result=result;v.category=selectedCategory;
    for(const auto& f:items)if(!selectedCategory||f.category==selectedCategory)++v.count;
    uint32_t elapsed=millis()-spinStarted;int step=0;
    while(step<16 && elapsed>=static_cast<uint32_t>(40+step*14)){elapsed-=40+step*14;++step;}
    v.spinning=spinActive&&step<16&&v.count>1;
    if(v.spinning){int index=step%v.count;for(const auto& f:items)if(!selectedCategory||f.category==selectedCategory){if(index--==0){v.result=f.name;break;}}}
    return v;
}
void FoodStore::roll(int index,bool draw) {
    Lock lock(mutex);if(!lock.ok)return;int category=index>0&&index<=static_cast<int>(categories.size())?categories[index-1].id:0;
    if(category!=selectedCategory){selectedCategory=category;result="";lastItem=0;spinActive=false;}
    if(draw && !(spinActive&&millis()-spinStarted<2320))choose(esp_random());
}
String FoodStore::change(JsonDocument& d) {
    Lock lock(mutex);if(!lock.ok)return "เครื่องกำลังทำงาน กรุณาลองใหม่";
    String action=d["action"]|"",name=d["name"]|"";name.trim();int id=d["id"]|0,cat=d["category"]|0;
    if(action=="draw"){
        bool found=cat==0;for(auto c:categories)if(c.id==cat)found=true;if(!found)return "ไม่พบหมวดหมู่";
        if(selectedCategory!=cat)lastItem=0;selectedCategory=cat;choose(esp_random());return "";
    }
    auto oldCats=categories;auto oldItems=items;int oldNext=nextId;
    if(nextId>=1000000)return "จำนวนรายการเกินขีดจำกัด";
    if(action=="category"){
        if(!validName(name,60))return "ชื่อหมวดหมู่ต้องยาว 1–60 ไบต์";
        for(auto c:categories)if(c.name==name&&c.id!=id)return "มีหมวดหมู่นี้แล้ว";
        if(id){auto it=std::find_if(categories.begin(),categories.end(),[&](const FoodCategory& c){return c.id==id;});if(it==categories.end())return "ไม่พบหมวดหมู่";it->name=name;}
        else{if(categories.size()>=12)return "เพิ่มได้สูงสุด 12 หมวดหมู่";categories.push_back({nextId++,name});}
    }else if(action=="item"){
        if(!validName(name,96))return "ชื่อเมนูต้องยาว 1–96 ไบต์";
        if(std::none_of(categories.begin(),categories.end(),[&](const FoodCategory& c){return c.id==cat;}))return "เลือกหมวดหมู่ก่อน";
        for(auto f:items)if(f.name==name&&f.category==cat&&f.id!=id)return "มีเมนูนี้ในหมวดหมู่แล้ว";
        if(id){auto it=std::find_if(items.begin(),items.end(),[&](const FoodItem& f){return f.id==id;});if(it==items.end())return "ไม่พบเมนู";it->name=name;it->category=cat;}
        else{if(items.size()>=60)return "เพิ่มได้สูงสุด 60 เมนู";items.push_back({nextId++,cat,name});}
    }else if(action=="deleteItem"){
        auto it=std::find_if(items.begin(),items.end(),[&](const FoodItem& f){return f.id==id;});if(it==items.end())return "ไม่พบเมนู";items.erase(it);
    }else if(action=="deleteCategory"){
        if(std::any_of(items.begin(),items.end(),[&](const FoodItem& f){return f.category==id;}))return "ย้ายหรือลบเมนูในหมวดหมู่ก่อน";
        auto it=std::find_if(categories.begin(),categories.end(),[&](const FoodCategory& c){return c.id==id;});if(it==categories.end())return "ไม่พบหมวดหมู่";categories.erase(it);
    }else return "คำสั่งไม่ถูกต้อง";
    if(!save()){categories=oldCats;items=oldItems;nextId=oldNext;return "บันทึกไม่สำเร็จ พื้นที่ตั้งค่าอาจเต็ม";}
    result="";lastItem=0;spinActive=false;if(std::none_of(categories.begin(),categories.end(),[&](const FoodCategory& c){return c.id==selectedCategory;}))selectedCategory=0;
    return "";
}
