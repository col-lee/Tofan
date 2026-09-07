// Owns UI state, sprite rendering, media lists and diagnostic screens.
#include "DisplayManager.hpp"
#include "../storage/FileManager.hpp"
#include "../audio/SoundManager.hpp"
#include "../core/GlobalState.hpp"
#include <WiFi.h>

LGFX tft;
LGFX_Sprite spr(&tft);
DisplayManager DISM;

bool isDisplay_install;

DisplayManager::DisplayManager() {

}

DisplayManager::~DisplayManager() {

}

void DisplayManager::initDisplay(){
    if(xSemaphoreTake(displaySemaphore, pdMS_TO_TICKS(500)) == pdTRUE) {
        tft.init();
        tft.setRotation(3);
        tft.setTextSize(2);
        tft.fillScreen(TFT_BLACK);

        isDisplay_install = true;
        xSemaphoreGive(displaySemaphore);
    }
}

void DisplayManager::resetDisplay(){
    if (xSemaphoreTake(displaySemaphore, pdMS_TO_TICKS(100)) == pdTRUE) {
        tft.fillScreen(TFT_BLACK);
        tft.setCursor(0, 0);
        xSemaphoreGive(displaySemaphore);
    }
}

void DisplayManager::createUISprite() {
    // Allocate the UI sprite only when no backing buffer exists.
    if (spr.getBuffer() == nullptr) {
        spr.setPsram(true);
        spr.setColorDepth(16); // ประหยัด RAM 50%
        if(spr.createSprite(tft.width(), tft.height())) {
            spr.setTextFont(1); // Default built-in font for UI text.
            spr.setTextSize(1);
            if (xSemaphoreTake(displaySemaphore, portMAX_DELAY) == pdTRUE)
            {
                spr.pushSprite(0, 0);
                Serial.println("create Sprite suc.");
                xSemaphoreGive(displaySemaphore);
            }
        } else {
            Serial.println("Not enough RAM for Sprite!");
        }
    }
}

void DisplayManager::deleteUISprite() {
    // Release the sprite buffer before direct media rendering.
    if (spr.getBuffer() != nullptr) {
        tft.fillScreen(TFT_BLACK);
        spr.deleteSprite();
    }
}

// ----------------------------------------------------
// 1. หน้า Loading
// ----------------------------------------------------
void DisplayManager::drawLoading(int percent, String text) {
    // Skip rendering until the sprite buffer is available.
    if(spr.getBuffer() == nullptr) return;

    spr.fillSprite(TFT_BLACK);

    int barWidth = 200;
    int barHeight = 8;
    int x = (tft.width() - barWidth) / 2;
    int y = 140;

    spr.fillRoundRect(x, y, barWidth, barHeight, 4, C_BAR_BG);
    spr.fillRoundRect(x, y, (barWidth * percent) / 100, barHeight, 4, C_BAR_FG);

    spr.setTextColor(C_TEXT);
    spr.setTextDatum(MC_DATUM);
    spr.drawString("ToFan OS Booting...", tft.width()/2, 110);
    spr.drawString(text, tft.width()/2, 160);

    if (xSemaphoreTake(displaySemaphore, portMAX_DELAY) == pdTRUE){
        spr.pushSprite(0, 0);
        xSemaphoreGive(displaySemaphore);
    }
}


