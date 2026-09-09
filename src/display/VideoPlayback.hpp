#pragma once

#include <Arduino.h>

// Raw MJPEG video player for ESP32-S3 + LovyanGFX.
//
// Supported input:
//   - .mjpeg / .mjpg files containing consecutive baseline JPEG frames
//   - up to 640x480 per frame
//
// Lifecycle:
//   openVideo(path) -> call advanceVideo() repeatedly -> closeVideo()
//
// advanceVideo() is non-blocking with respect to frame timing. It returns true
// while playback can continue and false after a fatal decode/open error.
bool openVideo(const String& path);
bool advanceVideo();
void closeVideo();

// Runtime status helpers for UI/debug screens.
bool videoIsPlaying();
uint32_t videoFramesDecoded();
