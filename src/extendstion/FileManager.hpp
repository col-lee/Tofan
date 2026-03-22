#include "GlobalVar.hpp"

#ifndef SD_H
#define SD_H
#include "SD.h"
#endif

// SDcard pin
#define MOSI 13
#define MISO 27
#define SCL 14
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
    void scanAll(File dir, int numTabs, JsonArray &jfiarray, JsonArray &jfoarray);
    void listFileAll();
    void scanfilterFile(File dir, int numTabs, JsonArray &jfiarray, JsonArray &jfoarray, String exts);
    void filterFile(String exts);
    void filterFileInPath(String path, String exts);
};

#ifndef FILE_CARD
#define FILE_CARD
    extern FileManager file_card;
#endif

