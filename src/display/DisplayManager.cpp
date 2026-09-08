// Owns UI state, sprite rendering, media lists and diagnostic screens.
#include "DisplayManager.hpp"
#include "../storage/FileManager.hpp"
#include "../audio/SoundManager.hpp"
#include "../core/GlobalState.hpp"
#include <WiFi.h>
#include "../network/Network.hpp"

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
void DisplayManager::applyTheme() {
    auto color=[](int role) { return preferences::rgb565(preferences::rgb(userSettings.values.colors[role])); };
    C_BG=color(preferences::Background); C_CARD=color(preferences::Surface);
    C_TEXT=color(preferences::Text); C_BAR_FG=color(preferences::Accent);
    C_MUTED=color(preferences::Muted); C_HILITE=color(preferences::Selection);
    C_BAR_BG=C_CARD;
    C_SELECT_TEXT=preferences::dark(preferences::rgb(userSettings.values.colors[preferences::Selection])) ? TFT_WHITE : tft.color565(30,42,48);
}
void DisplayManager::present(bool push) {
    if (push && xSemaphoreTake(displaySemaphore,portMAX_DELAY)==pdTRUE) {
        spr.pushSprite(0,0); xSemaphoreGive(displaySemaphore);
    }
}
void DisplayManager::pageHeader(const String& title, const String& subtitle) {
    spr.fillSprite(C_BG); spr.setTextSize(1); spr.setTextFont(1);
    spr.setTextDatum(TL_DATUM); spr.setTextColor(C_TEXT);
    spr.drawString(title,16,10,4);
    if (subtitle.length()) { spr.setTextColor(C_MUTED); spr.drawString(subtitle,17,40,1); }
}
void DisplayManager::footer(const String& text) {
    spr.setTextDatum(MC_DATUM); spr.setTextColor(C_MUTED); spr.setTextFont(1);
    spr.drawString(text,tft.width()/2,tft.height()-10,1);
}
void DisplayManager::toggle(int x, int y, bool on) {
    spr.fillRoundRect(x,y,42,22,11,on?C_BAR_FG:C_MUTED);
    spr.fillCircle(x+(on?31:11),y+11,8,C_BG);
}
void DisplayManager::drawLoading(int percent, String text) {
    if (!spr.getBuffer()) return;
    pageHeader("ToFan", "A little space for your day");
    spr.setTextDatum(MC_DATUM); spr.setTextColor(C_TEXT);
    spr.drawString(text,tft.width()/2,112,2);
    spr.fillRoundRect(28,146,tft.width()-56,6,3,C_CARD);
    int fill=(tft.width()-56)*preferences::clamp(percent,0,100)/100;
    if(fill) spr.fillRect(28,146,fill,6,C_BAR_FG);
    footer("Starting up"); present(true);
}
void DisplayManager::drawHomeMenu(bool pushToScreen) {
    if (!spr.getBuffer()) return;
    pageHeader("Home", "Your music, moments and little assistant");
    const char* labels[]={"Pictures","Music","Settings","AI Pet","Devices","Recorder"};
    const int cw=(tft.width()-44)/2;
    for(int i=0;i<6;++i) {
        int x=16+(i%2)*(cw+12),y=56+(i/2)*52;
        const bool selected=i==currentMenuIndex;
        const uint16_t ink=selected?C_SELECT_TEXT:C_TEXT;
        spr.fillRoundRect(x,y,cw,44,12,selected?C_HILITE:C_CARD);
        if(selected) spr.drawRoundRect(x,y,cw,44,12,C_BAR_FG);
        if(i==0) { spr.drawRoundRect(x+12,y+12,20,18,3,ink); spr.fillTriangle(x+14,y+27,x+22,y+19,x+29,y+27,ink); }
        else if(i==1) { spr.fillRect(x+24,y+10,3,22,ink); spr.fillRect(x+26,y+10,8,3,ink); spr.fillEllipse(x+20,y+30,6,4,ink); }
        else if(i==2) { for(int j=0;j<3;++j) spr.drawLine(x+12,y+14+j*8,x+32,y+14+j*8,ink); spr.fillCircle(x+19,y+14,3,ink); spr.fillCircle(x+27,y+22,3,ink); }
        else if(i==3) { spr.drawRoundRect(x+10,y+11,24,23,8,ink); spr.fillCircle(x+17,y+21,2,ink); spr.fillCircle(x+27,y+21,2,ink); }
        else if(i==4) { spr.drawRoundRect(x+12,y+10,20,24,4,ink); spr.fillCircle(x+22,y+29,2,ink); }
        else { spr.fillRoundRect(x+19,y+10,8,17,4,ink); spr.drawRoundRect(x+15,y+15,16,16,7,ink); spr.drawLine(x+23,y+30,x+23,y+35,ink); }
        spr.setTextDatum(ML_DATUM); spr.setTextColor(ink); spr.drawString(labels[i],x+43,y+22,2);
    }
    footer("Turn to browse  /  Press to open");
    isAnimatingMenu=false; animatedMenuIndex=animatedMenuIndex_target;
    present(pushToScreen);
}