// ----------------------------------------------------
// 2. หน้า Home: เมนูไอคอนแบบวนรอบ
// ----------------------------------------------------
void DisplayManager::drawHomeMenu(bool pushToScreen) {
    if(spr.getBuffer() == nullptr) return;

    spr.fillSprite(C_BG);

    // 1. คำนวณ Animation Index (Lerp) วิ่งตามค่าเป้าหมายดิบ
    animatedMenuIndex += (animatedMenuIndex_target - animatedMenuIndex) * 0.12;

    if (abs(animatedMenuIndex_target - animatedMenuIndex) < 0.01) {
        animatedMenuIndex = animatedMenuIndex_target;
        isAnimatingMenu = false;
    } else {
        isAnimatingMenu = true;
    }

    int centerX = tft.width() / 2;
    int centerY = 105;
    int spacingX = 110;
    float curveIntensity = 12.0;

    const char* menus[totalItems] = {"Display", "Music", "Settings", "Pet", "Debug", "Recorde"};

    for (int i = 0; i < totalItems; i++) {
        // 2. Infinite Loop Logic: ใช้ fmod เพื่อให้ค่าวนลูป 0-5 เสมอ
        // และคำนวณระยะห่าง (distance) แบบวงกลม
        float distance = i - fmod(animatedMenuIndex, (float)totalItems);
        while (distance > totalItems / 2.0) distance -= totalItems;
        while (distance < -totalItems / 2.0) distance += totalItems;

        if (abs(distance) > 2.2) continue;

        int x = centerX + (distance * spacingX);
        int y = centerY + (distance * distance * curveIntensity);

        float scale = 1.0 - (abs(distance) * 0.45);
        if (scale < 0.45) scale = 0.45;
        int iconSize = 90 * scale;

        // 3. ปรับสีตามความใกล้เคียง (Fading)
        // ใช้ i เทียบกับ currentMenuIndex ที่คำนวณใน InputController.cpp
        uint16_t color = (i == currentMenuIndex) ? C_HILITE : C_CARD;
        uint16_t iconColor = (i == currentMenuIndex) ? C_BG : C_BAR_BG;

        spr.fillRoundRect(x - iconSize/2, y - iconSize/2, iconSize, iconSize, 20 * scale, color);
        spr.setTextColor(iconColor);
        spr.setTextDatum(MC_DATUM);

        // --- วาดไอคอน ---
        if (i == 0) { // Display
            // 1. วาดตัวเครื่อง/กรอบจอ (Main Monitor Body)
            int monitorW = 40 * scale;
            int monitorH = 28 * scale;
            spr.fillRoundRect(x - monitorW / 2, y - monitorH / 2 - (4 * scale), monitorW, monitorH, 3 * scale, iconColor);

            // 2. วาดหน้าจอข้างใน (Inner Screen / Glow) - ใช้สีพื้นหลัง (color) เจาะรูให้ดูมีมิติ
            int screenW = 34 * scale;
            int screenH = 22 * scale;
            spr.fillRect(x - screenW / 2, y - screenH / 2 - (4 * scale), screenW, screenH, color);

            // 3. วาดฐานตั้งจอ (Monitor Stand)
            // ก้านคอจอ
            spr.fillRect(x - (2 * scale), y + (10 * scale), 4 * scale, 6 * scale, iconColor);
            // ฐานล่าง
            spr.fillRoundRect(x - (12 * scale), y + (14 * scale), 24 * scale, 4 * scale, 2 * scale, iconColor);
        }
        else if (i == 1) { // Music
            int noteX = x - (5 * scale); // ปรับตำแหน่งเล็กน้อยให้สมดุล
            int noteY = y + (5 * scale);

            // วาดหัวตัวโน้ต (วงกลมเอียงๆ)
            spr.fillEllipse(noteX, noteY, 8 * scale, 6 * scale, iconColor);

            // วาดก้านตัวโน้ต
            spr.fillRect(noteX + (5 * scale), noteY - (25 * scale), 3 * scale, 25 * scale, iconColor);

            // วาดธงตัวโน้ต (ใช้ Triangle หรือ Rect เฉียงๆ)
            spr.fillTriangle(noteX + (8 * scale), noteY - (25 * scale),
                             noteX + (20 * scale), noteY - (15 * scale),
                             noteX + (8 * scale), noteY - (18 * scale), iconColor);
        }
        else if (i == 2) { // Settings
            spr.fillCircle(x, y, 18*scale, iconColor);
            spr.fillCircle(x, y, 8*scale, color);
        }
        else if (i == 3) { // Pet
            spr.fillCircle(x, y, 18*scale, iconColor);
            spr.fillCircle(x - 6*scale, y - 3*scale, 3*scale, color);
            spr.fillCircle(x + 6*scale, y - 3*scale, 3*scale, color);
        }
        else if (i == 4) { // About
            spr.drawString("?", x, y, 4);
        }

        else if(i == 5) {
            spr.fillCircle(x, y, 18*scale, TFT_RED);
        }

        // 4. วาดชื่อเมนูเฉพาะตัวที่ "ถูกเลือก" (Active Feature)
        if (i == currentMenuIndex) {
            // คำนวณค่าความโปร่งใสหลอกๆ (ค่อยๆ ชัดขึ้นเมื่อเข้ากลางจอ)
            spr.setTextColor(C_TEXT);
            spr.setTextDatum(BC_DATUM);
            spr.drawString(menus[i], centerX, 225, 2);
        }
    }

    if (pushToScreen) {
        if(xSemaphoreTake(displaySemaphore, 0) == pdTRUE) {
            spr.pushSprite(0, 0);
            xSemaphoreGive(displaySemaphore);
        }
    }
}

// ----------------------------------------------------
// 3. หน้า Music Player (Modern UI)
// ----------------------------------------------------
void DisplayManager::drawMusicPlayer(String songName, int progress, bool isPlaying, bool pushToScreen) {
    drawMusicSurface(false, playlistNames.empty() ? "No tracks on SD" : isOnlineAudio ? "Choose a track" : songName, isOnlineAudio ? 0 : progress,
                     isPlaying && !isOnlineAudio, pushToScreen);
}

