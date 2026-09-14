#include "../core/MemoryPolicy.hpp"
// AI configuration, legacy HTTP WAV pipeline and Gemini Live voice-only transport.
#include "AIConversation.hpp"
#include "GoogleTrustRoots.hpp"
#include "ChatHistory.hpp"
#include "../audio/SoundManager.hpp"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <SD.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <esp_heap_caps.h>
#include <mbedtls/base64.h>

AIConversation aiConversation;

namespace {
constexpr const char* GEMINI_HOST = "generativelanguage.googleapis.com";
constexpr uint16_t GEMINI_PORT = 443;
constexpr const char* GEMINI_WS_PATH = "/ws/google.ai.generativelanguage.v1beta.GenerativeService.BidiGenerateContent";
constexpr size_t LIVE_MIC_CAPTURE_SAMPLES = 512; // 32 ms capture blocks @ 16 kHz
constexpr size_t LIVE_MIC_SEND_SAMPLES = LIVE_MIC_CAPTURE_SAMPLES * 3; // 96 ms packets, close to Gemini's ~100 ms guidance
constexpr size_t LIVE_MIC_BASE64_BYTES = (((LIVE_MIC_SEND_SAMPLES * sizeof(int16_t)) + 2) / 3) * 4 + 1;
constexpr size_t LIVE_MIC_JSON_BYTES = LIVE_MIC_BASE64_BYTES + 192;
constexpr uint32_t LIVE_RECONNECT_BASE_MS = 1500;
constexpr uint32_t LIVE_RECONNECT_MAX_MS = 30000;
constexpr uint32_t LIVE_SETUP_TIMEOUT_MS = 8000;
constexpr size_t LIVE_WS_FRAGMENT_INITIAL_BYTES = 16 * 1024;
constexpr size_t LIVE_WS_FRAGMENT_MAX_BYTES = 512 * 1024;
constexpr size_t AI_CONFIG_BODY_MAX_BYTES = 4096;
constexpr const char* AI_CONFIG_BODY_TOO_LARGE = "__TOFAN_AI_CONFIG_BODY_TOO_LARGE__";

String eventPayloadPreview(const uint8_t* payload, size_t length, size_t maxChars = 180) {
    if (!payload || length == 0) return "no reason supplied";
    const size_t count = length < maxChars ? length : maxChars;
    String text;
    text.reserve(count + 4);
    for (size_t i = 0; i < count; ++i) {
        const char c = static_cast<char>(payload[i]);
        text += (c >= 32 && c <= 126) ? c : '.';
    }
    if (length > count) text += "...";
    return text;
}

bool providerIsGeminiLive(const char* provider) {
    return provider && (strcasecmp(provider, "gemini-live") == 0 || strcasecmp(provider, "gemini") == 0);
}
}

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
    config.liveBargeIn = preferences.getBool("bargeIn", false);
    preferences.getString("url", config.pipelineUrl, sizeof(config.pipelineUrl));
    preferences.getString("apiKey", config.apiKey, sizeof(config.apiKey));
    preferences.getString("user", config.user, sizeof(config.user));
    preferences.getString("password", config.password, sizeof(config.password));
    preferences.getString("provider", config.provider, sizeof(config.provider));
    preferences.getString("model", config.model, sizeof(config.model));
    preferences.getString("voice", config.voice, sizeof(config.voice));
    preferences.getString("sysPrompt", config.systemInstruction, sizeof(config.systemInstruction));
    preferences.end();

    if (strlen(config.provider) == 0) strlcpy(config.provider, "gemini-live", sizeof(config.provider));
    if (providerIsGeminiLive(config.provider)) {
        if (strlen(config.model) == 0) strlcpy(config.model, "gemini-3.1-flash-live-preview", sizeof(config.model));
        if (strlen(config.voice) == 0) strlcpy(config.voice, "Kore", sizeof(config.voice));
        if (strlen(config.systemInstruction) == 0) {
            strlcpy(config.systemInstruction,
                    "You are ToFan, a friendly desk companion. Respond naturally and briefly. If the user speaks Thai, reply in Thai.",
                    sizeof(config.systemInstruction));
        }
    }
    lastError = "";
    state = "idle";
    return true;
}