void DisplayManager::drawMusicPlayer(String songName, int progress, bool isPlaying, bool pushToScreen) {
    drawMusicSurface(false, playlistNames.empty() ? "No tracks on SD" : isOnlineAudio ? "Choose a track" : songName, isOnlineAudio ? 0 : progress,
                     isPlaying && !isOnlineAudio, pushToScreen);
}

void DisplayManager::drawMusicSurface(bool online, String title, int progress, bool playing, bool pushToScreen) {
    if (!spr.getBuffer()) return;
    const int w = tft.width(), h = tft.height();
    const uint16_t ink = C_TEXT;
    const uint16_t muted = C_MUTED;
    const uint16_t accent = C_BAR_FG;
    const uint16_t paper = C_BG;
    const uint16_t soft = C_CARD;
    spr.fillSprite(paper);
    spr.setTextDatum(TL_DATUM);
    spr.setTextColor(ink);
    spr.drawString("Music", 18, 12, 4);
    const int sourceIndex = online ? 3 : 4;
    const bool sourceFocused = currentMusicControlIndex == sourceIndex;
    spr.fillRoundRect(w - 96, 14, 78, 27, 13, sourceFocused ? C_HILITE : soft);
    spr.setTextColor(sourceFocused ? C_SELECT_TEXT : ink);
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
    const char* status = online && networkSettingsBusy() ? "Connecting to Wi-Fi..." :
                         online && WiFi.status() != WL_CONNECTED ? "Wi-Fi required" :
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
        char timeLabel[40];
        preferences::playbackTime(timeLabel,sizeof(timeLabel),isOnlineAudio?0:currentAudioTime,isOnlineAudio?0:totalAudioDuration);
        spr.drawString(timeLabel,18,149,1);
        const bool listFocused = currentMusicControlIndex == 3;
        spr.fillRoundRect(w - 94, 146, 76, 23, 10, listFocused ? C_HILITE : soft);
        spr.setTextColor(listFocused ? C_SELECT_TEXT : ink);
        spr.setTextDatum(MC_DATUM);
        spr.drawString("Tracks", w - 56, 157, 1);
    }
    const int cy = h - 43;
    for (int i = 0; i < 3; ++i) {
        const int cx = w/2 + (i - 1) * 70;
        const bool focused = currentMusicControlIndex == i;
        spr.fillCircle(cx, cy, i == 1 ? 24 : 20, focused ? C_HILITE : soft);
        const uint16_t color = focused ? C_SELECT_TEXT : ink;
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
    spr.drawString(userSettings.values.shuffle ? "Shuffle on  /  Turn to select" : userSettings.values.autoNext ? "Auto-next on  /  Turn to select" : "Turn to select  /  Press to choose", w/2, h - 9, 1);
    if (pushToScreen && xSemaphoreTake(displaySemaphore, portMAX_DELAY) == pdTRUE) {
        spr.pushSprite(0, 0);
        xSemaphoreGive(displaySemaphore);
    }
}

void DisplayManager::drawPopupNoMusic(bool pushToScreen) {
    if (!spr.getBuffer()) return;
    pageHeader("Go online", "Network connection");
    spr.fillRoundRect(16,60,tft.width()-32,140,16,C_CARD);
    spr.setTextColor(C_TEXT); spr.setTextDatum(MC_DATUM);
    spr.drawString("Connect to saved Wi-Fi?",tft.width()/2,88,2);
    spr.setTextColor(C_MUTED); spr.drawString("Manage Wi-Fi in Network settings",tft.width()/2,114,1);
    for(int i=0;i<2;++i) {
        const int x=32+i*140;
        spr.fillRoundRect(x,146,116,34,12,popupSelectedIndex==i?C_HILITE:C_BG);
        spr.setTextColor(popupSelectedIndex==i?C_SELECT_TEXT:C_TEXT);
        spr.drawString(i==0?"Connect":"Cancel",x+58,163,2);
    }
    footer("Press to choose  /  Back to return"); present(pushToScreen);
}

