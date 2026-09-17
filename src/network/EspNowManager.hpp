#pragma once
#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include "EspNowProtocol.hpp"
// All radio operations run on runNet. Callbacks only copy into bounded queues.
namespace espnow
{
    struct Summary
    {
        bool enabled = false, ready = false;
        uint8_t channel = 0, count = 0, online = 0;
        uint32_t tx = 0, rx = 0, failed = 0;
    };
    using Receiver = void (*)(const uint8_t mac[6], const uint8_t *payload, size_t length);
    // TextReceiver receives the same Data payload as UTF-8/text bytes.
    // `text` is always NUL-terminated for convenience; `length` is the original byte count.
    using TextReceiver = void (*)(const uint8_t mac[6], const char *text, size_t length);
    void begin();
    void service();
    void beforeWifiChange();
    String status();   // Contains MAC addresses: expose only through authenticated routes.
    Summary summary(); // No MAC addresses; safe for the local TFT.
    bool submitConfig(const String &json);
    bool sendPacket(const uint8_t mac[6], const uint8_t *data, size_t length);
    // Sends raw bytes without the ToFan protocol header. Max 200 bytes.
    bool sendRaw(const uint8_t mac[6], const uint8_t *data, size_t length);
    // Sends raw UTF-8/text so ordinary ESP-NOW receivers get exactly these bytes.
    bool sendText(const uint8_t mac[6], const char *text);
    bool sendText(const uint8_t mac[6], const String &text);
    void setReceiver(Receiver callback); // Called by runNet, never the Wi-Fi callback.
    void setTextReceiver(TextReceiver callback); // Called by runNet, never the Wi-Fi callback.
}
