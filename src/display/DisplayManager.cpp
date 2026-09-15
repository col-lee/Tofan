// Owns UI state, sprite rendering, media lists and diagnostic screens.
#include "DisplayManager.hpp"
#include "../storage/FileManager.hpp"
#include "../audio/SoundManager.hpp"
#include "../core/GlobalState.hpp"
#include <WiFi.h>
#include "../network/Network.hpp"
#include "../network/EspNowManager.hpp"
#include "../food/FoodStore.hpp"
#include "FoodFont.hpp"

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
    const char* labels[]={"Media","Music","Settings","AI Pet","Devices","Recorder","Random foods"};
    const int cw=(tft.width()-44)/2;
    const int start=(currentMenuIndex/6)*6;
    for(int i=start;i<7 && i<start+6;++i) {
        const int slot=i-start;
        int x=16+(slot%2)*(cw+12),y=56+(slot/2)*52;
        const bool selected=i==currentMenuIndex;
        const uint16_t ink=selected?C_SELECT_TEXT:C_TEXT;
        spr.fillRoundRect(x,y,cw,44,12,selected?C_HILITE:C_CARD);
        if(selected) spr.drawRoundRect(x,y,cw,44,12,C_BAR_FG);
        if(i==0) { spr.drawRoundRect(x+12,y+12,20,18,3,ink); spr.fillTriangle(x+14,y+27,x+22,y+19,x+29,y+27,ink); }
        else if(i==1) { spr.fillRect(x+24,y+10,3,22,ink); spr.fillRect(x+26,y+10,8,3,ink); spr.fillEllipse(x+20,y+30,6,4,ink); }
        else if(i==2) { for(int j=0;j<3;++j) spr.drawLine(x+12,y+14+j*8,x+32,y+14+j*8,ink); spr.fillCircle(x+19,y+14,3,ink); spr.fillCircle(x+27,y+22,3,ink); }
        else if(i==3) { spr.drawRoundRect(x+10,y+11,24,23,8,ink); spr.fillCircle(x+17,y+21,2,ink); spr.fillCircle(x+27,y+21,2,ink); }
        else if(i==4) { spr.drawRoundRect(x+12,y+10,20,24,4,ink); spr.fillCircle(x+22,y+29,2,ink); }
        else if(i==5) { spr.fillRoundRect(x+19,y+10,8,17,4,ink); spr.drawRoundRect(x+15,y+15,16,16,7,ink); spr.drawLine(x+23,y+30,x+23,y+35,ink); }
        else {spr.drawRoundRect(x+10,y+10,24,24,5,ink);spr.fillCircle(x+16,y+16,2,ink);spr.fillCircle(x+28,y+28,2,ink);spr.fillCircle(x+22,y+22,2,ink);}
        spr.setTextDatum(ML_DATUM); spr.setTextColor(ink); spr.drawString(labels[i],x+43,y+22,i==6?1:2);
    }
    footer(start?"2/2  Turn to browse / Press to open":"1/2  Turn to browse / Press to open");
    isAnimatingMenu=false; animatedMenuIndex=animatedMenuIndex_target;
    present(pushToScreen);
}

