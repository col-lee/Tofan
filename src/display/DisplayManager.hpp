// Owns UI state, sprite rendering, media lists and diagnostic screens.
#ifndef DISPLAYMANAGER_HH
#define DISPLAYMANAGER_HH

#include "../hardware/DisplayDevice.hpp"
#include "../core/SharedResources.hpp"
#include "../hardware/HardwareManager.hpp"
#include <AnimatedGIF.h>
#include <vector>
#include <atomic>

namespace ui {

enum class State {
    BOOT_LOADING,
    HOME_MENU,
    APP_DISPLAY,
    APP_DISPLAY_LIST,
    APP_DISPLAY_CLOSING,
    APP_MUSIC,
    APP_MUSIC_LIST,
    APP_SETTINGS,
    GLOBAL_VOLUME,
    POPUP_NO_MUSIC,
    APP_ONLINE_MUSIC,
    APP_PET,
    DEBUG,
    RECORDE
};

} // namespace ui

#define USE_SPI_BUFFER
#define minimum(a,b)     (((a) < (b)) ? (a) : (b))

// State ของหน้าจอทั้งหมด
using UI_STATE = ui::State;

class DisplayManager {
private:

    const uint16_t C_BG = tft.color565(245, 245, 250);     // ขาวอมเทา
    const uint16_t C_TEXT = tft.color565(40, 40, 45);      // เทาเข้ม
    const uint16_t C_CARD = tft.color565(230, 230, 235);   // เทาอ่อน
    const uint16_t C_HILITE = tft.color565(80, 80, 90);    // สีตอน Hover
    const uint16_t C_BAR_BG = tft.color565(200, 200, 200); // พื้นหลังหลอด
    const uint16_t C_BAR_FG = tft.color565(100, 150, 255); // ฟ้ามินิมอล

public:
    std::atomic<bool> mediaClearComplete{true};
    UI_STATE currentState = UI_STATE::BOOT_LOADING;
    UI_STATE previousState = UI_STATE::HOME_MENU; // ใช้จำหน้าก่อนหน้าตอนเปิด Volume
    float animatedMenuIndex = 0.0;
    bool isAnimatingMenu = false;
    float animatedMenuIndex_target = 0.0;
    int boundaries_home = 2000;
    int totalItems = 6;

    int currentMusicControlIndex = 1;
    int currentMenuIndex = 0; // 0-5 สำหรับเมนูหลัก
    int currentVolLevel = 50; // 0-100
    unsigned long VolLevelHidden = 0;

    int popupSelectedIndex = 0;

    // ตัวแปรสำหรับหน้า Settings
    int settingSelectedIndex = 0; // 0 = Admin Mode, 1 = Wi-Fi
    int boundaries_setting = 2;
    bool isAdminModeOn = false;
    bool isWiFiOn = false;

    float petPulse = 0.0;     // สำหรับทำให้ "หายใจ" (ขยาย/ยุบ)
    float petYOffset = 0.0;   // สำหรับทำให้ "ลอยขึ้นลง"

    int petMood = 0;

    unsigned long lastMoodChange = 0;

    // ตัวแปร Lerp อารมณ์
    float cur_r = 255, cur_g = 255, cur_b = 255;
    float cur_eyeW = 12, cur_eyeH_L = 16, cur_eyeH_R = 16, cur_eyeY = -5; // ปรับขนาดตาให้เป็นสี่เหลี่ยมแนวตั้ง
    float cur_mouthW = 10, cur_mouthH = 4, cur_mouthY = 14, cur_mouthX = 0;

    // Current and target gaze positions for interpolation.
    float cur_gazeX = 0, cur_gazeY = 0;
    float tar_gazeX = 0, tar_gazeY = 0;

    unsigned long nextBlinkTime = 0;
    unsigned long nextGazeTime = 0; // เวลาที่จะเปลี่ยนจุดมองครั้งต่อไป

    // Recording timer variables (used by recorde() function)
    long seconds = 0;
    unsigned long previousMillis = 0;
    const long interval = 1000;

    std::vector<String> playlistNames;
    std::vector<String> playlistPaths;
    int playlistSelectedIndex = 0;
    int playlistScrollOffset = 0;
    int currentPlayingIndex = -1;

    std::vector<String> imageNames;
    std::vector<String> imagePaths;
    int imageSelectedIndex = 0;
    int imageScrollOffset = 0;

    int debugSelectedIndex = 0;
    int debugScrollOffset = 0;

    DisplayManager();
    ~DisplayManager();

    void initDisplay();
    void resetDisplay();
    void createUISprite();
    void deleteUISprite();

    void drawJpeg(const char *filename);

    // หน้าจอต่างๆ
    void drawLoading(int percent, String text);
    void drawHomeMenu(bool pushToScreen = true);
    void drawMusicPlayer(String songName, int progress, bool isPlaying, bool pushToScreen = true);
    void drawVolumeOverlay();

    void loadMusicList();
    void drawMusicList(bool pushToScreen = true);

    // ฟังก์ชันจัดการ Display List
    void loadImageList();
    void drawImageList(bool pushToScreen = true);

    void drawPopupNoMusic(bool pushToScreen = true);
    void drawOnlineMusicPlayer(bool pushToScreen = true);

    // ระบบเล่นภาพ (ยิงตรงลงจอ)
    bool openGif(const char *filename);
    int playGifFrame();
    void stopGif();

    void drawSettings(bool pushToScreen = true);

    void drawAIPet(bool pushToScreen = true);
    void debug();
    void recorde();
private:
    void drawMusicSurface(bool online, String title, int progress, bool playing, bool pushToScreen);
    bool getJpegSize(const char* filename, uint16_t &width, uint16_t &height);
};

void handleDisplay(void* pvParameter);

extern DisplayManager DISM;
#endif
