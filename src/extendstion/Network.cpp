#include "Network.hpp"
#include "FileManager.hpp"
#include <stdlib.h>
#include <string.h>

DNSServer dnsServer;
AsyncWebServer server(80);
AsyncWebSocket websocket("/ws");
Preferences prefs; // instance Preferences for Save ssid and password
JsonArray jsonAr;
JsonObject jObj;
QueueHandle_t fileMg;

bool isNetwork_install;

unsigned long ota_progress_millis = 0;

typedef struct
{
  char ssid[32] = "";
  char password[32] = "";
} PrefsObj_WiFiManager; // object for get ssid and password

typedef struct
{
  char username[32];
  char password[32];
} Username;

PrefsObj_WiFiManager prefs_Obj;
Username username_obj;
String jsonssid;
char token[64]; 

struct UploadState {
  File file;
  bool inProgress;
  size_t totalBytes;
  unsigned long lastUpdate;
  String filename;
};

UploadState uploadState = {File(), false, 0, 0, ""};

void onOTAStart() {
  // Log when OTA has started
  Serial.println("OTA update started!");
  // <Add your own code here>
}

void onOTAProgress(size_t current, size_t final) {
  // Log every 1 second
  if (millis() - ota_progress_millis > 1000) {
    ota_progress_millis = millis();
    Serial.printf("OTA Progress Current: %u bytes, Final: %u bytes\n", current, final);
  }
}

void onOTAEnd(bool success) {
  // Log when OTA has finished
  if (success) {
    Serial.println("OTA update finished successfully!");
  } else {
    Serial.println("There was an error during OTA update!");
  }
  // <Add your own code here>
}

bool NetworkManager::writePrefs()
{
  if(prefs.begin("WiFiConfig", false)) {
    prefs.putString("ssid", prefs_Obj.ssid);
    prefs.putString("password", prefs_Obj.password);
    prefs.end();
    Serial.println("Save ssid and password to Memory successfull.");
    return true;
  }
  return false;
}

bool NetworkManager::readPrefs()
{
  if(prefs.begin("WiFiConfig", true)) {
    prefs.getString("ssid", prefs_Obj.ssid, sizeof(prefs_Obj.ssid));
    prefs.getString("password", prefs_Obj.password, sizeof(prefs_Obj.password));
    prefs.end();
    Serial.println("read data from Preferences successfull.");
    return true;
  }
  return false;
}

bool NetworkManager::clearPref() {
  if(prefs.begin("WiFiConfig", false)) {
    prefs.clear();
    prefs.end();
    return true;
  }
  return false;
}

bool NetworkManager::writeUsername() {
   if(prefs.begin("UsernameConfig", false)) {
    prefs.putString("usesrname", username_obj.username);
    prefs.putString("password", username_obj.password);
    prefs.end();
    Serial.println("Save ssid and password to Memory successfull.");
    return true;
  }
  return false;
}

bool NetworkManager::readUsername() {
  if(prefs.begin("UsernameConfig", true)) {
    prefs.getString("usesrname", username_obj.username, sizeof(username_obj.username));
    prefs.getString("password", username_obj.password, sizeof(username_obj.password));
    prefs.end();
    Serial.println("read data from Preferences successfull.");
    return true;
  }
  return false;
}

bool NetworkManager::clearUsername() {
  if(prefs.begin("UsernameConfig", false)) {
    prefs.clear();
    prefs.end();
    return true;
  }
  return false;
}

void NetworkManager::generateToken(char* token, int length) {
  // ชุดตัวอักษรที่จะใช้สุ่ม (ตัดตัวที่สับสนง่ายออก เช่น l, 1, O, 0 ถ้าต้องการ)
  const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
  int charsetSize = sizeof(charset) - 1;

  for (int i = 0; i < length; i++) {
    // esp_random() คือ Hardware RNG ของ ESP32 (ได้ค่า uint32_t)
    // เอามา modulo (%) กับความยาว charset เพื่อเลือกตัวอักษร
    size_t index = esp_random() % charsetSize;
    token[i] = charset[index];
  }

  token[length] = '\0';
}

