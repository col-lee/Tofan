#pragma once

#include <cstddef>
#include <cstdint>

namespace media {

enum class FrameResult {
    Frame,
    End,
    Invalid,
    TooLarge,
    Unsupported
};

// Reads one JPEG frame from an MJPEG byte stream.
//
// Unlike the old implementation, this scans forward until JPEG SOI (FF D8)
// is found. That means it can tolerate common MJPEG boundary/header bytes
// between JPEG frames instead of requiring every frame to start at the
// exact next byte.
//
// Segment lengths are still respected, so FF D9 bytes inside APP/EXIF data
// are not mistaken for the end of the image.
template <class Reader>
FrameResult readMjpegFrame(
    Reader& source,
    uint8_t* out,
    size_t capacity,
    size_t& size
) {
    size = 0;

    if (!out || capacity < 2) {
        return FrameResult::TooLarge;
    }

    // Find JPEG SOI marker: FF D8.
    int previous = -1;
    size_t scanned = 0;

    while (true) {
        // Bound malformed data scanning so Back/display commands stay responsive.
        if (++scanned > 65536) return FrameResult::Invalid;
        const int value = source.read();

        if (value < 0) {
            return FrameResult::End;
        }

        if (previous == 0xFF && value == 0xD8) {
            break;
        }

        previous = value;
    }

    out[size++] = 0xFF;
    out[size++] = 0xD8;

    auto take = [&]() -> int {
        const int value = source.read();

        if (value < 0) {
            return -1;
        }

        if (size >= capacity) {
            return -2;
        }

        out[size++] = static_cast<uint8_t>(value);
        return value;
    };

    bool entropyData = false;
    bool sawScan = false;

    while (true) {
        const int prefix = take();

        if (prefix < 0) {
            return prefix == -2
                ? FrameResult::TooLarge
                : FrameResult::Invalid;
        }

        if (prefix != 0xFF) {
            // Raw compressed bytes are valid only after SOS.
            if (entropyData) {
                continue;
            }

            return FrameResult::Invalid;
        }

        int marker;

        do {
            marker = take();
        } while (marker == 0xFF);

        if (marker < 0) {
            return marker == -2
                ? FrameResult::TooLarge
                : FrameResult::Invalid;
        }

        // Inside entropy-coded data:
        // FF 00 is a stuffed FF byte.
        // FF D0..D7 are restart markers.
        if (entropyData &&
            (marker == 0x00 || (marker >= 0xD0 && marker <= 0xD7))) {
            continue;
        }

        // End Of Image.
        if (marker == 0xD9) {
            return sawScan
                ? FrameResult::Frame
                : FrameResult::Invalid;
        }

        // A second SOI inside the same frame is malformed.
        if (marker == 0xD8 || marker == 0x00) {
            return FrameResult::Invalid;
        }

        // TEM has no length payload.
        if (marker == 0x01) {
            continue;
        }

        // LovyanGFX's bundled TJpgD decoder handles baseline SOF0 JPEG.
        if (marker == 0xC2) {
            return FrameResult::Unsupported;
        }

        const int high = take();
        const int low = take();

        if (high < 0 || low < 0) {
            return (high == -2 || low == -2)
                ? FrameResult::TooLarge
                : FrameResult::Invalid;
        }

        const int segmentLength = (high << 8) | low;

        if (segmentLength < 2) {
            return FrameResult::Invalid;
        }

        // Length includes the two length bytes already copied above.
        for (int i = 2; i < segmentLength; ++i) {
            const int value = take();

            if (value < 0) {
                return value == -2
                    ? FrameResult::TooLarge
                    : FrameResult::Invalid;
            }
        }

        entropyData = (marker == 0xDA); // SOS
        if (entropyData) {
            sawScan = true;
        }
    }
}

} // namespace media
