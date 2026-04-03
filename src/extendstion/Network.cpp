#include "Network.hpp"
#include "FileManager.hpp"
#include <stdlib.h>
#include <string.h>

DNSServer dnsServer;
AsyncWebServer server(80);
IPAddress apIP(192,168,4,1);
Preferences prefs; // instance Preferences for Save ssid and password

bool isNetwork_install;

unsigned long ota_progress_millis = 0;

bool wifi_isconnect = true;

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
    MDNS.end();
    return false;
  }

  return false;
}

void NetworkManager::startAdminMode() {

    // ########################################################################################
    // #                                    WIFI AP SETTING                                   #
    // ########################################################################################

    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP("terngai555", "esp32-pass");
    WiFi.softAPConfig(apIP,apIP, IPAddress(255,255,255,0));

    if(xSemaphoreTake(displaySemaphore, pdMS_TO_TICKS(100)) == pdTRUE){
      tft.print("IP: ");
      tft.println(apIP);
      xSemaphoreGive(displaySemaphore);
    }

    dnsServer.start(53, "manager.local", apIP);
    vTaskDelay(pdMS_TO_TICKS(100));

    // #########################################################################################
    // #                                   WIFI STA SETTING                                    #
    // #########################################################################################

    nm.readPrefs();
    // !connectoWiFi(prefs_Obj.ssid, prefs_Obj.password) ? wifi_isconnect = true : wifi_isconnect = false;

    // #########################################################################################
    // #                            ROUTER FOR MANAGE WIFI PASSWORD                            #
    // #########################################################################################

    server.on("/wifiManager", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(SD, "/WEB_Source/WiFiManger/wifiManager.html", "text/html");
    });

    server.on("/wifi", HTTP_OPTIONS, [](AsyncWebServerRequest *request){
      AsyncWebServerResponse* response = request->beginResponse(200);
      response->addHeader("Access-Control-Allow-Origin", "*");
      response->addHeader("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
      response->addHeader("Access-Control-Allow-Headers", "Content-Type");
      request->send(response);
    });

    server.on("/wifi", HTTP_POST,
    [](AsyncWebServerRequest *request) {
        if (request->_tempObject != NULL) {
            String* res = (String*)request->_tempObject;
            if (*res == "REBOOT") {
                request->send(200, "text/plain", "WiFi credentials saved. Rebooting...");
                DefaultHeaders::Instance().addHeader("Connection", "close");
                xTaskCreate([](void*){
                    vTaskDelay(pdMS_TO_TICKS(1000));
                    ESP.restart();
                }, "reboot_task", 2048, NULL, 5, NULL);
            } else {
                request->send(200, "text/plain", *res);
            }
            delete res;
            request->_tempObject = NULL;
        } else {
            request->send(400, "text/plain", "Bad Request");
        }
    },
    NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, (const char*)data);
        String* responseBody = new String();

        if (error) {
            *responseBody = "{\"error\":\"Invalid JSON\"}";
        } else {
            const char* ssid = doc["ssid"] | "";
            const char* password = doc["password"] | "";

            if(strlen(ssid) > 0) {
                strncpy(prefs_Obj.ssid, ssid, sizeof(prefs_Obj.ssid) - 1);
                strncpy(prefs_Obj.password, password, sizeof(prefs_Obj.password) - 1);
                if(nm.writePrefs()) {
                    *responseBody = "REBOOT";
                } else {
                    *responseBody = "Failed to write to NVM";
                }
            } else {
                *responseBody = "Missing SSID or Password";
            }
        }
        request->_tempObject = responseBody;
    });

    // server.on("/wifi", HTTP_POST,[](AsyncWebServerRequest *request){},
    // NULL,
    // [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    //   JsonDocument doc;
    //   DeserializationError error = deserializeJson(doc, (const char*)data);

    //   if (error) {
    //     request->send(200, "application/json", "{\"error\":\"Invalid JSON\"}");
    //     return;
    //   }

    //   const char* ssid = doc["ssid"];
    //   const char* password = doc["password"];

    //   if(ssid && password && strlen(ssid) > 0) {

    //     strncpy(prefs_Obj.ssid, ssid, sizeof(prefs_Obj.ssid) - 1);
    //     strncpy(prefs_Obj.password, password, sizeof(prefs_Obj.password) - 1);

    //       if (strlen(prefs_Obj.ssid) > 0){
    //         if(nm.writePrefs()) {
    //           Serial.printf("ssid: %s password: %s", prefs_Obj.ssid, prefs_Obj.password);
    //           request->send(200, "text/plain", "WiFi credentials saved. Rebooting...");
    //           DefaultHeaders::Instance().addHeader("Connection", "close");

    //           xTaskCreate([](void*){
    //             vTaskDelay(pdMS_TO_TICKS(1000));
    //             ESP.restart();
    //           }, "reboot_task", 2048, NULL, 5, NULL);
    //         } else {
    //           Serial.println("ssid or password is valid or null.");
    //         }

    //         vTaskDelay(500);
    //       } else {
    //         request->send(200, "text/plain", "Failed to write to NVM");
    //       }
    //   } else {
    //     request->send(200, "text/plain", "Missing SSID or Password");
    //   }

    // });

    // ########################################################################################
    // # ROUTER FOR MANAGE LOGIN AND CHANGE USERNAME, PASSWORD ,CHECK TOKEN LOGIN, UPLOADFILE #
    // ########################################################################################

    if(isConnectSDcard) {

      server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->redirect("/index.html");
      });

      server.serveStatic("/", SD, "/WEB_Source/");
      server.serveStatic("/pageUploadFile/", SD, "/WEB_Source/");
      server.serveStatic("/controlPanel/", SD, "/WEB_Source/");
    }

    server.on("/api/signin", HTTP_OPTIONS, [](AsyncWebServerRequest *request) {
      AsyncWebServerResponse* response = request->beginResponse(200);
      response->addHeader("Access-Control-Allow-Origin", "*");
      response->addHeader("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
      response->addHeader("Access-Control-Allow-Headers", "Content-Type");
      request->send(response);
    });

    // ---------------------------------------------------------
    // 2. API: /api/signin
    // ---------------------------------------------------------
    server.on("/api/signin", HTTP_POST, 
    [](AsyncWebServerRequest *request) {
        if (request->_tempObject != NULL) {
            String* res = (String*)request->_tempObject;
            AsyncWebServerResponse* response = request->beginResponse(200, "application/json", *res);
            response->addHeader("Access-Control-Allow-Origin", "*");
            request->send(response);
            delete res;
            request->_tempObject = NULL;
        }
    },
    NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, (const char*)data);
        String* responseBody = new String();

        if (error) {
            *responseBody = "{\"status\":false, \"msg\":\"Invalid JSON\"}";
        } else {
            const char* username = doc["u"] | "";
            const char* password = doc["p"] | "";
            const char* token_login = doc["token"] | "";
            char token_[64], token[128];

            if(username != nullptr && password != nullptr && strlen(username) > 0 && strlen(password) > 0) {
                if(nm.readUsername()) {
                    if(strcmp(username_obj.username, username) == 0 && strcmp(username_obj.password, password) == 0){
                        if(token_login != nullptr && strlen(token_login) > 0) {
                            char check_token[64];
                            if(prefs.begin("UsernameConfig", true)) {
                                prefs.getString("auth", check_token, sizeof(check_token));
                                prefs.end();
                            }
                            if(strcmp(token_login, check_token) == 0) {
                                *responseBody = "{\"status\":true}";
                            } else {
                                snprintf(token, sizeof(token), "{\"status\":true,\"okte\":\"%s\"}", check_token);
                                *responseBody = String(token);
                            }
                        } else {
                            nm.generateToken(token_, 25);
                            if(prefs.begin("UsernameConfig", false)) {
                                prefs.putString("auth", token_);
                                prefs.end();
                            }
                            snprintf(token, sizeof(token), "{\"status\":true,\"okte\":\"%s\"}", token_);
                            *responseBody = String(token);
                        }
                    } else {
                        *responseBody = "{\"status\":false, \"msg\":\"Wrong username or password\"}";
                    }
                }
            } else {
                *responseBody = "{\"status\":false, \"msg\":\"Please enter your username and password\"}";
            }
        }
        request->_tempObject = responseBody;
    });

    server.on("/api/changeuser", HTTP_OPTIONS, [](AsyncWebServerRequest *request) {
      AsyncWebServerResponse* response = request->beginResponse(200);
      response->addHeader("Access-Control-Allow-Origin", "*");
      response->addHeader("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
      response->addHeader("Access-Control-Allow-Headers", "Content-Type");
      request->send(response);
    });

    // ---------------------------------------------------------
    // 3. API: /api/signup
    // ---------------------------------------------------------
    server.on("/api/changeuser", HTTP_POST, 
    [](AsyncWebServerRequest *request) {
        if (request->_tempObject != NULL) {
            String* res = (String*)request->_tempObject;
            AsyncWebServerResponse* response = request->beginResponse(200, "application/json", *res);
            response->addHeader("Access-Control-Allow-Origin", "*");
            request->send(response);
            delete res;
            request->_tempObject = NULL;
        }
    },
    NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, (const char*)data);
        String* responseBody = new String();

        if (error) {
            *responseBody = "{\"status\":false, \"msg\":\"Invalid JSON\"}";
        } else {
            const char* username = doc["u"] | "";
            const char* password = doc["p"] | "";

            if(strlen(username) > 0 && strlen(password) > 0) {
                strcpy(username_obj.username, username);
                strcpy(username_obj.password, password);
                if(nm.writeUsername()) {
                    *responseBody = "{\"status\":true,\"msg\":\"Sign up success.\"}";
                } else {
                    *responseBody = "{\"status\":false, \"msg\":\"failed to write to Memory\"}";
                }
            } else {
                *responseBody = "{\"status\":false, \"msg\":\"Please enter your username and password\"}";
            }
        }
        request->_tempObject = responseBody;
    });

    server.on("/api/checkToken", HTTP_OPTIONS, [](AsyncWebServerRequest *request) {
      AsyncWebServerResponse* response = request->beginResponse(200);
      response->addHeader("Access-Control-Allow-Origin", "*");
      response->addHeader("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
      response->addHeader("Access-Control-Allow-Headers", "Content-Type");
      request->send(response);
    });

    // ---------------------------------------------------------
    // 4. API: /api/checkToken
    // ---------------------------------------------------------
    server.on("/api/checkToken", HTTP_POST, 
    [](AsyncWebServerRequest *request) {
        if (request->_tempObject != NULL) {
            String* res = (String*)request->_tempObject;
            AsyncWebServerResponse* response = request->beginResponse(200, "application/json", *res);
            response->addHeader("Access-Control-Allow-Origin", "*");
            request->send(response);
            delete res;
            request->_tempObject = NULL;
        }
    },
    NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, (const char*)data);
        String* responseBody = new String();

        if (error) {
            *responseBody = "{\"status\":false, \"msg\":\"Invalid JSON\"}";
        } else {
            const char* token_check = doc["token"] | "";
            char token[64] = {0};

            if(token_check != nullptr && strlen(token_check) > 0) {
                if(prefs.begin("UsernameConfig", true)) {
                    prefs.getString("auth", token, sizeof(token));
                    prefs.end();
                }
                if(strcmp(token_check, token) == 0){
                    *responseBody = "{\"status\":true, \"msg\":\"Log in successfully.\"}";
                } else {
                    *responseBody = "{\"status\":false, \"msg\":\"Log in faild.\"}";
                }
            } else {
                *responseBody = "{\"status\":false, \"msg\":\"Please log in again.\"}";
            }
        }
        request->_tempObject = responseBody;
    });

  server.on("/uploadfile", HTTP_POST, [](AsyncWebServerRequest *request) {

    if (uploadState.inProgress) {
        if (uploadState.file) {
          uploadState.file.close();
        }
        uploadState.inProgress = false;
    }
    
    request->send(200, "text/plain", "successfull.");
    Serial.printf("✓ Upload finished: %s (%d bytes)\n",
                  uploadState.filename.c_str(),
                  uploadState.totalBytes);
    Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
  }, [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {

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

    request->redirect("/WEB_Source/uploadFile.html");
      
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

void NetworkManager::stopAdminMode() {
  dnsServer.stop();
  server.end();
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  Serial.println("Admin Mode Stopped. RAM Freed!");
}

void runNet(void* pvParameter) {
  for(;;) {
    ElegantOTA.loop();
    if(WiFi.getMode() == WIFI_MODE_AP) {
      dnsServer.processNextRequest();
    }
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}