void NetworkManager::writeLog(String &log) {
  File file = SD.open("/system/log/log.txt", FILE_WRITE);
  if(file){
    file.println(log);
    file.close();
  }
}

bool NetworkManager::connectoWiFi(const char *ssid, const char *password)
{

  if(ssid == "" && password == "" && ssid == NULL && password == NULL) {
    Serial.println("ssid or password is invalid or null.");
    return false;
  }

  unsigned long preTime = 0;
  unsigned long interval = 1000;
  int seconds = 0;

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to wifi.");
  while (WiFi.status() != WL_CONNECTED)
  {
    unsigned long currentTime = millis();
    if (seconds >= 5) {
      Serial.println("");
      Serial.println("WiFi connect Time out.");
      if (xSemaphoreTake(displaySemaphore, pdMS_TO_TICKS(50)) == pdTRUE)
      {
        tft.println("WiFi connect Time out.");
        xSemaphoreGive(displaySemaphore);
      }
      WiFi.disconnect();
      vTaskDelay(pdMS_TO_TICKS(100));
      return false;
    }
      
    if ((currentTime - preTime) >= interval)
    {
      preTime = currentTime;
      seconds++;
      Serial.print(".");
      if (xSemaphoreTake(displaySemaphore, pdMS_TO_TICKS(50)) == pdTRUE)
      {
        tft.print(".");
        xSemaphoreGive(displaySemaphore);
      }
    }
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("");
    Serial.println("WIFI Connected.");
    Serial.println("Ready.");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    Serial.print("speed: ");
    Serial.println(WiFi.RSSI());

    if(xSemaphoreTake(displaySemaphore ,pdMS_TO_TICKS(200)) == pdTRUE) {
      tft.println("");
      tft.println("WIFI Connected.");
      tft.println("Ready.");
      tft.print("IP: ");
      tft.println(WiFi.localIP());
      tft.print("speed: ");
      tft.println(WiFi.RSSI());
      xSemaphoreGive(displaySemaphore);
    }

    if(MDNS.begin("terngai")) {
      Serial.println("mDNS responder started");
      MDNS.addService("http", "tcp", 80);
    } else {
      Serial.println("Error setting up MDNS responder!");
    }
    return true;
  }
  else {
    Serial.println("WiFi connect Time out.");
    WiFi.disconnect();
    dnsServer.stop();
    return false;
  }

  return false;
}

