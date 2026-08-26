#include "DisplayManager.hpp"
#include "FileManager.hpp"
#include "SoundManager.hpp"
#define DISPLAYMANAGER_HH

LGFX tft; 
LGFX_Sprite spr(&tft);
DisplayManager DISM;

AnimatedGIF gif;
File gifFile;
bool isGifPlaying = false;

static int iXOff = 0; 
static int iYOff = 0;

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
    // 🌟 2. ใช้ spr.created() แทน spr == nullptr และเลิกใช้ new
    if (spr.getBuffer() == nullptr) {
        spr.setPsram(true);
        spr.setColorDepth(16); // ประหยัด RAM 50%
        if(spr.createSprite(tft.width(), tft.height())) {
            spr.setTextFont(1); // กันเหนียวเรื่องฟอนต์
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
    // 🌟 3. เลิกใช้ delete เพราะไม่ใช่ Pointer แล้ว
    if (spr.getBuffer() != nullptr) {
        tft.fillScreen(TFT_BLACK);
        spr.deleteSprite();
    }
}

// ----------------------------------------------------
// 1. หน้า Loading
// ----------------------------------------------------
void DisplayManager::drawLoading(int percent, String text) {
    // 🌟 4. เปลี่ยนเป็น !spr.created() ทุกฟังก์ชัน
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
    // spr.println("Hello 555");

    if (xSemaphoreTake(displaySemaphore, portMAX_DELAY) == pdTRUE){
        spr.pushSprite(0, 0);
        xSemaphoreGive(displaySemaphore);
    }
}


// ----------------------------------------------------
// 2. หน้า Home (Grid 3 Column x 2 Row)
// ----------------------------------------------------
void DisplayManager::drawHomeMenu(bool pushToScreen) {
    if(spr.getBuffer() == nullptr) return;
    
    spr.fillSprite(C_BG); 

    // 🌟 1. คำนวณ Animation Index (Lerp) วิ่งตามค่าเป้าหมายดิบ
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
        // 🌟 2. Infinite Loop Logic: ใช้ fmod เพื่อให้ค่าวนลูป 0-5 เสมอ
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

        // 🌟 3. ปรับสีตามความใกล้เคียง (Fading)
        // ใช้ i เทียบกับ currentMenuIndex ที่คำนวณไว้จาก main.cpp
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

        // 🌟 4. วาดชื่อเมนูเฉพาะตัวที่ "ถูกเลือก" (Active Feature)
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
    if(spr.getBuffer() == nullptr) return;
    spr.fillSprite(C_BG);

    // 🌟 1. วาด Burger Menu มุมขวาบน (Index = 3)
    int burgerX = 280, burgerY = 10, burgerW = 32, burgerH = 32;
    if (currentMusicControlIndex == 3) {
        spr.fillRoundRect(burgerX, burgerY, burgerW, burgerH, 5, C_HILITE);
        spr.drawRoundRect(burgerX-2, burgerY-2, burgerW+4, burgerH+4, 7, C_BAR_FG);
        spr.setTextColor(C_BG);
    } else {
        spr.fillRoundRect(burgerX, burgerY, burgerW, burgerH, 5, C_BG); // กลืนไปกับพื้นหลัง
        spr.setTextColor(C_TEXT);
    }
    // วาดขีด 3 ขีด (Burger)
    uint16_t burgerColor = (currentMusicControlIndex == 3) ? C_BG : C_TEXT;
    spr.fillRect(burgerX + 6, burgerY + 8, 20, 3, burgerColor);
    spr.fillRect(burgerX + 6, burgerY + 15, 20, 3, burgerColor);
    spr.fillRect(burgerX + 6, burgerY + 22, 20, 3, burgerColor);

    // 🌟 2. วาดส่วนแสดงชื่อเพลง และปกอัลบั้มจำลอง
    spr.fillCircle(tft.width()/2, 75, 40, C_CARD); // วงกลมปกอัลบั้ม
    // วาดรูตรงกลางให้เหมือนแผ่นเสียง
    spr.fillCircle(tft.width()/2, 75, 10, C_BG); 
    
    spr.setTextColor(C_TEXT);
    spr.setTextDatum(MC_DATUM);
    spr.drawString(songName, tft.width()/2, 135, 2); // ชื่อเพลง (ฟอนต์ขนาด 2)

    // 🌟 3. วาดหลอด Progress Bar

    char curTimeStr[10];
    char totTimeStr[10];
    snprintf(curTimeStr, sizeof(curTimeStr), "%02lu:%02lu", currentAudioTime / 60, currentAudioTime % 60);
    snprintf(totTimeStr, sizeof(totTimeStr), "%02lu:%02lu", totalAudioDuration / 60, totalAudioDuration % 60);

    int barW = 180, barH = 6; 
    int bx = (tft.width() - barW) / 2; // จัดหลอดให้อยู่กึ่งกลางจอ
    int by = 160;

    // วาดตัวเลขเวลาปัจจุบัน (ฝั่งซ้าย)
    spr.setTextColor(C_TEXT);
    spr.setTextDatum(MR_DATUM); // จัดให้ตัวเลขชิดขวากับขอบหลอด
    spr.drawString(String(curTimeStr), bx - 12, by + 3, 1);

    // วาดตัวเลขเวลาทั้งหมด (ฝั่งขวา)
    spr.setTextDatum(ML_DATUM); // จัดให้ตัวเลขชิดซ้ายกับขอบหลอด
    spr.drawString(String(totTimeStr), bx + barW + 12, by + 3, 1);

    // วาดหลอดสีเทา (พื้นหลัง) และสีฟ้า (ความคืบหน้า)
    spr.fillRoundRect(bx, by, barW, barH, 3, C_BAR_BG);
    spr.fillRoundRect(bx, by, (barW * progress) / 100, barH, 3, C_BAR_FG);

    // 🌟 4. วาดปุ่มควบคุม (Prev, Play, Next) -> Index 0, 1, 2
    int ctrlY = 185;
    int btnW = 55, btnH = 35;
    int spacing = 20;
    int startX = (tft.width() - (3 * btnW + 2 * spacing)) / 2;

    String icons[3] = {"|<", isPlaying ? "||" : ">", ">|"};

    for (int i = 0; i < 3; i++) {
        int cx = startX + i * (btnW + spacing);
        
        // ถ้าเคอร์เซอร์ชี้อยู่ที่ปุ่มนี้ (Hover Effect)
        if (currentMusicControlIndex == i) {
            spr.fillRoundRect(cx, ctrlY, btnW, btnH, 8, C_HILITE);
            spr.drawRoundRect(cx-2, ctrlY-2, btnW+4, btnH+4, 10, C_BAR_FG); // กรอบฟ้า
            spr.setTextColor(C_BG);
        } else {
            spr.fillRoundRect(cx, ctrlY, btnW, btnH, 8, C_CARD);
            spr.setTextColor(C_TEXT);
        }
        
        spr.setTextDatum(MC_DATUM);
        spr.drawString(icons[i], cx + btnW/2, ctrlY + btnH/2, 2);
    }

    if (pushToScreen) {
        if(xSemaphoreTake(displaySemaphore, portMAX_DELAY) == pdTRUE) {
            spr.pushSprite(0, 0);
            xSemaphoreGive(displaySemaphore);
        }
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
    spr.drawString("No Music on SD", tft.width()/2, by + 30, 2);
    spr.drawString("Switch to Online?", tft.width()/2, by + 50, 1);

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
    if(spr.getBuffer() == nullptr) return;
    
    spr.fillSprite(C_BG); // ขาวคลีน

    // 1. Header (y=8)
    spr.setTextColor(C_BAR_FG);
    spr.setTextDatum(TL_DATUM);
    spr.drawString("ONLINE STREAMING", 15, 8, 2);
    
    // WiFi Icon
    spr.fillCircle(tft.width() - 20, 15, 3, C_BAR_FG);
    spr.drawCircle(tft.width() - 20, 15, 6, C_BAR_FG);

    // 2. Album Art - ย่อขนาดลงเพื่อให้เหลือที่ว่าง (y=35, size=80)
    int artSize = 80;
    int artX = (tft.width() - artSize) / 2;
    int artY = 35;
    spr.fillRoundRect(artX, artY, artSize, artSize, 12, C_CARD);
    spr.setTextColor(C_BAR_BG);
    spr.setTextDatum(MC_DATUM);
    spr.drawString("♪", tft.width()/2, artY + artSize/2, 4);

    // 3. Song Info (y=125, 145)
    spr.setTextColor(C_TEXT);
    spr.setTextDatum(MC_DATUM);
    // ตัดชื่อเพลงถ้าเกินจอ
    String title = currentSongTitle;
    if(title.length() > 20) title = title.substring(0, 17) + "...";
    spr.drawString(title, tft.width()/2, 125, 2);
    
    spr.setTextColor(C_BAR_BG);
    spr.drawString("Live Stream", tft.width()/2, 145, 1);

    // 4. Visualizer - ปรับให้เตี้ยลง (y=165)
    int waveX = 85; 
    for(int i=0; i<15; i++) {
        int h = isPlayingAudio ? random(4, 15) : 4; 
        spr.fillRect(waveX + (i*10), 175 - h, 5, h, C_BAR_FG);
    }

    // 5. Controls - ขยับขึ้นมาจากขอบล่าง (y=190)
    int ctrlY = 195; // ขยับลงมานิดนึงให้สมดุล
    int spacing = 45; // เพิ่มระยะห่างระหว่างปุ่ม
    int startX = (tft.width() - (2 * spacing + 120)) / 2; // คำนวณจุดเริ่มวาดปุ่มแรก

    for (int i = 0; i < 3; i++) {
        int cx = startX + i * (spacing + 40) + 20; // จุดศูนย์กลางของแต่ละปุ่ม
        uint16_t color = (currentMusicControlIndex == i) ? C_BAR_FG : C_BAR_BG;
        
        // วาดวงกลมจางๆ หรือกรอบรอบปุ่มที่ถูกเลือก (Hover)
        if (currentMusicControlIndex == i) {
            spr.drawCircle(cx, ctrlY + 15, 22, C_BAR_FG); 
        }

        // --- วาดไอคอนด้วยรูปทรงเรขาคณิต ---
        if (i == 0) { // ⏪ ปุ่ม Previous (สถานีก่อนหน้า)
            spr.fillTriangle(cx - 2, ctrlY + 5, cx - 2, ctrlY + 25, cx - 12, ctrlY + 15, color);
            spr.fillTriangle(cx + 8, ctrlY + 5, cx + 8, ctrlY + 25, cx - 2, ctrlY + 15, color);
            spr.fillRect(cx - 15, ctrlY + 5, 3, 20, color);
        } 
        else if (i == 1) { // ▶️/⏸️ ปุ่ม Play หรือ Pause
            if (isPlayingAudio) { // วาด Pause (||)
                spr.fillRect(cx - 6, ctrlY + 5, 5, 20, color);
                spr.fillRect(cx + 2, ctrlY + 5, 5, 20, color);
            } else { // วาด Play (>)
                spr.fillTriangle(cx - 5, ctrlY + 3, cx - 5, ctrlY + 27, cx + 12, ctrlY + 15, color);
            }
        } 
        else if (i == 2) { // ⏩ ปุ่ม Next (สถานีถัดไป)
            spr.fillTriangle(cx + 2, ctrlY + 5, cx + 2, ctrlY + 25, cx + 12, ctrlY + 15, color);
            spr.fillTriangle(cx - 8, ctrlY + 5, cx - 8, ctrlY + 25, cx + 2, ctrlY + 15, color);
            spr.fillRect(cx + 12, ctrlY + 5, 3, 20, color);
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
// 4. หน้าต่าง Volume (ลอยออกมาจากซ้าย)
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
    
    // 🌟 ย้ายมาวาดด้านขวามือ
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

        // 🌟 จัดเรียงตามตัวอักษร (A-Z)
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
            
            // 🌟 Hover Effect
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
            
            // 🌟 Hover Effect
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
// วาดหน้าจอ Settings (Admin Mode & Bluetooth)
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

        // 🌟 1. วาด Hover Effect 
        if (i == settingSelectedIndex) {
            spr.fillRoundRect(5, y, tft.width() - 10, itemH - 2, 4, C_HILITE);
            spr.setTextColor(C_BG);
        } else {
            spr.setTextColor(C_TEXT);
        }

        // ชื่อรายการ
        spr.setTextDatum(ML_DATUM);
        spr.drawString(items[i], 15, y + (itemH/2), 2);

        // 🌟 2. วาดปุ่ม Toggle (สวิตช์)
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
    
    // 🌟 พื้นหลังดำสนิท
    spr.fillSprite(TFT_BLACK); 

    // 🌟 ตัวแปรสถานะแอนิเมชัน
    static float cur_r = 255, cur_g = 255, cur_b = 255; 
    static float cur_eyeW = 40, cur_eyeH_L = 60, cur_eyeH_R = 60, cur_eyeY = -15; 
    static float cur_mouthW = 40, cur_mouthH = 8, cur_mouthY = 45, cur_mouthX = 0; 
    
    static float cur_gazeX = 0, cur_gazeY = 0; 
    static float tar_gazeX = 0, tar_gazeY = 0;
    
    // 🌟 เปลือกตา (0 คือตาเปิดเต็มที่)
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
            tar_r = 255; tar_g = 255; tar_b = 255; // 🌟 สีขาวล้วนเหมือนหน้าปกติ
            tar_eyeW = 40; 
            tar_eyeH_L = 60; tar_eyeH_R = 60; tar_eyeY = -15; 
            
            tar_eyelidDrop = 35; // 🌟 เปลือกตาปิดลงมาเกินครึ่ง
            
            // 🌟 ลดความหนาปากลงเหลือแค่ 4 พิกเซล จะได้เป็นขีดเส้นตรง ไม่ดูเหมือนยิ้ม
            tar_mouthW = 40; tar_mouthH = 4; tar_mouthY = 45; tar_mouthX = 0; 
            break;

        case 2: // 😂 Laughing (หัวเราะตัวสั่น)
            tar_eyeH_L = 40; tar_eyeH_R = 40; // หรี่ตาลงนิดนึงให้ดูดุ
            tar_eyeY = -5;
            
            tar_angryBrow = 30; // 🌟 สั่งให้สามเหลี่ยมตัดขอบตาเฉียงลงมา 30 พิกเซล
            
            tar_mouthW = 15; // 🌟 ปากหดสั้นจู๋ (เหมือนเม้มปากแน่นด้วยความโกรธ)
            tar_mouthY = 40;
            break;
    }

    // Open the mouth from live microphone amplitude while listening or playing.
    float voiceActivity = abs((int)readMicData()) / 12000.0f;
    if (voiceActivity > 1.0f) voiceActivity = 1.0f;
    if (isRecording || isPlayingAudio) {
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

    // 🌟 1. วาดตาสี่เหลี่ยมขอบมนเต็มดวง
    spr.fillRoundRect(eyeL_X, eyeL_Y, cur_eyeW, cur_eyeH_L, eyeRad, faceColor);
    spr.fillRoundRect(eyeR_X, eyeR_Y, cur_eyeW, cur_eyeH_R, eyeRad, faceColor);

    // 🌟 2. วาด "เปลือกตาสีดำ" ทับส่วนบน
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

    // Update hardware status
    hwManager.updateAllStatus();

    spr.fillSprite(C_BG);
    spr.setTextColor(C_TEXT);
    spr.setTextFont(1);
    spr.setTextSize(1);

    // Header
    spr.setTextDatum(TL_DATUM);
    spr.drawString("HARDWARE DEBUG STATUS", 5, 5, 2);
    spr.drawLine(5, 25, tft.width() - 5, 25, C_HILITE);

    // Memory status panel
    const uint32_t sramTotal = ESP.getHeapSize();
    const uint32_t sramFree = ESP.getFreeHeap();
    const uint32_t psramTotal = psramFound() ? ESP.getPsramSize() : 0;
    const uint32_t psramFree = psramFound() ? ESP.getFreePsram() : 0;
    const uint32_t flashTotal = ESP.getFlashChipSize();
    const uint32_t flashUsed = ESP.getSketchSize();
    const uint32_t flashFree = flashTotal > flashUsed ? flashTotal - flashUsed : 0;

    const int memoryBoxY = 32;
    const int memoryBoxH = 91;
    spr.drawRect(5, memoryBoxY, tft.width() - 10, memoryBoxH, C_HILITE);
    spr.setTextColor(C_TEXT);
    spr.setTextDatum(TL_DATUM);
    spr.drawString("Memory (Total / Used / Free)", 10, memoryBoxY + 5, 1);

    char memoryInfo[80];
    snprintf(memoryInfo, sizeof(memoryInfo), "SRAM : %lu / %lu / %lu KB",
        sramTotal / 1024,
        (sramTotal - sramFree) / 1024,
        sramFree / 1024);
    spr.drawString(memoryInfo, 10, memoryBoxY + 22, 1);

    snprintf(memoryInfo, sizeof(memoryInfo), "PSRAM: %lu / %lu / %lu KB",
        psramTotal / 1024,
        (psramTotal - psramFree) / 1024,
        psramFree / 1024);
    spr.drawString(memoryInfo, 10, memoryBoxY + 39, 1);

    snprintf(memoryInfo, sizeof(memoryInfo), "FLASH: %lu / %lu / %lu KB",
        flashTotal / 1024,
        flashUsed / 1024,
        flashFree / 1024);
    spr.drawString(memoryInfo, 10, memoryBoxY + 56, 1);

    spr.setTextColor(C_HILITE);
    spr.drawString("Flash used = firmware size", 10, memoryBoxY + 73, 1);

    // Display devices status with encoder scrolling
    const int startY = memoryBoxY + memoryBoxH + 5;
    const int itemHeight = 28;
    const int listBottom = tft.height() - 23;
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

        // Draw status indicator circle
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
                statusColor = TFT_DARKGRAY;
        }

        spr.fillCircle(12, yPos + 8, 4, statusColor);

        // Device name
        spr.setTextColor(deviceIndex == debugSelectedIndex ? C_BG : C_TEXT);
        spr.setTextDatum(TL_DATUM);
        spr.drawString(dev.name, 25, yPos, 1);

        // Status string
        String statusStr = hwManager.getStatusString(dev.status);
        spr.setTextDatum(TR_DATUM);
        spr.setTextColor(statusColor);
        spr.drawString(statusStr, tft.width() - 5, yPos, 1);

        // Details
        spr.setTextColor(deviceIndex == debugSelectedIndex ? C_BG : C_HILITE);
        spr.setTextDatum(TL_DATUM);
        spr.drawString(dev.details, 25, yPos + 12, 1);
    }

    // Navigation hint
    spr.setTextColor(C_HILITE);
    spr.setTextDatum(BC_DATUM);
    spr.drawString("Back to exit debug menu", tft.width() / 2, tft.height() - 5, 1);

    if (xSemaphoreTake(displaySemaphore, portMAX_DELAY) == pdTRUE) {
        spr.pushSprite(0, 0);
        xSemaphoreGive(displaySemaphore);
    }
}

void DisplayManager::recorde() {
    if(spr.getBuffer() == nullptr) return;

    // Redraw UI every frame
    spr.fillSprite(C_BG);

    // Header
    spr.setTextColor(C_TEXT);
    spr.setTextFont(2);
    spr.setTextDatum(MC_DATUM);
    spr.drawString("Voice Recording", tft.width() / 2, 30, 2);

    // Recording status indicator with color
    spr.setTextFont(1);
    uint16_t statusColor;
    String statusText;

    if (isRecording) {
        statusColor = TFT_RED;
        statusText = "● RECORDING";
    } else {
        statusColor = TFT_DARKGRAY;
        statusText = "● READY";
    }

    spr.setTextColor(statusColor);
    spr.setTextDatum(MC_DATUM);
    spr.drawString(statusText, tft.width() / 2, 70, 2);

    // Timer display
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= interval) {
        previousMillis = currentMillis;
        if (isRecording) {
            seconds += 1;
        }
    }

    spr.setTextColor(C_TEXT);
    spr.setTextFont(2);
    char timeStr[16];
    int mins = seconds / 60;
    int secs = seconds % 60;
    snprintf(timeStr, sizeof(timeStr), "%02d:%02d", mins, secs);
    spr.drawString(timeStr, tft.width() / 2, 130);

    // Record button area
    int btnX = tft.width() / 2;
    int btnY = 200;
    int btnRadius = 40;

    // Draw button circle
    if (isRecording) {
        // Stop button (red square)
        spr.fillRect(btnX - 25, btnY - 25, 50, 50, TFT_RED);
        spr.drawRect(btnX - 25, btnY - 25, 50, 50, tft.color565(150, 0, 0));  // Dark red
    } else {
        // Record button (green circle)
        spr.fillCircle(btnX, btnY, btnRadius, TFT_GREEN);
        spr.drawCircle(btnX, btnY, btnRadius, tft.color565(0, 100, 0));  // Dark green
    }

    // Button label
    spr.setTextColor(TFT_WHITE);
    spr.setTextFont(1);
    spr.setTextDatum(MC_DATUM);
    if (isRecording) {
        spr.drawString("STOP", btnX, btnY);
    } else {
        spr.drawString("REC", btnX, btnY);
    }

    // Instructions
    spr.setTextColor(C_HILITE);
    spr.setTextFont(1);
    spr.setTextDatum(MC_DATUM);
    spr.drawString("Press to record/stop", tft.width() / 2, 280);
    spr.drawString("Back to exit", tft.width() / 2, 300);

    // File size info
    spr.setTextColor(C_TEXT);
    char fileInfo[64];
    snprintf(fileInfo, sizeof(fileInfo), "File: voice_record.wav");
    spr.drawString(fileInfo, tft.width() / 2, 320 - 20);

    if (xSemaphoreTake(displaySemaphore, portMAX_DELAY) == pdTRUE) {
        spr.pushSprite(0, 0);
        xSemaphoreGive(displaySemaphore);
    }
}

// ฟังก์ชันสำหรับแอบอ่านขนาดกว้าง/สูง จาก Header ของไฟล์ JPEG
bool DisplayManager::getJpegSize(const char* filename, uint16_t &width, uint16_t &height) {
    File f = SD.open(filename, FILE_READ);
    if (!f) return false;

    // ตรวจสอบว่าเป็นไฟล์ JPEG จริงหรือไม่ (ต้องขึ้นต้นด้วย 0xFF 0xD8)
    if (f.read() != 0xFF || f.read() != 0xD8) {
        f.close();
        return false;
    }

    while (f.available()) {
        uint8_t c = f.read();
        if (c == 0xFF) {
            uint8_t marker = f.read();
            
            // ข้าม byte ที่เป็น Padding (0xFF ซ้ำๆ)
            while (marker == 0xFF) marker = f.read();

            if (marker == 0xD8 || marker == 0x01) continue; // ข้าม Marker ที่ไม่มีขนาด
            if (marker == 0xD9 || marker == 0xDA) break;    // เจอข้อมูลภาพแล้ว แปลว่าหา Header ไม่เจอ

            // 0xC0 (SOF0) หรือ 0xC2 (SOF2) คือบล็อกที่เก็บขนาดกว้าง/สูง
            if (marker == 0xC0 || marker == 0xC2) {
                f.seek(f.position() + 3); // ข้ามช่อง Length(2) และ Precision(1)
                height = (f.read() << 8) | f.read(); // อ่านความสูง
                width = (f.read() << 8) | f.read();  // อ่านความกว้าง
                f.close();
                return true; // อ่านสำเร็จ!
            } else {
                // ถ้าเป็นบล็อกอื่น (เช่น ข้อมูลกล้อง EXIF) ให้กระโดดข้ามไปเลยเพื่อความรวดเร็ว
                uint16_t len = (f.read() << 8) | f.read();
                f.seek(f.position() + (len - 2));
            }
        }
    }
    f.close();
    return false;
}

// 🌟 เปลี่ยนพารามิเตอร์ให้รับขนาดรูปล่วงหน้า (แทน xpos, ypos เดิม)
// 🌟 เปลี่ยนกลับมาใช้ parameter เดียวเหมือนเดิม
void DisplayManager::drawJpeg(const char *filename) {
    if (xSemaphoreTake(displaySemaphore, pdMS_TO_TICKS(100)) == pdTRUE) {
        
        spr.fillScreen(TFT_BLACK);
        
        uint16_t imgWidth = 0, imgHeight = 0;
        
        // 1. ให้ฟังก์ชันไปเจาะอ่านขนาดของรูปภาพแบบอัตโนมัติ
        if (getJpegSize(filename, imgWidth, imgHeight)) {
            
            // 2. ถ้าอ่านขนาดได้ ก็คำนวณหา "มุมซ้ายบน" ให้อยู่กึ่งกลางจอ
            int startX = (spr.width() - imgWidth) / 2;
            int startY = (spr.height() - imgHeight) / 2;

            // ป้องกันพิกัดติดลบ (กรณีรูปใหญ่กว่าจอ)
            if (startX < 0) startX = 0;
            if (startY < 0) startY = 0;

            // 3. วาดภาพจัดกึ่งกลาง
            spr.drawJpgFile(SD, filename, startX, startY);
            
        } else {
            // ถ้าไฟล์เสีย, ไม่ใช่ JPEG, หรืออ่านขนาดไม่ได้ ให้เริ่มวาดที่มุมซ้ายบนตามปกติ
            spr.drawJpgFile(SD, filename, 0, 0);
        }
        
        spr.pushSprite(0, 0);
        xSemaphoreGive(displaySemaphore);
    }
}

// ==========================================
// Callback Functions สำหรับ AnimatedGIF
// ==========================================
void * GIFOpenFile(const char *fname, int32_t *pSize) {
  if(xSemaphoreTake(sdSemaphore, portMAX_DELAY) == pdTRUE) {
      gifFile = SD.open(fname);
      if (gifFile) {
        *pSize = gifFile.size();
        xSemaphoreGive(sdSemaphore);
        return (void *)&gifFile;
      }
      xSemaphoreGive(sdSemaphore);
  }
  return NULL;
}

void GIFCloseFile(void *pHandle) { 
  if(xSemaphoreTake(sdSemaphore, portMAX_DELAY) == pdTRUE) {
      gifFile.close(); 
      xSemaphoreGive(sdSemaphore);
  }
}

int32_t GIFReadFile(GIFFILE *pFile, uint8_t *pBuf, int32_t iLen) { 
    int32_t bytesRead = 0;
    if(xSemaphoreTake(sdSemaphore, portMAX_DELAY) == pdTRUE) {
        // 🌟 ให้ ESP32 จัดการเรื่อง EOF เองล้วนๆ ไม่ต้องมี Work-around
        bytesRead = gifFile.read(pBuf, iLen);
        pFile->iPos = gifFile.position();
        xSemaphoreGive(sdSemaphore);
    }
    return bytesRead;
}

int32_t GIFSeekFile(GIFFILE *pFile, int32_t iPosition) { 
    if(xSemaphoreTake(sdSemaphore, portMAX_DELAY) == pdTRUE) {
        gifFile.seek(iPosition);
        pFile->iPos = gifFile.position();
        xSemaphoreGive(sdSemaphore);
    }
    return pFile->iPos;
}

#define BUFFER_SIZE 256
uint16_t usTemp[1][BUFFER_SIZE]; 

void GIFDraw(GIFDRAW *pDraw) {
  uint8_t *s;
  uint16_t *d, *usPalette;
  int x, y, iWidth, iCount;

  iWidth = pDraw->iWidth;
  if (iWidth + pDraw->iX > tft.width()) iWidth = tft.width() - pDraw->iX;
    
  usPalette = pDraw->pPalette;
  y = iYOff + pDraw->iY + pDraw->y; // จัดกึ่งกลางแนวตั้ง

  if (y >= tft.height() || (iXOff + pDraw->iX) >= tft.width() || iWidth < 1) return;

  s = pDraw->pPixels;
  if (pDraw->ucDisposalMethod == 2) { 
    for (x = 0; x < iWidth; x++) {
      if (s[x] == pDraw->ucTransparent) s[x] = pDraw->ucBackground;
    }
    pDraw->ucHasTransparency = 0;
  }

  // 🌟 รอคิวหน้าจอ
  if (xSemaphoreTake(displaySemaphore, portMAX_DELAY) == pdTRUE) {
      if (pDraw->y == 0) Serial.println(">>> DRAWING 1 FRAME! <<<");
      // --- กรณีที่ 1: ภาพมีพื้นหลังโปร่งใส ---
      if (pDraw->ucHasTransparency) {
        uint8_t *pEnd, c, ucTransparent = pDraw->ucTransparent;
        pEnd = s + iWidth;
        x = 0;
        iCount = 0;
        
        while (x < iWidth) {
          c = ucTransparent - 1;
          d = &usTemp[0][0];
          
          while (c != ucTransparent && s < pEnd && iCount < BUFFER_SIZE) {
            c = *s++;
            if (c == ucTransparent) s--; 
            else { *d++ = usPalette[c]; iCount++; }
          } 
          
          if (iCount) {
            // 🌟 เปลี่ยนมาใช้ pushImage แก้ปัญหาหน้าจอไม่รับข้อมูล
            tft.pushImage(iXOff + pDraw->iX + x, y, iCount, 1, usTemp[0]);
            x += iCount;
            iCount = 0;
          }
          
          c = ucTransparent;
          while (c == ucTransparent && s < pEnd) {
            c = *s++;
            if (c == ucTransparent) x++; else s--;
          }
        }
      } 
      // --- กรณีที่ 2: ภาพทึบปกติ (เขียนโค้ดให้สั้นและไวขึ้นมาก) ---
      else {
        s = pDraw->pPixels;
        int currentX = iXOff + pDraw->iX; 
        
        while (iWidth > 0) {
            int toDraw = (iWidth <= BUFFER_SIZE) ? iWidth : BUFFER_SIZE;
            for (iCount = 0; iCount < toDraw; iCount++) {
                usTemp[0][iCount] = usPalette[*s++];
            }
            // 🌟 เปลี่ยนมาใช้ pushImage
            tft.pushImage(currentX, y, toDraw, 1, usTemp[0]);
            currentX += toDraw;
            iWidth -= toDraw;
        }
      }
      xSemaphoreGive(displaySemaphore);
  }
}

// ==========================================

bool DisplayManager::openGif(const char *filename) {
    gif.begin(GIF_PALETTE_RGB565_BE);
    if (gif.open(filename, GIFOpenFile, GIFCloseFile, GIFReadFile, GIFSeekFile, GIFDraw)) {
        Serial.printf("Playing GIF: %s\n", filename);
        
        // 🌟 คำนวณจัดกึ่งกลางภาพ
        iXOff = (tft.width() - gif.getCanvasWidth()) / 2;
        if (iXOff < 0) iXOff = 0;
        
        iYOff = (tft.height() - gif.getCanvasHeight()) / 2;
        if (iYOff < 0) iYOff = 0;
        
        return true;
    } else {
        Serial.println("Failed to open GIF file.");
        return false;
    }
}

int DisplayManager::playGifFrame() {
    // คืนค่ากลับไปให้ handleDisplay รู้ว่าเล่นจบหรือ Error หรือยัง
    return gif.playFrame(true, NULL); 
}

void DisplayManager::stopGif() {
    gif.close();
    Serial.println("GIF Stopped.");
}

// 🌟 นำโค้ด Task ตัวที่ถูกต้องมาใช้งาน (เช็ค waitTime)
void handleDisplay(void *pvParameters) {
    DISPLAY_COMMAND cmd;
    enum class STATE { IDLE, SHOW, CLEAR, PLAYING_GIF } state = STATE::IDLE;
    String lastPath = "";

    for (;;) {
        // 🌟 ไม่ Block คิวเวลาเล่น GIF ทำให้เฟรมเรตไม่ตก
        TickType_t waitTime = (state == STATE::PLAYING_GIF) ? pdMS_TO_TICKS(5) : pdMS_TO_TICKS(50);
        
        if(xQueueReceive(display_command, &cmd, waitTime) == pdPASS) {
            if(cmd.module == DISPLAY_COMMAND::MODULE::DIS) {
                
                if(cmd.display_state == DISPLAY_COMMAND::DISPLAY_STATE::SHOW) {
                    lastPath = cmd.path;
                    if(state == STATE::PLAYING_GIF) DISM.stopGif();
                    state = STATE::SHOW;
                    Serial.println("SHOW OK. Path: " + lastPath);
                }

                if(cmd.display_state == DISPLAY_COMMAND::DISPLAY_STATE::CLEAR) {
                    if(state == STATE::PLAYING_GIF) DISM.stopGif();
                    state = STATE::CLEAR;
                    Serial.println("CLEAR OK.");
                }
            }
        }

        switch (state) {
        case STATE::SHOW:
            if (lastPath != "") {
                String pathLower = lastPath;
                pathLower.toLowerCase();
                DISM.resetDisplay();
                
                if (pathLower.endsWith(".jpg") || pathLower.endsWith(".jpeg")) {
                    DISM.drawJpeg(lastPath.c_str());
                    state = STATE::IDLE; 
                } else if(pathLower.endsWith(".gif")){
                    if(DISM.openGif(lastPath.c_str())) {
                        Serial.println("openedd.");
                        state = STATE::PLAYING_GIF; 
                    } else {
                        state = STATE::IDLE;
                    }
                } else {
                     state = STATE::IDLE;
                }
            } else {
                state = STATE::IDLE;
            }
            break;

        case STATE::PLAYING_GIF:
        {   // 🌟 ใส่ปีกกาครอบเคสนี้ไว้ด้วย
            int result = DISM.playGifFrame();
            
            // ถ้าเล่นจนจบไฟล์แล้ว (<= 0)
            if (result <= 0) {
                DISM.stopGif(); // ปิดไฟล์เก่า
                
                // 🌟 พยายามเปิดไฟล์เดิมอีกครั้ง เพื่อเล่นแบบวนลูป
                if (DISM.openGif(lastPath.c_str())) {
                    // เปิดสำเร็จ จะทำงานต่อในลูปหน้า
                } else {
                    // แต่ถ้าไฟล์พัง เปิดไม่ติด ให้กลับไปหน้าจอดำ (IDLE) เพื่อไม่ให้สแปม
                    state = STATE::IDLE;
                }
            }
            vTaskDelay(pdMS_TO_TICKS(1)); 
            break;
        }

        case STATE::CLEAR:
            DISM.resetDisplay();
            state = STATE::IDLE;
            vTaskDelay(pdMS_TO_TICKS(10));
            break;
        
        case STATE::IDLE:
        default:
            vTaskDelay(pdMS_TO_TICKS(100));
            break;
        } 
    }
}