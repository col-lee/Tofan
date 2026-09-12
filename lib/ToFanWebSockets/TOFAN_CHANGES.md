# Local WebSockets 2.7.3

Source copied from the installed Links2004 WebSockets 2.7.3 package. Original
LGPL-2.1 license and source headers are retained.

The ESP32/ESP8266 branch in `src/WebSockets.h` now guards its default
`WEBSOCKETS_MAX_DATA_SIZE` with `#ifndef`. Upstream unconditionally defines 15 KiB,
overwriting this project's 128 KiB compiler flag and closing larger frames with
WebSocket code 1009. The receive payload allocation in `WebSockets.cpp` also
prefers ESP32 PSRAM, falling back to malloc, so larger audio messages do not
compete with TLS for internal heap. Other platforms retain upstream behavior.

PlatformIO uses this local library instead of downloading an unpatched copy.
`AIConversation.hpp` asserts the effective limit at compile time.