void NetworkManager::handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  DISPLAY_COMMAND dcmd;
  AUDIO_COMMAND acmd;
  JsonDocument docs;
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0; // Null-terminate
    String message = (char*)data;
    DeserializationError error = deserializeJson(docs, message);
    if(error) {
      Serial.println("JSON parse error.");
      return;
    }
    int module = docs[String("module")];
    int state = docs[String("state")];
    String path = docs["filepath"];

    Serial.println("Received: " + String(module) +  String(state));
    
    switch (module) {
    // ตรวจสอบว่าคุณตั้ง case เป็นอะไร (ปกติจะเป็น case 0: หรือ case DISPLAY_COMMAND::DIS:)
    case DISPLAY_COMMAND::DIS: 
    {
      dcmd.module = DISPLAY_COMMAND::MODULE::DIS;
      
      // ดึงค่า filepath ของรูปภาพ
      if (docs["filepath"].is<String>()) {
          dcmd.path = docs["filepath"].as<String>();
      } else {
          dcmd.path = "";
      }

      if(state == DISPLAY_COMMAND::DISPLAY_STATE::SHOW) dcmd.display_state = DISPLAY_COMMAND::DISPLAY_STATE::SHOW;
      if(state == DISPLAY_COMMAND::DISPLAY_STATE::CLEAR) dcmd.display_state = DISPLAY_COMMAND::DISPLAY_STATE::CLEAR;

      if(xQueueSend(display_command, &dcmd, 0) == pdPASS) {
        Serial.println("Display command sent.");
      }
      vTaskDelay(pdMS_TO_TICKS(100));
      break;
    }
    case AUDIO_COMMAND::AUDIO:
      acmd.module = AUDIO_COMMAND::MODULE::AUDIO;
      
      // ✅ 1. เปลี่ยนการเช็ค key เป็นแบบ v7
      if (docs["filepath"].is<String>()) {
          acmd.path = docs["filepath"].as<String>();
      } else {
          acmd.path = "";
      }

      if(state == AUDIO_COMMAND::AUDIO_STATE::PLAY) acmd.audio_state = AUDIO_COMMAND::AUDIO_STATE::PLAY;
      if(state == AUDIO_COMMAND::AUDIO_STATE::PUASE) acmd.audio_state = AUDIO_COMMAND::AUDIO_STATE::PUASE;
      
      // ✅ 2. เปลี่ยนการเช็ค key เป็นแบบ v7
      if(state == AUDIO_COMMAND::AUDIO_STATE::SEEK) {
          acmd.audio_state = AUDIO_COMMAND::AUDIO_STATE::SEEK;
          if (docs["seek_time"].is<int>() || docs["seek_time"].is<uint32_t>()) {
              acmd.seek_time = docs["seek_time"].as<uint32_t>();
          }
      }

      if(xQueueSend(audio_command, &acmd, 0) == pdPASS) {
        Serial.println("Audio command sent successfully.");
      }
      vTaskDelay(pdMS_TO_TICKS(100));
      break;

    case 2: // MODULE FILE
    { 
      // ดึงค่า filepath และ currentDir อย่างปลอดภัย
      String filePath = docs["filepath"].is<String>() ? docs["filepath"].as<String>() : "";
      String currentDir = docs["currentDir"].is<String>() ? docs["currentDir"].as<String>() : "/main";
      
      if(state == 3) { // 3 = SCAN
          // 🌟 1. ดึงค่า requester ว่าใครเป็นคนขอแสกน
          String requester = docs["requester"].is<String>() ? docs["requester"].as<String>() : "file_manager";
          
          String jsonList = file_card.getFileListJSON(filePath);
          
          // 🌟 2. แกะ JSON เพื่อเปลี่ยน type ให้ตรงกับที่หน้าเว็บรอ
          JsonDocument doc;
          deserializeJson(doc, jsonList);
          
          if (requester == "image_picker") {
              doc["type"] = "image_picker_list"; // ส่งให้ Popup เลือกรูป
          } else {
              doc["type"] = "file_list"; // ส่งให้ File Manager หลัก
          }
          
          String output;
          serializeJson(doc, output);
          websocket.textAll(output); 
      }
      else if(state == 0) { // 0 = CREATE FILE (สร้างไฟล์)
          file_card.createFile(filePath);
          websocket.textAll(file_card.getFileListJSON(currentDir));
      }
      else if(state == 4) { // 🌟 4 = CREATE FOLDER (สร้างโฟลเดอร์) เติมกลับเข้ามาแล้ว!
          file_card.createFolder(filePath);
          websocket.textAll(file_card.getFileListJSON(currentDir));
      }
      else if(state == 1) { // 1 = RENAME (เปลี่ยนชื่อ)
          String newPath = docs["newpath"].is<String>() ? docs["newpath"].as<String>() : "";
          file_card.renameFile(filePath, newPath);
          websocket.textAll(file_card.getFileListJSON(currentDir));
      }
      else if(state == 2) { // 2 = DELETE (ลบไฟล์/โฟลเดอร์)
          file_card.deleteFile(filePath);
          websocket.textAll(file_card.getFileListJSON(currentDir));
      }
      break;
    }
    
    default:
      break;
    }
  }
}

