#include <Audio.h>
#include "GlobalVar.hpp"

// Audio pin
#define RLC_PIN 26
#define BLCK_PIN 33
#define DIN_PIN 25

// Volume Pin
#define LEVEL_READ 32

extern Audio audio;

void initAudio();
void handleAudio(void *parameter);