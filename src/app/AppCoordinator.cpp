#include "AppCoordinator.hpp"
#include "../extensions/GlobalVar.hpp"
#include "../extensions/DisplayManager.hpp"
#include "../extensions/Network.hpp"
#include "../extensions/AIConversation.hpp"
#include "../extensions/FileManager.hpp"
#include "../extensions/SoundManager.hpp"
#include "../extensions/IOManager.hpp"
#include "../extensions/HardwareManager.hpp"
#include "../extensions/RGBLed.hpp"
#include "../core/GlobalState.hpp"

AppCoordinator appCoordinator;

namespace {
static constexpr unsigned long AI_PET_RECORDING_MS = 5000;
static const char* AI_PET_RECORDING_PATH = "/main/ai_pet_input.wav";

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
    DISM.currentState = UI_STATE::APP_PET;
    startAiPetListening();
    DISM.drawAIPet();
}

void AppCoordinator::update() {
    rgbLed.update();

    if (app::runtime.aiPetListening) {
        updateAiPetListening();
    }

    if (DISM.currentState == UI_STATE::APP_PET) {
        if (millis() - DISM.lastMoodChange > 3000) {
            DISM.petMood = random(0, 3);
            DISM.lastMoodChange = millis();
        }
        DISM.drawAIPet();
        vTaskDelay(10);
    }
}

void AppCoordinator::startAiPetListening() {
    if (app::runtime.aiPetListening || app::runtime.aiPetProcessing || !aiConversation.isConfigured()) return;
    if (!enterRecordingMode() || !startRecording(AI_PET_RECORDING_PATH)) {
        exitRecordingMode();
        Serial.println("Unable to start AI Pet recording");
        return;
    }
    app::runtime.aiPetListening = true;
    app::runtime.aiPetRecordingStartedAt = millis();
    Serial.println("AI Pet listening...");
}

void AppCoordinator::updateAiPetListening() {
    if (app::runtime.aiPetListening) {
        recordLoop();
        if (millis() - app::runtime.aiPetRecordingStartedAt >= AI_PET_RECORDING_MS) {
            stopRecording();
            app::runtime.isRecordingMode = false;
            app::runtime.aiPetListening = false;
            app::runtime.aiPetProcessing = true;
            if (xTaskCreatePinnedToCore(processAIPetVoice, "AIPetVoice", 8192, nullptr, 3, nullptr, 0) != pdPASS) {
                app::runtime.aiPetProcessing = false;
                Serial.println("Unable to create AI Pet task");
            }
        }
    }
}

void AppCoordinator::stopAiPetListening() {
    if (app::runtime.aiPetListening) stopRecording();
    app::runtime.aiPetListening = false;
    app::runtime.isRecordingMode = false;
}