void NetworkManager::onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  String log;
  switch (type) {
    case WS_EVT_CONNECT:
      // Serial.printf("Client %u connected\n", client->id());
      // Serial.println(client->remoteIP());
      log += "Client " + (String)client->id() + " connected " + "ip: " + (String)client->remoteIP();
      writeLog(log);
      if(xSemaphoreTake(displaySemaphore ,pdMS_TO_TICKS(100)) == pdTRUE) {

        // tft.printf("Client %u connected\n", client->id());
        // tft.println(client->remoteIP());
        xSemaphoreGive(displaySemaphore);
      }

      break;
    case WS_EVT_DISCONNECT:
      // Serial.printf("Client %u disconnected\n", client->id());
      break;
    case WS_EVT_DATA:
      handleWebSocketMessage(arg, data, len);
      // client->text("{\"volume\":\"" + audio.getVolume() + "\"}");
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

void handleScan() {
  int n = WiFi.scanNetworks(); 
  JsonDocument docj;  // scan ขณะที่อยู่ใน STA mode[web:16][web:82]
  JsonArray jarr = docj.to<JsonArray>();
  for (int i = 0; i < n; i++) {
    JsonObject jobj = jarr.add<JsonObject>();
    jobj["ssid"] = WiFi.SSID(i);
    jobj["rssi"] = WiFi.RSSI(i);
    
    wifi_auth_mode_t secure = WiFi.encryptionType(i);
    if(secure == WIFI_AUTH_OPEN) {
      jobj["secure"] =  false;
    } else {
      jobj["secure"] =  true;
    }
  }
  serializeJson(docj, jsonssid);
  WiFi.scanDelete();

  Serial.println(jsonssid);
  Serial.print("freeheap: ");
  Serial.println(ESP.getFreeHeap());
}

void NetworkManager::initWiFiManager() {
  nm.readPrefs();
  // Serial.println(prefs_Obj.ssid);
  // Serial.println(prefs_Obj.password);
  if (!connectoWiFi(prefs_Obj.ssid, prefs_Obj.password)) {
    IPAddress apIP(192,168,4,1);
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP("terngai555", "esp32-pass");
    WiFi.softAPConfig(apIP,apIP, IPAddress(255,255,255,0));
    IPAddress localIP = WiFi.softAPIP();
    Serial.printf("IP: %s \n", localIP);

    if(xSemaphoreTake(displaySemaphore, pdMS_TO_TICKS(100)) == pdTRUE){
      tft.print("IP: ");
      tft.println(localIP);
      xSemaphoreGive(displaySemaphore);
    }
    dnsServer.start(53, "terngai.local", apIP);
    handleScan();

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) { 
        request->send(SD, "/WEB_Source/WiFiManger/wifiManager.html", "text/html"); 
    });

    server.on("/scan", HTTP_GET, [](AsyncWebServerRequest *request) {
      
      AsyncWebServerResponse *response = request->beginResponse(200, "application/json", jsonssid);
      response->addHeader("Access-Control-Allow-Origin", "*");
      request->send(response);
    });

    server.on("/wifi", HTTP_OPTIONS, [](AsyncWebServerRequest *request){
      AsyncWebServerResponse* response = request->beginResponse(200);
      response->addHeader("Access-Control-Allow-Origin", "*");
      response->addHeader("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
      response->addHeader("Access-Control-Allow-Headers", "Content-Type");
      request->send(response);
    });

    server.on("/wifi", HTTP_POST,[](AsyncWebServerRequest *request){},
    NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, (const char*)data);

      if (error) {
        request->send(200, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
      }

      const char* ssid = doc["ssid"];
      const char* password = doc["password"];

      if(ssid && password && strlen(ssid) > 0 && strlen(password) > 0) {

        strncpy(prefs_Obj.ssid, ssid, sizeof(prefs_Obj.ssid) - 1);
        strncpy(prefs_Obj.password, password, sizeof(prefs_Obj.password) - 1);

          if (strlen(prefs_Obj.ssid) > 0 && strlen(prefs_Obj.password) > 0){
            if(nm.writePrefs()) {
              Serial.printf("ssid: %s password: %s", prefs_Obj.ssid, prefs_Obj.password);
              request->send(200, "text/plain", "WiFi credentials saved. Rebooting...");
              DefaultHeaders::Instance().addHeader("Connection", "close");

              xTaskCreate([](void*){
                vTaskDelay(pdMS_TO_TICKS(1000));
                ESP.restart();
              }, "reboot_task", 2048, NULL, 5, NULL);
            } else {
              Serial.println("ssid or password is valid or null.");
            }

            vTaskDelay(500);
          } else {
            request->send(500, "text/plain", "Failed to write to NVM");
          }
      } else {
        request->send(400, "text/plain", "Missing SSID or Password");
        // if(!isConnectSDcard) {
        
        // }
      }
    });
  }
}

