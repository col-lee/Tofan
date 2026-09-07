#ifndef ARDUINO
#include "../src/audio/RecordingPath.hpp"
#include <cassert>
#include <set>
#include <string>

int main() {
    std::set<std::string> files{"/main/Musics/voice_record.wav"};
    auto exists = [&](const char* path) { return files.count(path) != 0; };
    char path[64];
    uint32_t hint = 1;

    // Consecutive recording sessions produce independent files.
    assert(recording::nextPath(hint, exists, path, sizeof(path)));
    assert(std::string(path) == "/main/Musics/voice_record_000001.wav");
    files.insert(path);
    assert(recording::nextPath(hint, exists, path, sizeof(path)));
    assert(std::string(path) == "/main/Musics/voice_record_000002.wav");
    files.insert(path);

    // Reboot loses the hint, but still cannot overwrite an existing recording.
    hint = 1;
    assert(recording::nextPath(hint, exists, path, sizeof(path)));
    assert(std::string(path) == "/main/Musics/voice_record_000003.wav");

    // A filename added by another operation is skipped, including directories.
    files.insert("/main/Musics/voice_record_000004.wav");
    assert(recording::nextPath(hint, exists, path, sizeof(path)));
    assert(std::string(path) == "/main/Musics/voice_record_000005.wav");
    assert(files.count("/main/Musics/voice_record.wav") == 1);

    // Refuse truncated output; do not advance the hint.
    char tiny[8];
    hint = 1;
    assert(!recording::nextPath(hint, exists, tiny, sizeof(tiny)));
    assert(tiny[0] == '\0' && hint == 1);

    // Use the final number once, then stop without wrapping.
    hint = UINT32_MAX;
    assert(recording::nextPath(hint, exists, path, sizeof(path)));
    assert(std::string(path) == "/main/Musics/voice_record_4294967295.wav");
    assert(!recording::nextPath(hint, exists, path, sizeof(path)));

    // If the final name is already occupied, fail without returning that path.
    hint = UINT32_MAX;
    files.insert("/main/Musics/voice_record_4294967295.wav");
    assert(!recording::nextPath(hint, exists, path, sizeof(path)));
    assert(path[0] == '\0');
    std::puts("PASS: 6 recording filename regression scenarios");
}
#endif
