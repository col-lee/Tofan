// ESP32 Arduino 2.x / ESP-IDF 4.4. Configure the master's STA MAC and Wi-Fi channel.
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include "EspNowProtocol.hpp"
uint8_t masterMac[6]={0x02,0x00,0x00,0x00,0x00,0x01}; // replace from signed-in ToFan web page
constexpr uint8_t channel=6; // match channel shown by ToFan
struct Received {uint16_t length;uint8_t wire[espnow::Header+espnow::MaxPayload];};
QueueHandle_t received;
uint32_t sequence=0;
void onReceive(const uint8_t* mac,const uint8_t* data,int length){
 if(memcmp(mac,masterMac,6)||length<int(espnow::Header)||length>int(sizeof(Received::wire)))return;
 Received packet{};packet.length=length;memcpy(packet.wire,data,length);xQueueSend(received,&packet,0);
}
bool sendDataToMaster(const uint8_t* payload,size_t length){
 uint8_t wire[espnow::Header+espnow::MaxPayload];size_t n=espnow::encode(wire,espnow::Data,++sequence,payload,length);
 return n&&esp_now_send(masterMac,wire,n)==ESP_OK;
}
void setup(){
 Serial.begin(115200);WiFi.mode(WIFI_STA);WiFi.setSleep(false);
 esp_wifi_set_channel(channel,WIFI_SECOND_CHAN_NONE);
 Serial.printf("Slave STA MAC: %s | channel %u\n",WiFi.macAddress().c_str(),channel);
 received=xQueueCreate(4,sizeof(Received));if(!received||esp_now_init()!=ESP_OK){Serial.println("ESP-NOW init failed");return;}
 esp_now_peer_info_t peer{};memcpy(peer.peer_addr,masterMac,6);peer.channel=0;peer.ifidx=WIFI_IF_STA;peer.encrypt=false;
 if(esp_now_add_peer(&peer)!=ESP_OK){Serial.println("Add master failed");return;}esp_now_register_recv_cb(onReceive);
}
void loop(){
 Received packet;if(received&&xQueueReceive(received,&packet,pdMS_TO_TICKS(20))==pdTRUE){
  espnow::Type type;uint32_t seq;size_t length;
  if(!espnow::decode(packet.wire,packet.length,type,seq,length))return;
  if(type==espnow::Ping){uint8_t pong[espnow::Header];size_t n=espnow::encode(pong,espnow::Pong,seq,nullptr,0);esp_now_send(masterMac,pong,n);}
  else if(type==espnow::Data){
   Serial.printf("Data #%u (%u bytes): ",seq,unsigned(length));for(size_t i=0;i<length;++i)Serial.printf("%02X",packet.wire[espnow::Header+i]);Serial.println();
   // Demo: echo the payload back as Data. Replace this with your application.
   sendDataToMaster(packet.wire+espnow::Header,length);
  }
 }else delay(1);
}
