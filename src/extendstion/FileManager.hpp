#include "GlobalVar.hpp"

#ifndef SD_H
#define SD_H
#include "SD.h"
#endif

// SDcard pin
#define MOSI 23
#define MISO 19 
#define SCL 18
#define CS 15
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
};

#ifndef FILE_CARD
#define FILE_CARD
    extern FileManager file_card;
#endif

