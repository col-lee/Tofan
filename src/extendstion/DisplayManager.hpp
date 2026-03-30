#include "GlobalVar.hpp"
#include <freertos/task.h>
// #include <AnimatedGIF.h>
#include <JPEGDecoder.h>
#include <PNGdec.h>

#define USE_SPI_BUFFER
#define minimum(a,b)     (((a) < (b)) ? (a) : (b))

extern const char *giphy;
extern const char *fan;
extern const char *robot;
extern const char *mp3File;

extern int offsetY_of_scroll;

// GIF decoder
// extern AnimatedGIF gif;
extern File gifFile;

extern char GifComment[256];

class DisplayManager {

private:

public:
    DisplayManager();
    ~DisplayManager();
    void initDisplay();
    void println(String &text);
    void println(const String &text);
    void println(char &text);
    void println(const char &text);
    void println(int &text);
    void println(const int &text);
    void println(long &text);
    void println(const long &text);
    void println(float &text);
    void println(const float &text);
    void println(double &text);
    void println(const double &text);

    void autoScroll();
    void createArray(const char *filename);
    void jpegInfo();
    void jpegRender(int xpos, int ypos);
    void drawJpeg(const char *filename, int xpos, int ypos);
    void resetDisplay();
};

// static void * GIFOpenFile(const char *fname, int32_t *pSize);
// static void GIFCloseFile(void *pHandle);
void handleDisplay(void *pvParameters);

extern DisplayManager DISM;