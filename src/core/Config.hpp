// Board pins and firmware constants; runtime settings are stored by their owning modules.
#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <Arduino.h>
#include <driver/i2s.h>

#define TFT_WIDTH       240
#define TFT_HEIGHT      320
#define TFT_SPI_FREQ    80000000
#define TFT_SCLK        39
#define TFT_MOSI        38
#define TFT_DC          41
#define TFT_CS          40
#define TFT_RST         42

#define ENC_A_PIN 21
#define ENC_B_PIN 16
#define ENC_SW 10
#define BTN_BACK 7
#define ENC_VCC -1
#define ENC_STEPS 4
#define BUTTON_INPUT_MODE INPUT
#define BUTTON_ACTIVE_LEVEL HIGH
#define BUTTON_DEBOUNCE_MS 300

#define AUDIO_BCLK      4
#define AUDIO_LRCLK     5
#define AUDIO_DIN       6
#define AUDIO_SAMPLE_RATE 44100

#define MIC_I2S_PORT    I2S_NUM_1
#define MIC_WS_PIN      2
#define MIC_SCK_PIN     17
#define MIC_SD_PIN      1
#define MIC_SAMPLE_RATE 16000

#define SD_MOSI         11
#define SD_MISO         13
#define SD_SCK          12
#define SD_CS           14

// SD runs much more reliably for continuous audio when it is not limited to
// the old 4 MHz SPI clock. Start at 20 MHz, but keep conservative fallbacks
// for long perfboard wires/cards that cannot sustain the faster clock.
#define SD_SPI_TARGET_HZ   20000000U
#define SD_SPI_FALLBACK_HZ 10000000U
#define SD_SPI_SAFE_HZ      4000000U

#define IP5306_I2C_ADDRESS 0x75
#define IP5306_I2C_SDA     -1
#define IP5306_I2C_SCL     -1

#define LED_PIN         18

#define RGB_LED_PIN         48
#define RGB_LED_COUNT       1
#define RGB_LED_BRIGHTNESS  32
#define RGB_LED_INTERVAL_MS 20

#define TASK_STACK_AUDIO      (16 * 1024)
#define TASK_STACK_NETWORK    (4 * 1024)
#define TASK_STACK_DISPLAY    (4 * 1024)

#define MAX_PLAYLIST_SIZE     200
#define MAX_IMAGE_SIZE        100
#define MAX_FILE_NAME_LEN     256

#define ROTARY_DEBOUNCE_MS    20
#define BUTTON_LONG_PRESS_MS  1000
#define VOLUME_HIDE_DELAY_MS  2000
#define UI_REFRESH_RATE_MS    16

#endif
