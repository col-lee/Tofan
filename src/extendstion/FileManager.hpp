#include "GlobalVar.hpp"

#ifndef SD_H
#define SD_H
#include <SD.h>
#include <FS.h>
#endif

// SDcard pin
#define SD_MOSI 11
#define SD_MISO 13
#define SD_SCL 12
#define SD_CS 14
extern bool isConnectSDcard;

class FileManager
{
private:
    /* data */
public:
    FileManager();
    ~FileManager();
    void initSDCard();
    String getFileListJSON(String dirPath);
    bool createFile(String path);
    bool createFolder(String path);
    bool deleteFile(String path);
    bool renameFile(String oldPath, String newPath);
    bool updateFile(String path, String content);
};

#ifndef FILE_CARD
#define FILE_CARD
    extern FileManager file_card;
#endif