bool AIConversation::saveConfig(const JsonDocument& document) {
    const char* pipelineUrl = document["pipelineUrl"] | config.pipelineUrl;
    const char* provider = document["provider"] | config.provider;
    const char* model = document["model"] | config.model;
    const char* voice = document["voice"] | config.voice;
    const char* systemInstruction = document["systemInstruction"] | config.systemInstruction;
    const char* apiKey = document["apiKey"] | "";
    const char* user = document["user"] | config.user;
    const char* password = document["password"] | "";

    if (strlen(pipelineUrl) >= sizeof(config.pipelineUrl) || strlen(provider) >= sizeof(config.provider) ||
        strlen(model) >= sizeof(config.model) || strlen(voice) >= sizeof(config.voice) ||
        strlen(systemInstruction) >= sizeof(config.systemInstruction) || strlen(apiKey) >= sizeof(config.apiKey) ||
        strlen(user) >= sizeof(config.user) || strlen(password) >= sizeof(config.password)) {
        setError("One or more values are too long");
        return false;
    }

    const bool geminiLive = providerIsGeminiLive(provider);
    if (!geminiLive && strlen(pipelineUrl) == 0) {
        setError("pipelineUrl is required for HTTP backend mode");
        return false;
    }

    strlcpy(config.pipelineUrl, pipelineUrl, sizeof(config.pipelineUrl));
    strlcpy(config.provider, provider, sizeof(config.provider));
    strlcpy(config.model, model, sizeof(config.model));
    strlcpy(config.voice, voice, sizeof(config.voice));
    strlcpy(config.systemInstruction, systemInstruction, sizeof(config.systemInstruction));
    if (strlen(apiKey) > 0) strlcpy(config.apiKey, apiKey, sizeof(config.apiKey));
    strlcpy(config.user, user, sizeof(config.user));
    if (strlen(password) > 0) strlcpy(config.password, password, sizeof(config.password));
    config.enabled = document["enabled"] | config.enabled;
    config.allowInsecureTLS = document["allowInsecureTLS"] | config.allowInsecureTLS;
    config.liveBargeIn = document["liveBargeIn"] | config.liveBargeIn;

    if (geminiLive) {
        if (strlen(config.model) == 0) strlcpy(config.model, "gemini-3.1-flash-live-preview", sizeof(config.model));
        if (strlen(config.voice) == 0) strlcpy(config.voice, "Kore", sizeof(config.voice));
    }

    if (!preferences.begin("AIConfig", false)) {
        setError("Unable to write AIConfig");
        return false;
    }
    preferences.putBool("enabled", config.enabled);
    preferences.putBool("insecure", config.allowInsecureTLS);
    preferences.putBool("bargeIn", config.liveBargeIn);
    preferences.putString("url", config.pipelineUrl);
    preferences.putString("apiKey", config.apiKey);
    preferences.putString("user", config.user);
    preferences.putString("password", config.password);
    preferences.putString("provider", config.provider);
    preferences.putString("model", config.model);
    preferences.putString("voice", config.voice);
    preferences.putString("sysPrompt", config.systemInstruction);
    preferences.end();

    lastError = "";
    state = "idle";
    return true;
}

String AIConversation::getConfigJson(bool includeSecrets) const {
    JsonDocument document(memory::jsonAllocator());
    document["enabled"] = config.enabled;
    document["allowInsecureTLS"] = config.allowInsecureTLS;
    document["liveBargeIn"] = config.liveBargeIn;
    document["pipelineUrl"] = config.pipelineUrl;
    document["provider"] = config.provider;
    document["model"] = config.model;
    document["voice"] = config.voice;
    document["systemInstruction"] = config.systemInstruction;
    document["configured"] = isConfigured();
    document["liveProvider"] = isLiveProvider();
    document["state"] = state;
    document["lastError"] = lastError;
    document["liveActive"] = isLiveSessionActive();
    document["liveReady"] = isLiveSessionReady();
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
    JsonDocument document(memory::jsonAllocator());
    document["enabled"] = config.enabled;
    document["configured"] = isConfigured();
    document["provider"] = config.provider;
    document["state"] = state;
    document["lastError"] = lastError;
    document["liveActive"] = isLiveSessionActive();
    document["liveReady"] = isLiveSessionReady();
    document["txAudioChunks"] = liveTxAudioChunks.load();
    document["rxAudioChunks"] = liveRxAudioChunks.load();
    document["droppedAudioChunks"] = liveDroppedAudioChunks.load();
    document["speakerBufferedBytes"] = livePcmBufferedBytes();
    String output;
    serializeJson(document, output);
    return output;
}

bool AIConversation::isLiveProvider() const {
    return providerIsGeminiLive(config.provider);
}

bool AIConversation::isConfigured() const {
    if (!config.enabled) return false;
    if (isLiveProvider()) return strlen(config.apiKey) > 0 && strlen(config.model) > 0;
    return strlen(config.pipelineUrl) > 0;
}

bool AIConversation::isLiveSessionActive() const {
    return liveTaskHandle != nullptr && !liveStopRequested.load();
}

bool AIConversation::isLiveSessionReady() const {
    return liveSocketConnected.load() && liveSetupComplete.load();
}

