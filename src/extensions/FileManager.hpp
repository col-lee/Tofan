#pragma once

#include "GlobalVar.hpp"

#include <SD.h>
#include <FS.h>

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

extern FileManager file_card;

