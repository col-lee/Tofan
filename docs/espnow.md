# ESP-NOW connections

Sign in to the ToFan web portal, then open **ESP-NOW**. The page displays the device's **STA MAC address**, current radio channel, and up to eight slave devices. Add a name and the slave's STA MAC, enable the peer and ESP-NOW, then save. Edit the fields to change a peer; removal requires a modal confirmation and Save. Settings persist in NVS under `tofan-espnow/config`.

The TFT status screen is under **Settings → Network → ESP-NOW status** and shows radio readiness, channel, online peer count, TX/RX and failed sends. Press Back to return to Network settings. Configure peers on the authenticated web page.

ESP-NOW shares the current Wi-Fi channel with the router/access point. It does not retune an active connection. Set the slave to the channel shown on ToFan, and update it if the router changes channel. Enabling ESP-NOW enables the Wi-Fi radio even without a router connection. Peer packets are unencrypted in this implementation.

“Radio ready” means ESP-NOW initialized. TX counts successful MAC-layer sends and does not prove that the slave application processed a command. A peer is online only after a valid inbound ToFan packet within the last 15 seconds. Heartbeat probes cycle through the configured peers; the included slave responds with Pong. Unknown/disabled peers and malformed/version-mismatched packets are ignored. RX includes heartbeat traffic; the latest Data payload is displayed separately as hexadecimal. There is no automatic retry of application commands or guarantee of exactly-once delivery.

## Application API

Include `src/network/EspNowManager.hpp`:

```cpp
uint8_t slave[6] = {0x02,0x11,0x22,0x33,0x44,0x55};
uint8_t payload[] = {0x01,0x00,0xFF};
bool queued = espnow::sendPacket(slave, payload, sizeof(payload));

void received(const uint8_t mac[6], const uint8_t* bytes, size_t length) {
    // Copy data or enqueue application work here. Do not retain bytes/mac pointers.
    // Keep this function short: it runs on runNet, not on the Wi-Fi callback.
}
// Register once during application initialization:
espnow::setReceiver(received);
```

`sendPacket()` accepts binary payloads of 0–200 bytes. `true` means queued, not delivered; inspect TX/RX/failure counters. The peer must already be enabled and registered. Wi-Fi callbacks only copy packets to a bounded queue; payload queues and peer storage prefer PSRAM. No additional task stack is allocated. ESP-NOW is stopped before the network worker changes Wi-Fi mode, then peers are registered again.

Authenticated routes are `GET /api/espnow`, `POST /api/espnow` (form field `values`, JSON containing boolean `enabled` and `peers` array), and `POST /api/espnow/send` (`mac`, hexadecimal `hex`). Mutation routes require the existing portal client header and reject firmware-update overlap. The GET route uses `Cache-Control: no-store`. Settings/send requests return 202 for queued work; poll `revision` and `error` for the worker result.

## Wire format and slave example

All fields use explicit bytes rather than native C++ struct layout:

| Bytes | Content |
| --- | --- |
| 0–1 | ASCII `TF` |
| 2 | Version 1 |
| 3 | 1 = Data, 2 = Ping, 3 = Pong |
| 4–7 | uint32 sequence, little endian |
| 8–9 | uint16 payload length, little endian |
| 10 onward | Payload, max 200 bytes; Ping/Pong must be empty |

Open `examples/espnow_slave/espnow_slave.ino` with its adjacent `EspNowProtocol.hpp` using ESP32 Arduino 2.x (ESP-IDF 4.4). Set `masterMac` from the signed-in ToFan page and `channel` to the displayed channel. Flash the slave and add the slave STA MAC printed by its Serial monitor to ToFan. The example answers heartbeat and echoes Data packets. Replace the echo handler with your sensor/actuator logic. Arduino 3.x has different ESP-NOW callback signatures and requires adapting the example.

Native tests compile the production manager with mocked radio/NVS to test persistence, validation, radio restart, liveness, and packet handling. Browser tests cover login gating, peer CRUD, modal confirmation, send/receive display, and mobile layout. These do not replace a two-device radio test.

Protocol constraints follow [Espressif's ESP-NOW reference for ESP-IDF 4.4](https://docs.espressif.com/projects/esp-idf/en/v4.4/esp32s3/api-reference/network/esp_now.html).

## Music investigation

The existing workspace already contained a Gemini jitter buffer and larger music read-ahead buffer; these are preserved. A separate source of contention was the dashboard's repeated SD capacity queries under the same mutex used by music playback. Capacity is now cached for 30 seconds and refreshed only while playback/recording is inactive. Unknown capacity is omitted rather than falsely reported as a full card. Web upload writes and download reads are limited to 4 KiB per SD lock interval.

`/api/status` adds `musicSdWaits` (missed SD lock opportunities) and `musicMaxServiceGapMs` (largest gap between music decoder service calls, reset for a new track). These diagnose scheduling contention; they are not measured underrun counts. Physical amplifier noise, power quality, cabling, and damaged source files cannot be diagnosed from code alone. Actual music quality and simultaneous ESP-NOW/web operation still require hardware testing.
