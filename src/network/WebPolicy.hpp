#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
namespace portal {
inline bool accountName(const char* value) {
    size_t n=value?std::strlen(value):0;
    if(!n||n>31||value[0]==' '||value[n-1]==' ')return false;
    for(size_t i=0;i<n;i++)if(static_cast<unsigned char>(value[i])<32||value[i]==127)return false;
    return true;
}
constexpr uint64_t UploadReserve = 64u * 1024u;
inline bool uploadFits(uint64_t fileSize,uint64_t total,uint64_t used) {
    if(!fileSize || total < used) return false;
    const uint64_t available = total - used;
    return fileSize <= available && available - fileSize >= UploadReserve;
}
inline bool filename(const char* s) {
    const size_t n = s ? std::strlen(s) : 0;
    if (!n || n > 120 || s[0] == '.' || s[n-1] == ' ' || s[n-1] == '.') return false;
    for (size_t i=0;i<n;i++) if (static_cast<unsigned char>(s[i]) < 32 || std::strchr("/\\:*?\"<>|",s[i])) return false;
    return true;
}
inline bool directory(const char* s) {
    return s && (!std::strcmp(s,"Pictures") || !std::strcmp(s,"Musics") || !std::strcmp(s,"Videos") || !std::strcmp(s,"Files"));
}
inline bool extensionEquals(const char* name, const char* extension) {
    if (!name || !extension) return false;
    const char* dot = std::strrchr(name, '.');
    if (!dot || !dot[1]) return false;
    ++dot;
    for (; *dot && *extension; ++dot, ++extension) {
        char a=*dot,b=*extension;
        if(a>='A'&&a<='Z')a=static_cast<char>(a-'A'+'a');
        if(b>='A'&&b<='Z')b=static_cast<char>(b-'A'+'a');
        if(a!=b)return false;
    }
    return *dot=='\0' && *extension=='\0';
}
inline bool audioFilename(const char* name) {
    return extensionEquals(name,"mp3") || extensionEquals(name,"wav") || extensionEquals(name,"aac") ||
           extensionEquals(name,"m4a") || extensionEquals(name,"flac");
}
inline bool mediaFilename(const char* name,const char* dir) {
    if(!filename(name) || !directory(dir)) return false;
    if(!std::strcmp(dir,"Files")) return true;
    if(!std::strcmp(dir,"Pictures")) return extensionEquals(name,"jpg") || extensionEquals(name,"jpeg") ||
        extensionEquals(name,"png") || extensionEquals(name,"gif");
    if(!std::strcmp(dir,"Musics")) return audioFilename(name);
    return extensionEquals(name,"mp4") || extensionEquals(name,"webm") || extensionEquals(name,"mov") ||
        extensionEquals(name,"m4v") || extensionEquals(name,"avi") || extensionEquals(name,"mjpeg") || extensionEquals(name,"mjpg");
}
inline bool imageHeader(const uint8_t* bytes, size_t n) {
    // esp_image_header_t: magic, segment count, chip_id (ESP32-S3 = 9).
    return n >= 24 && bytes[0] == 0xe9 && bytes[1] > 0 && bytes[1] <= 16 && bytes[12] == 9 && bytes[13] == 0;
}
inline bool firmwareSize(size_t n, size_t partition) { return n >= 24 && n <= partition; }
inline bool uploadChunkValid(size_t expected,size_t received,size_t index,size_t length,bool finished) {
    return !finished && received<=expected && index==received && length<=expected-received;
}
}
