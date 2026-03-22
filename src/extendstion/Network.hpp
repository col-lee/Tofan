#include "GlobalVar.hpp"

#define DISPLAYMANAGER_HH

#ifndef NETWORK_LIBRARY
#define NETWORK_LIBRARY
    #include <WiFi.h>
    #include <AsyncTCP.h>
    #include <ESPAsyncWebServer.h>
    #include <Preferences.h>
    #include <DNSServer.h>
    #include <ESPmDNS.h>
    #include <ElegantOTA.h>
    #include <stdlib.h>
#endif

extern AsyncWebServer server;
extern AsyncWebSocket websocket;
extern Preferences prefs; // instance Preferences for Save ssid and password
extern JsonDocument docs;

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
    void handleWebSocketMessage(void *arg, uint8_t *data, size_t len);
    void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
    void initWiFiManager();
    void initWebServer();
    void generateToken(char* token, int length);
    void writeLog(String &log);
};

void runNet(void* pvParameter);

extern NetworkManager nm;


