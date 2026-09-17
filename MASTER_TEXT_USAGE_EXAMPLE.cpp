// Example only: adapt this to the file where your application initializes EspNowManager.
#include "EspNowManager.hpp"

static uint8_t slaveMac[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}; // replace with Slave MAC

static void printMac(const uint8_t mac[6])
{
    Serial.printf("%02X:%02X:%02X:%02X:%02X:%02X",
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static void onEspNowText(const uint8_t mac[6], const char *text, size_t length)
{
    Serial.print("[MASTER RX TEXT] from ");
    printMac(mac);
    Serial.printf(" | bytes=%u | ", static_cast<unsigned>(length));
    Serial.println(text);

    // Optional reply example:
    // espnow::sendText(mac, "Message received by Master");
}

void setupEspNowTextExample()
{
    // Keep your existing espnow::begin() call where it already is.
    espnow::setTextReceiver(onEspNowText);
}

void sendExampleTextToSlave()
{
    if (espnow::sendText(slaveMac, "Hello from Master!"))
        Serial.println("[MASTER TX TEXT] queued");
    else
        Serial.println("[MASTER TX TEXT] queue/send request failed");
}
