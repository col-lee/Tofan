#pragma once
#ifndef SOUNDMANAGER_H
#define SOUNDMANAGER_H

#include <Audio.h>
#include <driver/i2s.h>
#include "GlobalVar.hpp"

#ifndef ENCODER_VALUE
#define ENCODER_VALUE
    extern String currentSongTitle;
    extern bool isPlayingAudio;
    extern int currentAudioProgress; 
    extern unsigned long lastProgressUpdate;
    extern bool autoPlayNext;
    extern uint32_t currentAudioTime;
    extern uint32_t totalAudioDuration;
    extern int currentStationIndex;
    extern String onlineStations[];
    extern const int MAX_STATIONS;
#endif

extern Audio audio;
extern bool isAudio_install;

void initAudio();
void initMicrophone();
int16_t readMicData();
void handleAudio(void *parameter);
bool enterRecordingMode();
void exitRecordingMode();
bool startRecording(const char* path);
void recordLoop();
void stopRecording();
bool isMicrophoneReady();
bool isMicrophoneCapturing();
unsigned long getMicrophoneLastSampleMillis();
uint32_t getMicrophoneReadErrors();
uint32_t getRecordingDroppedFrames();

void detectWord();

// ประกาศฟังก์ชัน Callback ของไลบรารี I2S
void audio_info(const char *info);
void audio_id3data(const char *info);
void audio_eof_mp3(const char *info);
void audio_eof_wav(const char *info);
void audio_showstation(const char *info);
void audio_showstreamtitle(const char *info);

#endif
