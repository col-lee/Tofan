#pragma once
#include <map>
#include <string>
#include <vector>
#include <cstring>
// In-memory NVS adapter for testing the production UserSettings implementation.
class Preferences {
    std::string prefix;
public:
    static std::map<std::string,std::vector<unsigned char>>& storage() {
        static std::map<std::string,std::vector<unsigned char>> data; return data;
    }
    static bool& failWrite() { static bool failed=false; return failed; }
    bool begin(const char* name,bool) { prefix=std::string(name)+"/"; return true; }
    void end() {}
    std::size_t getBytesLength(const char* key) { return storage()[prefix+key].size(); }
    std::size_t getBytes(const char* key,void* out,std::size_t size) {
        auto& bytes=storage()[prefix+key]; if(bytes.size()!=size) return 0;
        std::memcpy(out,bytes.data(),size); return size;
    }
    std::size_t putBytes(const char* key,const void* data,std::size_t size) {
        if(failWrite()) return 0;
        const auto* first=static_cast<const unsigned char*>(data);
        storage()[prefix+key]=std::vector<unsigned char>(first,first+size); return size;
    }
};
