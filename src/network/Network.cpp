#include "../core/MemoryPolicy.hpp"
// Manages Wi-Fi, Admin Mode HTTP routes and WebSocket commands.
#include "Network.hpp"
#include "EspNowManager.hpp"
#include "WebPortal.hpp"
#include "../hardware/DisplayDevice.hpp"
#include "../ai/AIConversation.hpp"
#include "../storage/FileManager.hpp"
#include <stdlib.h>
#include <atomic>
#include <string.h>

DNSServer dnsServer;
AsyncWebServer server(80);
AsyncWebSocket websocket("/ws");
IPAddress apIP(192,168,4,1);
Preferences prefs; // NVS storage for Wi-Fi and Admin settings.
NetworkManager nm;
static std::atomic<int> requestedNetwork{-1};
static std::atomic<bool> applyingNetwork{false};
static std::atomic<bool> reconnectWifi{false};
void requestWiFiReconnect() { reconnectWifi=true; requestNetworkSettings(true,true); }
void requestNetworkSettings(bool wifi, bool admin) { requestedNetwork.store((wifi?1:0)|(admin?2:0)); }
bool networkSettingsBusy() { return applyingNetwork.load() || requestedNetwork.load() >= 0; }

int timeout_wifi_connection = 10;

bool isNetwork_install = false;
unsigned long ota_progress_millis = 0;

static bool isServerConfigured = false;

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
    prefs.putString("username", username_obj.username);
    prefs.putString("password", username_obj.password);
    prefs.end();
    Serial.println("Save username and password to Memory successfull.");
    return true;
  }
  return false;
}

