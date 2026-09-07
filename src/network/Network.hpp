// Manages Wi-Fi, Admin Mode HTTP routes and WebSocket commands.
#pragma once

#include "../core/SharedResources.hpp"

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Preferences.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <stdlib.h>
#include <Update.h>

typedef struct {
    char ssid[32] = "";
    char password[32] = "";
} PrefsObj_WiFiManager; // Stored Wi-Fi credential buffers.

extern AsyncWebServer server;
extern AsyncWebSocket websocket;
extern Preferences prefs; // NVS storage for Wi-Fi and Admin settings.

class NetworkManager {
public:
    bool isConnectWiFi = false;

public:
    bool writePrefs();
    bool readPrefs();
    bool clearPref();
    bool writeUsername();
    bool readUsername();
    bool clearUsername();
    bool connectoWiFi(const char *ssid, const char *password);
    bool connectoWiFi();
    void startAPMode();
    void closeWiFiSTA();
    void generateToken(char* token, int length);
    void writeLog(String &log);
    void startAdminMode();
    void stopAdminMode();
};

void runNet(void* pvParameter);
void handleWebSocketMessage(AsyncWebSocketClient *client, void *arg, uint8_t *data, size_t len);
void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);

extern NetworkManager nm;
