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

// Volume Pin
// #define LEVEL_READ 32

extern Audio audio;

void initAudio();
void handleAudio(void *parameter);