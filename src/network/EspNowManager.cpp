#include "EspNowManager.hpp"
#include "../core/MemoryPolicy.hpp"
#include <WiFi.h>
#include <Preferences.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <atomic>
namespace espnow
{
    namespace
    {
        struct Peer
        {
            uint8_t mac[6]{};
            char name[25]{};
            bool enabled = true, seen = false, registered = false;
            uint32_t last = 0, tx = 0, rx = 0, failed = 0;
            char latest[MaxPayload * 2 + 1]{};
        };
        struct Command
        {
            bool config = false;
            char json[1537]{};
            uint8_t mac[6]{}, payload[MaxPayload]{};
            uint16_t length = 0;
        };
        struct Event
        {
            uint8_t mac[6], wire[Header + MaxPayload];
            uint16_t length;
        };
        Peer *peers = nullptr;
        Peer *candidate = nullptr;
        Command *work = nullptr;
        QueueHandle_t commands = nullptr, events = nullptr;
        StaticQueue_t commandControl, eventControl;
        SemaphoreHandle_t mutex = nullptr;
        Summary state;
        String error;
        uint32_t revision = 0, sequence = 0, lastPing = 0;
        size_t pingIndex = 0;
        std::atomic<int> txResult{-1};
        std::atomic<uint32_t> dropped{0};
        std::atomic<Receiver> receiver{nullptr};
        bool busy = false;
        uint8_t sending[6]{};
        uint32_t sentAt = 0;
        struct Guard
        {
            bool held;
            Guard() : held(mutex && xSemaphoreTake(mutex, pdMS_TO_TICKS(50)) == pdTRUE) {}
            ~Guard()
            {
                if (held)
                    xSemaphoreGive(mutex);
            }
        };
        String format(const uint8_t *m)
        {
            char s[18];
            snprintf(s, sizeof(s), "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
            return s;
        }
        int find(const uint8_t *m)
        {
            for (int i = 0; i < state.count; ++i)
                if (!memcmp(peers[i].mac, m, 6))
                    return i;
            return -1;
        }
        void receive(const uint8_t *mac, const uint8_t *data, int len)
        {
            if (!events || !mac || !data || len < int(Header) || len > int(Header + MaxPayload))
                return;
            Event e{};
            memcpy(e.mac, mac, 6);
            memcpy(e.wire, data, len);
            e.length = len;
            if (xQueueSend(events, &e, 0) != pdTRUE)
                ++dropped;
        }
        void sent(const uint8_t *, esp_now_send_status_t result) { txResult.store(result == ESP_NOW_SEND_SUCCESS ? 1 : 0); }
        void stop()
        {
            if (state.ready)
            {
                esp_now_unregister_recv_cb();
                esp_now_unregister_send_cb();
                esp_now_deinit();
            }
            state.ready = false;
            busy = false;
            txResult = -1;
            state.online = 0;
            for (int i = 0; i < state.count; ++i)
            {
                peers[i].registered = false;
                peers[i].seen = false;
            }
            if (events)
                xQueueReset(events);
        }
        bool transmit(int i, Type type, const uint8_t *data, size_t len, uint32_t seq)
        {
            if (busy || !state.ready || i < 0 || !peers[i].enabled || !peers[i].registered)
                return false;
            uint8_t wire[Header + MaxPayload];
            size_t n = encode(wire, type, seq, data, len);
            if (!n)
                return false;
            memcpy(sending, peers[i].mac, 6);
            txResult = -1;
            busy = true;
            sentAt = millis();
            esp_err_t result = esp_now_send(sending, wire, n);
            if (result != ESP_OK)
            {
                busy = false;
                ++peers[i].failed;
                ++state.failed;
                error = String("Send failed: ") + esp_err_to_name(result);
                return false;
            }
            return true;
        }
        bool loadConfig(const String &text, bool save)
        {
            JsonDocument d(memory::jsonAllocator());
            if (deserializeJson(d, text) || !d["enabled"].is<bool>() || !d["peers"].is<JsonArray>() || d["peers"].size() > MaxPeers)
            {
                error = "Invalid ESP-NOW configuration";
                return false;
            }
            Peer *next = candidate;
            memset(next, 0, MaxPeers * sizeof(Peer));
            uint8_t count = 0, local[6]{};
            esp_read_mac(local, ESP_MAC_WIFI_STA);
            for (JsonObjectConst p : d["peers"].as<JsonArrayConst>())
            {
                const char *name = p["name"] | "";
                const char *address = p["mac"] | "";
                if (!name[0] || strlen(name) > 24 || !p["enabled"].is<bool>() || !mac(address, next[count].mac) || !memcmp(next[count].mac, local, 6))
                {
                    error = "Invalid name or unicast MAC (cannot use own MAC)";
                    return false;
                }
                for (int i = 0; i < count; ++i)
                    if (!memcmp(next[i].mac, next[count].mac, 6))
                    {
                        error = "Duplicate peer MAC";
                        return false;
                    }
                strlcpy(next[count].name, name, sizeof(next[count].name));
                next[count].enabled = p["enabled"];
                ++count;
            }
            if (save)
            {
                Preferences p;
                if (!p.begin("tofan-espnow", false))
                {
                    error = "Cannot open ESP-NOW settings";
                    return false;
                }
                bool ok = p.putString("config", text) == text.length();
                p.end();
                if (!ok)
                {
                    error = "Cannot save ESP-NOW settings";
                    return false;
                }
            }
            stop();
            memcpy(peers, next, MaxPeers * sizeof(Peer));
            state.count = count;
            state.enabled = d["enabled"];
            error = "";
            return true;
        }
    }
    void begin()
    {
        mutex = xSemaphoreCreateMutex();
        candidate = static_cast<Peer *>(memory::zeroAllocate(MaxPeers, sizeof(Peer)));
        work = static_cast<Command *>(memory::zeroAllocate(1, sizeof(Command)));
        peers = static_cast<Peer *>(memory::zeroAllocate(MaxPeers, sizeof(Peer)));
        auto *c = static_cast<uint8_t *>(memory::allocate(2 * sizeof(Command)));
        auto *e = static_cast<uint8_t *>(memory::allocate(8 * sizeof(Event)));
        if (c)
            commands = xQueueCreateStatic(2, sizeof(Command), c, &commandControl);
        if (e)
            events = xQueueCreateStatic(8, sizeof(Event), e, &eventControl);
        if (!mutex || !peers || !candidate || !work || !commands || !events)
        {
            error = "ESP-NOW memory unavailable";
            return;
        }
        Preferences p;
        if (p.begin("tofan-espnow", true))
        {
            String saved = p.getString("config", "");
            p.end();
            if (saved.length())
                loadConfig(saved, false);
        }
    }
    void beforeWifiChange()
    {
        Guard g;
        if (g.held && peers)
            stop();
    }
    bool submitConfig(const String &json)
    {
        if (!commands || json.length() > 1536)
            return false;
        Command c;
        c.config = true;
        strlcpy(c.json, json.c_str(), sizeof(c.json));
        return xQueueSend(commands, &c, 0) == pdTRUE;
    }
    bool sendPacket(const uint8_t address[6], const uint8_t *data, size_t length)
    {
        if (!commands || !address || length > MaxPayload || (!data && length))
            return false;
        Command c;
        memcpy(c.mac, address, 6);
        if (length)
            memcpy(c.payload, data, length);
        c.length = length;
        return xQueueSend(commands, &c, 0) == pdTRUE;
    }
    void setReceiver(Receiver callback) { receiver.store(callback); }
    void service()
    {
        Guard g;
        if (!g.held || !peers || !candidate || !work || !commands || !events)
            return;
        int result = txResult.exchange(-1);
        if (busy && result >= 0)
        {
            int i = find(sending);
            if (i >= 0)
            {
                if (result)
                {
                    ++peers[i].tx;
                    ++state.tx;
                }
                else
                {
                    ++peers[i].failed;
                    ++state.failed;
                }
            }
            busy = false;
        }
        if (busy && millis() - sentAt > 2000)
        {
            stop();
            error = "Send callback timed out; radio restarting";
        }
        if (!busy)
        {
            Command &c = *work;
            if (xQueueReceive(commands, &c, 0) == pdTRUE)
            {
                if (c.config)
                {
                    loadConfig(c.json, true);
                    ++revision;
                }
                else
                {
                    error = "";
                    int i = find(c.mac);
                    if (!transmit(i, Data, c.payload, c.length, ++sequence))
                        error = "Cannot send: enable ESP-NOW and a registered peer";
                    ++revision;
                }
            }
        }
        if (state.enabled && !state.ready)
        {
            wifi_mode_t mode = WiFi.getMode();
            if (mode == WIFI_OFF)
                WiFi.mode(WIFI_STA);
            else if (mode == WIFI_AP)
                WiFi.mode(WIFI_AP_STA);
            esp_err_t r = esp_now_init();
            if (r != ESP_OK)
            {
                error = String("ESP-NOW init: ") + esp_err_to_name(r);
                return;
            }
            state.ready = true;
            esp_now_register_recv_cb(receive);
            esp_now_register_send_cb(sent);
            for (int i = 0; i < state.count; ++i)
                if (peers[i].enabled)
                {
                    esp_now_peer_info_t p{};
                    memcpy(p.peer_addr, peers[i].mac, 6);
                    p.channel = 0;
                    p.ifidx = WIFI_IF_STA;
                    p.encrypt = false;
                    r = esp_now_add_peer(&p);
                    peers[i].registered = r == ESP_OK;
                    if (r != ESP_OK)
                        error = String("Peer registration: ") + esp_err_to_name(r);
                }
        }
        uint8_t channel = 0;
        wifi_second_chan_t secondary;
        esp_wifi_get_channel(&channel, &secondary);
        state.channel = channel;
        Event e;
        for (int n = 0; n < 4 && xQueueReceive(events, &e, 0) == pdTRUE; ++n)
        {
            int i = find(e.mac);
            Type type;
            uint32_t seq;
            size_t len;
            if (i < 0 || !peers[i].enabled || !decode(e.wire, e.length, type, seq, len))
                continue;
            auto &p = peers[i];
            p.seen = true;
            p.last = millis();
            ++p.rx;
            ++state.rx;
            if (type == Ping && !busy)
                transmit(i, Pong, nullptr, 0, seq);
            if (type == Data)
            {
                for (size_t j = 0; j < len; ++j)
                    snprintf(p.latest + j * 2, 3, "%02X", e.wire[Header + j]);
                p.latest[len * 2] = 0;
                // Invoke the application outside the state mutex (it may enqueue a reply).
                Receiver cb = receiver.load();
                if (cb)
                {
                    xSemaphoreGive(mutex);
                    cb(e.mac, e.wire + Header, len);
                    xSemaphoreTake(mutex, portMAX_DELAY);
                }
            }
        }
        state.online = 0;
        for (int i = 0; i < state.count; ++i)
            if (state.ready && peers[i].enabled && online(peers[i].seen, peers[i].last, millis()))
                ++state.online;
        if (state.ready && !busy && state.count && millis() - lastPing >= 625)
        {
            lastPing = millis();
            size_t i = pingIndex++ % state.count;
            if (peers[i].enabled)
                transmit(i, Ping, nullptr, 0, ++sequence);
        }
    }
    Summary summary()
    {
        Guard g;
        return g.held ? state : Summary{};
    }
    String status()
    {
        Guard g;
        JsonDocument d(memory::jsonAllocator());
        if (!g.held)
        {
            d["error"] = "ESP-NOW busy";
        }
        else
        {
            uint8_t local[6]{};
            esp_read_mac(local, ESP_MAC_WIFI_STA);
            d["mac"] = format(local);
            d["enabled"] = state.enabled;
            d["ready"] = state.ready;
            d["channel"] = state.channel;
            d["revision"] = revision;
            d["error"] = error;
            d["dropped"] = dropped.load();
            d["maxPayload"] = MaxPayload;
            auto rows = d["peers"].to<JsonArray>();
            if (peers)
                for (int i = 0; i < state.count; ++i)
                {
                    auto &p = peers[i];
                    auto row = rows.add<JsonObject>();
                    row["name"] = p.name;
                    row["mac"] = format(p.mac);
                    row["enabled"] = p.enabled;
                    row["registered"] = p.registered;
                    row["online"] = state.ready && p.enabled && online(p.seen, p.last, millis());
                    row["tx"] = p.tx;
                    row["rx"] = p.rx;
                    row["failed"] = p.failed;
                    row["ageMs"] = p.seen ? millis() - p.last : 0;
                    row["seen"] = p.seen;
                    row["payloadHex"] = p.latest;
                }
        }
        String out;
        serializeJson(d, out);
        return out;
    }
}
