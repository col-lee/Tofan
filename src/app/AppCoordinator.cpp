// Coordinates the AI Pet capture cycle, response task and RGB updates.
#include "AppCoordinator.hpp"
#include "../core/SharedResources.hpp"
#include "../display/DisplayManager.hpp"
#include "../network/Network.hpp"
#include "../ai/AIConversation.hpp"
#include "../storage/FileManager.hpp"
#include "../audio/SoundManager.hpp"
#include "../hardware/IOManager.hpp"
#include "../hardware/HardwareManager.hpp"
#include "../hardware/RGBLed.hpp"
#include "../core/GlobalState.hpp"
#include "../core/UserSettings.hpp"
#include <WiFi.h>

AppCoordinator appCoordinator;

namespace {
static constexpr unsigned long AI_PET_RECORDING_MS = 5000;
static constexpr unsigned long AI_PET_FRAME_MS = 33;      // ~30 FPS, enough for a small TFT.
static constexpr unsigned long AI_PET_VOICE_REACTION_MS = 850;
static const char* AI_PET_RECORDING_PATH = "/main/ai_pet_input.wav";

unsigned long lastPetFrame = 0;
unsigned long nextIdleMood = 0;
unsigned long lastVoiceReaction = 0;
unsigned long memoryPressureSince = 0;
float micEnvelope = 0.0f;
float micNoiseFloor = 0.012f;
uint32_t lastMicErrors = 0;
uint8_t idleMoodIndex = 0;
uint8_t touchReactionIndex = 0;

void scheduleIdleMood(unsigned long now) {
    nextIdleMood = now + random(4200, 9000);
}

void setIdleMood() {
    // Cycle with a little randomness so the pet doesn't look like a repeating pattern.
    static const ui::PetMood moods[] = {
        ui::PetMood::Neutral,
        ui::PetMood::Happy,
        ui::PetMood::Curious,
        ui::PetMood::Playful,
        ui::PetMood::Shy,
        ui::PetMood::Proud,
        ui::PetMood::Sleepy,
        ui::PetMood::Grumpy
    };
    static const char* lines[] = {
        "hmm~", "nice day!", "what's that?", "catch me!",
        "hehe...", "all good!", "so sleepy...", "hmph~"
    };

    const size_t count = sizeof(moods) / sizeof(moods[0]);
    idleMoodIndex = static_cast<uint8_t>((idleMoodIndex + 1 + random(0, 3)) % count);
    DISM.setPetMood(moods[idleMoodIndex], lines[idleMoodIndex], random(1700, 3000));
}

static void processAIPetVoice(void* parameter) {
    String responseBody;
    String audioUrl;
    bool success = aiConversation.submitAudioFile(AI_PET_RECORDING_PATH, responseBody);
    if (success && aiConversation.extractAudioUrl(responseBody, audioUrl)) {
        AUDIO_COMMAND command{};
        command.module = AUDIO_COMMAND::MODULE::AUDIO;
        command.audio_state = AUDIO_COMMAND::AUDIO_STATE::PLAY;
        command.path = audioUrl;
        xQueueSend(audio_command, &command, portMAX_DELAY);
        Serial.println("AI voice response queued for playback");
    } else {
        Serial.println("AI voice response did not contain a playable audioUrl");
    }
    app::runtime.aiPetProcessing = false;
    vTaskDelete(nullptr);
}
} // namespace

void AppCoordinator::begin() {
    Serial.println("AppCoordinator begin");
    rgbLed.begin();
    aiConversation.begin();
    DISM.currentState = UI_STATE::HOME_MENU;
    DISM.drawHomeMenu();
}

void AppCoordinator::update() {
    rgbLed.update();

    if (app::runtime.aiPetListening) {
        updateAiPetListening();
    }

    if (DISM.currentState == UI_STATE::APP_PET) {
        const unsigned long now = millis();
        updateAiPetBehavior();
        if (now - lastPetFrame >= AI_PET_FRAME_MS) {
            lastPetFrame = now;
            DISM.drawAIPet();
        }
    }
}

