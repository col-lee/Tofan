// ==========================================
// config.hpp - Centralized Configuration
// ==========================================

#ifndef CONFIG_HPP
#define CONFIG_HPP

// ========== DISPLAY (TFT ST7789) ==========
#define TFT_WIDTH       240
#define TFT_HEIGHT      320
#define TFT_SPI_FREQ    80000000
#define TFT_SCLK        39
#define TFT_MOSI        38
#define TFT_DC          41
#define TFT_CS          40
#define TFT_RST         42

// ========== ENCODER (Rotary) ==========
#define ENC_A_PIN       15
#define ENC_B_PIN       16
#define ENC_SW_PIN      10
#define ENC_VCC_PIN     -1
#define ENC_STEPS       2

// ========== BUTTONS ==========
#define BTN_BACK_PIN    7

// ========== AUDIO (MAX98357A) ==========
#define AUDIO_BCLK      4
#define AUDIO_LRCLK     5
#define AUDIO_DIN       6
#define AUDIO_SAMPLE_RATE 44100

// ========== MICROPHONE (I2S) ==========
#define MIC_I2S_PORT    I2S_NUM_1
#define MIC_WS_PIN      2
#define MIC_SCK_PIN     17
#define MIC_SD_PIN      1
#define MIC_SAMPLE_RATE 16000

// ========== SD CARD ==========
#define SD_MOSI         11
#define SD_MISO         13
#define SD_SCK          12
#define SD_CS           14

// ========== LED INDICATOR ==========
#define LED_PIN         18

// ========== TASK STACK SIZES (bytes) ==========
#define TASK_STACK_AUDIO      (4 * 1024)
#define TASK_STACK_NETWORK    (4 * 1024)
#define TASK_STACK_DISPLAY    (3 * 1024)

// ========== MEMORY LIMITS ==========
#define MAX_PLAYLIST_SIZE     200
#define MAX_IMAGE_SIZE        100
#define MAX_FILE_NAME_LEN     256

// ========== TIMING CONSTANTS ==========
#define ROTARY_DEBOUNCE_MS    20
#define BUTTON_LONG_PRESS_MS  1000
#define VOLUME_HIDE_DELAY_MS  2000
#define UI_REFRESH_RATE_MS    16   // ~60 FPS

#endif
