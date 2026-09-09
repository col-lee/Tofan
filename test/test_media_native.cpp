#ifndef ARDUINO
#include "../src/core/Commands.hpp"
#include "../src/core/MediaNavigation.hpp"
#include "../src/display/JpegDimensions.hpp"
#include "../src/display/MjpegFrame.hpp"
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

static std::vector<unsigned char> baselineFrame(unsigned short width=320, unsigned short height=240) {
    return {
        0xff,0xd8,
        0xff,0xc0,0x00,0x0b,0x08,
        static_cast<unsigned char>(height >> 8), static_cast<unsigned char>(height & 0xff),
        static_cast<unsigned char>(width >> 8), static_cast<unsigned char>(width & 0xff),
        0x01,0x01,0x11,0x00,
        0xff,0xda,0x00,0x08,0x01,0x01,0x00,0x00,0x3f,0x00,
        0x11,0x22,0xff,0x00,0x33,
        0xff,0xd9
    };
}
struct StreamReader {
    std::vector<unsigned char> bytes;
    std::size_t offset = 0;
    explicit StreamReader(std::vector<unsigned char> data) : bytes(std::move(data)) {}
    int read() { return offset < bytes.size() ? bytes[offset++] : -1; }
};
static void test_mjpeg_scans_boundary_and_extracts_frame() {
    auto jpeg = baselineFrame();
    std::vector<unsigned char> stream = {'-','-','b','o','u','n','d','a','r','y','\r','\n'};
    stream.insert(stream.end(), jpeg.begin(), jpeg.end());
    stream.insert(stream.end(), {'\r','\n','x','x'});
    StreamReader source(std::move(stream));
    std::vector<unsigned char> out(1024);
    std::size_t size = 0;
    assert(media::readMjpegFrame(source,out.data(),out.size(),size) == media::FrameResult::Frame);
    assert(size == jpeg.size());
    assert(out[0] == 0xff && out[1] == 0xd8);
    assert(out[size-2] == 0xff && out[size-1] == 0xd9);
}
static void test_mjpeg_two_consecutive_frames() {
    auto first = baselineFrame(320,240);
    auto second = baselineFrame(160,120);
    std::vector<unsigned char> stream;
    stream.insert(stream.end(), first.begin(), first.end());
    stream.insert(stream.end(), {'\r','\n','-','-','x','\r','\n'});
    stream.insert(stream.end(), second.begin(), second.end());
    StreamReader source(std::move(stream));
    std::vector<unsigned char> out(1024);
    std::size_t size = 0;
    assert(media::readMjpegFrame(source,out.data(),out.size(),size) == media::FrameResult::Frame);
    assert(media::readMjpegFrame(source,out.data(),out.size(),size) == media::FrameResult::Frame);
    Reader dimensionReader({});
    dimensionReader.bytes.assign(out.begin(), out.begin()+size);
    uint16_t w=0,h=0;
    assert(media::readJpegDimensions(dimensionReader,w,h));
    assert(w == 160 && h == 120);
}
static void test_mjpeg_frame_too_large() {
    auto jpeg = baselineFrame();
    StreamReader source(std::move(jpeg));
    unsigned char out[8]{};
    std::size_t size = 0;
    assert(media::readMjpegFrame(source,out,sizeof(out),size) == media::FrameResult::TooLarge);
}

int main() {
    test_empty_list(); test_first_loaded_list(); test_single_item(); test_station_wrap();
    test_display_queue_owns_path(); test_audio_queue_owns_url(); test_path_limits(); test_utf8_path();
    test_jpeg_dimensions(); test_jpeg_metadata_and_progressive();
    test_jpeg_truncated_marker(); test_jpeg_invalid_lengths();
    test_mjpeg_scans_boundary_and_extracts_frame(); test_mjpeg_two_consecutive_frames(); test_mjpeg_frame_too_large();
    std::puts("PASS: 15 media navigation, queue ownership, JPEG and MJPEG parser tests");
}

#endif