void AppCoordinator::updateAiPetBehavior() {
    const unsigned long now = millis();

    // Whole capture frames avoid flickering on individual waveform samples.
    const float sample = getMicrophoneVoiceLevel();
    micEnvelope = micEnvelope * 0.82f + sample * 0.18f;
    if (micEnvelope < micNoiseFloor * 2.0f) {
        micNoiseFloor = micNoiseFloor * 0.995f + micEnvelope * 0.005f;
    }
    if (micNoiseFloor < 0.006f) micNoiseFloor = 0.006f;
    DISM.petSpeaking = app::runtime.aiPetListening && aiConversation.isLiveProvider() && livePcmHasBufferedAudio();
    DISM.petSpeechLevel = getLiveSpeechLevel() * 10.0f;
    if (DISM.petSpeechLevel > 1.0f) DISM.petSpeechLevel = 1.0f;
    DISM.setPetVoiceLevel(DISM.petSpeaking ? 0.0f : micEnvelope * 6.0f);

    // TLS and Live audio buffers legitimately reduce free memory after connection.
    // A percentage of the pre-connection high-water mark falsely reports pressure
    // for the entire session. Only sustained low remaining memory is a warning.
    const uint32_t freeHeap = ESP.getFreeHeap();
    const uint32_t freePsram = ESP.getFreePsram();
    const bool dangerouslyLowHeap = freeHeap < 36u * 1024u;
    const bool psramPressure = psramFound() && freePsram < 384u * 1024u;
    const bool stressed = dangerouslyLowHeap || psramPressure;

    if (stressed) {
        if (!memoryPressureSince) memoryPressureSince = now;
    } else {
        memoryPressureSince = 0;
    }

    // Highest-priority machine/environment reactions.
    if (app::runtime.aiPetProcessing) {
        DISM.setPetMood(ui::PetMood::Thinking, "thinking...", 500);
        return;
    }
    if (memoryPressureSince && now - memoryPressureSince > 2200) {
        DISM.setPetMood(ui::PetMood::Tired, "so busy... tiny break?", 900);
        return;
    }

    const uint32_t micErrors = getMicrophoneReadErrors();
    if (micErrors > lastMicErrors) {
        lastMicErrors = micErrors;
        DISM.setPetMood(ui::PetMood::Dizzy, "my ears went bzz~", 1400);
        return;
    }

    if (DISM.petSpeaking && (!DISM.isPetMoodHeld() || DISM.petMood == ui::PetMood::Listening)) {
        DISM.setPetMood(ui::PetMood::Happy, "talking~", 500);
        return;
    }
    if (app::runtime.aiPetListening && !DISM.isPetMoodHeld()) {
        DISM.setPetMood(ui::PetMood::Listening, "I'm listening~", 650);
        return;
    }

    // User speech reaction works even when no cloud AI is configured.
    const float voiceThreshold = micNoiseFloor * 4.0f > 0.055f ? micNoiseFloor * 4.0f : 0.055f;
    const bool heardVoice = isMicrophoneReady() && micEnvelope > voiceThreshold;
    if (heardVoice && now - lastVoiceReaction > AI_PET_VOICE_REACTION_MS && !DISM.isPetMoodHeld()) {
        static const char* heard[] = {"hm?", "I hear you~", "mhm~", "say more?", "oh!"};
        const int i = random(0, static_cast<int>(sizeof(heard) / sizeof(heard[0])));
        DISM.setPetMood(i == 4 ? ui::PetMood::Surprised : ui::PetMood::Listening, heard[i], 1000);
        lastVoiceReaction = now;
        scheduleIdleMood(now);
        return;
    }

    // Do not overwrite a recent touch/voice reaction with background moods.
    if (DISM.isPetMoodHeld()) return;
    if (DISM.petMoodUntil != 0) {
        // A temporary reaction just expired: settle back to a calm face first.
        DISM.setPetMood(ui::PetMood::Neutral, "hmm~", 0);
    }

    if (isPlayingAudio) {
        DISM.setPetMood(ui::PetMood::Dancing, "tiny dance~", 900);
        return;
    }

    if (!isMicrophoneReady()) {
        DISM.setPetMood(ui::PetMood::Grumpy, "where are my ears?", 1200);
        return;
    }

    if (userSettings.values.wifi && WiFi.status() != WL_CONNECTED && !networkSettingsBusy()) {
        if (now >= nextIdleMood) {
            DISM.setPetMood(ui::PetMood::Curious, "where's Wi-Fi?", 1600);
            scheduleIdleMood(now);
        }
        return;
    }

    if (!isConnectSDcard) {
        if (now >= nextIdleMood) {
            DISM.setPetMood(ui::PetMood::Curious, "where's my SD?", 1600);
            scheduleIdleMood(now);
        }
        return;
    }

    if (!nextIdleMood) scheduleIdleMood(now);
    if (now >= nextIdleMood) {
        setIdleMood();
        scheduleIdleMood(now);
    }
}

