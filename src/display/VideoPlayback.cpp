#include "VideoPlayback.hpp"
#include "MjpegFrame.hpp"
#include "JpegDimensions.hpp"
#include "DisplayManager.hpp"

#include <algorithm>
#include <esp_heap_caps.h>
#include <esp_timer.h>

namespace {

// MJPEG playback is intentionally buffered much more deeply than before.
// Two 512 KiB PSRAM buffers form a ping-pong read-ahead cache: Core 1 decodes
// from one buffer while a small Core 0 task fills the other from SD.
constexpr size_t kFrameCapacity = 512 * 1024;
constexpr size_t kReadBufferSize = 512 * 1024;
constexpr size_t kReadSliceSize = 32 * 1024;
constexpr size_t kLowWatermark = 192 * 1024;
constexpr uint32_t kBufferWaitTimeoutMs = 2000;
constexpr uint32_t kVideoFps = 12;
constexpr int64_t kFrameIntervalUs = 1000000LL / kVideoFps;
constexpr uint32_t kPrefetchTaskStack = 4 * 1024;
constexpr UBaseType_t kPrefetchTaskPriority = 3;
constexpr BaseType_t kPrefetchTaskCore = 0;
constexpr uint8_t kStopPrefetch = 0xFF;

File video;
uint8_t* frame = nullptr;

enum class BufferState : uint8_t {
    Free,
    Filling,
    Ready
};

struct ReadAheadBuffer {
    uint8_t* data = nullptr;
    size_t length = 0;
    size_t position = 0;
    volatile BufferState state = BufferState::Free;
};

ReadAheadBuffer readBuffers[2];
uint8_t activeBuffer = 0;
portMUX_TYPE readBufferMux = portMUX_INITIALIZER_UNLOCKED;

QueueHandle_t prefetchQueue = nullptr;
SemaphoreHandle_t bufferReadySemaphore = nullptr;
SemaphoreHandle_t prefetchStoppedSemaphore = nullptr;
TaskHandle_t prefetchTaskHandle = nullptr;
volatile bool prefetchStopRequested = false;

int64_t nextFrameAtUs = 0;
bool hasFrame = false;
bool playing = false;
uint32_t decodedFrames = 0;
uint32_t refillCount = 0;
uint32_t bufferWaitCount = 0;
uint64_t totalBufferWaitUs = 0;
uint32_t maxBufferWaitUs = 0;

BufferState bufferState(uint8_t index) {
    BufferState state;
    portENTER_CRITICAL(&readBufferMux);
    state = readBuffers[index].state;
    portEXIT_CRITICAL(&readBufferMux);
    return state;
}

void setBufferState(uint8_t index, BufferState state, size_t length = 0) {
    portENTER_CRITICAL(&readBufferMux);
    readBuffers[index].length = length;
    readBuffers[index].position = 0;
    readBuffers[index].state = state;
    portEXIT_CRITICAL(&readBufferMux);
}

// Reads into one PSRAM buffer using short SD ownership slices. Releasing the
// SD mutex every 32 KiB lets audio.loop(), uploads and file browsing interleave
// instead of being blocked by one large 512 KiB read.
bool fillReadAheadBuffer(uint8_t index) {
    if (index > 1 || !readBuffers[index].data || !video) return false;

    uint8_t* destination = readBuffers[index].data;
    size_t written = 0;
    bool rewoundEmptyRead = false;

    while (written < kReadBufferSize && !prefetchStopRequested) {
        const size_t wanted = std::min(kReadSliceSize, kReadBufferSize - written);
        size_t got = 0;
        bool seekOk = true;

        if (xSemaphoreTake(sdSemaphore, pdMS_TO_TICKS(12)) != pdTRUE) {
            // A web upload/audio read may briefly own SD. Do not block the
            // display task; this prefetch task can simply try again.
            vTaskDelay(1);
            continue;
        }

        if (video) {
            got = video.read(destination + written, wanted);
            if (got == 0) {
                seekOk = video.seek(0);
            }
        } else {
            seekOk = false;
        }

        xSemaphoreGive(sdSemaphore);

        if (!seekOk) break;

        if (got == 0) {
            // If this buffer already contains the tail of the file, finish it
            // here. The next buffer starts at byte 0, preserving stream order.
            if (written > 0) break;

            // Exact buffer/file-size boundary: rewind once and continue so the
            // next cache buffer starts with the next loop immediately.
            if (rewoundEmptyRead) break;
            rewoundEmptyRead = true;
            continue;
        }

        rewoundEmptyRead = false;
        written += got;
        taskYIELD();
    }

    if (written == 0) {
        setBufferState(index, BufferState::Free);
        return false;
    }

    setBufferState(index, BufferState::Ready, written);
    ++refillCount;
    if (bufferReadySemaphore) xSemaphoreGive(bufferReadySemaphore);
    return true;
}

bool queueBufferFill(uint8_t index) {
    if (index > 1 || !prefetchQueue || prefetchStopRequested) return false;

    bool shouldQueue = false;
    portENTER_CRITICAL(&readBufferMux);
    if (readBuffers[index].state == BufferState::Free) {
        readBuffers[index].state = BufferState::Filling;
        shouldQueue = true;
    }
    portEXIT_CRITICAL(&readBufferMux);

    if (!shouldQueue) return true;

    if (xQueueSend(prefetchQueue, &index, 0) != pdPASS) {
        setBufferState(index, BufferState::Free);
        return false;
    }
    return true;
}

void prefetchTask(void*) {
    for (;;) {
        uint8_t index = kStopPrefetch;
        if (xQueueReceive(prefetchQueue, &index, pdMS_TO_TICKS(50)) != pdPASS) {
            if (prefetchStopRequested) break;
            continue;
        }

        if (prefetchStopRequested || index == kStopPrefetch) break;
        if (index > 1) continue;

        if (bufferState(index) == BufferState::Filling) {
            if (!fillReadAheadBuffer(index) && !prefetchStopRequested) {
                setBufferState(index, BufferState::Free);
                if (bufferReadySemaphore) xSemaphoreGive(bufferReadySemaphore);
            }
        }
    }

    prefetchTaskHandle = nullptr;
    if (prefetchStoppedSemaphore) xSemaphoreGive(prefetchStoppedSemaphore);
    vTaskDelete(nullptr);
}

bool startPrefetchTask() {
    prefetchStopRequested = false;

    if (!prefetchQueue) prefetchQueue = xQueueCreate(2, sizeof(uint8_t));
    if (!bufferReadySemaphore) bufferReadySemaphore = xSemaphoreCreateBinary();
    if (!prefetchStoppedSemaphore) prefetchStoppedSemaphore = xSemaphoreCreateBinary();
    if (!prefetchQueue || !bufferReadySemaphore || !prefetchStoppedSemaphore) return false;

    // Clear stale wakeups from a previous playback session.
    xQueueReset(prefetchQueue);
    while (xSemaphoreTake(bufferReadySemaphore, 0) == pdTRUE) {}
    while (xSemaphoreTake(prefetchStoppedSemaphore, 0) == pdTRUE) {}

    return xTaskCreatePinnedToCore(
        prefetchTask,
        "videoPrefetch",
        kPrefetchTaskStack,
        nullptr,
        kPrefetchTaskPriority,
        &prefetchTaskHandle,
        kPrefetchTaskCore
    ) == pdPASS;
}

void stopPrefetchTask() {
    if (!prefetchTaskHandle) return;

    prefetchStopRequested = true;
    if (prefetchQueue) {
        xQueueReset(prefetchQueue);
        const uint8_t stop = kStopPrefetch;
        xQueueSend(prefetchQueue, &stop, 0);
    }

    // The task holds SD only for <=32 KiB slices, so graceful shutdown should
    // complete quickly without ever deleting a task while it owns sdSemaphore.
    if (prefetchStoppedSemaphore &&
        xSemaphoreTake(prefetchStoppedSemaphore, pdMS_TO_TICKS(1000)) != pdTRUE) {
        Serial.println("[VIDEO] WARN: prefetch task shutdown timed out");
    }
}

bool waitForBufferReady(uint8_t index) {
    if (bufferState(index) == BufferState::Ready) return true;

    if (bufferState(index) == BufferState::Free) {
        queueBufferFill(index);
    }

    const int64_t waitStarted = esp_timer_get_time();
    bool counted = false;

    while (!prefetchStopRequested && playing) {
        if (bufferState(index) == BufferState::Ready) {
            const uint32_t waited = static_cast<uint32_t>(esp_timer_get_time() - waitStarted);
            if (waited > 1000) {
                ++bufferWaitCount;
                totalBufferWaitUs += waited;
                maxBufferWaitUs = std::max(maxBufferWaitUs, waited);
                counted = true;
            }
            if (counted) {
                Serial.printf("[VIDEO] Cache wait: %.1f ms (count=%lu, max=%.1f ms)\n",
                              waited / 1000.0f,
                              static_cast<unsigned long>(bufferWaitCount),
                              maxBufferWaitUs / 1000.0f);
            }
            return true;
        }

        const int64_t elapsed = esp_timer_get_time() - waitStarted;
        if (elapsed >= static_cast<int64_t>(kBufferWaitTimeoutMs) * 1000LL) {
            Serial.printf("[VIDEO] ERROR: cache refill timeout on buffer %u\n", index);
            return false;
        }

        if (bufferReadySemaphore) {
            xSemaphoreTake(bufferReadySemaphore, pdMS_TO_TICKS(4));
        } else {
            vTaskDelay(1);
        }
    }

    return false;
}

bool switchReadBuffer() {
    const uint8_t oldIndex = activeBuffer;
    const uint8_t nextIndex = oldIndex ^ 1U;

    if (!waitForBufferReady(nextIndex)) return false;

    // The old buffer is exhausted and now safe for the Core 0 prefetch task.
    setBufferState(oldIndex, BufferState::Free);
    activeBuffer = nextIndex;
    queueBufferFill(oldIndex);
    return true;
}

struct Reader {
    int read() {
        ReadAheadBuffer* current = &readBuffers[activeBuffer];

        if (!current->data) return -1;
        if (current->position >= current->length) {
            if (!switchReadBuffer()) return -1;
            current = &readBuffers[activeBuffer];
        }

        const int value = current->data[current->position++];
        const size_t remaining = current->length - current->position;

        // Normally the opposite buffer is already Ready. If SD was heavily
        // contended, re-poke the producer before this buffer reaches its end.
        if (remaining == kLowWatermark ||
            (current->position == 1 && current->length < kLowWatermark)) {
            const uint8_t nextIndex = activeBuffer ^ 1U;
            if (bufferState(nextIndex) == BufferState::Free) {
                queueBufferFill(nextIndex);
            }
        }

        return value;
    }

