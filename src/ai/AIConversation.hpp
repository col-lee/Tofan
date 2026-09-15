// Persists AI configuration, supports the legacy HTTP WAV pipeline and Gemini Live voice sessions.
#ifndef AI_CONVERSATION_HPP
#define AI_CONVERSATION_HPP

#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <ESPAsyncWebServer.h>
#include <WebSocketsClient.h>
static_assert(WEBSOCKETS_MAX_DATA_SIZE >= 131072, "Gemini Live requires the patched WebSocket payload limit");
#include <atomic>

struct AIServiceConfig {
    bool enabled = false;
    bool allowInsecureTLS = false;
    bool liveBargeIn = false;
    char pipelineUrl[192] = "";
    char apiKey[160] = "";
    char user[64] = "";
    char password[64] = "";
    char provider[24] = "gemini-live";
    char model[64] = "gemini-3.1-flash-live-preview";
    char voice[32] = "Kore";
    char systemInstruction[512] = "You are ToFan, a friendly desk companion. Respond naturally and briefly. If the user speaks Thai, reply in Thai.";
};

class AIConversation {
public:
    bool begin();
    bool loadConfig();
    bool saveConfig(const JsonDocument& document);
    void registerWebRoutes(AsyncWebServer& webServer);
    String getConfigJson(bool includeSecrets = false) const;
    String getStatusJson() const;

    // Legacy HTTP pipeline.
    bool submitAudioFile(const char* path, String& responseBody);
    bool extractAudioUrl(const String& responseBody, String& audioUrl) const;

    // Gemini Live voice-only session.
    bool startLiveSession();
    void stopLiveSession();
    bool isLiveProvider() const;
    bool isLiveSessionActive() const;
    bool isLiveSessionReady() const;
    bool isLiveResponseActive() const;
    bool isConfigured() const;
    bool hasLiveWorker() const {return liveWorkerRunning.load();}

private:
    Preferences preferences;
    AIServiceConfig config;
    std::atomic<const char*> state{"idle"};
    mutable portMUX_TYPE errorMux = portMUX_INITIALIZER_UNLOCKED;
    char lastError[192] = "";

    WebSocketsClient liveSocket;
    TaskHandle_t liveTaskHandle = nullptr;
    std::atomic<bool> liveWorkerRunning{false};
    String liveInitialHistory;
    bool liveHistoryInitial=false;
    std::atomic<bool> liveStopRequested{false};
    std::atomic<bool> liveSocketConnected{false};
    std::atomic<bool> liveSetupComplete{false};
    std::atomic<uint32_t> liveRxAudioChunks{0};
    std::atomic<uint32_t> liveTxAudioChunks{0};
    std::atomic<uint32_t> liveDroppedAudioChunks{0};
    std::atomic<bool> liveModelTurnActive{false};
    std::atomic<bool> liveGenerationComplete{false};
    uint32_t liveMicResumeAfterMs = 0;
    bool liveSpeakerWasBusy = false;
    bool liveMicSuspended = false;
    uint32_t liveLastModelEventMs = 0;
    uint32_t liveSetupSentAtMs = 0;
    uint32_t liveConnectedAtMs = 0;
    uint32_t liveReconnectBackoffMs = 1500;
    uint8_t liveConsecutiveFailures = 0;
    uint8_t* liveDecodeBuffer = nullptr;
    size_t liveDecodeCapacity = 0;
    uint8_t* liveTxScratchBuffer = nullptr;
    size_t liveTxScratchCapacity = 0;
    uint8_t* liveWsFragmentBuffer = nullptr;
    size_t liveWsFragmentCapacity = 0;
    size_t liveWsFragmentLength = 0;
    bool liveWsFragmentActive = false;
    String liveSessionHandle;
    bool liveGoAwaySeen = false;
    bool liveHistoryInterrupted = false;
    bool liveHistoryModelTranscriptSeen = false;

    void setError(const String& message);
    void clearError();
    String errorSnapshot() const;
    static void sendJson(AsyncWebServerRequest* request, int statusCode, const String& body);
    void handleConfigRequest(AsyncWebServerRequest* request);
    void handleConfigBody(AsyncWebServerRequest* request, uint8_t* data, size_t length, size_t index, size_t total);

    static void liveTaskEntry(void* parameter);
    static void liveSocketEvent(WStype_t type, uint8_t* payload, size_t length);
    void runLiveSession();
    void handleLiveSocketEvent(WStype_t type, uint8_t* payload, size_t length);
    bool sendLiveSetup();
    bool sendLiveMicFrame(const int16_t* samples, size_t sampleCount);
    bool ensureDecodeCapacity(size_t bytes);
    void handleLiveServerMessage(uint8_t* payload, size_t length);
    bool ensureLiveWsFragmentCapacity(size_t bytes);
    bool appendLiveWsFragment(const uint8_t* payload, size_t length);
    void resetLiveWsFragment();
    void releaseLiveWsFragmentBuffer();
    void releaseLiveDecodeBuffer();
    bool ensureLiveTxScratchCapacity(size_t bytes);
    void releaseLiveTxScratchBuffer();
};

extern AIConversation aiConversation;

#endif
