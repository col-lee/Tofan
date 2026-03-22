#include "FileManager.hpp"

SPIClass hspi(HSPI);
SemaphoreHandle_t sdSemaphore;
FileManager file_card;
bool isConnectSDcard = false;
String jsondata;
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
    hspi.begin(SCL, MISO, MOSI, CS);
    hspi.setFrequency(8000000);
    if (SD.begin(CS, hspi, 8000000))
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

void FileManager::scanAll(File dir, int numTabs, JsonArray &jfiarray, JsonArray &jfoarray) {
    while (true) {
        File root = dir.openNextFile();
        if(!root) break;
        // for (uint8_t i = 0; i < numTabs; i++) {
        //     Serial.print("  ");
        // }
        // Serial.print(root.name());
        
        if(root.isDirectory()) {
            JsonObject jObjfolder = jfoarray.add<JsonObject>();
            jObjfolder["folderName"] = root.name();
            jObjfolder["folderPath"] = root.path();
            scanAll(root, numTabs + 1, jfiarray, jfoarray);
        } else {
            if(root.name()) {
                JsonObject jObjfile = jfiarray.add<JsonObject>();
                jObjfile["name"] = root.name();
                jObjfile["path"] = root.path();
                jObjfile["size"] = root.size();
            }

            // Serial.print("\t\t");
            // Serial.print(root.size(), DEC);
            // Serial.println(" bytes");
        }
        root.close();
    }
}

void FileManager::listFileAll() {
    if (xSemaphoreTake(sdSemaphore, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        JsonDocument doc;
        doc["root"] = "/";
        JsonArray files = doc["files"].to<JsonArray>();
        JsonArray folders = doc["folders"].to<JsonArray>();
        // String jsondataFolder;
        // String jsondataFile;
        // String jsondata;
        File root = SD.open("/main");
        if(!root) {
            return;
        }
        scanAll(root, 0, files, folders);
        root.close();
        serializeJson(doc, jsondata);
        Serial.println(jsondata);
        xSemaphoreGive(sdSemaphore);
        doc.clear();
    }
}

void FileManager::scanfilterFile(File dir, int numTabs, JsonArray &jfiarray, JsonArray &jfoarray, String exts) {
    while (true) {
        File root = dir.openNextFile();
        if(!root) break;
        // for (uint8_t i = 0; i < numTabs; i++) {
        //     Serial.print("  ");
        // }
        // Serial.print(root.name());
        
        if(root.isDirectory()) {
            JsonObject jObjfolder = jfoarray.add<JsonObject>();
            jObjfolder["folderName"] = root.name();
            jObjfolder["folderPath"] = root.path();
            scanfilterFile(root, numTabs + 1, jfiarray, jfoarray, exts);
        } else {
            if(root.name()) {
                String extendsion = root.name();
                extendsion.toLowerCase();
                if(extendsion.endsWith(".jpg") || extendsion.endsWith(".jpeg") || extendsion.endsWith(".png") && exts.equals("images")) {
                    JsonObject jObjfile = jfiarray.add<JsonObject>();
                    jObjfile["name"] = root.name();
                    jObjfile["path"] = root.path();
                    jObjfile["size"] = root.size();
                } else if(extendsion.endsWith(".gif") && exts.equals("gif") || exts.equals(".gif")) {
                    JsonObject jObjfile = jfiarray.add<JsonObject>();
                    jObjfile["name"] = root.name();
                    jObjfile["path"] = root.path();
                    jObjfile["size"] = root.size();
                }
                
            }

            // Serial.print("\t\t");
            // Serial.print(root.size(), DEC);
            // Serial.println(" bytes");
        }
        root.close();
    }
}

void FileManager::filterFile(String exts) {
    if (xSemaphoreTake(sdSemaphore, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        JsonDocument doc;
        doc["root"] = "/main";
        JsonArray files = doc["files"].to<JsonArray>();
        JsonArray folders = doc["folders"].to<JsonArray>();
        // String jsondataFolder;
        // String jsondataFile;
        // String jsondata;
        File root = SD.open("/main");
        if(!root) {
            return;
        }
        scanfilterFile(root, 0, files, folders, exts);
        root.close();
        serializeJson(doc, jsondata);
        Serial.println(jsondata);
        xSemaphoreGive(sdSemaphore);
        doc.clear();
    }
}

void FileManager::filterFileInPath(String path, String exts) {
    if (xSemaphoreTake(sdSemaphore, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        JsonDocument doc;
        doc["root"] = path;
        JsonArray files = doc["files"].to<JsonArray>();
        JsonArray folders = doc["folders"].to<JsonArray>();
        // String jsondataFolder;
        // String jsondataFile;
        // String jsondata;
        File root = SD.open(path);
        if(!root) {
            return;
        }
        scanfilterFile(root, 0, files, folders, exts);
        root.close();
        serializeJson(doc, jsondata);
        Serial.println(jsondata);
        xSemaphoreGive(sdSemaphore);
        doc.clear();
    }
}