    void reset() {
        activeBuffer = 0;
        for (auto& buffer : readBuffers) {
            buffer.length = 0;
            buffer.position = 0;
            buffer.state = BufferState::Free;
        }
    }
} reader;

// Explicit constructor fixes:
// "no matching function for call to FrameReader({frame,size})"
// on the C++ mode/toolchain used by Arduino-ESP32.
struct FrameReader {
    const uint8_t* bytes;
    size_t length;
    size_t pos;

    FrameReader(const uint8_t* data, size_t len)
        : bytes(data), length(len), pos(0) {}

    int read() {
        return pos < length ? bytes[pos++] : -1;
    }

    bool available() const {
        return pos < length;
    }

    size_t size() const {
        return length;
    }

    size_t position() const {
        return pos;
    }

    bool seek(size_t n) {
        if (n > length) return false;
        pos = n;
        return true;
    }
};

void showError(const char* text) {
    Serial.printf("[VIDEO] ERROR: %s\n", text);

    if (xSemaphoreTake(displaySemaphore, pdMS_TO_TICKS(1000)) == pdTRUE) {
        tft.fillScreen(TFT_BLACK);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(TFT_WHITE);
        tft.drawString(text, tft.width() / 2, tft.height() / 2, 2);
        tft.drawString("Press Back to return",
                       tft.width() / 2,
                       tft.height() / 2 + 24,
                       2);
        xSemaphoreGive(displaySemaphore);
    }
}

const char* frameErrorText(media::FrameResult result) {
    switch (result) {
        case media::FrameResult::TooLarge:
            return "Video frame too large";
        case media::FrameResult::Unsupported:
            return "Progressive JPEG unsupported";
        case media::FrameResult::Invalid:
            return "Invalid MJPEG frame";
        case media::FrameResult::End:
            return "Video has no JPEG frames";
        case media::FrameResult::Frame:
        default:
            return "Video decode error";
    }
}

void releasePlaybackResources() {
    for (auto& buffer : readBuffers) {
        if (buffer.data) {
            heap_caps_free(buffer.data);
            buffer.data = nullptr;
        }
        buffer.length = 0;
        buffer.position = 0;
        buffer.state = BufferState::Free;
    }

    if (frame) {
        heap_caps_free(frame);
        frame = nullptr;
    }

    if (prefetchQueue) {
        vQueueDelete(prefetchQueue);
        prefetchQueue = nullptr;
    }
    if (bufferReadySemaphore) {
        vSemaphoreDelete(bufferReadySemaphore);
        bufferReadySemaphore = nullptr;
    }
    if (prefetchStoppedSemaphore) {
        vSemaphoreDelete(prefetchStoppedSemaphore);
        prefetchStoppedSemaphore = nullptr;
    }

    reader.reset();
}

} // namespace