bool NetworkManager::readUsername() {
  if(prefs.begin("UsernameConfig", true)) {
    prefs.getString("username", username_obj.username, sizeof(username_obj.username));
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
  // Token alphabet: uppercase, lowercase and decimal digits.
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

void NetworkManager::closeWiFiSTA() {
  WiFi.disconnect(false);
  WiFi.mode(isNetwork_install ? WIFI_AP : WIFI_OFF);
  isConnectWiFi = false;
}

bool NetworkManager::connectoWiFi(const char *ssid, const char *password)
{

  isConnectWiFi = false;

  if(ssid == nullptr || password == nullptr || strlen(ssid) == 0) {
    Serial.println("ssid or password is invalid or null.");
    return false;
  }

  unsigned long preTime = 0;
  unsigned long interval = 1000;
  int seconds = 0;

  WiFi.mode(isNetwork_install ? WIFI_AP_STA : WIFI_STA);
  WiFi.begin(ssid, password);
  WiFi.setSleep(false);
  Serial.print("Connecting to wifi.");
  while (WiFi.status() != WL_CONNECTED)
  {
    vTaskDelay(pdMS_TO_TICKS(20));
    unsigned long currentTime = millis();
    if (seconds >= timeout_wifi_connection) {
      Serial.println("");
      Serial.println("WiFi connect Time out.");

      WiFi.disconnect();
      vTaskDelay(pdMS_TO_TICKS(100));
      isConnectWiFi = false;
      return false;
    }

    if ((currentTime - preTime) >= interval)
    {
      preTime = currentTime;
      seconds++;
      Serial.print(".");

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



    if(MDNS.begin("terngai")) {
      Serial.println("mDNS responder started");
      MDNS.addService("http", "tcp", 80);
    } else {
      Serial.println("Error setting up MDNS responder!");
    }
    isConnectWiFi = true;
    return true;
  }
  else {
    Serial.println("WiFi connect Time out.");
    WiFi.disconnect();
    WiFi.mode(isNetwork_install ? WIFI_AP : WIFI_OFF);
    MDNS.end();
    isConnectWiFi = false;
    return false;
  }

  return false;
}

bool NetworkManager::connectoWiFi() {

  nm.readPrefs();
  isConnectWiFi = false;
  if (strlen(prefs_Obj.ssid) == 0) {
    Serial.println("Saved ssid or password is empty.");
    return false;
  }

  unsigned long preTime = 0;
  unsigned long interval = 1000;
  int seconds = 0;

  WiFi.mode(isNetwork_install ? WIFI_AP_STA : WIFI_STA);
  WiFi.begin(prefs_Obj.ssid, prefs_Obj.password);
  WiFi.setSleep(false);
  Serial.print("Connecting to wifi.");
  while (WiFi.status() != WL_CONNECTED)
  {
    vTaskDelay(pdMS_TO_TICKS(20));
    unsigned long currentTime = millis();
    if (seconds >= timeout_wifi_connection) {
      Serial.println("");
      Serial.println("WiFi connect Time out.");

      WiFi.disconnect();
      vTaskDelay(pdMS_TO_TICKS(100));
      isConnectWiFi = false;
      return false;
    }

    if ((currentTime - preTime) >= interval)
    {
      preTime = currentTime;
      seconds++;
      Serial.print(".");

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



    if(MDNS.begin("terngai")) {
      Serial.println("mDNS responder started");
      MDNS.addService("http", "tcp", 80);
    } else {
      Serial.println("Error setting up MDNS responder!");
    }
    isConnectWiFi = true;
    return true;
  }
  else {
    Serial.println("WiFi connect Time out.");
    WiFi.disconnect();
    WiFi.mode(isNetwork_install ? WIFI_AP : WIFI_OFF);
    MDNS.end();
    isConnectWiFi = false;
    return false;
  }

  return false;
}

void NetworkManager::startAPMode() {
  // ########################################################################################
  // #                                    WIFI AP SETTING                                   #
  // ########################################################################################
  WiFi.mode(WiFi.status() == WL_CONNECTED ? WIFI_AP_STA : WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP("terngai555", "esp32-pass", 1, 0, 1);


  dnsServer.start(53, "manager.setup", apIP);
  vTaskDelay(pdMS_TO_TICKS(100));
}

void NetworkManager::startAdminMode() {
    if (isNetwork_install) return;
    startAPMode();
    if (!isServerConfigured) { registerWebPortal(server); isServerConfigured = true; }
    server.begin();
    isNetwork_install = true;
}

void NetworkManager::stopAdminMode() {
  const bool stationConnected = WiFi.status() == WL_CONNECTED;
  server.end(); dnsServer.stop();
  WiFi.softAPdisconnect(true);
  isNetwork_install = false;
  WiFi.mode(stationConnected ? WIFI_STA : WIFI_OFF);
  isConnectWiFi = stationConnected;
}

void handleWebSocketMessage(AsyncWebSocketClient *client, void *arg, uint8_t *data, size_t len) {
    AwsFrameInfo *info = (AwsFrameInfo*)arg;

    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {

        // Deserialize exactly the received WebSocket payload length.
        String msg((char*)data, len);

        JsonDocument doc(memory::jsonAllocator());
        DeserializationError error = deserializeJson(doc, msg);
        if (error) {
            Serial.println("WS JSON Error!");
            return;
        }

        String action = doc["action"] | "";
        String path = doc["path"] | "";
        String responseMsg;
        bool res = false;

        if (action == "scan") {
            String jsonStr = file_card.getFileListJSON(path);
            // ส่งกลับไปเฉพาะ Client ที่ร้องขอมาเท่านั้น ไม่ใช้ textAll()
            client->text(jsonStr);
            return;
        }
        else if (action == "create") {
            String type = doc["type"] | "file";
            res = (type == "folder") ? file_card.createFolder(path) : file_card.createFile(path);
        }
        else if (action == "delete") {
            res = file_card.deleteFile(path);
        }
        else if (action == "rename" || action == "move") {
            String newPath = doc["newPath"] | "";
            res = file_card.renameFile(path, newPath);
        }
        else if (action == "update") {
            String content = doc["content"] | "";
            res = file_card.updateFile(path, content);
        }

        // ส่งกลับเฉพาะคนสั่ง
        responseMsg = "{\"action\":\"" + action + "\", \"path\":\"" + path + "\", \"status\":" + String(res ? "true" : "false") + "}";
        client->text(responseMsg);

        Serial.println("WS Action: " + action + " | Status: " + String(res ? "Success" : "Failed"));
    }
}

void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
    if (type == WS_EVT_CONNECT) {
        Serial.printf("WS Client connected: %u\n", client->id());

        // กฎเหล็ก: มี Admin ได้แค่คนเดียวเท่านั้น!
        // ลูปหา Client ตัวอื่นๆ ที่ค้างอยู่ในระบบ แล้วเตะทิ้งเพื่อคืน RAM
        for (auto& c : server->getClients()) {
            // Close other connected clients to keep a single Admin WebSocket.
            if (c.id() != client->id()) {
                Serial.printf("Kicking old zombie client: %u to save RAM!\n", c.id());
                c.close();
            }
        }
    } else if (type == WS_EVT_DISCONNECT) {
        Serial.printf("WS Client disconnected: %u\n", client->id());
    } else if (type == WS_EVT_DATA) {
        handleWebSocketMessage(client, arg, data, len);
    }
}

void runNet(void* pvParameter) {
  for(;;) {
    const int desired=requestedNetwork.exchange(-1);
    if (desired >= 0) {
      applyingNetwork.store(true);
      espnow::beforeWifiChange();
      const bool reconnect = reconnectWifi.exchange(false);
      if (reconnect) WiFi.disconnect(false);
      if (!(desired&2) && isNetwork_install) nm.stopAdminMode();
      if ((desired&2) && !isNetwork_install) nm.startAdminMode();
      if (desired&1) { if (reconnect || WiFi.status()!=WL_CONNECTED) nm.connectoWiFi(); }
      else nm.closeWiFiSTA();
      applyingNetwork.store(false);
    }
    if(isNetwork_install){
      if(WiFi.getMode() == WIFI_MODE_AP || WiFi.getMode() == WIFI_MODE_APSTA) {
        dnsServer.processNextRequest();
      }
       websocket.cleanupClients();
    }

    espnow::service();
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}
