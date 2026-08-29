#include "AIConversation.hpp"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <SD.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

AIConversation aiConversation;

bool AIConversation::begin() {
    return loadConfig();
}

bool AIConversation::loadConfig() {
    if (!preferences.begin("AIConfig", true)) {
        setError("Unable to open AIConfig");
        return false;
    }

    config.enabled = preferences.getBool("enabled", false);
    config.allowInsecureTLS = preferences.getBool("insecure", false);
    preferences.getString("url", config.pipelineUrl, sizeof(config.pipelineUrl));
    preferences.getString("apiKey", config.apiKey, sizeof(config.apiKey));
    preferences.getString("user", config.user, sizeof(config.user));
    preferences.getString("password", config.password, sizeof(config.password));
    preferences.getString("provider", config.provider, sizeof(config.provider));
    preferences.getString("model", config.model, sizeof(config.model));
    preferences.end();
    lastError = "";
    return true;
}

bool AIConversation::saveConfig(const JsonDocument& document) {
    const char* pipelineUrl = document["pipelineUrl"] | config.pipelineUrl;
    const char* provider = document["provider"] | config.provider;
    const char* model = document["model"] | config.model;
    const char* apiKey = document["apiKey"] | "";
    const char* user = document["user"] | config.user;
    const char* password = document["password"] | "";

    if (strlen(pipelineUrl) >= sizeof(config.pipelineUrl) || strlen(provider) >= sizeof(config.provider) || strlen(model) >= sizeof(config.model) ||
        strlen(apiKey) >= sizeof(config.apiKey) || strlen(user) >= sizeof(config.user) || strlen(password) >= sizeof(config.password)) {
        setError("One or more values are too long");
        return false;
    }

    if (strlen(pipelineUrl) == 0) {
        setError("pipelineUrl is required");
        return false;
    }

    strlcpy(config.pipelineUrl, pipelineUrl, sizeof(config.pipelineUrl));
    strlcpy(config.provider, provider, sizeof(config.provider));
    strlcpy(config.model, model, sizeof(config.model));
    if (strlen(apiKey) > 0) strlcpy(config.apiKey, apiKey, sizeof(config.apiKey));
    strlcpy(config.user, user, sizeof(config.user));
    if (strlen(password) > 0) strlcpy(config.password, password, sizeof(config.password));
    config.enabled = document["enabled"] | config.enabled;
    config.allowInsecureTLS = document["allowInsecureTLS"] | config.allowInsecureTLS;

    if (!preferences.begin("AIConfig", false)) {
        setError("Unable to write AIConfig");
        return false;
    }
    preferences.putBool("enabled", config.enabled);
    preferences.putBool("insecure", config.allowInsecureTLS);
    preferences.putString("url", config.pipelineUrl);
    preferences.putString("apiKey", config.apiKey);
    preferences.putString("user", config.user);
    preferences.putString("password", config.password);
    preferences.putString("provider", config.provider);
    preferences.putString("model", config.model);
    preferences.end();
    lastError = "";
    return true;
}

String AIConversation::getConfigJson(bool includeSecrets) const {
    JsonDocument document;
    document["enabled"] = config.enabled;
    document["allowInsecureTLS"] = config.allowInsecureTLS;
    document["pipelineUrl"] = config.pipelineUrl;
    document["provider"] = config.provider;
    document["model"] = config.model;
    document["configured"] = isConfigured();
    if (includeSecrets) {
        document["apiKey"] = config.apiKey;
        document["user"] = config.user;
        document["password"] = config.password;
    } else {
        document["apiKeySet"] = strlen(config.apiKey) > 0;
        document["userSet"] = strlen(config.user) > 0;
        document["passwordSet"] = strlen(config.password) > 0;
    }
    String output;
    serializeJson(document, output);
    return output;
}

String AIConversation::getStatusJson() const {
    JsonDocument document;
    document["enabled"] = config.enabled;
    document["configured"] = isConfigured();
    document["state"] = state;
    document["lastError"] = lastError;
    String output;
    serializeJson(document, output);
    return output;
}

bool AIConversation::isConfigured() const {
    return config.enabled && strlen(config.pipelineUrl) > 0;
}