void DisplayManager::drawMusicSurface(bool online, String title, int progress, bool playing, bool pushToScreen) {
    if (!spr.getBuffer()) return;
    const int w = tft.width(), h = tft.height();
    const uint16_t ink = tft.color565(28, 37, 43);
    const uint16_t muted = tft.color565(103, 115, 121);
    const uint16_t accent = tft.color565(34, 128, 111);
    const uint16_t paper = tft.color565(247, 248, 245);
    const uint16_t soft = tft.color565(228, 235, 229);
    spr.fillSprite(paper);
    spr.setTextDatum(TL_DATUM);
    spr.setTextColor(ink);
    spr.drawString("Music", 18, 12, 4);
    const int sourceIndex = online ? 3 : 4;
    const bool sourceFocused = currentMusicControlIndex == sourceIndex;
    spr.fillRoundRect(w - 96, 14, 78, 27, 13, sourceFocused ? accent : soft);
    spr.setTextColor(sourceFocused ? TFT_WHITE : ink);
    spr.setTextDatum(MC_DATUM);
    spr.drawString(online ? "SD card" : "Online", w - 57, 27, 2);

    spr.fillRoundRect(18, 54, 64, 64, 15, soft);
    // A simple musical note, drawn as geometry for built-in font compatibility.
    spr.fillRect(51, 69, 3, 28, accent);
    spr.fillRect(54, 69, 13, 4, accent);
    spr.fillEllipse(46, 98, 8, 5, accent);
    spr.setTextDatum(TL_DATUM);
    spr.setTextColor(muted);
    spr.drawString(online ? "ONLINE RADIO" : "YOUR LIBRARY", 96, 57, 1);
    if (!title.length()) title = online ? "Select a station" : "Choose a track";
    spr.setTextFont(2);
    while (title.length() && spr.textWidth(title) > w - 120) {
        // Remove a whole UTF-8 code point, never a partial multibyte character.
        int end = title.length() - 1;
        while (end > 0 && (static_cast<unsigned char>(title[end]) & 0xC0) == 0x80) --end;
        title.remove(end);
    }
    spr.setTextFont(1);
    spr.setTextColor(ink);
    spr.drawString(title, 96, 77, 2);
    spr.setTextColor(muted);
    const char* status = online && WiFi.status() != WL_CONNECTED ? "Wi-Fi required" :
                         playing ? (online ? "Live stream" : "Playing") : "Ready / paused";
    spr.drawString(status, 96, 101, 1);
    if (online) {
        spr.fillCircle(22, 143, 3, playing ? accent : muted);
        spr.drawString(onlineStationNames[currentStationIndex], 33, 138, 2);
    } else {
        progress = progress < 0 ? 0 : progress > 100 ? 100 : progress;
        spr.fillRoundRect(18, 137, w - 36, 4, 2, soft);
        const int fill = (w - 36) * progress / 100;
        if (fill > 0) spr.fillRect(18, 137, fill, 4, accent);
        spr.drawString(String(isOnlineAudio ? 0 : currentAudioTime / 60) + ":" +
                       ((isOnlineAudio ? 0 : currentAudioTime % 60) < 10 ? "0" : "") +
                       String(isOnlineAudio ? 0 : currentAudioTime % 60), 18, 148, 1);
        const bool listFocused = currentMusicControlIndex == 3;
        spr.fillRoundRect(w - 94, 146, 76, 23, 10, listFocused ? accent : soft);
        spr.setTextColor(listFocused ? TFT_WHITE : ink);
        spr.setTextDatum(MC_DATUM);
        spr.drawString("Tracks", w - 56, 157, 1);
    }
    const int cy = h - 43;
    for (int i = 0; i < 3; ++i) {
        const int cx = w/2 + (i - 1) * 70;
        const bool focused = currentMusicControlIndex == i;
        spr.fillCircle(cx, cy, i == 1 ? 24 : 20, focused ? accent : soft);
        const uint16_t color = focused ? TFT_WHITE : ink;
        if (i == 1 && playing) {
            spr.fillRect(cx - 6, cy - 8, 4, 16, color);
            spr.fillRect(cx + 2, cy - 8, 4, 16, color);
        } else if (i == 0) {
            spr.fillTriangle(cx + 5, cy - 7, cx + 5, cy + 7, cx - 4, cy, color);
            spr.fillRect(cx - 7, cy - 7, 2, 14, color);
        } else {
            spr.fillTriangle(cx - 5, cy - 8, cx - 5, cy + 8, cx + 6, cy, color);
            if (i == 2) spr.fillRect(cx + 7, cy - 7, 2, 14, color);
        }
    }
    spr.setTextColor(muted);
    spr.setTextDatum(MC_DATUM);
    spr.drawString("Turn to select  /  Press to play", w/2, h - 9, 1);
    if (pushToScreen && xSemaphoreTake(displaySemaphore, portMAX_DELAY) == pdTRUE) {
        spr.pushSprite(0, 0);
        xSemaphoreGive(displaySemaphore);
    }
}

void DisplayManager::drawPopupNoMusic(bool pushToScreen) {
    if(spr.getBuffer() == nullptr) return;

    // วาดพื้นหลังเบลอๆ หรือสีทึบ
    spr.fillSprite(C_BG);

    // วาดกล่องข้อความ
    int boxW = 200, boxH = 120;
    int bx = (tft.width() - boxW) / 2;
    int by = (tft.height() - boxH) / 2;
    spr.fillRoundRect(bx, by, boxW, boxH, 10, C_CARD);

    spr.setTextColor(C_TEXT);
    spr.setTextDatum(MC_DATUM);
    spr.drawString("Wi-Fi is offline", tft.width()/2, by + 30, 2);
    spr.drawString("Connect to play online?", tft.width()/2, by + 50, 1);

    // ปุ่ม Yes / No
    int btnW = 60, btnH = 30;
    int yBtn = by + 75;

    // ปุ่ม Yes (Index 0)
    if(popupSelectedIndex == 0) {
        spr.fillRoundRect(bx + 20, yBtn, btnW, btnH, 5, C_HILITE);
        spr.setTextColor(C_BG);
    } else {
        spr.fillRoundRect(bx + 20, yBtn, btnW, btnH, 5, C_BG);
        spr.setTextColor(C_TEXT);
    }
    spr.drawString("Yes", bx + 20 + btnW/2, yBtn + btnH/2, 2);

    // ปุ่ม No (Index 1)
    if(popupSelectedIndex == 1) {
        spr.fillRoundRect(bx + boxW - 20 - btnW, yBtn, btnW, btnH, 5, C_HILITE);
        spr.setTextColor(C_BG);
    } else {
        spr.fillRoundRect(bx + boxW - 20 - btnW, yBtn, btnW, btnH, 5, C_BG);
        spr.setTextColor(C_TEXT);
    }
    spr.drawString("No", bx + boxW - 20 - btnW/2, yBtn + btnH/2, 2);

    if (pushToScreen) {
        if(xSemaphoreTake(displaySemaphore, portMAX_DELAY) == pdTRUE) {
            spr.pushSprite(0, 0);
            xSemaphoreGive(displaySemaphore);
        }
    }
}

void DisplayManager::drawOnlineMusicPlayer(bool pushToScreen) {
    drawMusicSurface(true, isOnlineAudio ? currentSongTitle : String(onlineStationNames[currentStationIndex]),
                     0, isPlayingAudio && isOnlineAudio, pushToScreen);
}

