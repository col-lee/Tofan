#include "GlobalVar.hpp"
#include <JPEGDecoder.h>
#include <AnimatedGIF.h>

#define USE_SPI_BUFFER
#define minimum(a,b)     (((a) < (b)) ? (a) : (b))

class DisplayManager {

private:

public:
    DisplayManager();
    ~DisplayManager();
    void initDisplay();
    void createArray(const char *filename);
    void jpegInfo();
    void jpegRender(int xpos, int ypos);
    void drawJpeg(const char *filename, int xpos, int ypos);
    void resetDisplay();
    
    bool openGif(const char *filename);
    int playGifFrame();
    void stopGif();
};

void handleDisplay(void *pvParameters);

extern DisplayManager DISM;