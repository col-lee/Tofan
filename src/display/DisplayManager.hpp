// Owns UI state, sprite rendering, media lists and diagnostic screens.
#ifndef DISPLAYMANAGER_HH
#define DISPLAYMANAGER_HH

#include "../hardware/DisplayDevice.hpp"
#include "../core/SharedResources.hpp"
#include "../hardware/HardwareManager.hpp"
#include <AnimatedGIF.h>
#include <vector>
#include <atomic>
#include "../core/UserSettings.hpp"

namespace ui {

enum class SettingsPage { Root, Display, Network, Sound, Voice };

enum class PetMood {
    Neutral,
    Happy,
    Curious,
    Sleepy,
    Tired,
    Listening,
    Surprised,
    Playful,
    Shy,
    Thinking,
    Grumpy,
    Dizzy,
    Proud,
    Excited,
    Dancing
};

enum class State {
    BOOT_LOADING,
    HOME_MENU,
    APP_DISPLAY,
    APP_DISPLAY_LIST,
    APP_DISPLAY_CLOSING,
    APP_MUSIC,
    APP_MUSIC_LIST,
    APP_SETTINGS,
    APP_COLOR_PICKER,
    GLOBAL_VOLUME,
    POPUP_NO_MUSIC,
    APP_ONLINE_MUSIC,
    APP_PET,
    DEBUG,
    RECORDE,
    APP_FOODS
};

} // namespace ui

#define USE_SPI_BUFFER
#define minimum(a,b)     (((a) < (b)) ? (a) : (b))

// State ของหน้าจอทั้งหมด
using UI_STATE = ui::State;

class DisplayManager {
private:

    uint16_t C_BG, C_TEXT, C_CARD, C_HILITE, C_BAR_BG, C_BAR_FG, C_MUTED, C_SELECT_TEXT;
    void pageHeader(const String& title, const String& subtitle = "");
    void footer(const String& text);
    void present(bool push);
    void toggle(int x, int y, bool on);
    void mediaList(const char* title, const std::vector<String>& names, int& selected, int& scroll, bool push);

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
    ui::SettingsPage settingsPage = ui::SettingsPage::Root;
    int settingsScroll = 0;
    int colorRole = 0, colorPhase = 0;
    preferences::HSV colorBackup{};
    int settingSelectedIndex = 0; // 0 = Admin Mode, 1 = Wi-Fi
    int boundaries_setting = 2;
    bool isAdminModeOn = false;
    bool isWiFiOn = false;

    // AI Pet state. Its palette is intentionally independent from the UI theme.
    ui::PetMood petMood = ui::PetMood::Neutral;
    char petMessage[36] = "hi~";
    unsigned long petMoodUntil = 0;
    unsigned long lastMoodChange = 0;
    float petVoiceLevel = 0.0f;
    float petSpeechLevel = 0.0f;
    bool petSpeaking = false;
    int foodCategoryIndex = 0;
    uint32_t petInteractionCount = 0;
    int petLookDirection = 0;
    unsigned long petLookUntil = 0;

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
    void applyTheme();
    int settingsCount() const;
    void drawColorPicker(bool pushToScreen = true);
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
    void drawFoods(bool pushToScreen = true);
    void setPetMood(ui::PetMood mood, const char* message = nullptr, unsigned long holdMs = 0);
    bool isPetMoodHeld() const;
    void setPetVoiceLevel(float level);
    void debug();
    void recorde();
private:
    void drawMusicSurface(bool online, String title, int progress, bool playing, bool pushToScreen);
    bool getJpegSize(const char* filename, uint16_t &width, uint16_t &height);
};

void handleDisplay(void* pvParameter);

extern DisplayManager DISM;
#endif
