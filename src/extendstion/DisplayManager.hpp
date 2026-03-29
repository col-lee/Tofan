#include "GlobalVar.hpp"
// #include <AnimatedGIF.h>
#include <JPEGDecoder.h>
#include <PNGdec.h>

#define USE_SPI_BUFFER
#define minimum(a,b)     (((a) < (b)) ? (a) : (b)) // Return the minimum of two values a and b

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
void drawPng(const char *filename, int xpos, int ypos);
void resetDisplay();
};

static void * GIFOpenFile(const char *fname, int32_t *pSize);
static void GIFCloseFile(void *pHandle);
// static int32_t GIFReadFile(GIFFILE *pFile, uint8_t *pBuf, int32_t iLen);
// static int32_t GIFSeekFile(GIFFILE *pFile, int32_t iPosition);
// static void TFTDraw(int x, int y, int w, int h, uint16_t* lBuf );
// void GIFDraw(GIFDRAW *pDraw);
void handleDisplay(void *pvParameters);

extern DisplayManager DISM;