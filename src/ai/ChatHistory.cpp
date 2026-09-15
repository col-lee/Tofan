#include "../core/MemoryPolicy.hpp"
#include "ChatHistory.hpp"
#include "../core/SharedResources.hpp"
#include <SD.h>
#include <esp_heap_caps.h>
#include <time.h>
ChatHistory chatHistory;
namespace
{
    constexpr const char *Path = "/tofan-chat/history.jsonl";
    struct Lock
    {
        SemaphoreHandle_t m;
        bool ok;
        Lock(SemaphoreHandle_t m, int timeout = 20) : m(m), ok(m && xSemaphoreTake(m, pdMS_TO_TICKS(timeout)) == pdTRUE) {}
        ~Lock()
        {
            if (ok)
                xSemaphoreGive(m);
        }
    };
    void encode(JsonObject o, const chat::Entry &e)
    {
        o["id"] = e.id;
        o["role"] = e.model ? "model" : "user";
        o["text"] = e.text;
        o["time"] = e.time;
        o["uptime"] = e.uptime;
        o["partial"] = e.partial;
        o["truncated"] = e.truncated;
    }
    bool decode(JsonDocument &d, chat::Entry &e)
    {
        const char *role = d["role"] | "";
        const char *text = d["text"] | "";
        if ((strcmp(role, "user") && strcmp(role, "model")) || !text[0] || strlen(text) > chat::TextLimit)
            return false;
        e = {};
        e.id = d["id"] | 0u;
        if (!e.id)
            return false;
        e.model = !strcmp(role, "model");
        e.time = d["time"] | 0u;
        e.uptime = d["uptime"] | 0u;
        e.partial = d["partial"] | false;
        e.truncated = d["truncated"] | false;
        chat::append(e, text);
        return true;
    }
}
void ChatHistory::remember(const chat::Entry &e)
{
    unsigned position = (recentStart + recentCount) % chat::RecentLimit;
    if (recentCount == chat::RecentLimit)
    {
        position = recentStart;
        recentStart = (recentStart + 1) % chat::RecentLimit;
    }
    else
        ++recentCount;
    recent[position] = e;
    if (e.id >= nextId)
        nextId = e.id + 1;
    truncated |= e.truncated;
}
void ChatHistory::begin()
{
    assembly = static_cast<chat::Entry *>(memory::zeroAllocate(2, sizeof(chat::Entry)));
    incomingPool = static_cast<Incoming *>(memory::zeroAllocate(chat::IncomingLimit, sizeof(Incoming)));
    mutex = xSemaphoreCreateMutex();
    queue = xQueueCreate(8, sizeof(chat::Entry *));
    incoming = xQueueCreate(chat::IncomingLimit, sizeof(uint8_t));
    incomingFree = xQueueCreate(chat::IncomingLimit, sizeof(uint8_t));
    recent = static_cast<chat::Entry *>(memory::zeroAllocate(chat::RecentLimit, sizeof(chat::Entry)));
    index = static_cast<Index *>(memory::zeroAllocate(chat::RecordLimit, sizeof(Index)));
    if (!assembly || !incomingPool || !mutex || !queue || !incoming || !incomingFree || !recent || !index)
    {
        error = "History memory allocation failed";
        if (queue)
        {
            vQueueDelete(queue);
            queue = nullptr;
        }
        if (incoming)
        {
            vQueueDelete(incoming);
            incoming = nullptr;
        }
        if (incomingFree)
        {
            vQueueDelete(incomingFree);
            incomingFree = nullptr;
        }
        if (mutex)
        {
            vSemaphoreDelete(mutex);
            mutex = nullptr;
        }
        memory::release(assembly);
        assembly = nullptr;
        memory::release(incomingPool);
        incomingPool = nullptr;
        memory::release(recent);
        recent = nullptr;
        memory::release(index);
        index = nullptr;
        return;
    }
    for (uint8_t slot = 0; slot < chat::IncomingLimit; ++slot)
        xQueueSend(incomingFree, &slot, 0);
    Lock sd(sdSemaphore, 1500);
    persistent = sd.ok && isConnectSDcard;
    if (!persistent)
        return;
    if (!SD.exists("/tofan-chat") && !SD.mkdir("/tofan-chat"))
    {
        error = "Cannot create history folder";
        return;
    }
    if (!SD.exists(Path))
        return;
    File file = SD.open(Path, FILE_READ);
    if (!file)
    {
        error = "Cannot read chat history";
        return;
    }
    bytes = file.size();
    full = bytes >= chat::ArchiveLimit;
    auto *buffer = static_cast<char *>(memory::allocate(4096));
    auto *entry = static_cast<chat::Entry *>(memory::allocate(sizeof(chat::Entry)));
    if (!buffer || !entry)
    {
        memory::release(buffer);
        memory::release(entry);
        file.close();
        error = "History load allocation failed";
        damaged = true;
        return;
    }
    while (file.available() && count < chat::RecordLimit && file.position() < chat::ArchiveLimit)
    {
        const uint32_t start = file.position();
        size_t length = 0;
        bool ended = false, oversize = false;
        while (file.available() && file.position() < chat::ArchiveLimit)
        {
            int c = file.read();
            if (c == '\n')
            {
                ended = true;
                break;
            }
            if (length < 4095)
                buffer[length++] = c;
            else
                oversize = true;
        }
        buffer[length] = 0;
        JsonDocument d(memory::jsonAllocator());
        if (!ended || oversize || deserializeJson(d, buffer, length) || !decode(d, *entry))
        {
            damaged = true;
            continue;
        }
        index[count++] = {start, static_cast<uint32_t>(file.position() - start)};
        remember(*entry);
    }
    if (file.available())
        full = true;
    memory::release(entry);
    memory::release(buffer);
    file.close();
    if (damaged)
        error = "History contains an incomplete or invalid record; export/read before reset";
}
void ChatHistory::transcript(bool model, const char *text)
{
    if (resetPending || !assembly || !recent || !queue || !incoming || !incomingFree || !incomingPool || !text || !text[0])
        return;
    // Preserve a couple of pool slots for turn-boundary markers. Losing a text
    // fragment is recoverable; losing finish() merges the next conversation turn
    // into the current one and makes assemblingBytes grow forever.
    if (uxQueueMessagesWaiting(incomingFree) <= chat::FinishReserve)
    {
        ++dropped;
        return;
    }
    uint8_t slot = 0;
    if (xQueueReceive(incomingFree, &slot, 0) != pdTRUE)
    {
        ++dropped;
        return;
    }
    Incoming &e = incomingPool[slot];
    e = {};
    e.model = model;
    const size_t length = strlen(text), copied = length > chat::TextLimit ? chat::TextLimit : length;
    memcpy(e.text, text, copied);
    e.text[copied] = 0;
    e.truncated = length > copied;
    ++receivedChunks;
    receivedBytes.fetch_add(static_cast<uint32_t>(copied));
    if (xQueueSend(incoming, &slot, 0) != pdTRUE)
    {
        xQueueSend(incomingFree, &slot, 0);
        ++dropped;
    }
}
void ChatHistory::commit(chat::Entry &e, bool partial)
{
    if (!e.length)
        return;
    e.id = nextId++;
    e.partial = partial;
    e.uptime = millis() / 1000;
    time_t now = time(nullptr);
    e.time = now > 1700000000 ? static_cast<uint32_t>(now) : 0;
    remember(e);
    auto *pending = static_cast<chat::Entry *>(memory::allocate(sizeof(chat::Entry)));
    if (pending)
    {
        *pending = e;
        if (xQueueSend(queue, &pending, 0) != pdTRUE)
        {
            memory::release(pending);
            ++dropped;
        }
    }
    else
        ++dropped;
    e = {};
}
void ChatHistory::finishMask(uint8_t mask, bool partial)
{
    if (resetPending || !assembly || !recent || !queue || !incoming || !incomingFree || !incomingPool)
        return;
    // A turn boundary must be more reliable than ordinary transcript traffic.
    // Transcript producers reserve slots for finish markers and we also allow a
    // short wait for service() to return a slot under a burst.
    uint8_t slot = 0;
    if (xQueueReceive(incomingFree, &slot, pdMS_TO_TICKS(40)) != pdTRUE)
    {
        ++dropped;
        ++boundaryDrops;
        return;
    }
    Incoming &e = incomingPool[slot];
    e = {};
    e.finishMask = mask;
    e.partial = partial;
    if (xQueueSend(incoming, &slot, 0) != pdTRUE)
    {
        xQueueSend(incomingFree, &slot, 0);
        ++dropped;
    }
}
void ChatHistory::service()
{
    if (!assembly || !recent || !queue || !incoming || !incomingFree || !incomingPool || resetPending)
        return;
    Lock lock(mutex, 0);
    if (!lock.ok)
        return;
    uint8_t slot = 0;
    for (int i = 0; i < chat::IncomingLimit && xQueueReceive(incoming, &slot, 0) == pdTRUE; ++i)
    {
        Incoming &e = incomingPool[slot];
        if (e.finishMask)
        {
            if (e.finishMask & FinishUser)
                commit(assembly[0], e.partial);
            if (e.finishMask & FinishModel)
                commit(assembly[1], e.partial);
        }
        else
        {
            auto &part = assembly[e.model ? 1 : 0];
            chat::append(part, e.text);
            part.truncated |= e.truncated;
            assembly[1].model = true;
        }
        e = {};
        xQueueSend(incomingFree, &slot, 0);
    }
    if (millis() - lastAttempt < 100)
        return;
    lastAttempt = millis();
    chat::Entry *pending = nullptr;
    if (xQueuePeek(queue, &pending, 0) != pdTRUE)
        return;
    if (persistent && !full && !damaged)
    {
        Lock sd(sdSemaphore, 0);
        if (!sd.ok)
            return;
        if (!isConnectSDcard)
        {
            error = "SD unavailable: chat history is not being saved";
            return;
        }
        if (!SD.exists("/tofan-chat") && !SD.mkdir("/tofan-chat"))
        {
            error = "Cannot create history folder";
            return;
        }
        JsonDocument d(memory::jsonAllocator());
        encode(d.to<JsonObject>(), *pending);
        String line;
        serializeJson(d, line);
        line += '\n';
        if (count >= chat::RecordLimit || bytes + line.length() > chat::ArchiveLimit)
            full = true;
        else
        {
            if (SD.totalBytes() <= SD.usedBytes() + line.length() + 65536)
            {
                error = "SD card is full: chat history is not being saved";
                return;
            }
            File file = SD.open(Path, FILE_APPEND);
            if (!file)
            {
                error = "Cannot open chat history for writing";
                return;
            }
            if (file.size() != bytes)
            {
                file.close();
                damaged = true;
                error = "History file changed; reset required before writing";
                return;
            }
            size_t written = file.print(line);
            file.flush();
            file.close();
            if (written != line.length())
            {
                damaged = true;
                error = "Incomplete history write; read existing history before reset";
                return;
            }
            index[count++] = {bytes, static_cast<uint32_t>(written)};
            bytes += written;
            error = "";
        }
    }
    xQueueReceive(queue, &pending, 0);
    memory::release(pending);
}
void ChatHistory::status(JsonDocument &d) const
{
    d["storage"] = persistent ? "sd" : "ram";
    d["count"] = persistent ? count : recentCount;
    d["bytes"] = bytes;
    d["maxBytes"] = chat::ArchiveLimit;
    d["maxMessages"] = chat::RecordLimit;
    d["contextMessages"] = recentCount;
    d["contextBytes"] = chat::ContextLimit;
    d["long"] = nextId > chat::RecentLimit + 1 || bytes >= chat::ArchiveLimit * 4 / 5 || count >= 800;
    d["full"] = full;
    d["damaged"] = damaged;
    d["truncated"] = truncated;
    d["dropped"] = dropped.load();
    d["boundaryDrops"] = boundaryDrops.load();
    d["error"] = error;
    d["resetting"] = resetPending.load();
    d["resetRevision"] = resetRevision;
    d["pending"] = queue ? uxQueueMessagesWaiting(queue) : 0;
    d["receivedChunks"] = receivedChunks.load();
    d["receivedBytes"] = receivedBytes.load();
    d["incoming"] = incoming ? uxQueueMessagesWaiting(incoming) : 0;
    d["assemblingUserBytes"] = assembly ? assembly[0].length : 0;
    d["assemblingModelBytes"] = assembly ? assembly[1].length : 0;
    d["assemblingBytes"] = assembly ? assembly[0].length + assembly[1].length : 0;
}
String ChatHistory::page(unsigned before, bool includeMessages)
{
    Lock lock(mutex, 500);
    JsonDocument d(memory::jsonAllocator());
    if (!lock.ok)
    {
        d["error"] = "History busy";
        String out;
        serializeJson(d, out);
        return out;
    }
    status(d);
    if (!includeMessages)
    {
        String out;
        serializeJson(d, out);
        return out;
    }
    auto rows = d["messages"].to<JsonArray>();
    unsigned available = persistent ? count : recentCount;
    unsigned end = before && before < available ? before : available, start = end > 4 ? end - 4 : 0;
    d["before"] = start;
    d["hasOlder"] = start > 0;
    if (persistent && end)
    {
        Lock sd(sdSemaphore, 50);
        if (!sd.ok || !isConnectSDcard)
            d["error"] = "SD unavailable or busy";
        else
        {
            File file = SD.open(Path, FILE_READ);
            if (!file)
                d["error"] = "Cannot read history";
            else
            {
                auto *buffer = static_cast<char *>(memory::allocate(4097));
                if (!buffer)
                    d["error"] = "History read allocation failed";
                else
                {
                    for (unsigned i = start; i < end; ++i)
                    {
                        const size_t length = index[i].length;
                        JsonDocument item(memory::jsonAllocator());
                        if (length > 4096 || !file.seek(index[i].offset) || file.readBytes(buffer, length) != length)
                        {
                            d["error"] = "Cannot read history record";
                            break;
                        }
                        buffer[length] = 0;
                        if (deserializeJson(item, static_cast<const char *>(buffer), length))
                        {
                            d["error"] = "Cannot decode history record";
                            break;
                        }
                        rows.add(item.as<JsonObject>());
                    }
                    memory::release(buffer);
                }
                file.close();
            }
        }
    }
    else if (recent)
    {
        for (unsigned i = start; i < end; ++i)
            encode(rows.add<JsonObject>(), recent[(recentStart + i) % chat::RecentLimit]);
    }
    String out;
    serializeJson(d, out);
    return out;
}
bool ChatHistory::hasContext()
{
    Lock lock(mutex);
    return lock.ok && recentCount > 0 && !resetPending;
}
String ChatHistory::context()
{
    Lock lock(mutex);
    if (!lock.ok || !recent || resetPending)
        return "[]";
    unsigned start = recentCount;
    size_t size = 0;
    while (start > 0)
    {
        const auto &e = recent[(recentStart + start - 1) % chat::RecentLimit];
        if (size + e.length > chat::ContextLimit)
            break;
        size += e.length;
        --start;
    }
    while (start < recentCount && recent[(recentStart + start) % chat::RecentLimit].model)
        ++start;
    JsonDocument d(memory::jsonAllocator());
    auto turns = d.to<JsonArray>();
    for (unsigned i = start; i < recentCount; ++i)
    {
        const auto &e = recent[(recentStart + i) % chat::RecentLimit];
        auto turn = turns.add<JsonObject>();
        turn["role"] = e.model ? "model" : "user";
        turn["parts"][0]["text"] = e.text;
    }
    String out;
    serializeJson(d, out);
    return out;
}
void ChatHistory::reset()
{
    if (!resetPending)
        return;
    if (!mutex || !assembly || !recent || !queue || !incoming || !incomingFree || !incomingPool)
    {
        error = "History is unavailable";
        resetPending = false;
        return;
    }
    Lock lock(mutex, 100);
    if (!lock.ok)
        return;
    if (persistent)
    {
        Lock sd(sdSemaphore, 50);
        if (!sd.ok)
            return;
        if (!isConnectSDcard || (SD.exists(Path) && !SD.remove(Path)))
        {
            error = "Cannot reset: insert writable SD card and try again";
            resetPending = false;
            return;
        }
    }
    chat::Entry *entry = nullptr;
    if (queue)
        while (xQueueReceive(queue, &entry, 0) == pdTRUE)
            memory::release(entry);
    xQueueReset(incoming);
    xQueueReset(incomingFree);
    for (uint8_t slot = 0; slot < chat::IncomingLimit; ++slot)
    {
        incomingPool[slot] = {};
        xQueueSend(incomingFree, &slot, 0);
    }
    receivedChunks = 0;
    receivedBytes = 0;
    boundaryDrops = 0;
    assembly[0] = {};
    assembly[1] = {};
    recentCount = recentStart = count = bytes = 0;
    nextId = 1;
    full = damaged = truncated = false;
    dropped = 0;
    error = "";
    ++resetRevision;
    resetPending = false;
}