// ----------------------------------------------------
// 4. หน้าต่าง Volume ด้านขวาของจอ
// ----------------------------------------------------
void DisplayManager::drawVolumeOverlay() {
    if(spr.getBuffer() == nullptr) return;

    // วาดหน้าจอเดิมทับไปก่อนเพื่อให้ดูเหมือนหลอดลอยทับอยู่
    if(previousState == UI_STATE::HOME_MENU) drawHomeMenu(false);
    else if(previousState == UI_STATE::APP_MUSIC) drawMusicPlayer(currentSongTitle, currentAudioProgress, isPlayingAudio, false);
    else if(previousState == UI_STATE::APP_MUSIC_LIST) drawMusicList(false);
    else if(previousState == UI_STATE::APP_DISPLAY_LIST) drawImageList(false);
    else if(previousState == UI_STATE::APP_SETTINGS) drawSettings(false);
    else if(previousState == UI_STATE::APP_ONLINE_MUSIC) drawOnlineMusicPlayer(false);

    // Place the volume overlay at the right edge.
    int boxW = 50, boxH = 180;
    int boxX = tft.width() - boxW - 10; // ชิดขวา
    int boxY = (tft.height() - boxH) / 2;

    spr.fillRoundRect(boxX, boxY, boxW, boxH, 8, C_CARD);

    // หลอดแนวตั้ง
    int barW = 10, barH = 150;
    int bx = boxX + (boxW - barW) / 2;
    int by = boxY + 15;
    spr.fillRect(bx, by, barW, barH, C_BAR_BG);

    // คำนวณความสูงตาม volume (พิกัด Y กลับหัว)
    int fillH = (barH * currentVolLevel) / 100;
    spr.fillRect(bx, by + (barH - fillH), barW, fillH, C_BAR_FG);

    spr.setTextColor(C_TEXT);
    spr.setTextDatum(BC_DATUM);
    spr.drawNumber(currentVolLevel, boxX + (boxW/2), boxY + boxH - 5);

    if(xSemaphoreTake(displaySemaphore, portMAX_DELAY) == pdTRUE) {
        spr.pushSprite(0, 0);
        xSemaphoreGive(displaySemaphore);
    }
}

// ----------------------------------------------------
// โหลดและจัดเรียงรายชื่อเพลง (A-Z)
// ----------------------------------------------------
void DisplayManager::loadMusicList() {
    playlistNames.clear();
    playlistPaths.clear();
    playlistSelectedIndex = 0;
    playlistScrollOffset = 0;

    String jsonStr = file_card.getFileListJSON("/main/Musics");
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, jsonStr);

    if (!error) {
        JsonArray files = doc["files"];
        for (JsonObject file : files) {
            bool isDir = file["isDir"];
            String name = file["name"].as<String>();
            // คัดเฉพาะไฟล์เสียง
            if (!isDir && (name.endsWith(".mp3") || name.endsWith(".wav"))) {
                playlistNames.push_back(name);
                playlistPaths.push_back("/main/Musics/" + name);
            }
        }

        // จัดเรียงตามตัวอักษร (A-Z)
        for(size_t i = 0; i < playlistNames.size(); i++) {
            for(size_t j = i + 1; j < playlistNames.size(); j++) {
                if(playlistNames[i].compareTo(playlistNames[j]) > 0) {
                    std::swap(playlistNames[i], playlistNames[j]);
                    std::swap(playlistPaths[i], playlistPaths[j]);
                }
            }
        }
    }
}

// ----------------------------------------------------
// วาดหน้าจอ Music List
// ----------------------------------------------------
void DisplayManager::drawMusicList(bool pushToScreen) {
    if(spr.getBuffer() == nullptr) return;
    spr.fillSprite(C_BG);

    spr.setTextColor(C_TEXT);
    spr.setTextDatum(TL_DATUM);
    spr.drawString("Playlist (SD Card)", 10, 10, 2);

    int startY = 35;
    int itemH = 32;
    int maxVisible = 6;

    if (playlistNames.empty()) {
        spr.setTextDatum(MC_DATUM);
        spr.drawString("No Music Found", tft.width()/2, tft.height()/2, 2);
    } else {
        // ระบบเลื่อนจอ (Scroll)
        if(playlistSelectedIndex < playlistScrollOffset) playlistScrollOffset = playlistSelectedIndex;
        if(playlistSelectedIndex >= playlistScrollOffset + maxVisible) playlistScrollOffset = playlistSelectedIndex - maxVisible + 1;

        for(int i = 0; i < maxVisible; i++) {
            int idx = playlistScrollOffset + i;
            if(idx >= (int)playlistNames.size()) break;

            int y = startY + (i * itemH);

            // Hover Effect
            if(idx == playlistSelectedIndex) {
                spr.fillRoundRect(5, y, tft.width() - 10, itemH - 2, 4, C_HILITE);
                spr.setTextColor(C_BG);
            } else {
                spr.setTextColor(C_TEXT);
            }

            // ตัดชื่อเพลงถ้ามันยาวเกินไป
            String displayName = playlistNames[idx];
            if(displayName.length() > 25) displayName = displayName.substring(0, 22) + "...";

            spr.setTextDatum(ML_DATUM);
            spr.drawString(displayName, 15, y + (itemH/2), 2);
        }
    }

    if (pushToScreen) {
        if(xSemaphoreTake(displaySemaphore, portMAX_DELAY) == pdTRUE) {
            spr.pushSprite(0, 0);
            xSemaphoreGive(displaySemaphore);
        }
    }
}