void DisplayManager::drawOnlineMusicPlayer(bool pushToScreen) {
    drawMusicSurface(true, isOnlineAudio ? currentSongTitle : String(onlineStationNames[currentStationIndex]),
                     0, isPlayingAudio && isOnlineAudio, pushToScreen);
}

// ----------------------------------------------------
// 4. หน้าต่าง Volume ด้านขวาของจอ
// ----------------------------------------------------
void DisplayManager::drawVolumeOverlay() {
    if (!spr.getBuffer()) return;
    pageHeader("Volume", "Turn to adjust your listening level");
    spr.fillRoundRect(16,64,tft.width()-32,135,18,C_CARD);
    spr.setTextDatum(MC_DATUM); spr.setTextColor(C_TEXT);
    spr.drawString(String(currentVolLevel),tft.width()/2,111,6);
    spr.setTextColor(C_MUTED); spr.drawString("out of 100",tft.width()/2,145,1);
    spr.fillRoundRect(36,172,tft.width()-72,7,3,C_BG);
    const int fill=(tft.width()-72)*currentVolLevel/100;
    if(fill) spr.fillRect(36,172,fill,7,C_BAR_FG);
    footer(userSettings.saveFailed?"Save failed - try again":"Press or wait to return"); present(true);
}

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
void DisplayManager::mediaList(const char* title, const std::vector<String>& names, int& selected, int& scroll, bool push) {
    if (!spr.getBuffer()) return;
    pageHeader(title, String(names.size())+" items  /  SD card");
    selected=preferences::clamp(selected,0,names.empty()?0:static_cast<int>(names.size()-1));
    const int visible=5, itemH=32;
    if(selected<scroll) scroll=selected;
    if(selected>=scroll+visible) scroll=selected-visible+1;
    if(names.empty()) { spr.setTextDatum(MC_DATUM); spr.setTextColor(C_MUTED); spr.drawString("Nothing here yet",tft.width()/2,125,2); }
    for(int row=0;row<visible && scroll+row<static_cast<int>(names.size());++row) {
        int i=scroll+row,y=54+row*itemH;
        spr.fillRoundRect(16,y,tft.width()-32,itemH-3,8,i==selected?C_HILITE:C_CARD);
        spr.setTextColor(i==selected?C_SELECT_TEXT:C_TEXT); spr.setTextDatum(ML_DATUM);
        String name=names[i];
        spr.setTextFont(2);
        while(name.length() && spr.textWidth(name)>tft.width()-66) {
            int end=name.length()-1;
            while(end>0 && (static_cast<unsigned char>(name[end])&0xc0)==0x80) --end;
            name.remove(end);
        }
        spr.drawString(name,27,y+14,2); spr.setTextFont(1);
    }
    footer("Turn to browse  /  Press to open  /  Back"); present(push);
}
void DisplayManager::drawMusicList(bool pushToScreen) { mediaList("Tracks",playlistNames,playlistSelectedIndex,playlistScrollOffset,pushToScreen); }

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
void DisplayManager::drawImageList(bool pushToScreen) { mediaList("Pictures",imageNames,imageSelectedIndex,imageScrollOffset,pushToScreen); }