void AppCoordinator::reactToAiPetRotation(int steps) {
    if (!steps || DISM.currentState != UI_STATE::APP_PET) return;
    const unsigned long now = millis();
    const bool quick = DISM.petLookUntil &&
        static_cast<int32_t>(DISM.petLookUntil - now) > 1380;
    DISM.petLookDirection = steps > 0 ? 1 : -1;
    DISM.petLookUntil = now + 1500;
    ++DISM.petInteractionCount;
    // Keep cloud processing feedback intact; looking still responds locally.
    if (!app::runtime.aiPetProcessing) {
        const bool excited = quick || steps >= 4 || steps <= -4;
        DISM.setPetMood(excited ? ui::PetMood::Excited : ui::PetMood::Playful,
                        excited ? "wheee!" : steps > 0 ? "over here?" : "this way?", 1500);
    }
    scheduleIdleMood(now);
}

void AppCoordinator::reactToAiPetTouch() {
    const unsigned long now = millis();
    ++DISM.petInteractionCount;

    if (app::runtime.aiPetProcessing) {
        DISM.setPetMood(ui::PetMood::Thinking, "one sec~", 1200);
        return;
    }

    static const ui::PetMood moods[] = {
        ui::PetMood::Happy,
        ui::PetMood::Shy,
        ui::PetMood::Playful,
        ui::PetMood::Excited,
        ui::PetMood::Dizzy,
        ui::PetMood::Proud,
        ui::PetMood::Surprised
    };
    static const char* lines[] = {
        "hehe!", "boop~", "again!", "yay!", "whoa~", "I like that!", "oh!"
    };

    const size_t count = sizeof(moods) / sizeof(moods[0]);
    touchReactionIndex = static_cast<uint8_t>((touchReactionIndex + 1 + random(0, 2)) % count);
    DISM.setPetMood(moods[touchReactionIndex], lines[touchReactionIndex], 1500);
    scheduleIdleMood(now);
}

void AppCoordinator::startAiPetListening() {
    if (app::runtime.aiPetListening || app::runtime.aiPetProcessing || !aiConversation.isConfigured()) return;

    // Gemini Live keeps one bidirectional voice session open for the entire time
    // AI Pet is on screen. No temporary WAV file or fixed 5-second turn is needed.
    if (aiConversation.isLiveProvider()) {
        if (!aiConversation.startLiveSession()) {
            DISM.setPetMood(ui::PetMood::Dizzy, "can't reach Gemini...", 1800);
            Serial.println("Unable to start Gemini Live AI Pet session");
            return;
        }
        app::runtime.aiPetListening = true;
        app::runtime.aiPetProcessing = false;
        DISM.setPetMood(ui::PetMood::Listening, "connecting ears~", 1200);
        Serial.println("AI Pet Gemini Live session starting...");
        return;
    }

    // Legacy HTTP backend: record a short WAV, POST it, then play returned audioUrl.
    if (!enterRecordingMode() || !startRecording(AI_PET_RECORDING_PATH)) {
        exitRecordingMode();
        Serial.println("Unable to start AI Pet recording");
        return;
    }
    app::runtime.aiPetListening = true;
    app::runtime.aiPetRecordingStartedAt = millis();
    DISM.setPetMood(ui::PetMood::Listening, "I'm listening~", 1200);
    Serial.println("AI Pet listening...");
}

void AppCoordinator::updateAiPetListening() {
    if (!app::runtime.aiPetListening) return;

    if (aiConversation.isLiveProvider()) {
        // The Gemini task owns the WebSocket/microphone queue. Keep the pet page
        // responsive here; reconnection is handled inside AIConversation.
        if (aiConversation.isLiveSessionReady() && !livePcmHasBufferedAudio() && !DISM.isPetMoodHeld()) {
            DISM.setPetMood(ui::PetMood::Listening, "I'm listening~", 500);
        }
        return;
    }

    recordLoop();
    if (millis() - app::runtime.aiPetRecordingStartedAt >= AI_PET_RECORDING_MS) {
        exitRecordingMode();
        app::runtime.aiPetListening = false;
        app::runtime.aiPetProcessing = true;
        DISM.setPetMood(ui::PetMood::Thinking, "let me think...", 1500);
        if (xTaskCreatePinnedToCore(processAIPetVoice, "AIPetVoice", 8192, nullptr, 3, nullptr, 0) != pdPASS) {
            app::runtime.aiPetProcessing = false;
            DISM.setPetMood(ui::PetMood::Dizzy, "brain hiccup...", 1600);
            Serial.println("Unable to create AI Pet task");
        }
    }
}

void AppCoordinator::stopAiPetListening() {
    if (aiConversation.isLiveProvider()) {
        aiConversation.stopLiveSession();
    } else if (app::runtime.aiPetListening) {
        exitRecordingMode();
    }
    app::runtime.aiPetListening = false;
    app::runtime.aiPetProcessing = false;
    DISM.setPetVoiceLevel(0.0f);
}
