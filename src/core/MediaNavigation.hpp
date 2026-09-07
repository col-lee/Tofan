#pragma once
#include <cstddef>

namespace media {
inline int lastIndex(std::size_t count) { return count ? static_cast<int>(count - 1) : 0; }
inline bool validIndex(int index, std::size_t count) {
    return index >= 0 && static_cast<std::size_t>(index) < count;
}
inline int wrapIndex(int index, std::size_t count) {
    if (!count) return 0;
    const int size = static_cast<int>(count);
    return (index % size + size) % size;
}
}