// ----------------------------------------------------
// โหลดและจัดเรียงรายชื่อรูปภาพ/GIF (A-Z)
// ----------------------------------------------------
void DisplayManager::loadImageList() {
    imageNames.clear();
    imagePaths.clear();
    imageSelectedIndex = 0;
    imageScrollOffset = 0;

    String jsonStr = file_card.getFileListJSON("/main/Pictures");
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, jsonStr);

    if (!error) {
        JsonArray files = doc["files"];
        for (JsonObject file : files) {
            bool isDir = file["isDir"];
            String name = file["name"].as<String>();
            String lowerName = name;
            lowerName.toLowerCase();

            // คัดเฉพาะไฟล์ภาพ
            if (!isDir && (lowerName.endsWith(".jpg") || lowerName.endsWith(".jpeg") || lowerName.endsWith(".gif"))) {
                imageNames.push_back(name);
                imagePaths.push_back("/main/Pictures/" + name);
            }
        }

        // จัดเรียงตามตัวอักษร (A-Z)
        for(size_t i = 0; i < imageNames.size(); i++) {
            for(size_t j = i + 1; j < imageNames.size(); j++) {
                if(imageNames[i].compareTo(imageNames[j]) > 0) {
                    std::swap(imageNames[i], imageNames[j]);
                    std::swap(imagePaths[i], imagePaths[j]);
                }
            }
        }
    }
}

// ----------------------------------------------------
// วาดหน้าจอ Image List
// ----------------------------------------------------
void DisplayManager::drawImageList(bool pushToScreen) {
    if(spr.getBuffer() == nullptr) return;
    spr.fillSprite(C_BG);

    spr.setTextColor(C_TEXT);
    spr.setTextDatum(TL_DATUM);
    spr.drawString("Pictures List", 10, 10, 2);

    int startY = 35;
    int itemH = 32;
    int maxVisible = 6;

    if (imageNames.empty()) {
        spr.setTextDatum(MC_DATUM);
        spr.drawString("No Images Found", tft.width()/2, tft.height()/2, 2);
    } else {
        // ระบบเลื่อนจอ (Scroll)
        if(imageSelectedIndex < imageScrollOffset) imageScrollOffset = imageSelectedIndex;
        if(imageSelectedIndex >= imageScrollOffset + maxVisible) imageScrollOffset = imageSelectedIndex - maxVisible + 1;

        for(int i = 0; i < maxVisible; i++) {
            int idx = imageScrollOffset + i;
            if(idx >= (int)imageNames.size()) break;

            int y = startY + (i * itemH);

            // Hover Effect
            if(idx == imageSelectedIndex) {
                spr.fillRoundRect(5, y, tft.width() - 10, itemH - 2, 4, C_HILITE);
                spr.setTextColor(C_BG);
            } else {
                spr.setTextColor(C_TEXT);
            }

            String displayName = imageNames[idx];
            if(displayName.length() > 25) displayName = displayName.substring(0, 22) + "...";

            spr.setTextDatum(ML_DATUM);
            spr.drawString(displayName, 15, y + (itemH/2), 2);
        }
    }

    if (pushToScreen) {
        if(xSemaphoreTake(displaySemaphore, portMAX_DELAY) == pdTRUE) {
            spr.pushSprite(0, 0);
            xSemaphoreGive(displaySemaphore);
        }
    }
}

// ----------------------------------------------------
// วาดหน้าจอ Settings (Admin Mode & Wi-Fi)
// ----------------------------------------------------
void DisplayManager::drawSettings(bool pushToScreen) {
    if(spr.getBuffer() == nullptr) return;
    spr.fillSprite(C_BG);

    spr.setTextColor(C_TEXT);
    spr.setTextDatum(TL_DATUM);
    spr.drawString("Settings", 10, 10, 2);

    // รายการเมนูและสถานะ
    String items[boundaries_setting] = {"Admin Mode", "WiFi"};
    bool states[boundaries_setting] = {isAdminModeOn, isWiFiOn};

    int startY = 45;
    int itemH = 40;

    for (int i = 0; i < 2; i++) {
        int y = startY + (i * itemH);

        // 1. วาด Hover Effect
        if (i == settingSelectedIndex) {
            spr.fillRoundRect(5, y, tft.width() - 10, itemH - 2, 4, C_HILITE);
            spr.setTextColor(C_BG);
        } else {
            spr.setTextColor(C_TEXT);
        }

        // ชื่อรายการ
        spr.setTextDatum(ML_DATUM);
        spr.drawString(items[i], 15, y + (itemH/2), 2);

        // 2. วาดปุ่ม Toggle (สวิตช์)
        int toggleW = 40;
        int toggleH = 20;
        int toggleX = tft.width() - toggleW - 15; // ชิดขวา
        int toggleY = y + (itemH/2) - (toggleH/2);

        if (states[i]) {
            // สถานะ ON (เปิด): พื้นหลังสีฟ้า/เขียว, วงกลมอยู่ขวา
            spr.fillRoundRect(toggleX, toggleY, toggleW, toggleH, toggleH/2, C_BAR_FG);
            spr.fillCircle(toggleX + toggleW - (toggleH/2), toggleY + (toggleH/2), (toggleH/2) - 2, C_BG);
        } else {
            // สถานะ OFF (ปิด): พื้นหลังสีเทา, วงกลมอยู่ซ้าย
            spr.fillRoundRect(toggleX, toggleY, toggleW, toggleH, toggleH/2, C_BAR_BG);
            spr.fillCircle(toggleX + (toggleH/2), toggleY + (toggleH/2), (toggleH/2) - 2, C_BG);
        }
    }

    if (pushToScreen) {
        if(xSemaphoreTake(displaySemaphore, portMAX_DELAY) == pdTRUE) {
            spr.pushSprite(0, 0);
            xSemaphoreGive(displaySemaphore);
        }
    }
}

