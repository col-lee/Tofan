#ifndef SOUNDMANAGER_H
#define SOUNDMANAGER_H

#include <Audio.h>
#include "GlobalVar.hpp"

// Audio pin
#define RLC_PIN 26
#define BLCK_PIN 33
#define DIN_PIN 25

#ifndef ENCODER_VALUE
#define ENCODER_VALUE
    extern volatile int encoderValue;
#endif

extern Audio audio;
extern bool isAudio_install;

void initAudio();
void handleAudio(void *parameter);

// ประกาศฟังก์ชัน Callback ของไลบรารี I2S
void audio_info(const char *info);
void audio_id3data(const char *info);
void audio_eof_mp3(const char *info);

#endif