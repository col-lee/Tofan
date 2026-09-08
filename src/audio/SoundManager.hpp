// Owns speaker playback, I2S capture, WAV recording and local voice inference.
#pragma once
#ifndef SOUNDMANAGER_H
#define SOUNDMANAGER_H

#include <Audio.h>
#include <driver/i2s.h>
#include "../core/SharedResources.hpp"

#ifndef ENCODER_VALUE
#define ENCODER_VALUE
    extern String currentSongTitle;
    extern bool isPlayingAudio;
    extern bool isOnlineAudio;
    extern bool hasPausedAudio;
    extern const char* onlineStationNames[];
    extern int currentAudioProgress;
    extern unsigned long lastProgressUpdate;
    uint32_t consumeFinishedTrack();
    int consumeStartedTrack();
    extern uint32_t currentAudioTime;
    extern uint32_t totalAudioDuration;
    extern int currentStationIndex;
    extern String onlineStations[];
    extern const int MAX_STATIONS;
#endif

extern Audio audio;
extern bool isAudio_install;

void initAudio();
void setOutputVolume(int percent);
void setVoiceAssistantEnabled(bool enabled);
void initMicrophone();
int16_t readMicData();
void handleAudio(void *parameter);
bool enterRecordingMode();
void exitRecordingMode();
// No path: create a new numbered recording. Explicit paths are scratch files (AI Pet).
bool startRecording(const char* path = nullptr);
const String& getRecordingName();
void recordLoop();
void stopRecording();
bool isMicrophoneReady();
bool isMicrophoneCapturing();
unsigned long getMicrophoneLastSampleMillis();
uint32_t getMicrophoneReadErrors();
uint32_t getRecordingDroppedFrames();

void detectWord();

void audio_info(const char *info);
void audio_id3data(const char *info);
void audio_eof_mp3(const char *info);
void audio_eof_wav(const char *info);
void audio_showstation(const char *info);
void audio_showstreamtitle(const char *info);

#endif
