#include "FileManager.hpp"
#include <ArduinoJson.h>

SPIClass vspi(VSPI);
SemaphoreHandle_t sdSemaphore;
bool isConnectSDcard = false;
bool isFileManager_install;

FileManager::FileManager()
{

}

FileManager::~FileManager()
{

}

void FileManager::initSDCard()
{
    pinMode(CS, OUTPUT);
    digitalWrite(CS, HIGH);
    vspi.begin(SCL, MISO, MOSI, CS);
    vspi.setFrequency(8000000);
    if (SD.begin(CS, vspi, 8000000))
    {
        isConnectSDcard = true;
        Serial.println("SD mount card.");
        Serial.print("Card size: ");
        Serial.println(SD.cardSize() / (1024 * 1024));

        if (xSemaphoreTake(displaySemaphore, pdMS_TO_TICKS(100)) == pdTRUE) {
            tft.println("SD mount card.");
            tft.print("Card size: ");
            tft.print(SD.cardSize() / (1024 * 1024));
            tft.println("MB");
            xSemaphoreGive(displaySemaphore);
        }

        if (xSemaphoreTake(sdSemaphore, pdMS_TO_TICKS(100)) == pdTRUE)
        {
            if (!SD.exists("/main"))
                SD.mkdir("/main");
            if (!SD.exists("/main/Pictures"))
                SD.mkdir("/main/Pictures");
            if (!SD.exists("/main/Musics"))
                SD.mkdir("/main/Musics");
            if (!SD.exists("/WEB_Source"))
                SD.mkdir("/WEB_Source");
            xSemaphoreGive(sdSemaphore);
        }

        isFileManager_install = true;
    }
    else
    {
        isConnectSDcard = false;
        isFileManager_install = false;
        Serial.println("Please attach SDCard.");
        if(xSemaphoreTake(displaySemaphore, pdMS_TO_TICKS(100)) == pdTRUE) {
            tft.setTextColor(TFT_RED);
            tft.println("Please attach SDCard.");
            tft.setTextColor(TFT_WHITE);
            xSemaphoreGive(displaySemaphore);
        }
    }
}

// ฟังก์ชันแสกนโฟลเดอร์และแปลงเป็น JSON
// 1. ฟังก์ชันแสกนโฟลเดอร์
String FileManager::getFileListJSON(String dirPath) {
    // 🚨 ล็อกชั้นที่ 1: ถ้าไม่ใช่โฟลเดอร์ /main ให้ปฏิเสธการเข้าถึงทันที
    if (!dirPath.startsWith("/main")) {
        return "{\"type\":\"file_list\",\"status\":\"error\",\"msg\":\"Access Denied\"}";
    }

    if (xSemaphoreTake(sdSemaphore, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return "{\"type\":\"file_list\",\"status\":\"busy\"}";
    }

    File dir = SD.open(dirPath);
    if (!dir || !dir.isDirectory()) {
        xSemaphoreGive(sdSemaphore);
        return "{\"type\":\"file_list\",\"path\":\"" + dirPath + "\",\"files\":[]}";
    }

    JsonDocument doc;
    doc["type"] = "file_list";
    doc["path"] = dirPath;
    JsonArray files = doc["files"].to<JsonArray>();

    File file = dir.openNextFile();
    while (file) {
        JsonObject item = files.add<JsonObject>();
        String fileName = String(file.name());
        int slashIndex = fileName.lastIndexOf('/');
        if (slashIndex != -1) fileName = fileName.substring(slashIndex + 1);

        item["name"] = fileName;
        item["isDir"] = file.isDirectory();
        item["size"] = file.size();
        
        file = dir.openNextFile();
    }
    
    xSemaphoreGive(sdSemaphore);

    String output;
    serializeJson(doc, output);
    return output;
}

// 2. ฟังก์ชันสร้างไฟล์
bool FileManager::createFile(String path) {
    if (!path.startsWith("/main")) return false; // 🚨 บล็อก
    if (xSemaphoreTake(sdSemaphore, pdMS_TO_TICKS(500)) == pdTRUE) {
        File f = SD.open(path, FILE_WRITE);
        if(f) { f.close(); xSemaphoreGive(sdSemaphore); return true; }
        xSemaphoreGive(sdSemaphore);
    }
    return false;
}

// 3. ฟังก์ชันลบไฟล์
bool FileManager::deleteFile(String path) {
    // 🚨 บล็อก และ ห้ามลบโฟลเดอร์ /main ทิ้งเด็ดขาด!
    if (!path.startsWith("/main") || path == "/main") return false; 
    
    if (xSemaphoreTake(sdSemaphore, pdMS_TO_TICKS(500)) == pdTRUE) {
        bool res;
        File f = SD.open(path);
        if(f && f.isDirectory()) {
            f.close();
            res = SD.rmdir(path);
        } else {
            f.close();
            res = SD.remove(path);
        }
        xSemaphoreGive(sdSemaphore);
        return res;
    }
    return false;
}

// 4. ฟังก์ชันเปลี่ยนชื่อ
bool FileManager::renameFile(String oldPath, String newPath) {
    // 🚨 บล็อกทั้งชื่อเก่าและชื่อใหม่
    if (!oldPath.startsWith("/main") || !newPath.startsWith("/main") || oldPath == "/main") return false;
    
    if (xSemaphoreTake(sdSemaphore, pdMS_TO_TICKS(500)) == pdTRUE) {
        bool res = SD.rename(oldPath, newPath);
        xSemaphoreGive(sdSemaphore);
        return res;
    }
    return false;
}

// ฟังก์ชันสร้างโฟลเดอร์
bool FileManager::createFolder(String path) {
    if (!path.startsWith("/main")) return false; 
    
    if (xSemaphoreTake(sdSemaphore, pdMS_TO_TICKS(500)) == pdTRUE) {
        bool res = SD.mkdir(path.c_str()); 
        xSemaphoreGive(sdSemaphore);
        return res;
    }
    return false;
}