bool AIConversation::isLiveResponseActive() const {
    // A streamed reply remains active through gaps between PCM packets, and
    // after turnComplete until the speaker has finished the queued samples.
    return isLiveSessionReady() &&
        (liveModelTurnActive.load() || livePcmHasBufferedAudio());
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

    if (*body == AI_CONFIG_BODY_TOO_LARGE) {
        delete body;
        request->_tempObject = nullptr;
        sendJson(request, 413, "{\"ok\":false,\"error\":\"AI configuration body is too large\"}");
        return;
    }

    // Do not silently stop AI Pet underneath AppCoordinator. The main web API
    // already rejects this case; keep the direct compatibility route consistent.
    if (isLiveSessionActive()) {
        delete body;
        request->_tempObject = nullptr;
        sendJson(request, 409, "{\"ok\":false,\"error\":\"Exit AI Pet before changing AI settings\"}");
        return;
    }

    JsonDocument document(memory::jsonAllocator());
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
    (void)index;
    if (total > AI_CONFIG_BODY_MAX_BYTES) {
        String* body = static_cast<String*>(request->_tempObject);
        if (body == nullptr) {
            body = new String(AI_CONFIG_BODY_TOO_LARGE);
            request->_tempObject = body;
        }
        return;
    }

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
    if (!isConfigured() || isLiveProvider()) {
        setError("HTTP AI pipeline is not configured");
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
    JsonDocument document(memory::jsonAllocator());
    if (deserializeJson(document, responseBody)) return false;
    const char* url = document["audioUrl"] | "";
    if (strlen(url) == 0) return false;
    audioUrl = url;
    return true;
}

bool AIConversation::startLiveSession() {
    if(chatHistory.resetting()){setError("Chat history reset in progress");return false;}
    if (!isLiveProvider()) return false;
    if (!isConfigured()) { setError("Gemini Live requires an API key and model"); return false; }
    if (WiFi.status() != WL_CONNECTED) { setError("WiFi is not connected"); return false; }
    if (liveTaskHandle != nullptr) return true;
    if (!startLivePcmOutput()) { setError("Unable to allocate live speaker buffer"); return false; }

    liveStopRequested.store(false);
    liveSocketConnected.store(false);
    liveSetupComplete.store(false);
    liveRxAudioChunks.store(0);
    liveTxAudioChunks.store(0);
    liveDroppedAudioChunks.store(0);
    liveModelTurnActive.store(false);
    liveGenerationComplete.store(false);
    liveMicResumeAfterMs = 0;
    liveMicSuspended = false;
    liveLastModelEventMs = millis();
    liveSpeakerWasBusy = false;
    liveSessionHandle = "";
    liveGoAwaySeen = false;
    state = "live_connecting";
    lastError = "";

    liveWorkerRunning=true;
    if (xTaskCreatePinnedToCore(liveTaskEntry, "GeminiLive", 18 * 1024, this, 3, &liveTaskHandle, 0) != pdPASS) {
        liveWorkerRunning=false;
        liveTaskHandle = nullptr;
        stopLivePcmOutput();
        setError("Unable to create Gemini Live task");
        return false;
    }
    return true;
}

void AIConversation::stopLiveSession() {
    liveStopRequested.store(true);
    stopLiveMicrophoneStream();
    // Silence the speaker immediately when leaving AI Pet. The task still owns
    // the WebSocket and sends audioStreamEnd + closes it safely on Core 0.
    stopLivePcmOutput();
}

void AIConversation::liveTaskEntry(void* parameter) {
    static_cast<AIConversation*>(parameter)->runLiveSession();
}

void AIConversation::liveSocketEvent(WStype_t type, uint8_t* payload, size_t length) {
    aiConversation.handleLiveSocketEvent(type, payload, length);
}

bool AIConversation::sendLiveSetup() {
    JsonDocument document(memory::jsonAllocator());
    JsonObject setup = document["setup"].to<JsonObject>();
    String modelName = config.model;
    modelName.trim();
    if (!modelName.startsWith("models/")) modelName = String("models/") + modelName;
    setup["model"] = modelName;
    // Per the Live API WebSocket reference, response modalities and speech
    // configuration belong inside setup.generationConfig. systemInstruction
    // remains directly under setup.
    JsonObject generationConfig = setup["generationConfig"].to<JsonObject>();
    JsonArray modalities = generationConfig["responseModalities"].to<JsonArray>();
    modalities.add("AUDIO");
    if (config.voice[0] != '\0') {
        JsonObject voiceConfig = generationConfig["speechConfig"]["voiceConfig"]["prebuiltVoiceConfig"].to<JsonObject>();
        voiceConfig["voiceName"] = config.voice;
    }
    JsonArray parts = setup["systemInstruction"]["parts"].to<JsonArray>();
    JsonObject instructionPart = parts.add<JsonObject>();
    instructionPart["text"] = config.systemInstruction;
    setup["inputAudioTranscription"].to<JsonObject>();
    setup["outputAudioTranscription"].to<JsonObject>();
    // Replay role-labelled conversation only into new sessions, never resumed ones.
    liveInitialHistory=liveSessionHandle.length()?String():chatHistory.context();
    liveHistoryInitial=String(config.model).indexOf("3.1")>=0;
    if(liveInitialHistory.length()>2&&liveHistoryInitial)setup["historyConfig"]["initialHistoryInClientContent"]=true;

    // Keep server-side VAD enabled, but make the interruption policy match the
    // AI Conversation setting. Without this, Gemini defaults to
    // START_OF_ACTIVITY_INTERRUPTS, so speaker echo can cut its own reply even
    // when the local UI says barge-in is disabled.
    JsonObject realtimeConfig = setup["realtimeInputConfig"].to<JsonObject>();
    realtimeConfig["activityHandling"] = config.liveBargeIn
        ? "START_OF_ACTIVITY_INTERRUPTS"
        : "NO_INTERRUPTION";
    JsonObject vad = realtimeConfig["automaticActivityDetection"].to<JsonObject>();
    vad["disabled"] = false;
    vad["startOfSpeechSensitivity"] = "START_SENSITIVITY_LOW";
    vad["endOfSpeechSensitivity"] = "END_SENSITIVITY_LOW";
    vad["prefixPaddingMs"] = 100;
    vad["silenceDurationMs"] = 1000;

    // Ask Gemini for resumable session handles on every connection. If the
    // transport is rotated by the service, reconnect with the latest handle so
    // the conversation context survives the WebSocket change.
    JsonObject resumption = setup["sessionResumption"].to<JsonObject>();
    if (liveSessionHandle.length()) resumption["handle"] = liveSessionHandle;

    // Native audio accumulates context quickly. Sliding-window compression keeps
    // a long-running AI Pet session from hitting the audio-only context lifetime.
    JsonObject compression = setup["contextWindowCompression"].to<JsonObject>();
    compression["slidingWindow"].to<JsonObject>();

    String message;
    serializeJson(document, message);
    const bool sent = liveSocket.sendTXT(message);
    if (sent) {
        liveSetupSentAtMs = millis();
        state = "live_setup";
        Serial.printf("[GEMINI] Setup sent | model=%s | voice=%s | barge-in=%s | activity=%s | resume=%s | compression=sliding\n",
                      modelName.c_str(), config.voice[0] ? config.voice : "default",
                      config.liveBargeIn ? "on" : "off",
                      config.liveBargeIn ? "START_OF_ACTIVITY_INTERRUPTS" : "NO_INTERRUPTION",
                      liveSessionHandle.length() ? "yes" : "new");
    }
    return sent;
}

bool AIConversation::ensureDecodeCapacity(size_t bytes) {
    if (bytes <= liveDecodeCapacity && liveDecodeBuffer) return true;
    size_t target = max(static_cast<size_t>(16 * 1024), bytes + 64);
    target = (target + 4095) & ~static_cast<size_t>(4095);
    uint8_t* next = static_cast<uint8_t*>(heap_caps_malloc(
        target, psramFound() ? (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) : (MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)));
    if (!next && psramFound()) next = static_cast<uint8_t*>(heap_caps_malloc(target, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    if (!next) return false;
    if (liveDecodeBuffer) heap_caps_free(liveDecodeBuffer);
    liveDecodeBuffer = next;
    liveDecodeCapacity = target;
    return true;
}


bool AIConversation::ensureLiveWsFragmentCapacity(size_t bytes) {
    if (bytes > LIVE_WS_FRAGMENT_MAX_BYTES) return false;
    if (liveWsFragmentBuffer && bytes <= liveWsFragmentCapacity) return true;

    size_t target = liveWsFragmentCapacity ? liveWsFragmentCapacity : LIVE_WS_FRAGMENT_INITIAL_BYTES;
    while (target < bytes) {
        if (target >= LIVE_WS_FRAGMENT_MAX_BYTES / 2) {
            target = LIVE_WS_FRAGMENT_MAX_BYTES;
            break;
        }
        target *= 2;
    }
    if (target > LIVE_WS_FRAGMENT_MAX_BYTES) target = LIVE_WS_FRAGMENT_MAX_BYTES;

    const uint32_t caps = psramFound() ? (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
                                       : (MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    uint8_t* next = static_cast<uint8_t*>(heap_caps_malloc(target, caps));
    if (!next && psramFound()) {
        next = static_cast<uint8_t*>(heap_caps_malloc(target, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    }
    if (!next) return false;

    if (liveWsFragmentBuffer && liveWsFragmentLength) {
        memcpy(next, liveWsFragmentBuffer, liveWsFragmentLength);
    }
    if (liveWsFragmentBuffer) heap_caps_free(liveWsFragmentBuffer);
    liveWsFragmentBuffer = next;
    liveWsFragmentCapacity = target;
    return true;
}

bool AIConversation::appendLiveWsFragment(const uint8_t* payload, size_t length) {
    if (!payload || length == 0) return true;
    const size_t required = liveWsFragmentLength + length;
    if (required < liveWsFragmentLength || !ensureLiveWsFragmentCapacity(required)) {
        Serial.printf("[GEMINI] RX fragmented message too large or PSRAM allocation failed | required=%u max=%u\n",
                      static_cast<unsigned>(required), static_cast<unsigned>(LIVE_WS_FRAGMENT_MAX_BYTES));
        resetLiveWsFragment();
        return false;
    }
    memcpy(liveWsFragmentBuffer + liveWsFragmentLength, payload, length);
    liveWsFragmentLength += length;
    return true;
}

void AIConversation::resetLiveWsFragment() {
    liveWsFragmentLength = 0;
    liveWsFragmentActive = false;
}

void AIConversation::releaseLiveWsFragmentBuffer() {
    if (liveWsFragmentBuffer) heap_caps_free(liveWsFragmentBuffer);
    liveWsFragmentBuffer = nullptr;
    liveWsFragmentCapacity = 0;
    liveWsFragmentLength = 0;
    liveWsFragmentActive = false;
}

void AIConversation::releaseLiveDecodeBuffer() {
    if (liveDecodeBuffer) heap_caps_free(liveDecodeBuffer);
    liveDecodeBuffer = nullptr;
    liveDecodeCapacity = 0;
}

bool AIConversation::ensureLiveTxScratchCapacity(size_t bytes) {
    if (liveTxScratchBuffer && bytes <= liveTxScratchCapacity) return true;
    size_t target = (bytes + 4095) & ~static_cast<size_t>(4095);
    const uint32_t caps = psramFound() ? (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
                                       : (MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    uint8_t* next = static_cast<uint8_t*>(heap_caps_malloc(target, caps));
    if (!next && psramFound()) {
        next = static_cast<uint8_t*>(heap_caps_malloc(target, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    }
    if (!next) return false;
    if (liveTxScratchBuffer) heap_caps_free(liveTxScratchBuffer);
    liveTxScratchBuffer = next;
    liveTxScratchCapacity = target;
    return true;
}

void AIConversation::releaseLiveTxScratchBuffer() {
    if (liveTxScratchBuffer) heap_caps_free(liveTxScratchBuffer);
    liveTxScratchBuffer = nullptr;
    liveTxScratchCapacity = 0;
}

void AIConversation::handleLiveServerMessage(uint8_t* payload, size_t length) {
    // Mutable input lets ArduinoJson reference strings in the WebSocket payload rather than
    // duplicating large base64 audio strings into a second heap allocation.
    JsonDocument document(memory::jsonAllocator());
    DeserializationError error = deserializeJson(document, reinterpret_cast<char*>(payload), length);
    if (error) {
        Serial.printf("[GEMINI] JSON parse error: %s (%u bytes)\n", error.c_str(), static_cast<unsigned>(length));
        return;
    }

    if (!document["error"].isNull()) {
        String diagnostic;
        serializeJson(document["error"], diagnostic);
        if (diagnostic.length() > 700) diagnostic = diagnostic.substring(0, 700) + "...";
        Serial.printf("[GEMINI] Server error: %s\n", diagnostic.c_str());
        setError(String("Gemini server error: ") + diagnostic);
        // A stale/invalid resumption handle can make setup fail. Clear it before
        // the next retry so we always have a fresh-session recovery path.
        if (!liveSetupComplete.load() && liveSessionHandle.length()) {
            Serial.println("[GEMINI] Clearing session resumption handle after setup error");
            liveSessionHandle = "";
        }
        liveSocket.disconnect();
        return;
    }

    if (!document["setupComplete"].isNull()) {
        if(liveInitialHistory.length()>2){
            JsonDocument history(memory::jsonAllocator());history["clientContent"]["turns"]=serialized(liveInitialHistory);
            history["clientContent"]["turnComplete"]=liveHistoryInitial;
            String payload;serializeJson(history,payload);
            if(!liveSocket.sendTXT(payload)){setError("Unable to restore chat memory");liveSocket.disconnect();return;}
            Serial.println("[GEMINI] Recent conversation memory restored");
        }
        liveInitialHistory=String();
        liveSetupComplete.store(true);
        liveSetupSentAtMs = 0;
        liveConsecutiveFailures = 0;
        liveReconnectBackoffMs = LIVE_RECONNECT_BASE_MS;
        liveSocket.setReconnectInterval(liveReconnectBackoffMs);
        liveModelTurnActive.store(false);
        liveGenerationComplete.store(false);
        liveMicResumeAfterMs = 0;
        liveMicSuspended = false;
        liveLastModelEventMs = millis();
        liveSpeakerWasBusy = false;
        lastError = "";
        Serial.println("[GEMINI] Setup complete");
        Serial.printf("[MEMORY] Live ready | internal=%u PSRAM=%u largest=%u minimum=%u stackFree=%u TLS=%s\n",ESP.getFreeHeap(),ESP.getFreePsram(),ESP.getMaxAllocHeap(),ESP.getMinFreeHeap(),static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)),memory::tlsUsesPsram()?"PSRAM preferred":"SDK default");
        if (!startLiveMicrophoneStream()) {
            setError("Microphone stream could not start");
            liveStopRequested.store(true);
            return;
        }
        state = "live_listening";
        Serial.printf("[GEMINI] Live ready | model=%s | voice=%s | barge-in=%s\n",
                      config.model, config.voice, config.liveBargeIn ? "on" : "off");
        return;
    }

    if (!document["goAway"].isNull()) {
        liveGoAwaySeen = true;
        String diagnostic;
        serializeJson(document["goAway"], diagnostic);
        Serial.printf("[GEMINI] GoAway received; connection rotation expected: %s\n", diagnostic.c_str());
        return;
    }

    if (!document["sessionResumptionUpdate"].isNull()) {
        JsonObjectConst update = document["sessionResumptionUpdate"];
        const bool resumable = update["resumable"] | false;
        const char* newHandle = update["newHandle"] | "";
        if (resumable && newHandle[0]) liveSessionHandle = newHandle;
        Serial.printf("[GEMINI] Session resumption update | resumable=%s | handle=%s\n",
                      resumable ? "yes" : "no",
                      (resumable && newHandle[0]) ? "updated" : "unchanged");
        return;
    }

    JsonObjectConst serverContent = document["serverContent"];
    if (serverContent.isNull()) {
        if (!liveSetupComplete.load()) {
            String diagnostic;
            serializeJson(document, diagnostic);
            if (diagnostic.length() > 700) diagnostic = diagnostic.substring(0, 700) + "...";
            Serial.printf("[GEMINI] Pre-setup message: %s\n", diagnostic.c_str());
        }
        return;
    }

    chatHistory.transcript(false,serverContent["inputTranscription"]["text"]|"");
    chatHistory.transcript(true,serverContent["outputTranscription"]["text"]|"");
    if (serverContent["interrupted"] | false) {
        chatHistory.finish(true);
        clearLivePcmOutput();
        liveModelTurnActive.store(false);
        liveGenerationComplete.store(false);
        liveMicResumeAfterMs = millis() + 250;
        liveSpeakerWasBusy = false;
        state = "live_listening";
        Serial.println("[GEMINI] Response interrupted; speaker queue cleared");
    }

    JsonArrayConst parts = serverContent["modelTurn"]["parts"].as<JsonArrayConst>();
    for (JsonObjectConst part : parts) {
        JsonObjectConst inlineData = part["inlineData"];
        if (inlineData.isNull()) continue;
        const char* mime = inlineData["mimeType"] | "";
        const char* encoded = inlineData["data"] | "";
        if (!encoded[0] || strncmp(mime, "audio/pcm", 9) != 0) continue;

        const size_t encodedLength = strlen(encoded);
        const size_t decodedMax = (encodedLength / 4) * 3 + 4;
        if (!ensureDecodeCapacity(decodedMax)) {
            liveDroppedAudioChunks.fetch_add(1);
            setError("Not enough memory for Gemini audio chunk");
            continue;
        }
        size_t decodedLength = 0;
        const int result = mbedtls_base64_decode(liveDecodeBuffer, liveDecodeCapacity, &decodedLength,
                                                 reinterpret_cast<const unsigned char*>(encoded), encodedLength);
        if (result != 0 || decodedLength == 0) {
            liveDroppedAudioChunks.fetch_add(1);
            continue;
        }
        liveLastModelEventMs = millis();
        // A 4096-byte PCM block needs ~85 ms at 24 kHz. A 15 ms wait drops
        // nearly every burst once full; wait long enough for the consumer.
        if (!queueLivePcmAudio(liveDecodeBuffer, decodedLength, pdMS_TO_TICKS(250))) {
            liveDroppedAudioChunks.fetch_add(1);
            Serial.println("[GEMINI] Speaker queue full; dropping one response chunk to keep latency bounded");
        } else {
            liveRxAudioChunks.fetch_add(1);
            liveModelTurnActive.store(true);
            liveGenerationComplete.store(false);
            liveSpeakerWasBusy = true;
            state = "live_speaking";
        }
    }

    if (serverContent["generationComplete"] | false) {
        liveLastModelEventMs = millis();
        liveGenerationComplete.store(true);
        Serial.println("[GEMINI] Generation complete; draining speaker buffer");
    }

    if (serverContent["turnComplete"] | false) {
        chatHistory.finish();
        liveModelTurnActive.store(false);
        liveGenerationComplete.store(false);
        // Keep the microphone gated briefly after the final queued samples drain
        // so the speaker tail/reverb is not immediately classified as user speech.
        liveMicResumeAfterMs = millis() + 250;
        state = livePcmHasBufferedAudio() ? "live_speaking" : "live_listening";
        Serial.println("[GEMINI] Turn complete; ready for next user turn");
    }
}

void AIConversation::handleLiveSocketEvent(WStype_t type, uint8_t* payload, size_t length) {
    switch (type) {
        case WStype_CONNECTED:
            liveSocketConnected.store(true);
            liveSetupComplete.store(false);
            liveConnectedAtMs = millis();
            liveSetupSentAtMs = 0;
            Serial.printf("[GEMINI] WebSocket connected | attempt=%u\n",
                          static_cast<unsigned>(liveConsecutiveFailures + 1));
            if (!sendLiveSetup()) {
                setError("Unable to send Gemini Live setup");
                liveSocket.disconnect();
            }
            break;
        case WStype_TEXT:
            if (!liveSetupComplete.load()) {
                Serial.printf("[GEMINI] RX text frame | %u bytes\n", static_cast<unsigned>(length));
            }
            handleLiveServerMessage(payload, length);
            break;
        case WStype_BIN:
            // Gemini's official SDK accepts WebSocket responses as string, Blob,
            // or ArrayBuffer. On ArduinoWebSockets, binary/ArrayBuffer responses
            // arrive as WStype_BIN, so they must go through the same JSON parser.
            if (!liveSetupComplete.load()) {
                Serial.printf("[GEMINI] RX binary frame | %u bytes\n", static_cast<unsigned>(length));
            }
            handleLiveServerMessage(payload, length);
            break;
        case WStype_FRAGMENT_TEXT_START:
        case WStype_FRAGMENT_BIN_START:
            resetLiveWsFragment();
            liveWsFragmentActive = true;
            if (!appendLiveWsFragment(payload, length)) break;
            if (!liveSetupComplete.load()) {
                Serial.printf("[GEMINI] RX fragmented frame start | type=%s first=%u bytes\n",
                              type == WStype_FRAGMENT_BIN_START ? "binary" : "text",
                              static_cast<unsigned>(length));
            }
            break;
        case WStype_FRAGMENT:
            if (liveWsFragmentActive) appendLiveWsFragment(payload, length);
            break;
        case WStype_FRAGMENT_FIN:
            if (liveWsFragmentActive && appendLiveWsFragment(payload, length)) {
                if (!liveSetupComplete.load()) {
                    Serial.printf("[GEMINI] RX fragmented message complete | %u bytes\n",
                                  static_cast<unsigned>(liveWsFragmentLength));
                }
                handleLiveServerMessage(liveWsFragmentBuffer, liveWsFragmentLength);
            }
            resetLiveWsFragment();
            break;
        case WStype_DISCONNECTED: {
            chatHistory.finish(true);
            const bool wasReady = liveSetupComplete.load();
            const bool plannedRotation = liveGoAwaySeen;
            liveGoAwaySeen = false;
            liveSocketConnected.store(false);
            liveSetupComplete.store(false);
            liveSetupSentAtMs = 0;
            liveModelTurnActive.store(false);
            liveGenerationComplete.store(false);
            liveMicResumeAfterMs = 0;
            liveMicSuspended = false;
            liveLastModelEventMs = millis();
            liveSpeakerWasBusy = false;
            stopLiveMicrophoneStream();
            resetLiveWsFragment();

            const String reason = eventPayloadPreview(payload, length);

            if (!liveStopRequested.load()) {
                state = "live_reconnecting";
                if (!wasReady && liveConsecutiveFailures < 10) ++liveConsecutiveFailures;
                const uint8_t rawShift = liveConsecutiveFailures > 0 ? static_cast<uint8_t>(liveConsecutiveFailures - 1) : 0;
                const uint8_t shift = rawShift < 5 ? rawShift : 5;
                uint32_t delayMs = LIVE_RECONNECT_BASE_MS << shift;
                if (delayMs > LIVE_RECONNECT_MAX_MS) delayMs = LIVE_RECONNECT_MAX_MS;
                liveReconnectBackoffMs = delayMs;
                liveSocket.setReconnectInterval(liveReconnectBackoffMs);
                const uint32_t connectedForMs = liveConnectedAtMs ? static_cast<uint32_t>(millis() - liveConnectedAtMs) : 0;
                Serial.printf("[GEMINI] WebSocket disconnected | ready=%s | rotation=%s | resume=%s | connected=%lu ms | TX=%u RX=%u dropped=%u | heap=%u psram=%u | reason=%s | retry=%lu ms\n",
                              wasReady ? "yes" : "no", plannedRotation ? "yes" : "no",
                              liveSessionHandle.length() ? "yes" : "no",
                              static_cast<unsigned long>(connectedForMs),
                              static_cast<unsigned>(liveTxAudioChunks.load()),
                              static_cast<unsigned>(liveRxAudioChunks.load()),
                              static_cast<unsigned>(liveDroppedAudioChunks.load()),
                              static_cast<unsigned>(ESP.getFreeHeap()),
                              static_cast<unsigned>(ESP.getFreePsram()),
                              reason.c_str(),
                              static_cast<unsigned long>(liveReconnectBackoffMs));
            } else {
                Serial.printf("[GEMINI] WebSocket disconnected | reason=%s\n", reason.c_str());
            }
            break;
        }
        case WStype_ERROR: {
            const String reason = eventPayloadPreview(payload, length);
            Serial.printf("[GEMINI] WebSocket error: %s\n", reason.c_str());
            break;
        }
        default:
            break;
    }
}

bool AIConversation::sendLiveMicFrame(const int16_t* samples, size_t sampleCount) {
    if (!samples || !sampleCount || sampleCount > LIVE_MIC_SEND_SAMPLES || !liveSetupComplete.load()) {
        Serial.println("[GEMINI] Microphone packet rejected: invalid samples or session not ready");
        return false;
    }
    const size_t scratchBytes = LIVE_MIC_BASE64_BYTES + LIVE_MIC_JSON_BYTES;
    if (!ensureLiveTxScratchCapacity(scratchBytes)) {
        setError("Not enough PSRAM for Gemini microphone transport");
        return false;
    }

    unsigned char* encoded = liveTxScratchBuffer;
    char* message = reinterpret_cast<char*>(liveTxScratchBuffer + LIVE_MIC_BASE64_BYTES);
    size_t encodedLength = 0;
    const size_t inputBytes = sampleCount * sizeof(int16_t);
    // mbedTLS includes the trailing NUL in the required destination capacity.
    // A full 1536-sample frame needs 4096 encoded characters PLUS one byte.
    const int encodeResult = mbedtls_base64_encode(encoded, LIVE_MIC_BASE64_BYTES, &encodedLength,
                              reinterpret_cast<const unsigned char*>(samples), inputBytes);
    if (encodeResult != 0) {
        Serial.printf("[GEMINI] Microphone Base64 failed | err=%d input=%u capacity=%u required=%u\n",
            encodeResult, static_cast<unsigned>(inputBytes), static_cast<unsigned>(LIVE_MIC_BASE64_BYTES),
            static_cast<unsigned>(encodedLength));
        return false;
    }
    encoded[encodedLength] = 0;

    const int written = snprintf(message, LIVE_MIC_JSON_BYTES,
        "{\"realtimeInput\":{\"audio\":{\"data\":\"%s\",\"mimeType\":\"audio/pcm;rate=16000\"}}}", encoded);
    if (written <= 0 || static_cast<size_t>(written) >= LIVE_MIC_JSON_BYTES) {
        Serial.println("[GEMINI] Microphone JSON exceeded packet capacity");
        return false;
    }
    const bool sent = liveSocket.sendTXT(reinterpret_cast<uint8_t*>(message), static_cast<size_t>(written));
    if (sent) liveTxAudioChunks.fetch_add(1);
    else Serial.printf("[GEMINI] Microphone WebSocket write failed | bytes=%u\n", static_cast<unsigned>(written));
    return sent;
}

void AIConversation::runLiveSession() {
    Serial.printf("[GEMINI] Effective WebSocket frame limit = %u bytes\n", static_cast<unsigned>(WEBSOCKETS_MAX_DATA_SIZE));
    liveSocket.onEvent(liveSocketEvent);
    liveReconnectBackoffMs = LIVE_RECONNECT_BASE_MS;
    liveConsecutiveFailures = 0;
    liveSetupSentAtMs = 0;
    liveConnectedAtMs = 0;
    liveSocket.setReconnectInterval(liveReconnectBackoffMs);
    // Do not use arduinoWebSockets forced heartbeat disconnect here. The prior
    // 4,000 ms pong timeout matched the observed ~3.95-4.00 s disconnects and
    // could tear down an otherwise healthy Gemini stream. Gemini Live already
    // has application traffic plus its own connection lifecycle/GoAway signals.
    liveSocket.disableHeartbeat();
    Serial.println("[GEMINI] Client heartbeat watchdog disabled (Gemini lifecycle is authoritative)");

    String path = String(GEMINI_WS_PATH) + "?key=" + config.apiKey;
    if (config.allowInsecureTLS) {
        // Explicit opt-in for test environments only.
        liveSocket.beginSSL(GEMINI_HOST, GEMINI_PORT, path.c_str());
        Serial.println("[GEMINI] WARNING: TLS certificate verification disabled by setting");
    } else {
        liveSocket.beginSslWithCA(GEMINI_HOST, GEMINI_PORT, path.c_str(), GOOGLE_GTS_ROOTS);
    }

    int16_t micFrame[LIVE_MIC_CAPTURE_SAMPLES];
    int16_t micSendBuffer[LIVE_MIC_SEND_SAMPLES];
    size_t micSendSamples = 0;
    uint32_t capturedMicFrames = 0, lastDiagnosticMs = millis();
    Serial.printf("[GEMINI] Mic transport packet = %u samples (~%u ms)\n",
                  static_cast<unsigned>(LIVE_MIC_SEND_SAMPLES),
                  static_cast<unsigned>((LIVE_MIC_SEND_SAMPLES * 1000UL) / 16000UL));
    while (!liveStopRequested.load()) {
        if (WiFi.status() != WL_CONNECTED) {
            state = "live_waiting_wifi";
            vTaskDelay(pdMS_TO_TICKS(150));
            continue;
        }

        liveSocket.loop();

        if (!liveSetupComplete.load()) micSendSamples = 0;

        if (liveSocketConnected.load() && !liveSetupComplete.load() && liveSetupSentAtMs != 0 &&
            static_cast<uint32_t>(millis() - liveSetupSentAtMs) > LIVE_SETUP_TIMEOUT_MS) {
            Serial.printf("[GEMINI] Setup timeout after %lu ms; closing socket for backoff retry\n",
                          static_cast<unsigned long>(LIVE_SETUP_TIMEOUT_MS));
            setError("Gemini Live setup timed out");
            liveSetupSentAtMs = 0;
            liveSocket.disconnect();
        }

        if (liveSetupComplete.load()) {
            const uint32_t nowMs = millis();
            const bool speakerBusy = livePcmHasBufferedAudio();

            // When the speaker queue transitions from busy -> empty, allow a short
            // acoustic tail guard before reopening the microphone. This prevents
            // the final syllable/reverb from being fed back as a new user turn.
            if (liveSpeakerWasBusy && !speakerBusy) {
                liveMicResumeAfterMs = nowMs + 250;
            }
            liveSpeakerWasBusy = speakerBusy;

            const bool tailGuard = liveMicResumeAfterMs != 0 && static_cast<int32_t>(liveMicResumeAfterMs - nowMs) > 0;
            const bool suppressMic = !config.liveBargeIn &&
                (liveModelTurnActive.load() || speakerBusy || tailGuard);
            if (suppressMic && !liveMicSuspended) {
                // VAD must see a stream boundary when capture is muted. Sending
                // audio later reopens the stream without a new setup/session.
                if (!liveSocket.sendTXT("{\"realtimeInput\":{\"audioStreamEnd\":true}}")) {
                    liveSocket.disconnect();
                    continue;
                }
                liveMicSuspended = true;
                micSendSamples = 0;
                Serial.println("[GEMINI] Microphone paused for response playback");
            } else if (!suppressMic && liveMicSuspended) {
                size_t discarded = 0;
                // Discard buffered speaker echo from before the guard expired.
                for (int i = 0; i < 16 && readLiveMicrophoneFrame(micFrame, LIVE_MIC_CAPTURE_SAMPLES, discarded, 0); ++i) {}
                liveMicSuspended = false;
                micSendSamples = 0;
                Serial.println("[GEMINI] Microphone resumed for next user turn");
            }
            if (liveModelTurnActive.load() && !speakerBusy &&
                static_cast<uint32_t>(nowMs - liveLastModelEventMs) > 30000) {
                // A missing completion event must not leave the microphone muted
                // forever. Recover the session only after playback has drained.
                Serial.println("[GEMINI] Turn stalled with empty speaker queue; reconnecting");
                liveSocket.disconnect();
                continue;
            }

            size_t sampleCount = 0;
            if (readLiveMicrophoneFrame(micFrame, LIVE_MIC_CAPTURE_SAMPLES, sampleCount, 0)) {
                ++capturedMicFrames;
                if (suppressMic) {
                    // Never carry speaker echo / stale samples into the next user turn.
                    micSendSamples = 0;
                } else {
                    size_t sampleOffset = 0;
                    while (sampleOffset < sampleCount) {
                        const size_t room = LIVE_MIC_SEND_SAMPLES - micSendSamples;
                        const size_t remaining = sampleCount - sampleOffset;
                        const size_t copyCount = remaining < room ? remaining : room;
                        memcpy(micSendBuffer + micSendSamples, micFrame + sampleOffset, copyCount * sizeof(int16_t));
                        micSendSamples += copyCount;
                        sampleOffset += copyCount;
                        if (micSendSamples >= LIVE_MIC_SEND_SAMPLES) {
                            if (!sendLiveMicFrame(micSendBuffer, micSendSamples)) {
                                liveDroppedAudioChunks.fetch_add(1);
                                Serial.println("[GEMINI] Microphone send failed; reconnecting");
                                liveSocket.disconnect();
                            }
                            micSendSamples = 0;
                        }
                    }
                }
            }
            if (!speakerBusy && !liveModelTurnActive.load() && state == "live_speaking") state = "live_listening";
            if (static_cast<uint32_t>(nowMs - lastDiagnosticMs) >= 10000) {
                lastDiagnosticMs = nowMs;
                Serial.printf("[GEMINI] Audio flow | captured=%u TX=%u RX=%u queued=%u micPaused=%u modelTurn=%u\n",
                    static_cast<unsigned>(capturedMicFrames), static_cast<unsigned>(liveTxAudioChunks.load()),
                    static_cast<unsigned>(liveRxAudioChunks.load()), static_cast<unsigned>(livePcmBufferedBytes()),
                    static_cast<unsigned>(liveMicSuspended), static_cast<unsigned>(liveModelTurnActive.load()));
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    if (liveSetupComplete.load() && liveSocketConnected.load()) {
        liveSocket.sendTXT("{\"realtimeInput\":{\"audioStreamEnd\":true}}");
        for (int i = 0; i < 3; ++i) { liveSocket.loop(); vTaskDelay(pdMS_TO_TICKS(10)); }
    }
    liveSocket.disconnect();
    stopLiveMicrophoneStream();
    stopLivePcmOutput();
    releaseLiveDecodeBuffer();
    releaseLiveWsFragmentBuffer();
    releaseLiveTxScratchBuffer();
    liveSocketConnected.store(false);
    liveSetupComplete.store(false);
    liveSessionHandle = "";
    liveGoAwaySeen = false;
    state = "idle";
    lastError = "";
    liveTaskHandle = nullptr;
    liveStopRequested.store(false);
    liveWorkerRunning=false;
    Serial.printf("[GEMINI] Live session closed | TX=%u RX=%u dropped=%u\n",
                  static_cast<unsigned>(liveTxAudioChunks.load()),
                  static_cast<unsigned>(liveRxAudioChunks.load()),
                  static_cast<unsigned>(liveDroppedAudioChunks.load()));
    vTaskDelete(nullptr);
}
