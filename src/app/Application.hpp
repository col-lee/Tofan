// Initializes devices and background tasks, then dispatches application updates.
#pragma once

// Application lifecycle called by the Arduino entry points.
namespace app {
void begin();
void update();
} // namespace app