void AIConversation::setError(const String& message) {
    lastError = message;
    state = "error";
}

void AIConversation::sendJson(AsyncWebServerRequest* request, int statusCode, const String& body) {
    AsyncWebServerResponse* response = request->beginResponse(statusCode, "application/json", body);
    response->addHeader("Cache-Control", "no-store");
    request->send(response);
}

void AIConversation::handleConfigRequest(AsyncWebServerRequest* request) {
    String* body = static_cast<String*>(request->_tempObject);
    if (body == nullptr) {
        sendJson(request, 400, "{\"ok\":false,\"error\":\"Request body is required\"}");
        return;
    }

    JsonDocument document;
    DeserializationError error = deserializeJson(document, *body);
    delete body;
    request->_tempObject = nullptr;
    if (error) {
        sendJson(request, 400, "{\"ok\":false,\"error\":\"Invalid JSON\"}");
        return;
    }

    if (!saveConfig(document)) {
        sendJson(request, 400, String("{\"ok\":false,\"error\":\"") + lastError + "\"}");
        return;
    }
    sendJson(request, 200, "{\"ok\":true}");
}

void AIConversation::handleConfigBody(AsyncWebServerRequest* request, uint8_t* data, size_t length, size_t index, size_t total) {
    String* body = static_cast<String*>(request->_tempObject);
    if (body == nullptr) {
        body = new String();
        body->reserve(total + 1);
        request->_tempObject = body;
    }
    body->concat(reinterpret_cast<const char*>(data), length);
}

void AIConversation::registerWebRoutes(AsyncWebServer& webServer) {
    webServer.on("/api/ai/config", HTTP_GET, [](AsyncWebServerRequest* request) {
        AIConversation::sendJson(request, 200, aiConversation.getConfigJson(false));
    });
    webServer.on("/api/ai/config", HTTP_POST,
        [](AsyncWebServerRequest* request) { aiConversation.handleConfigRequest(request); },
        nullptr,
        [](AsyncWebServerRequest* request, uint8_t* data, size_t length, size_t index, size_t total) {
            aiConversation.handleConfigBody(request, data, length, index, total);
        });
    webServer.on("/api/ai/status", HTTP_GET, [](AsyncWebServerRequest* request) {
        AIConversation::sendJson(request, 200, aiConversation.getStatusJson());
    });
}

bool AIConversation::submitAudioFile(const char* path, String& responseBody) {
    if (!isConfigured()) {
        setError("AI pipeline is not configured");
        return false;
    }
    if (WiFi.status() != WL_CONNECTED) {
        setError("WiFi is not connected");
        return false;
    }

    File audioFile = SD.open(path, FILE_READ);
    if (!audioFile) {
        setError("Audio file could not be opened");
        return false;
    }

    state = "processing";
    HTTPClient http;
    WiFiClient plainClient;
    WiFiClientSecure secureClient;
    bool https = String(config.pipelineUrl).startsWith("https://");
    if (https) {
        if (config.allowInsecureTLS) secureClient.setInsecure();
        if (!http.begin(secureClient, config.pipelineUrl)) {
            audioFile.close();
            setError("Unable to connect to HTTPS pipeline");
            return false;
        }
    } else if (!http.begin(plainClient, config.pipelineUrl)) {
        audioFile.close();
        setError("Unable to connect to pipeline");
        return false;
    }

    http.addHeader("Content-Type", "audio/wav");
    http.addHeader("X-AI-Provider", config.provider);
    if (strlen(config.apiKey) > 0) http.addHeader("Authorization", String("Bearer ") + config.apiKey);
    int statusCode = http.sendRequest("POST", &audioFile, audioFile.size());
    responseBody = http.getString();
    http.end();
    audioFile.close();

    if (statusCode < 200 || statusCode >= 300) {
        setError(String("Pipeline HTTP error ") + statusCode);
        return false;
    }
    state = "completed";
    lastError = "";
    return true;
}

bool AIConversation::extractAudioUrl(const String& responseBody, String& audioUrl) const {
    JsonDocument document;
    if (deserializeJson(document, responseBody)) return false;

    const char* url = document["audioUrl"] | "";
    if (strlen(url) == 0) return false;
    audioUrl = url;
    return true;
}
