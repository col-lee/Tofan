// Mounts the SD card and implements directory and file operations.
#pragma once

#include "../core/SharedResources.hpp"

#include <SD.h>
#include <FS.h>

extern bool isConnectSDcard;

class FileManager
{
private:
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
