#pragma once
#include <Arduino.h>

bool openVideo(const String& path);
bool advanceVideo();
void closeVideo();

// Diagnostics for the display/debug layer.
bool videoIsPlaying();
uint32_t videoFramesDecoded();
