#include "VideoPlayback.hpp"
#include "MjpegFrame.hpp"
#include "JpegDimensions.hpp"
#include "DisplayManager.hpp"

#include <algorithm>
#include <esp_heap_caps.h>

namespace {

// ESP32-S3 N16R8 has PSRAM, so keep one compressed JPEG frame there.
// 512 KiB gives enough headroom for most 320x240 / 640x480 MJPEG frames.
constexpr size_t kFrameCapacity = 512 * 1024;
constexpr size_t kReadBufferSize = 8 * 1024;
constexpr uint32_t kVideoFps = 12;
constexpr uint32_t kFrameIntervalMs = 1000 / kVideoFps;

File video;
uint8_t* frame = nullptr;

uint8_t readBuffer[kReadBufferSize];
size_t cursor = 0;
size_t buffered = 0;

uint32_t nextFrameAt = 0;
bool hasFrame = false;

struct Reader {
    int read() {
        if (cursor >= buffered) {
            buffered = video.read(readBuffer, sizeof(readBuffer));
            cursor = 0;
        }

        if (cursor >= buffered) {
            return -1;
        }

        return readBuffer[cursor++];
    }

    void reset() {
        cursor = 0;
        buffered = 0;
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

bool rewindVideo() {
    if (!video.seek(0)) {
        return false;
    }

    reader.reset();
    return true;
}

} // namespace

void closeVideo() {
    if (xSemaphoreTake(sdSemaphore, portMAX_DELAY) == pdTRUE) {
        if (video) {
            video.close();
        }
        xSemaphoreGive(sdSemaphore);
    }

    if (frame) {
        heap_caps_free(frame);
        frame = nullptr;
    }

    reader.reset();
    nextFrameAt = 0;
    hasFrame = false;
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

    if (!frame) {
        showError("Not enough PSRAM for video");
        return false;
    }

    if (xSemaphoreTake(sdSemaphore, pdMS_TO_TICKS(1000)) == pdTRUE) {
        video = SD.open(path, FILE_READ);
        reader.reset();
        xSemaphoreGive(sdSemaphore);
    }

    if (!video) {
        closeVideo();
        showError("Cannot open video");
        return false;
    }

    Serial.printf(
        "[VIDEO] Open: %s | file=%lu bytes | buffer=%u KiB | fps=%lu\n",
        path.c_str(),
        static_cast<unsigned long>(video.size()),
        static_cast<unsigned>(kFrameCapacity / 1024),
        static_cast<unsigned long>(kVideoFps)
    );

    nextFrameAt = millis(); // show first frame immediately
    hasFrame = false;
    return true;
}

bool advanceVideo() {
    if (!frame || !video) {
        return false;
    }

    const uint32_t now = millis();

    // Signed subtraction keeps millis() rollover safe.
    if (static_cast<int32_t>(now - nextFrameAt) < 0) {
        return true;
    }

    size_t frameSize = 0;
    media::FrameResult result = media::FrameResult::Invalid;

    // Keep the SD bus locked while Reader is consuming bytes.
    if (xSemaphoreTake(sdSemaphore, pdMS_TO_TICKS(50)) != pdTRUE) {
        return true;
    }

    result = media::readMjpegFrame(reader, frame, kFrameCapacity, frameSize);

    // Loop the video when the stream reaches EOF.
    if (result == media::FrameResult::End && hasFrame) {
        if (rewindVideo()) {
            result = media::readMjpegFrame(reader, frame, kFrameCapacity, frameSize);
        }
    }

    xSemaphoreGive(sdSemaphore);

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

    // Keep decode work reasonable for the ESP32-S3.
    if (width > 640 || height > 480) {
        closeVideo();
        showError("Use video up to 640 x 480");
        return false;
    }

    if (!hasFrame) {
        Serial.printf(
            "[VIDEO] First frame: %ux%u | %u bytes\n",
            width,
            height,
            static_cast<unsigned>(frameSize)
        );
    }

    const float scale = std::min(
        static_cast<float>(tft.width()) / static_cast<float>(width),
        static_cast<float>(tft.height()) / static_cast<float>(height)
    );

    // LovyanGFX JPEG scaling is most predictable when we give drawJpg()
    // an explicit top-left destination. Using middle_center together with
    // scaled JPEGs can shift the image depending on decoder/datum handling.
    const int scaledWidth = std::max(1, static_cast<int>(width * scale + 0.5f));
    const int scaledHeight = std::max(1, static_cast<int>(height * scale + 0.5f));
    const int drawX = (static_cast<int>(tft.width()) - scaledWidth) / 2;
    const int drawY = (static_cast<int>(tft.height()) - scaledHeight) / 2;

    bool drawn = false;

    if (xSemaphoreTake(displaySemaphore, pdMS_TO_TICKS(1000)) == pdTRUE) {
        if (!hasFrame) {
            // Clear once so letterbox/pillarbox areas remain black.
            tft.fillScreen(TFT_BLACK);
        }

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

    // Frame pacing. Do not try to catch up by decoding many frames at once.
    nextFrameAt += kFrameIntervalMs;

    const uint32_t afterDraw = millis();
    if (static_cast<int32_t>(afterDraw - nextFrameAt) >
        static_cast<int32_t>(kFrameIntervalMs)) {
        nextFrameAt = afterDraw + kFrameIntervalMs;
    }

    return true;
}
