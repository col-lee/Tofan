// Owns speaker playback, I2S capture, WAV recording and local voice inference.
#pragma once
#ifndef SOUNDMANAGER_H
#define SOUNDMANAGER_H

#include <Audio.h>
#include <driver/i2s.h>
#include "../core/SharedResources.hpp"

#ifndef ENCODER_VALUE
#define ENCODER_VALUE
    String getCurrentSongTitle();
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

// MJPEG files do not contain audio. Video playback uses a same-name
// companion audio file stored beside the .mjpeg/.mjpg file.
bool startVideoCompanionAudio(const String& path);
void stopVideoCompanionAudio();
bool videoCompanionAudioActive();
uint32_t videoCompanionAudioClockMs();

void setVoiceAssistantEnabled(bool enabled);
void initMicrophone();
int16_t readMicData();
void handleAudio(void *parameter);
bool enterRecordingMode();
void exitRecordingMode();
// No path: create a new numbered recording. Explicit paths are scratch files (AI Pet).
bool startRecording(const char* path = nullptr);
String getRecordingName();
void recordLoop();
void stopRecording();
bool isMicrophoneReady();
bool isMicrophoneCapturing();
unsigned long getMicrophoneLastSampleMillis();
uint32_t getMicrophoneReadErrors();
uint32_t getRecordingDroppedFrames();

// Gemini Live / realtime voice transport. Microphone frames are 16-bit mono
// PCM at 16 kHz; model output is queued as 16-bit mono PCM at 24 kHz.
bool startLiveMicrophoneStream();
void stopLiveMicrophoneStream();
bool readLiveMicrophoneFrame(int16_t* output, size_t capacitySamples, size_t& sampleCount, TickType_t timeout = 0);

bool startLivePcmOutput();
void stopLivePcmOutput();
void clearLivePcmOutput();
bool queueLivePcmAudio(const uint8_t* data, size_t length, TickType_t timeout = 0);
void finishLivePcmInput();
bool livePcmHasBufferedAudio();
bool livePcmIsBuffering();
size_t livePcmBufferedBytes();
uint32_t getLivePcmUnderruns();
float getMicrophoneVoiceLevel();
float getLiveSpeechLevel();

void detectWord();

void audio_info(const char *info);
void audio_id3data(const char *info);
void audio_eof_mp3(const char *info);
void audio_eof_wav(const char *info);
void audio_showstation(const char *info);
void audio_showstreamtitle(const char *info);

#endif

uint32_t getMusicSdWaits();
uint32_t getMusicMaxServiceGapMs();
