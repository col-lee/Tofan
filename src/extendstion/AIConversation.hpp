#ifndef AI_CONVERSATION_HPP
#define AI_CONVERSATION_HPP

#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <ESPAsyncWebServer.h>

struct AIServiceConfig {
    bool enabled = false;
    bool allowInsecureTLS = false;
    char pipelineUrl[192] = "";
    char apiKey[128] = "";
    char user[64] = "";
    char password[64] = "";
    char provider[24] = "backend";
    char model[48] = "";
};

class AIConversation {
public:
    bool begin();
    bool loadConfig();
    bool saveConfig(const JsonDocument& document);
    void registerWebRoutes(AsyncWebServer& webServer);
    String getConfigJson(bool includeSecrets = false) const;
    String getStatusJson() const;
    bool submitAudioFile(const char* path, String& responseBody);
    bool extractAudioUrl(const String& responseBody, String& audioUrl) const;
    bool isConfigured() const;

private:
    Preferences preferences;
    AIServiceConfig config;
    String state = "idle";
    String lastError;

    void setError(const String& message);
    static void sendJson(AsyncWebServerRequest* request, int statusCode, const String& body);
    void handleConfigRequest(AsyncWebServerRequest* request);
    void handleConfigBody(AsyncWebServerRequest* request, uint8_t* data, size_t length, size_t index, size_t total);
};

extern AIConversation aiConversation;

#endif