void NetworkManager::initWebServer() {
    if(isConnectSDcard) {
    // 1. หน้าหลัก (ระบุเจาะจง)
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
        { request->send(SD, "/WEB_Source/index.html", "text/html"); });

    // 2. หน้า Upload (ระบุเจาะจง)
    server.on("/pageUploadFile", HTTP_GET, [](AsyncWebServerRequest *request)
        { request->send(SD, "/WEB_Source/uploadFile.html", "text/html"); });

    server.serveStatic("/controlPanel/", SD, "/WEB_Source/");
    }

    server.on("/api/signin", HTTP_OPTIONS, [](AsyncWebServerRequest *request) {
      AsyncWebServerResponse* response = request->beginResponse(200);
      response->addHeader("Access-Control-Allow-Origin", "*");
      response->addHeader("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
      response->addHeader("Access-Control-Allow-Headers", "Content-Type");
      request->send(response);
    });

    server.on("/api/signin", HTTP_POST, [](AsyncWebServerRequest *request) {},
    NULL,
     [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        AsyncWebServerResponse* response;
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, (const char*)data);
        const char* username = doc["u"];
        const char* password = doc["p"];
        const char* token_login = doc["token"];
        char token_[64];
        Serial.println(username);
        Serial.println(password);

        if (error)
        {
          Serial.print("JSON Error: ");
          Serial.println(error.c_str());
          response = request->beginResponse(200, "application/json", "{\"status\":false, \"msg\":\"Invalid JSON\"}");
          response->addHeader("Access-Control-Allow-Origin", "*");
          request->send(response);
          return;
        }

        if(strlen(username) > 0 && strlen(password) > 0) {
          if(nm.readUsername()) {

            Serial.printf("1 u: %s p: %s \n", username_obj.username, username_obj.password);
            Serial.printf("2 u: %s p: %s \n", username, password);

            if(strcmp(username_obj.username, username) == 0 && strcmp(username_obj.password, password) == 0){

              if(strlen(token_login) > 0) {
                char check_token[64];
                if(prefs.begin("UsernameConfig", true)) {
                  prefs.getString("auth", check_token, sizeof(check_token));
                  prefs.end();
                  Serial.println("Save auth to Memory successfull.");
                }

                if(strcmp(token_login, check_token) == 0) {
                  // snprintf(token, sizeof(token), "{\"status\":true}");
                  // Serial.printf("token_signup_token: %s\n", token);
                  response = request->beginResponse(200, "application/json", "{\"status\":true}");
                  response->addHeader("Access-Control-Allow-Origin", "*");
                  request->send(response);
                  Serial.println("send token.");
                } else {
                  response = request->beginResponse(200, "application/json", "{\"status\":false}");
                  response->addHeader("Access-Control-Allow-Origin", "*");
                  request->send(response);
                }

              } else {

                nm.generateToken(token_, 25);

                // Serial.printf("token_signup: %s\n", token_);
                if(prefs.begin("UsernameConfig", false)) {
                  prefs.putString("auth", token_);
                  prefs.end();
                  Serial.println("Save auth to Memory successfull.");
                }

                snprintf(token, sizeof(token), "{\"status\":true,\"okte\":\"%s\"}", token_);
                // Serial.printf("token_signup_token: %s\n", token);
                response = request->beginResponse(200, "application/json", token);
                response->addHeader("Access-Control-Allow-Origin", "*");
                request->send(response);
                Serial.println("send token.");
                return;
              }
              
            } else {
              response = request->beginResponse(200, "application/json", "{\"status\":false, \"msg\":\"Please enter your username and password\"}");
              response->addHeader("Access-Control-Allow-Origin", "*");
              request->send(response);
            }
          }
        } else {
          // Serial.print("JSON Error: ");
          // Serial.println(error.c_str());
          response = request->beginResponse(200, "application/json", "{\"status\":false, \"msg\":\"Please enter your username and password\"}");
          response->addHeader("Access-Control-Allow-Origin", "*");
          request->send(response);
        }
    });

    server.on("/api/signup", HTTP_OPTIONS, [](AsyncWebServerRequest *request) {
      AsyncWebServerResponse* response = request->beginResponse(200);
      response->addHeader("Access-Control-Allow-Origin", "*");
      response->addHeader("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
      response->addHeader("Access-Control-Allow-Headers", "Content-Type");
      request->send(response);
    });

    server.on("/api/signup", HTTP_POST, [](AsyncWebServerRequest *request) {},
    NULL,
     [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        AsyncWebServerResponse* response;
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, (const char*)data);
        const char* username = doc["u"];
        const char* password = doc["p"];
        char msg[64];
        Serial.println(username);
        Serial.println(password);

        if (error)
        {
          Serial.printf("JSON Error: %s\n", error.c_str());
          response = request->beginResponse(200, "application/json", "{\"status\":false, \"msg\":\"Invalid JSON\"}");
          response->addHeader("Access-Control-Allow-Origin", "*");
          request->send(response);
          return;
        }

        if(strlen(username) > 0 && strlen(password) > 0) {
          strcpy(username_obj.username, username);
          strcpy(username_obj.password, password);
          if(strlen(username_obj.username) > 0 && strlen(username_obj.password) > 0) {
            if(nm.writeUsername()) {
              snprintf(msg, sizeof(msg), "{\"status\":true,\"msg\":\"Sign up to success.\"}");
              // Serial.printf("token_signup_token: %s\n", token);
              response = request->beginResponse(200, "application/json", msg);
              response->addHeader("Access-Control-Allow-Origin", "*");
              request->send(response);
            } else {
              Serial.printf("Faild to read NVS.\n");
              response = request->beginResponse(200, "application/json", "{\"status\":false, \"msg\":\"faild to read Preference\"}");
              response->addHeader("Access-Control-Allow-Origin", "*");
              request->send(response);
              return;
            }
          }  else {
            Serial.printf("username and password is null.\n");
            response = request->beginResponse(200, "application/json", "{\"status\":false, \"msg\":\"Please enter your username and password\"}");
            response->addHeader("Access-Control-Allow-Origin", "*");
            request->send(response);
            return;
          }
        } else {
          Serial.printf("username and password is null.\n");
          response = request->beginResponse(200, "application/json", "{\"status\":false, \"msg\":\"Please enter your username and password\"}");
          response->addHeader("Access-Control-Allow-Origin", "*");
          request->send(response);
          return;
        }

    });

    server.on("/api/checkToken", HTTP_OPTIONS, [](AsyncWebServerRequest *request) {
      AsyncWebServerResponse* response = request->beginResponse(200);
      response->addHeader("Access-Control-Allow-Origin", "*");
      response->addHeader("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
      response->addHeader("Access-Control-Allow-Headers", "Content-Type");
      request->send(response);
    });

    server.on("/api/checkToken", HTTP_POST, [](AsyncWebServerRequest *request) {},
    NULL,
     [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        AsyncWebServerResponse *response;
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, (const char*)data);
        const char* token_check = doc["token"];
        // Serial.printf("token: %s\n", token_check);

        if (error)
        {
          Serial.print("JSON Error: ");
          Serial.println(error.c_str());
          response = request->beginResponse(400, "application/json", "{\"status\":false, \"msg\":\"Invalid JSON\"}");
          response->addHeader("Access-Control-Allow-Origin", "*");
          request->send(response);
          return;
        }

        if(strlen(token_check) > 0) {
          Serial.println("Checking Token.");
            if(prefs.begin("UsernameConfig", true)) {
              prefs.getString("auth", token, sizeof(token));
              Serial.println("read auth");
              // Serial.println(token);
              prefs.end();
            }

            if(strcmp(token_check, token) == 0){
              response = request->beginResponse(200, "application/json", "{\"status\":true, \"msg\":\"Log in successfully.\"}");
              response->addHeader("Access-Control-Allow-Origin", "*");
              request->send(response);
              strcpy(token, "");
              Serial.println("Check Token successful.");
            } else {
              response = request->beginResponse(200, "application/json", "{\"status\":false, \"msg\":\"Log in faild.\"}");
              response->addHeader("Access-Control-Allow-Origin", "*");
              request->send(response);
            }
        } else {
          request->beginResponse(400, "application/json", "{\"status\":false, \"msg\":\"Please log in again.\"}");
          response->addHeader("Access-Control-Allow-Origin", "*");
          request->send(response);
          return;
        }
    });

    // API GET DATA

    server.on("/api/data", HTTP_GET, [](AsyncWebServerRequest *request) {
      AsyncWebServerResponse *response = request->beginResponse(200, "application/json", "jsondata");
      response->addHeader("Access-Control-Allow-Origin", "*");
      request->send(response);
    });

    server.on("/api/data/file", HTTP_OPTIONS, [](AsyncWebServerRequest *request) {
      AsyncWebServerResponse* response = request->beginResponse(200);
      response->addHeader("Access-Control-Allow-Origin", "*");
      response->addHeader("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
      response->addHeader("Access-Control-Allow-Headers", "Content-Type");
      request->send(response);
    });

    server.on("/api/data/file", HTTP_POST, [](AsyncWebServerRequest *request) {},
    NULL,
     [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      AsyncWebServerResponse *response;
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, (const char*)data);
        String type = doc["type"];
        Serial.println(type);

        if (error)
        {
          Serial.print("JSON Error: ");
          Serial.println(error.c_str());
          request->send(400, "application/json", "{\"status\":\"error\", \"msg\":\"Invalid JSON\"}");
          return;
        }

        // xQueueSend(fileMg, &type, 0);

        response = request->beginResponse(200, "application/json", "jsondata");
        response->addHeader("Access-Control-Allow-Origin", "*");
        request->send(response);

    });

  server.on("/uploadfile", HTTP_POST, [](AsyncWebServerRequest *request) {

    if (uploadState.inProgress) {
        if (uploadState.file) {
          uploadState.file.close();
        }
        uploadState.inProgress = false;
        // xSemaphoreGive(sdSemaphore);
    }
    
    request->send(200, "text/plain", "successfull.");
    Serial.printf("✓ Upload finished: %s (%d bytes)\n",
                  uploadState.filename.c_str(),
                  uploadState.totalBytes);
    Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
  }, [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
    // esp_task_wdt_reset();
    // bool controlRes = false;

    if (!index) {

      Serial.printf("Upload start %s\n", filename.c_str());
      Serial.printf("Heap before upload %d byte \n", ESP.getFreeHeap());

      if (xSemaphoreTake(sdSemaphore, pdMS_TO_TICKS(2000)) != pdTRUE) {
          Serial.println("✗ Failed to get SD semaphore");
          request->send(500, "text/plain", "SD busy");
          return;
      }

      String filePath;
      if (filename.endsWith(".jpg") || filename.endsWith(".jpeg") || 
          filename.endsWith(".png") || filename.endsWith(".gif")) {
        filePath = "/main/Pictures/" + filename;
      } else if (filename.endsWith(".mp3") || filename.endsWith(".wav")) {
        filePath = "/main/Musics/" + filename;
      } else if(filename.equals("wifiManager.html")) {
        filePath = "/WEB_Source/WiFiManger/" + filename;
      } else if (filename.endsWith(".html") || filename.endsWith(".js") || 
                  filename.endsWith(".css")) {
        filePath = "/WEB_Source/" + filename;
      } else {
        filePath = "/main/" + filename;
      }
        
      // Delete existing file
      if (SD.exists(filePath)) {
        SD.remove(filePath);
        delay(10);
      }

      uploadState.file = SD.open(filePath, FILE_WRITE);

      if (!uploadState.file) {
        Serial.println("✗ Failed to open file for writing");
        if(xSemaphoreTake(displaySemaphore, pdMS_TO_TICKS(100)) == pdTRUE) {
          tft.println("✗ Failed to open file for writing");
          xSemaphoreGive(displaySemaphore);
        }
        xSemaphoreGive(sdSemaphore);
        request->send(500, "text/plain", "File open failed");
        return;
      }

      uploadState.inProgress = true;
      uploadState.totalBytes = 0;
      uploadState.lastUpdate = millis();
      uploadState.filename = filename;

      Serial.printf("→ Writing to: %s\n", filePath.c_str());
    }

    if (len && uploadState.inProgress) {
      size_t written = uploadState.file.write(data, len);

      if (written != len) {
        Serial.printf("✗ Write error: %d/%d bytes\n", written, len);
      }

      uploadState.totalBytes += written;

      // Progress update every 100KB
      if (uploadState.totalBytes % 102400 < len) {
        Serial.printf("  %d KB...\n", uploadState.totalBytes / 1024);
      }

      // CRITICAL: Reset watchdog periodically
      if (millis() - uploadState.lastUpdate > 1000) {
        uploadState.lastUpdate = millis();
      }
    }

    if (final && uploadState.inProgress) {
      uploadState.file.close();
      uploadState.inProgress = false;
      xSemaphoreGive(sdSemaphore);
      Serial.printf("✓ Upload complete: %.2f MB\n", (float)uploadState.totalBytes / (1024.0 * 1024.0));
      Serial.println("Upload complete");
      Serial.printf("Heap after upload: %d bytes\n", ESP.getFreeHeap());
      // if (filename.equals("index.html")) controlRes = true;
    }

    // if (controlRes == true) {
    //   delay(1000);
    //   ESP.restart();
    // }
    // controlRes = false;
    request->redirect("/WEB_Source/uploadFile.html");
      
  });

  server.on("/review", HTTP_GET, [](AsyncWebServerRequest *request) {
    AsyncWebServerResponse *respone = request->beginResponse(200,"text/plain", "ok");
  });

  server.serveStatic("/", SD, "/");
    
  server.onNotFound([](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "Not found");
  });

  ElegantOTA.begin(&server);    // Start ElegantOTA
  // ElegantOTA callbacks
  ElegantOTA.onStart(onOTAStart);
  ElegantOTA.onProgress(onOTAProgress);
  ElegantOTA.onEnd(onOTAEnd);

  isNetwork_install = true;

  server.begin();
  Serial.printf("Free heap after setup: %d bytes\n", ESP.getFreeHeap());
}

void runNet(void* pvParameter) {
  for(;;) {
    websocket.cleanupClients();
    ElegantOTA.loop();
    if(WiFi.getMode() == WIFI_MODE_AP) {
      dnsServer.processNextRequest();
    }
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}