void DisplayManager::drawAIPet(bool pushToScreen) {
    if(spr.getBuffer() == nullptr) return;

    // พื้นหลังดำสนิท
    spr.fillSprite(TFT_BLACK);

    // ตัวแปรสถานะแอนิเมชัน
    static float cur_r = 255, cur_g = 255, cur_b = 255;
    static float cur_eyeW = 40, cur_eyeH_L = 60, cur_eyeH_R = 60, cur_eyeY = -15;
    static float cur_mouthW = 40, cur_mouthH = 8, cur_mouthY = 45, cur_mouthX = 0;

    static float cur_gazeX = 0, cur_gazeY = 0;
    static float tar_gazeX = 0, tar_gazeY = 0;

    // เปลือกตา (0 คือตาเปิดเต็มที่)
    static float cur_eyelidDrop = 0;
    static float cur_angryBrow = 0;

    static unsigned long nextBlinkTime = millis() + 2000;
    static unsigned long nextGazeTime = millis() + 1000;

    // --- ระบบฟิสิกส์การลอยตัว ---
    float time = millis() / 1000.0;
    float floatY = sin(time * 2.5) * 10.0;
    int anchorX = tft.width() / 2;
    int anchorY = tft.height() / 2 + floatY;

    // --- ระบบ AI สอดส่ายสายตา ---
    if (millis() > nextGazeTime) {
        if (random(100) > 30) {
            tar_gazeX = random(-50, 50);
            tar_gazeY = random(-15, 15);
        } else {
            tar_gazeX = 0; tar_gazeY = 0;
        }
        nextGazeTime = millis() + random(800, 3000);
    }

    // --- ระบบกะพริบตา ---
    if (millis() > nextBlinkTime) {
        cur_eyeH_L = 0;
        cur_eyeH_R = 0;
        nextBlinkTime = millis() + random(3000, 6000);
    }

    // --- กำหนดเป้าหมายอารมณ์ (Target Moods) ---
    // ค่าพื้นฐาน (หน้าปกติ สีขาวล้วน)
    float tar_r = 255, tar_g = 255, tar_b = 255;
    float tar_eyeW = 40, tar_eyeH_L = 60, tar_eyeH_R = 60, tar_eyeY = -15;
    float tar_mouthW = 40, tar_mouthH = 8, tar_mouthY = 45, tar_mouthX = 0;
    float tar_eyelidDrop = 0;
    float tar_angryBrow = 0;

    switch(petMood) {
        case 1: // 😒 Deadpan (หน้าเซ็ง/หน้าตาย แบบในรูป)
            tar_r = 255; tar_g = 255; tar_b = 255; // สีขาวล้วนเหมือนหน้าปกติ
            tar_eyeW = 40;
            tar_eyeH_L = 60; tar_eyeH_R = 60; tar_eyeY = -15;

            tar_eyelidDrop = 35; // เปลือกตาปิดลงมาเกินครึ่ง

            // ลดความหนาปากลงเหลือแค่ 4 พิกเซล จะได้เป็นขีดเส้นตรง ไม่ดูเหมือนยิ้ม
            tar_mouthW = 40; tar_mouthH = 4; tar_mouthY = 45; tar_mouthX = 0;
            break;

        case 2: // 😂 Laughing (หัวเราะตัวสั่น)
            tar_eyeH_L = 40; tar_eyeH_R = 40; // หรี่ตาลงนิดนึงให้ดูดุ
            tar_eyeY = -5;

            tar_angryBrow = 30; // สั่งให้สามเหลี่ยมตัดขอบตาเฉียงลงมา 30 พิกเซล

            tar_mouthW = 15; // ปากหดสั้นจู๋ (เหมือนเม้มปากแน่นด้วยความโกรธ)
            tar_mouthY = 40;
            break;
    }

    // Open the mouth from live microphone amplitude while listening or playing.
    float voiceActivity = abs((int)readMicData()) / 12000.0f;
    if (voiceActivity > 1.0f) voiceActivity = 1.0f;
    if (app::runtime.isRecording || isPlayingAudio) {
        tar_mouthH += voiceActivity * 26.0f;
    }

    // --- สมการ Lerp (Smooth Transition) ---
    float speed = 0.25;
    float gazeSpeed = 0.35;

    cur_r += (tar_r - cur_r) * speed;
    cur_g += (tar_g - cur_g) * speed;
    cur_b += (tar_b - cur_b) * speed;
    cur_eyeW += (tar_eyeW - cur_eyeW) * speed;
    cur_eyeH_L += (tar_eyeH_L - cur_eyeH_L) * speed;
    cur_eyeH_R += (tar_eyeH_R - cur_eyeH_R) * speed;
    cur_eyeY += (tar_eyeY - cur_eyeY) * speed;
    cur_mouthW += (tar_mouthW - cur_mouthW) * speed;
    cur_mouthH += (tar_mouthH - cur_mouthH) * speed;
    cur_mouthY += (tar_mouthY - cur_mouthY) * speed;
    cur_mouthX += (tar_mouthX - cur_mouthX) * speed;

    cur_eyelidDrop += (tar_eyelidDrop - cur_eyelidDrop) * speed; // Lerp เปลือกตา
    cur_angryBrow += (tar_angryBrow - cur_angryBrow) * speed;

    cur_gazeX += (tar_gazeX - cur_gazeX) * gazeSpeed;
    cur_gazeY += (tar_gazeY - cur_gazeY) * gazeSpeed;

    // --- เริ่มการวาด ---
    uint16_t faceColor = tft.color565((int)cur_r, (int)cur_g, (int)cur_b);
    uint16_t blushColor = tft.color565(180, 70, 100);

    int faceX = anchorX + cur_gazeX;
    int faceY = anchorY + cur_gazeY;
    int eyeSpacing = 70;

    // วาดแก้มแดง
    int blushX = anchorX + (cur_gazeX * 0.75);
    int blushY = anchorY + (cur_gazeY * 0.75);
    spr.fillEllipse(blushX - eyeSpacing - 25, blushY + 25, 20, 10, blushColor);
    spr.fillEllipse(blushX + eyeSpacing + 25, blushY + 25, 20, 10, blushColor);

    int eyeRad = 15;

    // พิกัดตาซ้ายและขวา
    int eyeL_X = faceX - eyeSpacing - cur_eyeW/2;
    int eyeL_Y = faceY + cur_eyeY - cur_eyeH_L/2;
    int eyeR_X = faceX + eyeSpacing - cur_eyeW/2;
    int eyeR_Y = faceY + cur_eyeY - cur_eyeH_R/2;

    // 1. วาดตาสี่เหลี่ยมขอบมนเต็มดวง
    spr.fillRoundRect(eyeL_X, eyeL_Y, cur_eyeW, cur_eyeH_L, eyeRad, faceColor);
    spr.fillRoundRect(eyeR_X, eyeR_Y, cur_eyeW, cur_eyeH_R, eyeRad, faceColor);

    // 2. วาด "เปลือกตาสีดำ" ทับส่วนบน
    if (cur_eyelidDrop > 1.0) {
        spr.fillRect(eyeL_X, eyeL_Y - 2, cur_eyeW, cur_eyelidDrop + 2, TFT_BLACK);
        spr.fillRect(eyeR_X, eyeR_Y - 2, cur_eyeW, cur_eyelidDrop + 2, TFT_BLACK);
    }

    if (cur_angryBrow > 1.0) {
        // ตาซ้าย (มุมตัดเฉียงลงไปทางขวา \)
        spr.fillTriangle(eyeL_X - 10, eyeL_Y - 10,
                         eyeL_X + cur_eyeW + 10, eyeL_Y - 10,
                         eyeL_X + cur_eyeW + 10, eyeL_Y + cur_angryBrow, TFT_BLACK);
        // ตาขวา (มุมตัดเฉียงลงไปทางซ้าย /)
        spr.fillTriangle(eyeR_X - 10, eyeR_Y - 10,
                         eyeR_X + cur_eyeW + 10, eyeR_Y - 10,
                         eyeR_X - 10, eyeR_Y + cur_angryBrow, TFT_BLACK);
    }

    // วาดปาก
    int pMouthX = faceX + cur_mouthX;
    int pMouthY = faceY + cur_mouthY;

    if (cur_mouthH <= 12) {
        // ปากเส้นตรง
        spr.fillRoundRect(pMouthX - cur_mouthW/2, pMouthY - cur_mouthH/2, cur_mouthW, cur_mouthH, 4, faceColor);

    } else {
        // ปากโค้งยิ้ม
        spr.fillEllipse(pMouthX, pMouthY, cur_mouthW/2, cur_mouthH/2, faceColor);
        if (petMood == 0) {
            spr.fillRect(pMouthX - cur_mouthW, pMouthY - cur_mouthH, cur_mouthW*2, cur_mouthH, TFT_BLACK);
        }
    }

    if (pushToScreen) {
        if(xSemaphoreTake(displaySemaphore, 0) == pdTRUE) {
            spr.pushSprite(0, 0);
            xSemaphoreGive(displaySemaphore);
        }
    }
}