void closeVideo() {
    playing = false;
    stopPrefetchTask();

    if (xSemaphoreTake(sdSemaphore, pdMS_TO_TICKS(1000)) == pdTRUE) {
        if (video) video.close();
        xSemaphoreGive(sdSemaphore);
    }

    if (decodedFrames || bufferWaitCount) {
        Serial.printf(
            "[VIDEO] Close: frames=%lu refills=%lu cacheWaits=%lu avgWait=%.1f ms maxWait=%.1f ms\n",
            static_cast<unsigned long>(decodedFrames),
            static_cast<unsigned long>(refillCount),
            static_cast<unsigned long>(bufferWaitCount),
            bufferWaitCount ? (totalBufferWaitUs / 1000.0f / bufferWaitCount) : 0.0f,
            maxBufferWaitUs / 1000.0f
        );
    }

    releasePlaybackResources();

    nextFrameAtUs = 0;
    hasFrame = false;
    decodedFrames = 0;
    refillCount = 0;
    bufferWaitCount = 0;
    totalBufferWaitUs = 0;
    maxBufferWaitUs = 0;
    prefetchStopRequested = false;
}

bool openVideo(const String& path) {
    closeVideo();

    if (!psramFound()) {
        showError("PSRAM not detected");
        return false;
    }

    frame = static_cast<uint8_t*>(
        heap_caps_malloc(kFrameCapacity, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
    );
    readBuffers[0].data = static_cast<uint8_t*>(
        heap_caps_malloc(kReadBufferSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
    );
    readBuffers[1].data = static_cast<uint8_t*>(
        heap_caps_malloc(kReadBufferSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
    );

    if (!frame || !readBuffers[0].data || !readBuffers[1].data) {
        closeVideo();
        showError("Not enough PSRAM for video buffers");
        return false;
    }

    if (!prefetchQueue) prefetchQueue = xQueueCreate(2, sizeof(uint8_t));
    if (!bufferReadySemaphore) bufferReadySemaphore = xSemaphoreCreateBinary();
    if (!prefetchStoppedSemaphore) prefetchStoppedSemaphore = xSemaphoreCreateBinary();
    if (!prefetchQueue || !bufferReadySemaphore || !prefetchStoppedSemaphore) {
        closeVideo();
        showError("Cannot create video cache task");
        return false;
    }

    if (xSemaphoreTake(sdSemaphore, pdMS_TO_TICKS(1000)) == pdTRUE) {
        video = SD.open(path, FILE_READ);
        xSemaphoreGive(sdSemaphore);
    }

    if (!video || video.size() == 0) {
        closeVideo();
        showError("Cannot open video");
        return false;
    }

    reader.reset();
    activeBuffer = 0;
    prefetchStopRequested = false;

    // Prime both halves before playback starts. From this point on all SD reads
    // happen on Core 0 in short slices while Core 1 decodes/displays.
    setBufferState(0, BufferState::Filling);
    if (!fillReadAheadBuffer(0)) {
        closeVideo();
        showError("Cannot buffer video");
        return false;
    }

    setBufferState(1, BufferState::Filling);
    if (!fillReadAheadBuffer(1)) {
        closeVideo();
        showError("Cannot buffer video");
        return false;
    }

    if (!startPrefetchTask()) {
        closeVideo();
        showError("Cannot start video cache task");
        return false;
    }

    Serial.printf(
        "[VIDEO] Open: %s | file=%lu bytes | frame=%u KiB | cache=%u KiB x2 | low-water=%u KiB | fps=%lu | PSRAM free=%lu KiB\n",
        path.c_str(),
        static_cast<unsigned long>(video.size()),
        static_cast<unsigned>(kFrameCapacity / 1024),
        static_cast<unsigned>(kReadBufferSize / 1024),
        static_cast<unsigned>(kLowWatermark / 1024),
        static_cast<unsigned long>(kVideoFps),
        static_cast<unsigned long>(ESP.getFreePsram() / 1024)
    );

    playing = true;
    nextFrameAtUs = esp_timer_get_time(); // show first frame immediately
    hasFrame = false;
    return true;
}

bool advanceVideo() {
    if (!frame || !video || !playing) return false;

    const int64_t nowUs = esp_timer_get_time();
    if (nowUs < nextFrameAtUs) return true;

    size_t frameSize = 0;
    const media::FrameResult result =
        media::readMjpegFrame(reader, frame, kFrameCapacity, frameSize);

    if (result != media::FrameResult::Frame) {
        const char* error = frameErrorText(result);
        closeVideo();
        showError(error);
        return false;
    }

    // Read JPEG dimensions from the compressed frame without reopening SD.
    FrameReader input(frame, frameSize);
    uint16_t width = 0;
    uint16_t height = 0;

    if (!media::readJpegDimensions(input, width, height)) {
        closeVideo();
        showError("Cannot read JPEG dimensions");
        return false;
    }

    // Keep decode work reasonable for the ESP32-S3. This is a playback decoder
    // limit only; web compression/upload remains warning-only as requested.
    if (width > 640 || height > 480) {
        closeVideo();
        showError("Use video up to 640 x 480");
        return false;
    }

    if (!hasFrame) {
        Serial.printf("[VIDEO] First frame: %ux%u | %u bytes\n",
                      width,
                      height,
                      static_cast<unsigned>(frameSize));
    }

    const float scale = std::min(
        static_cast<float>(tft.width()) / static_cast<float>(width),
        static_cast<float>(tft.height()) / static_cast<float>(height)
    );

    const int scaledWidth = std::max(1, static_cast<int>(width * scale + 0.5f));
    const int scaledHeight = std::max(1, static_cast<int>(height * scale + 0.5f));
    const int drawX = (static_cast<int>(tft.width()) - scaledWidth) / 2;
    const int drawY = (static_cast<int>(tft.height()) - scaledHeight) / 2;

    bool drawn = false;
    if (xSemaphoreTake(displaySemaphore, pdMS_TO_TICKS(1000)) == pdTRUE) {
        if (!hasFrame) tft.fillScreen(TFT_BLACK);

        drawn = tft.drawJpg(
            frame,
            static_cast<uint32_t>(frameSize),
            drawX,
            drawY,
            0,
            0,
            0,
            0,
            scale,
            scale,
            lgfx::datum_t::top_left
        );
        xSemaphoreGive(displaySemaphore);
    }

    if (!drawn) {
        closeVideo();
        showError("Cannot decode JPEG frame");
        return false;
    }

    hasFrame = true;
    ++decodedFrames;

    // Absolute microsecond pacing: JPEG decode/draw time is part of the frame
    // budget instead of being added on top of a fixed delay.
    nextFrameAtUs += kFrameIntervalUs;
    const int64_t afterDrawUs = esp_timer_get_time();

    // If the device is more than one whole frame behind (for example after a
    // rare SD stall), resynchronise once instead of decoding a burst of frames.
    if (afterDrawUs - nextFrameAtUs > kFrameIntervalUs) {
        nextFrameAtUs = afterDrawUs + kFrameIntervalUs;
    }

    return true;
}

bool videoIsPlaying() {
    return playing;
}

uint32_t videoFramesDecoded() {
    return decodedFrames;
}
