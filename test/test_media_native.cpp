#ifndef ARDUINO
#include "../src/core/Commands.hpp"
#include "../src/core/MediaNavigation.hpp"
#include "../src/display/JpegDimensions.hpp"
#include <vector>
#include <cassert>
#include <cstdio>
#include <string>

static void test_empty_list() {
    assert(media::lastIndex(0) == 0);
    assert(!media::validIndex(0, 0));
    assert(media::wrapIndex(-1, 0) == 0);
}
static void test_first_loaded_list() {
    assert(media::lastIndex(8) == 7);
    for (int i = 0; i <= media::lastIndex(8); ++i) assert(media::validIndex(i, 8));
    assert(!media::validIndex(8, 8));
    assert(!media::validIndex(-1, 8));
}
static void test_single_item() {
    assert(media::lastIndex(1) == 0);
    assert(media::wrapIndex(-1, 1) == 0);
    assert(media::wrapIndex(1, 1) == 0);
}
static void test_station_wrap() {
    assert(media::wrapIndex(-1, 3) == 2);
    assert(media::wrapIndex(3, 3) == 0);
    assert(media::wrapIndex(1, 3) == 1);
}
static void test_display_queue_owns_path() {
    DISPLAY_COMMAND received{};
    {
        std::string filename = "/main/Pictures/first-image-with-a-long-name.jpg";
        DISPLAY_COMMAND sent{};
        sent.path = filename;
        std::memcpy(&received, &sent, sizeof(sent));
        filename.assign("changed");
        sent.path = "/main/Pictures/second.gif";
    }
    assert(std::string(received.path.c_str()) == "/main/Pictures/first-image-with-a-long-name.jpg");
}
static void test_audio_queue_owns_url() {
    AUDIO_COMMAND sent{}, received{};
    sent.path = std::string("https://example.com/live/audio.mp3");
    std::memcpy(&received, &sent, sizeof(sent));
    sent.path = "";
    assert(std::string(received.path.c_str()) == "https://example.com/live/audio.mp3");
}
static void test_path_limits() {
    CommandPath path;
    path = std::string(1023, 'x');
    assert(path.valid && std::strlen(path.c_str()) == 1023);
    path = std::string(1024, 'x');
    assert(!path.valid && path.c_str()[0] == '\0');
    path = "";
    assert(path.valid && path.c_str()[0] == '\0');
    path = nullptr;
    assert(path.valid && path.c_str()[0] == '\0');
}
static void test_utf8_path() {
    CommandPath sent, received;
    const char* path = u8"/main/Pictures/รูปภาพ.jpg";
    sent = path;
    std::memcpy(&received, &sent, sizeof(sent));
    sent = "";
    assert(std::strcmp(received.c_str(), path) == 0);
}
struct Reader {
    std::vector<unsigned char> bytes;
    std::size_t offset = 0;
    explicit Reader(std::initializer_list<unsigned char> data) : bytes(data) {}
    int read() { return available() ? bytes[offset++] : -1; }
    bool available() const { return offset < bytes.size(); }
    std::size_t size() const { return bytes.size(); }
    std::size_t position() const { return offset; }
    bool seek(std::size_t target) { if (target > size()) return false; offset = target; return true; }
};
static void test_jpeg_dimensions() {
    Reader file{0xff,0xd8,0xff,0xc0,0,11,8,0,240,1,64,1,1,0x11,0};
    uint16_t w, h;
    assert(media::readJpegDimensions(file,w,h));
    assert(w == 320 && h == 240);
}
static void test_jpeg_metadata_and_progressive() {
    Reader file{0xff,0xd8,0xff,0xe0,0,4,0,0,0xff,0xc2,0,11,8,1,64,0,240,1,1,0x11,0};
    uint16_t w, h;
    assert(media::readJpegDimensions(file,w,h));
    assert(w == 240 && h == 320);
}
static void test_jpeg_truncated_marker() {
    Reader file{0xff,0xd8,0xff,0xff,0xff};
    uint16_t w=99, h=99;
    assert(!media::readJpegDimensions(file,w,h));
    assert(w == 0 && h == 0);
}
static void test_jpeg_invalid_lengths() {
    uint16_t w, h;
    Reader shortSegment{0xff,0xd8,0xff,0xe0,0,1};
    Reader pastEnd{0xff,0xd8,0xff,0xe0,0xff,0xff};
    Reader shortFrame{0xff,0xd8,0xff,0xc0,0,3,8};
    assert(!media::readJpegDimensions(shortSegment,w,h));
    assert(!media::readJpegDimensions(pastEnd,w,h));
    assert(!media::readJpegDimensions(shortFrame,w,h));
}
int main() {
    test_empty_list(); test_first_loaded_list(); test_single_item(); test_station_wrap();
    test_display_queue_owns_path(); test_audio_queue_owns_url(); test_path_limits(); test_utf8_path();
    test_jpeg_dimensions(); test_jpeg_metadata_and_progressive();
    test_jpeg_truncated_marker(); test_jpeg_invalid_lengths();
    std::puts("PASS: 12 media navigation, queue ownership and JPEG parser tests");
}

#endif