void DisplayManager::drawFoods(bool pushToScreen) {
    if (!spr.getBuffer()) return;
    const auto v=foodStore.view();
    pageHeader("Random foods", "Turn: category / Press: random");
    String category="ทุกหมวดหมู่";
    for(const auto& c:v.categories)if(c.id==v.category)category=c.name;
    spr.fillRoundRect(16,58,288,36,12,C_HILITE);
    spr.setTextDatum(MC_DATUM);spr.setTextColor(C_SELECT_TEXT);spr.setFont(&foodFont);
    spr.drawString(category,160,76);
    spr.fillRoundRect(16,100,288,116,16,C_CARD);
    spr.setTextColor(C_TEXT);
    if(v.result.length()) {
        // Wrap at UTF-8 character boundaries so Thai names never split a byte sequence.
        std::vector<String> lines;
        float size=2;
        do {
            spr.setTextSize(size);lines.clear();String line;
            for(size_t i=0;i<v.result.length();) {
                size_t end=i+1;while(end<v.result.length()&&(static_cast<uint8_t>(v.result[end])&0xc0)==0x80)++end;
                String next=v.result.substring(i,end);
                if(spr.textWidth(line+next)>260){lines.push_back(line);line="";}
                line+=next;i=end;
            }
            lines.push_back(line);
            if(lines.size()<=3 || size<=1)break;
            size-=0.5f;
        }while(true);
        int spacing=static_cast<int>(18*size),y=158-(lines.size()-1)*spacing/2;
        for(const auto& line:lines){spr.drawString(line,160,y);y+=spacing;}
    }else {
        spr.setTextFont(1);
        spr.drawString(v.count?"Press to pick a meal":"No menus in this category",160,144,2);
        spr.drawString("Manage menus on the website",160,177,1);
    }
    spr.setTextSize(1);
    footer(v.spinning?"Picking... / Back: home":String(v.count)+" choices / Press: again");present(pushToScreen);
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
            String lowerName = name;
            lowerName.toLowerCase();
            // Match the codecs supported by the pinned ESP32-audioI2S build.
            if (!isDir && (lowerName.endsWith(".mp3") || lowerName.endsWith(".wav") ||
                           lowerName.endsWith(".aac") || lowerName.endsWith(".m4a") ||
                           lowerName.endsWith(".flac"))) {
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

    for (const char* directory : {"/main/Pictures", "/main/Videos"}) {
    String jsonStr = file_card.getFileListJSON(directory);
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
            if (!isDir && (lowerName.endsWith(".jpg") || lowerName.endsWith(".jpeg") || lowerName.endsWith(".gif") || lowerName.endsWith(".png") || lowerName.endsWith(".mjpeg") || lowerName.endsWith(".mjpg"))) {
                imageNames.push_back(name);
                imagePaths.push_back(String(directory) + "/" + name);
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
}

// ----------------------------------------------------
// วาดหน้าจอ Image List
// ----------------------------------------------------
void DisplayManager::drawImageList(bool pushToScreen) { mediaList("Photos & Video",imageNames,imageSelectedIndex,imageScrollOffset,pushToScreen); }

int DisplayManager::settingsCount() const {
    switch(settingsPage) {
        case ui::SettingsPage::Root:return 4; case ui::SettingsPage::Display:return 7;
        case ui::SettingsPage::Network:return 3; case ui::SettingsPage::Sound:return 4;
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
    const char* network[]={"Wi-Fi","Admin access point","ESP-NOW status"};
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
        bool isToggle=(settingsPage==Page::Network && i<2) || settingsPage==Page::Voice || (settingsPage==Page::Sound && i<2);
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

void DisplayManager::setPetMood(ui::PetMood mood, const char* message, unsigned long holdMs) {
    petMood = mood;
    petMoodUntil = holdMs ? millis() + holdMs : 0;
    lastMoodChange = millis();
    if (message && *message) {
        snprintf(petMessage, sizeof(petMessage), "%s", message);
    } else {
        petMessage[0] = '\0';
    }
}

bool DisplayManager::isPetMoodHeld() const {
    return petMoodUntil != 0 && static_cast<int32_t>(petMoodUntil - millis()) > 0;
}

void DisplayManager::setPetVoiceLevel(float level) {
    if (level < 0.0f) level = 0.0f;
    if (level > 1.0f) level = 1.0f;
    petVoiceLevel = level;
}

void DisplayManager::drawAIPet(bool pushToScreen) {
    if (spr.getBuffer() == nullptr) return;

    // Personality stays visible during listening, speaking and temporary moods.
    const bool custom=userSettings.values.petPersonality==8;
    const unsigned personality=custom?userSettings.customPet.base:userSettings.values.petPersonality;
    const auto look=custom?pet::appearance(userSettings.customPet):pet::appearance(personality);
    const uint16_t PET_BG       = preferences::rgb565(look.background);
    const uint16_t PET_PANEL    = tft.color565(29, 34, 52);
    const uint16_t PET_FACE     = preferences::rgb565(look.face);
    const uint16_t PET_BLUSH    = preferences::rgb565(look.cheeks);
    const uint16_t PET_MINT     = tft.color565(111, 232, 201);
    const uint16_t PET_SKY      = tft.color565(123, 203, 255);
    const uint16_t PET_YELLOW   = tft.color565(255, 220, 116);
    const uint16_t PET_CORAL    = tft.color565(255, 134, 123);
    const uint16_t PET_LAVENDER = tft.color565(190, 166, 255);
    const uint16_t PET_MUTED    = tft.color565(151, 158, 184);

    struct Visual {
        float eyeW = 38, eyeHL = 56, eyeHR = 56, eyeY = -10;
        float mouthW = 34, mouthH = 6, mouthY = 47;
        float eyelid = 0, brow = 0, blush = 1.0f;
        int gazeX = 0, gazeY = 0;
        bool xEyes = false, openMouth = false, sweat = false;
        bool sparkles = false, soundWaves = false, thoughtDots = false, notes = false;
        uint16_t accent = 0;
    } target;
    target.accent = PET_MINT;

    switch (petMood) {
        case ui::PetMood::Happy:
            target.eyeW=42; target.eyeHL=34; target.eyeHR=34; target.eyeY=-8;
            target.mouthW=44; target.mouthH=18; target.mouthY=44; target.openMouth=true;
            target.blush=1.35f; target.accent=PET_YELLOW; target.sparkles=true; break;
        case ui::PetMood::Curious:
            target.eyeW=42; target.eyeHL=60; target.eyeHR=45; target.gazeX=16; target.gazeY=-7;
            target.mouthW=14; target.mouthH=15; target.openMouth=true;
            target.accent=PET_MINT; target.thoughtDots=true; break;
        case ui::PetMood::Sleepy:
            target.eyeW=46; target.eyeHL=9; target.eyeHR=9; target.eyeY=-3;
            target.mouthW=28; target.mouthH=5; target.mouthY=46; target.blush=.65f;
            target.accent=PET_SKY; break;
        case ui::PetMood::Tired:
            target.eyeW=42; target.eyeHL=27; target.eyeHR=23; target.eyeY=-3;
            target.eyelid=8; target.mouthW=34; target.mouthH=4; target.mouthY=48;
            target.blush=.45f; target.sweat=true; target.accent=PET_SKY; break;
        case ui::PetMood::Listening:
            target.eyeW=44; target.eyeHL=54; target.eyeHR=64; target.eyeY=-10;
            target.gazeX=-6; target.mouthW=16; target.mouthH=5;
            target.eyeHL+=petVoiceLevel*8.0f; target.eyeHR+=petVoiceLevel*8.0f;
            target.mouthY=46; target.openMouth=true; target.soundWaves=true;
            target.accent=PET_SKY; break;
        case ui::PetMood::Surprised:
            target.eyeW=50; target.eyeHL=66; target.eyeHR=66; target.eyeY=-12;
            target.mouthW=19; target.mouthH=25; target.mouthY=48; target.openMouth=true;
            target.blush=.7f; target.accent=PET_YELLOW; break;
        case ui::PetMood::Playful:
            target.eyeW=42; target.eyeHL=8; target.eyeHR=46; target.gazeX=10;
            target.mouthW=42; target.mouthH=15; target.openMouth=true;
            target.blush=1.4f; target.accent=PET_MINT; target.sparkles=true; break;
        case ui::PetMood::Shy:
            target.eyeW=33; target.eyeHL=32; target.eyeHR=32; target.eyeY=2; target.gazeY=9;
            target.mouthW=20; target.mouthH=5; target.mouthY=45; target.blush=1.75f;
            target.accent=PET_BLUSH; break;
        case ui::PetMood::Thinking:
            target.eyeW=35; target.eyeHL=43; target.eyeHR=43; target.gazeX=18; target.gazeY=-12;
            target.mouthW=12; target.mouthH=10; target.openMouth=true;
            target.thoughtDots=true; target.accent=PET_LAVENDER; break;
        case ui::PetMood::Grumpy:
            target.eyeW=42; target.eyeHL=34; target.eyeHR=34; target.eyeY=-4;
            target.brow=20; target.mouthW=30; target.mouthH=4; target.blush=.45f;
            target.accent=PET_CORAL; break;
        case ui::PetMood::Dizzy:
            target.xEyes=true; target.eyeW=38; target.eyeHL=38; target.eyeHR=38;
            target.mouthW=18; target.mouthH=18; target.openMouth=true;
            target.accent=PET_LAVENDER; target.sparkles=true; break;
        case ui::PetMood::Proud:
            target.eyeW=44; target.eyeHL=17; target.eyeHR=17; target.eyeY=-5;
            target.mouthW=43; target.mouthH=13; target.openMouth=true;
            target.blush=1.05f; target.accent=PET_YELLOW; break;
        case ui::PetMood::Excited:
            target.eyeW=49; target.eyeHL=61; target.eyeHR=61; target.eyeY=-12;
            target.mouthW=47; target.mouthH=26; target.openMouth=true;
            target.blush=1.5f; target.sparkles=true; target.accent=PET_YELLOW; break;
        case ui::PetMood::Dancing:
            target.eyeW=45; target.eyeHL=9; target.eyeHR=9; target.eyeY=-4;
            target.mouthW=42; target.mouthH=16; target.openMouth=true;
            target.blush=1.2f; target.notes=true; target.accent=PET_LAVENDER; break;
        case ui::PetMood::Neutral:
        default:
            target.accent=PET_MINT; break;
    }

    target.eyeW*=look.eyeWidth;target.eyeHL*=look.eyeHeight;target.eyeHR*=look.eyeHeight;
    target.blush*=look.blush;
    if(personality||custom)target.accent=preferences::rgb565(look.accent);
    if(personality==1)target.sparkles=true;
    const bool cheeky=personality==6 && petMood==ui::PetMood::Neutral && !petSpeaking;
    if(cheeky){target.eyeHL*=.4f;target.mouthW=38;target.mouthH=5;}
    if(personality==7&&!target.xEyes){target.brow=18;if(!petSpeaking){target.mouthW=46;target.mouthH=6;}}
    if (petSpeaking) {
        const float energy=petSpeechLevel<.025f?0.0f:petSpeechLevel;
        target.mouthW=20+energy*28; target.mouthH=4+energy*34;
        target.openMouth=true; target.soundWaves=false;
    }
    const unsigned long now = millis();
    static float curEyeW=38,curEyeHL=56,curEyeHR=56,curEyeY=-10;
    static float curMouthW=34,curMouthH=6,curMouthY=47;
    static float curLid=0,curBrow=0,curBlush=1;
    static float curGazeX=0,curGazeY=0;
    static int randomGazeX=0,randomGazeY=0;
    static unsigned long nextGaze=0,nextBlink=0,blinkUntil=0;

    if (now >= nextGaze) {
        randomGazeX=random(-12,13); randomGazeY=random(-5,6);
        nextGaze=now+random(900,2600);
    }
    if (now >= nextBlink && petMood!=ui::PetMood::Sleepy && !target.xEyes) {
        blinkUntil=now+105;
        nextBlink=now+random(2600,5600);
    }
    const bool blinking=static_cast<int32_t>(blinkUntil-now)>0;
    if (blinking) { target.eyeHL=5; target.eyeHR=5; target.eyelid=0; }

    const float lerp=.22f;
    curEyeW+=(target.eyeW-curEyeW)*lerp; curEyeHL+=(target.eyeHL-curEyeHL)*lerp;
    curEyeHR+=(target.eyeHR-curEyeHR)*lerp; curEyeY+=(target.eyeY-curEyeY)*lerp;
    const float mouthEase=petSpeaking?.6f:lerp;
    curMouthW+=(target.mouthW-curMouthW)*mouthEase; curMouthH+=(target.mouthH-curMouthH)*mouthEase;
    curMouthY+=(target.mouthY-curMouthY)*lerp; curLid+=(target.eyelid-curLid)*lerp;
    curBrow+=(target.brow-curBrow)*lerp; curBlush+=(target.blush-curBlush)*lerp;
    const bool followingWheel = petLookUntil && static_cast<int32_t>(petLookUntil-now)>0;
    const float gazeX = followingWheel ? petLookDirection*18.0f : target.gazeX+randomGazeX;
    curGazeX+=(gazeX-curGazeX)*.18f;
    curGazeY+=((target.gazeY+randomGazeY)-curGazeY)*.18f;

    spr.fillSprite(PET_BG);

    // Status bubble / mumble text.
    if (petMessage[0]) {
        spr.setTextFont(1); spr.setTextSize(1); spr.setTextDatum(MC_DATUM);
        int bubbleW=spr.textWidth(petMessage)+28;
        if(bubbleW<70)bubbleW=70; if(bubbleW>220)bubbleW=220;
        const int bx=(tft.width()-bubbleW)/2;
        spr.fillRoundRect(bx,8,bubbleW,28,12,PET_PANEL);
        spr.drawRoundRect(bx,8,bubbleW,28,12,target.accent);
        spr.setTextColor(PET_FACE); spr.drawString(petMessage,tft.width()/2,22,1);
        spr.fillTriangle(tft.width()/2-5,35,tft.width()/2+5,35,tft.width()/2,42,PET_PANEL);
    }

    float bob=sin(now*(petMood == ui::PetMood::Excited ? .010f : .0042f))*4.0f;
    if(petMood==ui::PetMood::Dancing) bob=sin(now*.016f)*7.0f;
    const int anchorX=tft.width()/2;
    const int anchorY=tft.height()/2+8+static_cast<int>(bob);
    const int spacing=68;
    const int faceX=anchorX+static_cast<int>(curGazeX);
    const int faceY=anchorY+static_cast<int>(curGazeY);

    // Cute cheeks; intensity varies by mood.
    const int blushW=static_cast<int>(18*curBlush), blushH=static_cast<int>(8*curBlush);
    if(blushW>3 && blushH>2){
        spr.fillEllipse(faceX-spacing-31,faceY+24,blushW,blushH,PET_BLUSH);
        spr.fillEllipse(faceX+spacing+31,faceY+24,blushW,blushH,PET_BLUSH);
    }

    const int eyeLX=faceX-spacing-static_cast<int>(curEyeW/2);
    const int eyeRX=faceX+spacing-static_cast<int>(curEyeW/2);
    const int eyeLY=faceY+static_cast<int>(curEyeY-curEyeHL/2);
    const int eyeRY=faceY+static_cast<int>(curEyeY-curEyeHR/2);

    if(target.xEyes){
        const int r=16;
        for(int o=-2;o<=2;++o){
            spr.drawLine(faceX-spacing-r,faceY-12-r+o,faceX-spacing+r,faceY-12+r+o,PET_FACE);
            spr.drawLine(faceX-spacing-r,faceY-12+r+o,faceX-spacing+r,faceY-12-r+o,PET_FACE);
            spr.drawLine(faceX+spacing-r,faceY-12-r+o,faceX+spacing+r,faceY-12+r+o,PET_FACE);
            spr.drawLine(faceX+spacing-r,faceY-12+r+o,faceX+spacing+r,faceY-12-r+o,PET_FACE);
        }
    }else{
        const int radius=look.radius;
        spr.fillRoundRect(eyeLX,eyeLY,static_cast<int>(curEyeW),static_cast<int>(curEyeHL),radius,PET_FACE);
        spr.fillRoundRect(eyeRX,eyeRY,static_cast<int>(curEyeW),static_cast<int>(curEyeHR),radius,PET_FACE);
        if(curLid>1){
            spr.fillRect(eyeLX-2,eyeLY-2,static_cast<int>(curEyeW)+4,static_cast<int>(curLid)+2,PET_BG);
            spr.fillRect(eyeRX-2,eyeRY-2,static_cast<int>(curEyeW)+4,static_cast<int>(curLid)+2,PET_BG);
        }
        if(curBrow>1){
            spr.fillTriangle(eyeLX-8,eyeLY-10,eyeLX+static_cast<int>(curEyeW)+8,eyeLY-10,eyeLX+static_cast<int>(curEyeW)+8,eyeLY+static_cast<int>(curBrow),PET_BG);
            spr.fillTriangle(eyeRX-8,eyeRY-10,eyeRX+static_cast<int>(curEyeW)+8,eyeRY-10,eyeRX-8,eyeRY+static_cast<int>(curBrow),PET_BG);
        }
    }

    if(personality==3 && !target.xEyes){
        // Round spectacles follow the eyes without covering the mouth animation.
        spr.drawRoundRect(eyeLX-7,eyeLY-7,static_cast<int>(curEyeW)+14,static_cast<int>(curEyeHL)+14,14,target.accent);
        spr.drawRoundRect(eyeRX-7,eyeRY-7,static_cast<int>(curEyeW)+14,static_cast<int>(curEyeHR)+14,14,target.accent);
        spr.drawLine(eyeLX+static_cast<int>(curEyeW)+7,faceY-10,eyeRX-7,faceY-10,target.accent);
    }
    if(personality==4){
        spr.fillTriangle(faceX-22,faceY-66,faceX-25,faceY-84,faceX-6,faceY-71,target.accent);
        spr.fillTriangle(faceX-12,faceY-66,faceX,faceY-89,faceX+12,faceY-66,target.accent);
        spr.fillTriangle(faceX+6,faceY-71,faceX+25,faceY-84,faceX+22,faceY-66,target.accent);
        spr.fillRoundRect(faceX-22,faceY-68,44,5,2,target.accent);
    }
    const int mouthX=faceX, mouthY=faceY+static_cast<int>(curMouthY);
    const int mouthW=static_cast<int>(curMouthW), mouthH=static_cast<int>(curMouthH);
    if(target.openMouth && mouthH>8){
        spr.fillEllipse(mouthX,mouthY,mouthW/2,mouthH/2,PET_FACE);
        if(petMood!=ui::PetMood::Surprised && petMood!=ui::PetMood::Listening && petMood!=ui::PetMood::Dizzy){
            spr.fillRect(mouthX-mouthW/2-2,mouthY-mouthH/2-2,mouthW+4,mouthH/2+2,PET_BG);
        }else if(mouthW>10 && mouthH>12){
            spr.fillEllipse(mouthX,mouthY,mouthW/4,mouthH/4,PET_BG);
        }
    }else{
        spr.fillRoundRect(mouthX-mouthW/2,mouthY-mouthH/2,mouthW,mouthH,3,PET_FACE);
    }

    // Mood decorations.
    if(cheeky||personality==7){
        spr.drawLine(mouthX+mouthW/2-2,mouthY,mouthX+mouthW/2+7,mouthY-7,PET_FACE);
        spr.drawLine(mouthX+mouthW/2-2,mouthY+1,mouthX+mouthW/2+7,mouthY-6,PET_FACE);
    }
    if(personality==7){
        // Slashed eyebrow marks and pointed horns reinforce the fierce expression.
        spr.fillTriangle(faceX-103,faceY-47,faceX-112,faceY-78,faceX-82,faceY-58,target.accent);
        spr.fillTriangle(faceX+103,faceY-47,faceX+112,faceY-78,faceX+82,faceY-58,target.accent);
        for(int offset=0;offset<3;++offset){
            spr.drawLine(eyeLX-4,eyeLY-10-offset,eyeLX+static_cast<int>(curEyeW),eyeLY+2-offset,target.accent);
            spr.drawLine(eyeRX,eyeRY+2-offset,eyeRX+static_cast<int>(curEyeW)+4,eyeRY-10-offset,target.accent);
        }
    }
    if(target.sweat){
        spr.fillTriangle(faceX+spacing+37,faceY-43,faceX+spacing+29,faceY-27,faceX+spacing+44,faceY-27,PET_SKY);
        spr.fillCircle(faceX+spacing+36,faceY-26,7,PET_SKY);
    }
    if(target.sparkles){
        spr.fillCircle(faceX-spacing-50,faceY-42,3,target.accent);
        spr.drawLine(faceX-spacing-56,faceY-42,faceX-spacing-44,faceY-42,target.accent);
        spr.drawLine(faceX-spacing-50,faceY-48,faceX-spacing-50,faceY-36,target.accent);
        spr.fillCircle(faceX+spacing+48,faceY-34,2,target.accent);
    }
    if(target.soundWaves){
        const int wave=4+static_cast<int>(petVoiceLevel*9.0f);
        spr.drawCircle(faceX+spacing+40,faceY-8,wave,target.accent);
        spr.drawCircle(faceX+spacing+40,faceY-8,wave+7,target.accent);
    }
    if(target.thoughtDots){
        spr.fillCircle(faceX+spacing+40,faceY-48,3,target.accent);
        spr.fillCircle(faceX+spacing+51,faceY-58,4,target.accent);
        spr.fillCircle(faceX+spacing+64,faceY-69,5,target.accent);
    }
    if(target.notes){
        spr.fillCircle(faceX-spacing-51,faceY-31,4,target.accent); spr.fillRect(faceX-spacing-47,faceY-47,3,17,target.accent);
        spr.fillCircle(faceX+spacing+51,faceY-39,4,target.accent); spr.fillRect(faceX+spacing+55,faceY-56,3,18,target.accent);
    }

    // Small fixed-color footer. Do not call footer(), because footer() uses UI theme colors.
    spr.setTextDatum(MC_DATUM); spr.setTextFont(1); spr.setTextSize(1); spr.setTextColor(PET_MUTED);
    spr.drawString("Roll: play / Press: pet / Speak / Back",tft.width()/2,tft.height()-10,1);

    if(pushToScreen && xSemaphoreTake(displaySemaphore,0)==pdTRUE){
        spr.pushSprite(0,0);
        xSemaphoreGive(displaySemaphore);
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

void DisplayManager::drawEspNow(bool pushToScreen) {
    if(!spr.getBuffer())return;
    auto s=espnow::summary();pageHeader("ESP-NOW",s.ready?"Radio ready":s.enabled?"Starting radio...":"Disabled");
    spr.setTextDatum(TL_DATUM);spr.setTextColor(C_TEXT);
    spr.drawString(String("Peers online: ")+String(s.online)+" / "+String(s.count),20,62,4);
    spr.setTextColor(C_MUTED);spr.drawString(String("Wi-Fi channel: ")+String(s.channel),20,101,2);
    spr.drawString(String("TX ")+String(s.tx)+"    RX "+String(s.rx),20,130,2);
    spr.drawString(String("Send failures: ")+String(s.failed),20,157,2);
    spr.drawString("Manage peers on the signed-in web page",20,185,1);
    footer("Settings > Network   /   Back to return");present(pushToScreen);
}
