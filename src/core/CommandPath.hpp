#pragma once
#include <cstring>
#include <cstddef>

// FreeRTOS queues copy bytes: keep the payload self-contained, without String pointers.
struct CommandPath {
    char bytes[1024]{};
    bool valid = true;
    CommandPath& operator=(const char* value) {
        const auto length = value ? std::strlen(value) : 0;
        // Reject oversized paths instead of opening a silently truncated filename/URL.
        valid = length < sizeof(bytes);
        if (!valid) bytes[0] = '\0';
        else if (value) std::memcpy(bytes, value, length + 1);
        else bytes[0] = '\0';
        return *this;
    }
    CommandPath& operator=(std::nullptr_t) { return *this = static_cast<const char*>(nullptr); }
    template<class Text> CommandPath& operator=(const Text& value) { return *this = value.c_str(); }
    const char* c_str() const { return bytes; }
};