int DisplayManager::settingsCount() const {
    switch(settingsPage) {
        case ui::SettingsPage::Root:return 4; case ui::SettingsPage::Display:return 7;
        case ui::SettingsPage::Network:return 2; case ui::SettingsPage::Sound:return 4;
        default:return 1;
    }
}
void DisplayManager::drawSettings(bool pushToScreen) {
    if (!spr.getBuffer()) return;
    using Page=ui::SettingsPage;
    const char* titles[]={"Settings","Display settings","Network settings","Music & Sound","Voice Assistant"};
    const char* root[]={"Display","Network","Music & Sound","Voice Assistant"};
    const char* display[]={"Background","Cards & panels","Text","Accent","Secondary text","Selection","Reset palette"};
    const char* sound[]={"Auto-next","Shuffle","Volume step","Volume"};
    const char* network[]={"Wi-Fi","Admin access point"};
    const auto& v=userSettings.values;
    String subtitle="Make it feel like you";
    if(settingsPage==Page::Network) subtitle=networkSettingsBusy()?"Applying network settings...":WiFi.status()==WL_CONNECTED?"Wi-Fi connected":"Wi-Fi disconnected";
    if(settingsPage==Page::Voice) subtitle="On-device voice commands";
    pageHeader(titles[static_cast<int>(settingsPage)],subtitle);
    const int count=settingsCount(), visible=5;
    if(settingSelectedIndex<settingsScroll) settingsScroll=settingSelectedIndex;
    if(settingSelectedIndex>=settingsScroll+visible) settingsScroll=settingSelectedIndex-visible+1;
    for(int row=0;row<visible && row+settingsScroll<count;++row) {
        int i=row+settingsScroll,y=54+row*32;
        bool selected=i==settingSelectedIndex;
        spr.fillRoundRect(16,y,tft.width()-32,29,8,selected?C_HILITE:C_CARD);
        spr.setTextColor(selected?C_SELECT_TEXT:C_TEXT); spr.setTextDatum(ML_DATUM);
        const char* label=settingsPage==Page::Root?root[i]:settingsPage==Page::Display?display[i]:
                          settingsPage==Page::Network?network[i]:settingsPage==Page::Sound?sound[i]:"Voice recognition";
        spr.drawString(label,27,y+14,2);
        bool isToggle=settingsPage==Page::Network || settingsPage==Page::Voice || (settingsPage==Page::Sound && i<2);
        if(isToggle) {
            bool enabled=settingsPage==Page::Network?(i==0?v.wifi:v.admin):settingsPage==Page::Voice?v.voice:(i==0?v.autoNext:v.shuffle);
            toggle(tft.width()-69,y+3,enabled);
        } else if(settingsPage==Page::Display && i<6) {
            spr.fillCircle(tft.width()-43,y+14,9,preferences::rgb565(preferences::rgb(v.colors[i])));
            spr.drawCircle(tft.width()-43,y+14,10,C_MUTED);
        } else {
            String value=">";
            if(settingsPage==Page::Sound) value=String(i==2?v.volumeStep:v.volume);
            spr.setTextDatum(MR_DATUM); spr.drawString(value,tft.width()-28,y+14,2);
        }
    }
    footer(userSettings.saveFailed?"Save failed - press to retry":"Turn to select  /  Press to change  /  Back"); present(pushToScreen);
}
void DisplayManager::drawColorPicker(bool pushToScreen) {
    if (!spr.getBuffer()) return;
    static const char* labels[]={"Background","Cards & panels","Text","Accent","Secondary text","Selection"};
    const auto color=userSettings.values.colors[colorRole];
    pageHeader(labels[colorRole],"Preview your color before saving");
    const int cx=94,cy=132,radius=67;
    for(int y=-radius;y<=radius;++y) for(int x=-radius;x<=radius;++x) {
        const float distance=sqrtf(static_cast<float>(x*x+y*y));
        if(distance>radius) continue;
        float angle=atan2f(static_cast<float>(y),static_cast<float>(x))*180.0f/3.14159265f;
        if(angle<0) angle+=360;
        preferences::HSV pixel{static_cast<uint16_t>(angle),static_cast<uint8_t>(distance*100/radius),color.value};
        spr.drawPixel(cx+x,cy+y,preferences::rgb565(preferences::rgb(pixel)));
    }
    const float angle=color.hue*3.14159265f/180;
    int px=cx+cosf(angle)*radius*color.saturation/100,py=cy+sinf(angle)*radius*color.saturation/100;
    spr.drawCircle(px,py,5,TFT_BLACK); spr.drawCircle(px,py,6,TFT_WHITE);
    const char* components[]={"Hue","Saturation","Brightness"};
    int values[]={color.hue,color.saturation,color.value};
    for(int i=0;i<3;++i) {
        int y=72+i*42;
        spr.fillRoundRect(178,y,tft.width()-194,34,9,i==colorPhase?C_HILITE:C_CARD);
        spr.setTextColor(i==colorPhase?C_SELECT_TEXT:C_TEXT); spr.setTextDatum(TL_DATUM);
        spr.drawString(components[i],186,y+4,1);
        spr.drawString(String(values[i])+(i==0?" deg":"%"),186,y+16,1);
    }
    footer(colorPhase==2?"Turn to adjust / Press save / Back cancel":"Turn to adjust / Press next / Back cancel"); present(pushToScreen);
}