void DisplayManager::debug() {
    if(spr.getBuffer() == nullptr) return;

    hwManager.updateAllStatus();

    spr.fillSprite(C_BG);
    spr.setTextColor(C_TEXT);
    spr.setTextFont(1);
    spr.setTextSize(1);

    spr.setTextDatum(TL_DATUM);
    spr.drawString("HARDWARE DEBUG", 5, 5, 2);

    const uint32_t sramTotal = ESP.getHeapSize();
    const uint32_t sramFree = ESP.getFreeHeap();
    const uint32_t psramTotal = psramFound() ? ESP.getPsramSize() : 0;
    const uint32_t psramFree = psramFound() ? ESP.getFreePsram() : 0;
    const uint32_t flashTotal = ESP.getFlashChipSize();
    const uint32_t flashUsed = ESP.getSketchSize();
    const uint32_t flashFree = flashTotal > flashUsed ? flashTotal - flashUsed : 0;

    char memoryInfo[80];
    snprintf(memoryInfo, sizeof(memoryInfo), "SRAM %lu/%luKB  PSRAM %lu/%luKB",
        sramFree / 1024,
        sramTotal / 1024,
        psramFree / 1024,
        psramTotal / 1024);
    spr.drawString(memoryInfo, 5, 28, 1);

    snprintf(memoryInfo, sizeof(memoryInfo), "FLASH free %luKB  used %luKB",
        flashFree / 1024,
        flashUsed / 1024);
    spr.drawString(memoryInfo, 5, 41, 1);
    spr.drawLine(5, 56, tft.width() - 5, 56, C_HILITE);

    const int startY = 62;
    const int itemHeight = 20;
    const int listBottom = tft.height() - 16;
    const int devicesPerScreen = max(1, (listBottom - startY) / itemHeight);

    int totalDevices = hwManager.getDeviceCount();

    if (totalDevices > 0) {
        if (debugSelectedIndex < 0) debugSelectedIndex = 0;
        if (debugSelectedIndex >= totalDevices) debugSelectedIndex = totalDevices - 1;
        if (debugSelectedIndex < debugScrollOffset) debugScrollOffset = debugSelectedIndex;
        if (debugSelectedIndex >= debugScrollOffset + devicesPerScreen) {
            debugScrollOffset = debugSelectedIndex - devicesPerScreen + 1;
        }

        int maxScrollOffset = max(0, totalDevices - devicesPerScreen);
        if (debugScrollOffset > maxScrollOffset) debugScrollOffset = maxScrollOffset;
    }

    for (int row = 0; row < devicesPerScreen; row++) {
        int deviceIndex = debugScrollOffset + row;
        if (deviceIndex >= totalDevices) break;

        HardwareDevice dev = hwManager.getDevice(deviceIndex);
        int yPos = startY + (row * itemHeight);

        if (deviceIndex == debugSelectedIndex) {
            spr.fillRoundRect(5, yPos - 2, tft.width() - 10, itemHeight - 2, 4, C_HILITE);
        }

        uint16_t statusColor;
        switch (dev.status) {
            case DEVICE_STATUS::WORKING:
                statusColor = TFT_GREEN;
                break;
            case DEVICE_STATUS::CONNECTING:
                statusColor = TFT_YELLOW;
                break;
            case DEVICE_STATUS::ERROR:
                statusColor = TFT_RED;
                break;
            default:
                statusColor = C_HILITE;
        }

        spr.fillCircle(12, yPos + 8, 4, statusColor);

        spr.setTextColor(deviceIndex == debugSelectedIndex ? C_BG : C_TEXT);
        spr.setTextDatum(TL_DATUM);
        spr.drawString(dev.name, 25, yPos, 1);

        String statusStr = hwManager.getStatusString(dev.status);
        spr.setTextDatum(TR_DATUM);
        spr.setTextColor(statusColor);
        spr.drawString(statusStr, tft.width() - 5, yPos, 1);

        spr.setTextColor(deviceIndex == debugSelectedIndex ? C_BG : C_HILITE);
        spr.setTextDatum(TL_DATUM);
        spr.drawString(dev.details, 25, yPos + 12, 1);
    }

    spr.setTextColor(C_HILITE);
    spr.setTextDatum(BC_DATUM);
    spr.drawString("Back: exit", tft.width() / 2, tft.height() - 3, 1);

    if (xSemaphoreTake(displaySemaphore, portMAX_DELAY) == pdTRUE) {
        spr.pushSprite(0, 0);
        xSemaphoreGive(displaySemaphore);
    }
}

