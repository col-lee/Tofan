"""Run production peer manager/protocol against a deterministic radio and NVS."""
from pathlib import Path
import re,subprocess
root=Path(__file__).resolve().parents[1];out=root/'.pio/espnow-test';out.mkdir(exist_ok=True)
stub=r'''
#include <ArduinoJson.h>
#include <atomic>
#include <string>
#include <vector>
#include <deque>
#include <map>
#include <cassert>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <iostream>
class String:public std::string{public:using std::string::string;String()=default;String(const std::string&s):std::string(s){}size_t write(uint8_t c){push_back(c);return 1;}size_t write(const uint8_t*p,size_t n){append((const char*)p,n);return n;}};
size_t strlcpy(char*d,const char*s,size_t n){size_t len=strlen(s);if(n){memcpy(d,s,std::min(len,n-1));d[std::min(len,n-1)]=0;}return len;}
uint32_t clockMs=1;uint32_t millis(){return clockMs;}
using SemaphoreHandle_t=void*;void* xSemaphoreCreateMutex(){return (void*)1;}int xSemaphoreTake(void*,int){return 1;}void xSemaphoreGive(void*){}
constexpr int pdTRUE=1,portMAX_DELAY=1000;int pdMS_TO_TICKS(int x){return x;}
struct Queue {size_t capacity,item;std::deque<std::vector<uint8_t>> data;};using QueueHandle_t=Queue*;struct StaticQueue_t{};
Queue* xQueueCreateStatic(size_t n,size_t s,uint8_t*,StaticQueue_t*){return new Queue{n,s,{}};}
int xQueueSend(Queue*q,const void*p,int){if(q->data.size()>=q->capacity)return 0;q->data.emplace_back((const uint8_t*)p,(const uint8_t*)p+q->item);return 1;}
int xQueueReceive(Queue*q,void*p,int){if(q->data.empty())return 0;memcpy(p,q->data.front().data(),q->item);q->data.pop_front();return 1;}void xQueueReset(Queue*q){q->data.clear();}
namespace memory{void* allocate(size_t n){return malloc(n);}void* zeroAllocate(size_t n,size_t s){return calloc(n,s);}ArduinoJson::Allocator* jsonAllocator(){return ArduinoJson::detail::DefaultAllocator::instance();}}
String persisted;bool failSave=false;
struct Preferences{bool begin(const char*,bool){return true;}void end(){}String getString(const char*,const char*){return persisted;}size_t putString(const char*,const String&s){if(failSave)return 0;persisted=s;return s.size();}};
constexpr int WIFI_OFF=0,WIFI_STA=1,WIFI_AP=2,WIFI_AP_STA=3,WIFI_IF_STA=0,ESP_MAC_WIFI_STA=0,ESP_OK=0;
using wifi_mode_t=int;using wifi_second_chan_t=int;using esp_err_t=int;using esp_now_send_status_t=int;constexpr int ESP_NOW_SEND_SUCCESS=0;
struct {int modeValue=WIFI_STA;int getMode(){return modeValue;}void mode(int x){modeValue=x;}} WiFi;
uint8_t localMac[6]={2,0,0,0,0,1};void esp_read_mac(uint8_t*m,int){memcpy(m,localMac,6);}void esp_wifi_get_channel(uint8_t*c,int*s){*c=6;*s=0;}
const char* esp_err_to_name(int){return "mock failure";}
struct esp_now_peer_info_t{uint8_t peer_addr[6];int channel,ifidx;bool encrypt;};
using Rx=void(*)(const uint8_t*,const uint8_t*,int);using Tx=void(*)(const uint8_t*,int);Rx rxCallback=nullptr;Tx txCallback=nullptr;
std::vector<esp_now_peer_info_t> radioPeers;std::vector<uint8_t> lastWire;uint8_t lastMac[6];bool failSend=false;
int esp_now_init(){return 0;}void esp_now_deinit(){radioPeers.clear();}void esp_now_unregister_recv_cb(){rxCallback=nullptr;}void esp_now_unregister_send_cb(){txCallback=nullptr;}
int esp_now_register_recv_cb(Rx r){rxCallback=r;return 0;}int esp_now_register_send_cb(Tx t){txCallback=t;return 0;}int esp_now_add_peer(esp_now_peer_info_t*p){radioPeers.push_back(*p);return 0;}
int esp_now_send(const uint8_t*m,const uint8_t*p,size_t n){memcpy(lastMac,m,6);lastWire.assign(p,p+n);return failSend?1:0;}
'''
header=(root/'src/network/EspNowManager.hpp').read_text(encoding='utf8')
source=(root/'src/network/EspNowManager.cpp').read_text(encoding='utf8')
strip=lambda s:re.sub(r'^#(?:include|pragma).*$', '',s,flags=re.M)
code=stub+'\n#include "'+(root/'src/network/EspNowProtocol.hpp').as_posix()+'"\n'+strip(header)+strip(source)+r'''
JsonDocument status(){JsonDocument d;assert(!deserializeJson(d,espnow::status()));return d;}
int received=0;void onPacket(const uint8_t*,const uint8_t*p,size_t n){assert(n==3&&p[0]==0&&p[1]==255);++received;}
int main(){
 uint8_t m[6],wire[210],data[200]{};assert(espnow::mac("02:11:22:33:44:55",m));assert(!espnow::mac("FF:FF:FF:FF:FF:FF",m));assert(!espnow::mac("00:00:00:00:00:00",m));assert(!espnow::mac("02:GG:00:00:00:00",m));
 auto n=espnow::encode(wire,espnow::Data,0x12345678,data,200);assert(n==210);espnow::Type type;uint32_t seq;size_t size;assert(espnow::decode(wire,n,type,seq,size)&&size==200&&seq==0x12345678);assert(!espnow::decode(wire,n-1,type,seq,size));assert(!espnow::encode(wire,espnow::Data,1,data,201));wire[2]=2;assert(!espnow::decode(wire,n,type,seq,size));assert(espnow::online(true,0xfffffff0u,20)&&!espnow::online(true,1,15001));
 espnow::begin();assert(!status()["enabled"]);
 String config=R"({"enabled":true,"peers":[{"name":"Lamp","mac":"02:11:22:33:44:55","enabled":true}]})";
 assert(espnow::submitConfig(config));espnow::service();assert(status()["ready"]&&radioPeers.size()==1&&radioPeers[0].channel==0);assert(persisted==config);assert(!status()["peers"][0]["online"]);
 espnow::mac("02:11:22:33:44:55",m);uint8_t payload[]={0,255,42};assert(espnow::sendPacket(m,payload,3));espnow::service();assert(espnow::decode(lastWire.data(),lastWire.size(),type,seq,size)&&type==espnow::Data&&size==3);
 txCallback(m,0);espnow::service();assert(status()["peers"][0]["tx"]==1&&!status()["peers"][0]["online"]);
 espnow::setReceiver(onPacket);n=espnow::encode(wire,espnow::Data,8,payload,3);rxCallback(m,wire,n);espnow::service();assert(received==1&&status()["peers"][0]["online"]&&status()["peers"][0]["payloadHex"]=="00FF2A");
 uint8_t unknown[6]={2,1,1,1,1,1};rxCallback(unknown,wire,n);wire[2]=9;rxCallback(m,wire,n);espnow::service();assert(received==1);
 clockMs=16002;espnow::service();assert(!status()["peers"][0]["online"]);txCallback(m,1);espnow::service();assert(status()["peers"][0]["failed"]==1);
 failSave=true;assert(espnow::submitConfig(R"({"enabled":false,"peers":[]})"));espnow::service();assert(status()["enabled"]&&status()["peers"].size()==1);failSave=false;
 assert(espnow::submitConfig(R"({"enabled":true,"peers":[{"name":"self","mac":"02:00:00:00:00:01","enabled":true}]})"));espnow::service();assert(status()["peers"][0]["name"]=="Lamp");
 espnow::beforeWifiChange();assert(!status()["ready"]);espnow::service();assert(status()["ready"]&&!status()["peers"][0]["online"]);
 assert(espnow::submitConfig(R"({"enabled":false,"peers":[]})"));espnow::service();assert(!status()["ready"]&&status()["peers"].size()==0&&radioPeers.empty());
 std::cout<<"PASS: MAC/protocol bounds, persistent CRUD, send/receive, unknown peers, malformed frames, radio ACK vs online, timeout and Wi-Fi restart\n";
}
'''
(out/'main.cpp').write_text(code,encoding='utf8')
subprocess.run(['g++','-std=c++17','-I'+str(root/'.pio/libdeps/esp32-s3-devkitc-1-n16r8v/ArduinoJson/src'),str(out/'main.cpp'),'-o',str(out/'test.exe')],check=True)
subprocess.run([str(out/'test.exe')],check=True)