void DisplayManager::drawAIPet(bool pushToScreen) {
    if(spr.getBuffer() == nullptr) return;

    // พื้นหลังดำสนิท
    spr.fillSprite(C_BG);

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
    uint16_t faceColor = C_TEXT;
    uint16_t blushColor = C_BAR_FG;

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
        spr.fillRect(eyeL_X, eyeL_Y - 2, cur_eyeW, cur_eyelidDrop + 2, C_BG);
        spr.fillRect(eyeR_X, eyeR_Y - 2, cur_eyeW, cur_eyelidDrop + 2, C_BG);
    }

    if (cur_angryBrow > 1.0) {
        // ตาซ้าย (มุมตัดเฉียงลงไปทางขวา \)
        spr.fillTriangle(eyeL_X - 10, eyeL_Y - 10,
                         eyeL_X + cur_eyeW + 10, eyeL_Y - 10,
                         eyeL_X + cur_eyeW + 10, eyeL_Y + cur_angryBrow, C_BG);
        // ตาขวา (มุมตัดเฉียงลงไปทางซ้าย /)
        spr.fillTriangle(eyeR_X - 10, eyeR_Y - 10,
                         eyeR_X + cur_eyeW + 10, eyeR_Y - 10,
                         eyeR_X - 10, eyeR_Y + cur_angryBrow, C_BG);
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
            spr.fillRect(pMouthX - cur_mouthW, pMouthY - cur_mouthH, cur_mouthW*2, cur_mouthH, C_BG);
        }
    }

    footer("AI Pet  /  Hold Back for volume  /  Back");
    if (pushToScreen) {
        if(xSemaphoreTake(displaySemaphore, 0) == pdTRUE) {
            spr.pushSprite(0, 0);
            xSemaphoreGive(displaySemaphore);
        }
    }
}

void DisplayManager::debug() {
    if (!spr.getBuffer()) return;
    hwManager.updateAllStatus();
    pageHeader("Devices", "Live hardware status");
    int count=hwManager.getDeviceCount();
    debugSelectedIndex=preferences::clamp(debugSelectedIndex,0,count?count-1:0);
    if(debugSelectedIndex<debugScrollOffset) debugScrollOffset=debugSelectedIndex;
    if(debugSelectedIndex>=debugScrollOffset+4) debugScrollOffset=debugSelectedIndex-3;
    for(int row=0;row<4 && row+debugScrollOffset<count;++row) {
        int i=row+debugScrollOffset,y=54+row*34;
        auto dev=hwManager.getDevice(i);
        bool selected=i==debugSelectedIndex;
        spr.fillRoundRect(16,y,tft.width()-32,31,8,selected?C_HILITE:C_CARD);
        spr.setTextColor(selected?C_SELECT_TEXT:C_TEXT);spr.setTextDatum(TL_DATUM);
        spr.drawString(dev.name,27,y+4,1);
        spr.drawString(dev.details.substring(0,42),27,y+18,1);
        spr.setTextDatum(TR_DATUM);spr.drawString(hwManager.getStatusString(dev.status),tft.width()-28,y+4,1);
    }
    char memory[64];
    snprintf(memory,sizeof(memory),"RAM free %lu KB  /  PSRAM free %lu KB",
        static_cast<unsigned long>(ESP.getFreeHeap()/1024),static_cast<unsigned long>(ESP.getFreePsram()/1024));
    spr.setTextDatum(MC_DATUM);spr.setTextColor(C_MUTED);spr.drawString(memory,tft.width()/2,207,1);
    footer("Turn for more devices  /  Back to return");present(true);
}

void DisplayManager::recorde() {
    if (!spr.getBuffer()) return;
    const bool active=app::runtime.isRecording;
    pageHeader("Recorder",active?"Recording your moment":"A new file for every recording");
    if(millis()-previousMillis>=1000) { previousMillis=millis(); if(active) ++seconds; }
    spr.fillRoundRect(16,56,tft.width()-32,145,18,C_CARD);
    char time[24]; snprintf(time,sizeof(time),"%02ld:%02ld",seconds/60,seconds%60);
    spr.setTextDatum(MC_DATUM); spr.setTextColor(C_TEXT); spr.drawString(time,tft.width()/2,90,6);
    spr.fillRoundRect(tft.width()/2-57,126,114,38,16,active?C_HILITE:C_BAR_FG);
    spr.setTextColor(active?C_SELECT_TEXT:C_TEXT); spr.drawString(active?"Stop & save":"Record",tft.width()/2,145,2);
    spr.setTextColor(C_MUTED); spr.drawString(getRecordingName(),tft.width()/2,183,1);
    footer("Press to record / stop  /  Back to exit"); present(true);
}