void DisplayManager::recorde() {
    if(spr.getBuffer() == nullptr) return;

    spr.fillSprite(C_BG);

    spr.setTextColor(C_TEXT);
    spr.setTextFont(2);
    spr.setTextDatum(MC_DATUM);
    spr.drawString("Voice Recording", tft.width() / 2, 24, 2);

    spr.setTextFont(1);
    uint16_t statusColor;
    String statusText;

    if (app::runtime.isRecording) {
        statusColor = TFT_RED;
        statusText = "RECORDING";
    } else {
        statusColor = TFT_DARKGRAY;
        statusText = "READY";
    }

    spr.setTextColor(statusColor);
    spr.setTextDatum(MC_DATUM);
    spr.drawString(statusText, tft.width() / 2, 56, 2);

    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= interval) {
        previousMillis = currentMillis;
        if (app::runtime.isRecording) {
            seconds += 1;
        }
    }

    spr.setTextColor(C_TEXT);
    spr.setTextFont(2);
    char timeStr[16];
    int mins = seconds / 60;
    int secs = seconds % 60;
    snprintf(timeStr, sizeof(timeStr), "%02d:%02d", mins, secs);
    spr.drawString(timeStr, tft.width() / 2, 94);

    int btnX = tft.width() / 2;
    int btnY = 153;
    int btnRadius = 36;

    if (app::runtime.isRecording) {
        spr.fillRect(btnX - 25, btnY - 25, 50, 50, TFT_RED);
        spr.drawRect(btnX - 25, btnY - 25, 50, 50, tft.color565(150, 0, 0));
    } else {
        spr.fillCircle(btnX, btnY, btnRadius, TFT_GREEN);
        spr.drawCircle(btnX, btnY, btnRadius, tft.color565(0, 100, 0));
    }

    spr.setTextColor(TFT_WHITE);
    spr.setTextFont(1);
    spr.setTextDatum(MC_DATUM);
    if (app::runtime.isRecording) {
        spr.drawString("STOP", btnX, btnY);
    } else {
        spr.drawString("REC", btnX, btnY);
    }

    spr.setTextColor(C_HILITE);
    spr.setTextFont(1);
    spr.setTextDatum(MC_DATUM);
    spr.drawString("Press: record/stop", tft.width() / 2, 214);
    spr.drawString("Back: exit", tft.width() / 2, 229);

    spr.setTextColor(C_TEXT);
    char fileInfo[64];
    snprintf(fileInfo, sizeof(fileInfo), "File: %s", getRecordingName().c_str());
    spr.drawString(fileInfo, tft.width() / 2, 202);

    if (xSemaphoreTake(displaySemaphore, portMAX_DELAY) == pdTRUE) {
        spr.pushSprite(0, 0);
        xSemaphoreGive(displaySemaphore);
    }
}
