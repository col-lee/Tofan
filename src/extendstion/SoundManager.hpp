#pragma once
#ifndef SOUNDMANAGER_H
#define SOUNDMANAGER_H

#include <Audio.h>
#include <driver/i2s.h>
#include "GlobalVar.hpp"

#define LED_PIN 18

// Audio pin
#define RLC_PIN 5
#define BLCK_PIN 4
#define DIN_PIN 6

// Microphone Pin
#define MIC_I2S_PORT I2S_NUM_1
#define MIC_WS_PIN   2
#define MIC_SCK_PIN  17
#define MIC_SD_PIN   1

#define SAMPLE_RATE 16000

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
void startRecording(const char* path);
void recordLoop();
void stopRecording();

void detectWord();
static void audio_inference_callback(uint32_t n_bytes);
static void capture_samples(void* arg);
static bool microphone_inference_start(uint32_t n_samples);
static bool microphone_inference_record(void);
static int microphone_audio_signal_get_data(size_t offset, size_t length, float *out_ptr);
static void microphone_inference_end(void);
static int i2s_init(uint32_t sampling_rate);
static int i2s_deinit(void);

// ประกาศฟังก์ชัน Callback ของไลบรารี I2S
void audio_info(const char *info);
void audio_id3data(const char *info);
void audio_eof_mp3(const char *info);
void audio_eof_wav(const char *info);
void audio_showstation(const char *info);
void audio_showstreamtitle(const char *info);

#endif