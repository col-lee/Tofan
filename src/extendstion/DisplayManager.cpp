#include "DisplayManager.hpp"
#include "FileManager.hpp"
#define DISPLAYMANAGER_HH

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite txtDis = TFT_eSprite(&tft);
// AnimatedGIF gif;
DisplayManager DISM;

const char *giphy = "/main/Pictures/giphy2.gif";
const char *fan = "/main/Pictures/fan.gif";
const char *robot = "/main/Pictures/robot240.gif";
const char *mp3File = "/main/Musics/Exil_-_Hiboky.mp3";

static int xOffset = 0;
static int yOffset = 0;

int offsetY_of_scroll = 0;
bool isDisplay_install;

DisplayManager::DisplayManager() {

}

DisplayManager::~DisplayManager() {
  
}

void DisplayManager::autoScroll() {
  
}

void DisplayManager::initDisplay(){
  if(xSemaphoreTake(displaySemaphore ,pdMS_TO_TICKS(500)) == pdTRUE) {
    tft.init();
    tft.setRotation(0);
    tft.setTextSize(2);
    tft.fillScreen(TFT_BLACK);
    isDisplay_install = true;

    // txtDis.setColorDepth(8);
    // txtDis.createSprite(240, 240);
    xSemaphoreGive(displaySemaphore);
  }
}

void DisplayManager::resetDisplay(){
  if (xSemaphoreTake(displaySemaphore, pdTICKS_TO_MS(100)) == pdTRUE)
  {
    tft.fillScreen(TFT_BLACK);
    tft.setCursor(0, 0);
    xSemaphoreGive(displaySemaphore);
  }
}

void println(String &text) {
  tft.println(text.c_str());
}

void println(const String &text) {
  tft.println(text.c_str());
}

void println(char &text) {
  tft.println(text);
}

void println(const char &text) {
  tft.println(text);
}

void println(int &text) {
  tft.println(text);
}

void println(const int &text) {
  tft.println(text);
}

void println(long &text) {
  tft.println(text);
}

void println(const long &text) {
  tft.println(text);
}

void println(float &text) {
  tft.println(text);
}

void println(const float &text) {
  tft.println(text);
}

void println(double &text) {
  tft.println(text);
}

void println(const double &text) {
  tft.println(text);
}



// static void * GIFOpenFile(const char *fname, int32_t *pSize) {
//   if(xSemaphoreTake(sdSemaphore, pdMS_TO_TICKS(500)) == pdTRUE) {
//     File *f = new File(SD.open(fname));
//     if (f && f->available())
//     {
//       *pSize = f->size();
//       xSemaphoreGive(sdSemaphore);
//       return (void *)f;
//     }
//     xSemaphoreGive(sdSemaphore);
//     delete f;
//   }
  
//   return nullptr;
// }

// static void GIFCloseFile(void *pHandle) {
//   File *f = static_cast<File *>(pHandle);
//   if (f)
//   {
//     f->close();
//     delete f;
//   }
// }

// static int32_t GIFReadFile(GIFFILE *pFile, uint8_t *pBuf, int32_t iLen)
// {
//   int32_t iBytesRead;
//   iBytesRead = iLen;
//   File *f = static_cast<File *>(pFile->fHandle);
//   // Note: If you read a file all the way to the last byte, seek() stops working
//   if ((pFile->iSize - pFile->iPos) < iLen)
//     iBytesRead = pFile->iSize - pFile->iPos - 1; // <-- ugly work-around
//   if (iBytesRead <= 0)
//     return 0;
//   iBytesRead = (int32_t)f->read(pBuf, iBytesRead);
//   pFile->iPos = f->position();

//   return iBytesRead;
// }

// static int32_t GIFSeekFile(GIFFILE *pFile, int32_t iPosition)
// {
//   int i = micros();
//   File *f = static_cast<File *>(pFile->fHandle);
//   f->seek(iPosition);
//   pFile->iPos = (int32_t)f->position();
//   i = micros() - i;
//   // log_d("Seek time = %d us\n", i);

//   return pFile->iPos;
// }

// static void TFTDraw(int x, int y, int w, int h, uint16_t* lBuf )
// {
//   if (xSemaphoreTake(displaySemaphore ,pdMS_TO_TICKS(200)) == pdTRUE)
//   {
//     tft.pushRect(x + xOffset, y + yOffset, w, h, lBuf);
//     xSemaphoreGive(displaySemaphore);
//   }
// }

// void GIFDraw(GIFDRAW *pDraw)
// {
//   uint8_t *s;
//   uint16_t *d, *usPalette, usTemp[320];
//   int x, y, iWidth;

//   iWidth = pDraw->iWidth;
//   if (iWidth > TFT_WIDTH)
//       iWidth = TFT_WIDTH;
//   usPalette = pDraw->pPalette;
//   y = pDraw->iY + pDraw->y; // current line

//   s = pDraw->pPixels;
//   if (pDraw->ucDisposalMethod == 2) {// restore to background color
//     for (x=0; x<iWidth; x++) {
//       if (s[x] == pDraw->ucTransparent)
//           s[x] = pDraw->ucBackground;
//     }
//     pDraw->ucHasTransparency = 0;
//   }
//   // Apply the new pixels to the main image
//   if (pDraw->ucHasTransparency) { // if transparency used
//     uint8_t *pEnd, c, ucTransparent = pDraw->ucTransparent;
//     int x, iCount;
//     pEnd = s + iWidth;
//     x = 0;
//     iCount = 0; // count non-transparent pixels
//     while(x < iWidth) {
//       c = ucTransparent-1;
//       d = usTemp;
//       while (c != ucTransparent && s < pEnd) {
//         c = *s++;
//         if (c == ucTransparent) { // done, stop
//           s--; // back up to treat it like transparent
//         } else { // opaque
//             *d++ = usPalette[c];
//             iCount++;
//         }
//       } // while looking for opaque pixels
//       if (iCount) { // any opaque pixels?
//         TFTDraw( pDraw->iX+x, y, iCount, 1, (uint16_t*)usTemp );
//         x += iCount;
//         iCount = 0;
//       }
//       // no, look for a run of transparent pixels
//       c = ucTransparent;
//       while (c == ucTransparent && s < pEnd) {
//         c = *s++;
//         if (c == ucTransparent)
//             iCount++;
//         else
//             s--;
//       }
//       if (iCount) {
//         x += iCount; // skip these
//         iCount = 0;
//       }
//     }
//   } else {
//     s = pDraw->pPixels;
//     // Translate the 8-bit pixels through the RGB565 palette (already byte reversed)
//     for (x=0; x<iWidth; x++)
//       usTemp[x] = usPalette[*s++];
//     TFTDraw( pDraw->iX, y, iWidth, 1, (uint16_t*)usTemp );
//   }
// }

void DisplayManager::createArray(const char *filename) {

  // Open the named file
  // fs::File jpgFile = SD.open( filename, FILE_READ);    // File handle reference for SPIFFS
   File jpgFile = SD.open( filename, FILE_READ);  // or, file handle reference for SD library

  if ( !jpgFile ) {
    Serial.print("ERROR: File \""); Serial.print(filename); Serial.println ("\" not found!");
    return;
  }

  uint8_t data;
  byte line_len = 0;
  Serial.println("");
  Serial.println("// Generated by a JPEGDecoder library example sketch:");
  Serial.println("// https://github.com/Bodmer/JPEGDecoder");
  Serial.println("");
  Serial.println("#if defined(__AVR__)");
  Serial.println("  #include <avr/pgmspace.h>");
  Serial.println("#endif");
  Serial.println("");
  Serial.print  ("const uint8_t ");
  while (*filename != '.') Serial.print(*filename++);
  Serial.println("[] PROGMEM = {"); // PROGMEM added for AVR processors, it is ignored by Due

  while ( jpgFile.available()) {

    data = jpgFile.read();
    Serial.print("0x"); if (abs(data) < 16) Serial.print("0");
    Serial.print(data, HEX); Serial.print(",");// Add value and comma
    line_len++;
    if ( line_len >= 32) {
      line_len = 0;
      Serial.println();
    }

  }

  Serial.println("};\r\n");
  jpgFile.close();
}

void DisplayManager::jpegInfo() {

  Serial.println("===============");
  Serial.println("JPEG image info");
  Serial.println("===============");
  Serial.print  ("Width      :"); Serial.println(JpegDec.width);
  Serial.print  ("Height     :"); Serial.println(JpegDec.height);
  Serial.print  ("Components :"); Serial.println(JpegDec.comps);
  Serial.print  ("MCU / row  :"); Serial.println(JpegDec.MCUSPerRow);
  Serial.print  ("MCU / col  :"); Serial.println(JpegDec.MCUSPerCol);
  Serial.print  ("Scan type  :"); Serial.println(JpegDec.scanType);
  Serial.print  ("MCU width  :"); Serial.println(JpegDec.MCUWidth);
  Serial.print  ("MCU height :"); Serial.println(JpegDec.MCUHeight);
  Serial.println("===============");
  Serial.println("");
}

void DisplayManager::jpegRender(int xpos, int ypos) {

  // retrieve infomration about the image
  uint16_t  *pImg;
  uint16_t mcu_w = JpegDec.MCUWidth;
  uint16_t mcu_h = JpegDec.MCUHeight;
  uint32_t max_x = JpegDec.width;
  uint32_t max_y = JpegDec.height;

  // Jpeg images are draw as a set of image block (tiles) called Minimum Coding Units (MCUs)
  // Typically these MCUs are 16x16 pixel blocks
  // Determine the width and height of the right and bottom edge image blocks
  uint32_t min_w = minimum(mcu_w, max_x % mcu_w);
  uint32_t min_h = minimum(mcu_h, max_y % mcu_h);

  // save the current image block size
  uint32_t win_w = mcu_w;
  uint32_t win_h = mcu_h;

  // record the current time so we can measure how long it takes to draw an image
  uint32_t drawTime = millis();

  // save the coordinate of the right and bottom edges to assist image cropping
  // to the screen size
  max_x += xpos;
  max_y += ypos;

  // read each MCU block until there are no more
#ifdef USE_SPI_BUFFER
  while ( JpegDec.readSwappedBytes()) { // Swap byte order so the SPI buffer can be used
#else
  while ( JpegDec.read()) { // Normal byte order read
#endif
    // save a pointer to the image block
    pImg = JpegDec.pImage;

    // calculate where the image block should be drawn on the screen
    int mcu_x = JpegDec.MCUx * mcu_w + xpos;
    int mcu_y = JpegDec.MCUy * mcu_h + ypos;

    if ( mcu_y < tft.height() )
    {
      // check if the image block size needs to be changed for the right edge
      if (mcu_x + mcu_w <= max_x) win_w = mcu_w;
      else win_w = min_w;

      // check if the image block size needs to be changed for the bottom edge
      if (mcu_y + mcu_h <= max_y) win_h = mcu_h;
      else win_h = min_h;

      // copy pixels into a smaller block
      if (win_w != mcu_w)
      {
        for (int h = 1; h < win_h; h++)
        {
          memcpy(pImg + h * win_w, pImg + h * mcu_w, win_w << 1);
        }
      }
      if(xSemaphoreTake(displaySemaphore, pdMS_TO_TICKS(100)) == pdTRUE) {
        tft.pushImage(mcu_x, mcu_y, win_w, win_h, pImg);
        xSemaphoreGive(displaySemaphore);
      }
    }
    else
    {
      JpegDec.abort();
    }
  }

  // calculate how long it took to draw the image
  drawTime = millis() - drawTime; // Calculate the time it took

  // print the results to the serial port
  Serial.print  ("Total render time was    : "); Serial.print(drawTime); Serial.println(" ms");
  Serial.println("=====================================");

}

void DisplayManager::drawJpeg(const char *filename, int xpos, int ypos) {

  Serial.println("===========================");
  Serial.print("Drawing file: "); Serial.println(filename);
  Serial.println("===========================");

  // Open the named file (the Jpeg decoder library will close it after rendering image)
  // fs::File jpegFile = SD.open(filename, FILE_READ); // File handle reference for SDCard

  // if (!jpegFile)
  // {
  //   Serial.print("ERROR: File \"");
  //   Serial.print(filename);
  //   Serial.println("\" not found!");
  //   return;
  // }
   if(xSemaphoreTake(sdSemaphore, pdMS_TO_TICKS(100)) == pdTRUE) {
     File jpegFile = SD.open(filename, FILE_READ); // or, file handle reference for SD library

     if (!jpegFile)
     {
       Serial.println("open file error.");
       xSemaphoreGive(sdSemaphore);
       return;
     }

     // Use one of the three following methods to initialise the decoder:
     // boolean decoded = JpegDec.decodeFsFile(jpegFile); // Pass a SPIFFS file handle to the decoder,
     boolean decoded = JpegDec.decodeSdFile(jpegFile); // or pass the SD file handle to the decoder,
     // boolean decoded = JpegDec.decodeFsFile(filename);  // or pass the filename (leading / distinguishes SPIFFS files)
     // Note: the filename can be a String or character array type
     if (decoded) {
       // print information about the image to the serial port
      //  jpegInfo();

       // render the image onto the screen at given coordinates
       jpegRender(xpos, ypos);
     } else {
       Serial.println("Jpeg file format not supported!");
     }
     xSemaphoreGive(sdSemaphore);
   }
}

PNG png;             // 🌟 ต้องมีตัวแปรนี้สำหรับเรียกใช้ Library PNGdec
int png_x = 0;       // 🌟 ตำแหน่งแกน X 
int png_y = 0;
File pngFile;
static uint16_t lineBuffer[480];

void * pngOpen(const char *filename, int32_t *size) {
  pngFile = SD.open(filename, FILE_READ);
  if (pngFile) {
    *size = pngFile.size();
    return (void *)&pngFile;
  }
  return NULL;
}

void pngClose(void *handle) {
  if (pngFile) {
      pngFile.close();
  }
}

int32_t pngRead(PNGFILE *page, uint8_t *buffer, int32_t length) {
  if (!pngFile) return 0;
  return pngFile.read(buffer, length);
}

int32_t pngSeek(PNGFILE *page, int32_t position) {
  if (!pngFile) return 0;
  if (pngFile.seek(position)) {
      return position; // ต้องคืนค่า position กลับไปถ้าเลื่อนไฟล์สำเร็จ
  }
  return 0;
}

// Callback สำหรับดันพิกเซล PNG ลงจอ TFT ทีละบรรทัด
int pngDraw(PNGDRAW *pDraw) {
    // 🚨 1. ป้องกัน Buffer Overflow เด็ดขาด (สาเหตุหลักที่ทำให้เครื่องค้าง)
    int drawWidth = pDraw->iWidth;
    if (drawWidth > 480) {
        drawWidth = 480; 
    }

    // แปลงสีเป็น RGB565 (สำหรับจอ TFT)
    png.getLineAsRGB565(pDraw, lineBuffer, PNG_RGB565_LITTLE_ENDIAN, 0xffffffff);
    
    // ดันภาพลงจอ (ตรวจสอบขอบเขตขอบจอด้วย)
    if (png_x + drawWidth <= tft.width() && (png_y + pDraw->y) < tft.height()) {
        tft.pushImage(png_x, png_y + pDraw->y, drawWidth, 1, lineBuffer);
    }
    
    // 🚨 2. ป้องกัน Watchdog Timer รีเซ็ตเครื่อง
    // ยอมให้ Task พัก 1 มิลลิวินาที ทุกๆ การวาด 8 บรรทัด เพื่อให้ CPU หายใจ
    if (pDraw->y % 8 == 0) {
        vTaskDelay(pdMS_TO_TICKS(1)); 
    }
    
    return 1; // คืนค่า 1 เพื่อบอกว่าวาดบรรทัดนี้สำเร็จ
}

// ==========================================
// ฟังก์ชันสั่งวาด PNG
// ==========================================
void DisplayManager::drawPng(const char *filename, int xpos, int ypos) {
    if (xSemaphoreTake(sdSemaphore, pdMS_TO_TICKS(2000)) == pdTRUE) {
        if (xSemaphoreTake(displaySemaphore, pdMS_TO_TICKS(2000)) == pdTRUE) {
            png_x = xpos;
            png_y = ypos;
            
            int rc = png.open(filename, pngOpen, pngClose, pngRead, pngSeek, pngDraw);
            if (rc == PNG_SUCCESS) {
                if (png.getWidth() > 480) { 
                    Serial.println("PNG too wide for buffer!");
                } else {
                    tft.startWrite();
                    // ตรวจสอบการล่มจาก Heap ไม่พอ
                    int decode_rc = png.decode(NULL, 0); 
                    if (decode_rc != PNG_SUCCESS) {
                        Serial.printf("PNG Decode Failed: %d\n", decode_rc);
                    }
                    tft.endWrite();
                }
                
                // 🚨 3. สำคัญมาก: ต้องปิดไฟล์เสมอ! ไม่งั้น File Handle จะค้างจนล่ม
                png.close(); 
            } else {
                Serial.printf("PNG Open Error: %d\n", rc);
                // ปิดไฟล์เผื่อไว้ในกรณีที่ไลบรารีไม่ได้ปิดให้
                if (pngFile) {
                    pngFile.close(); 
                }
            }
            xSemaphoreGive(displaySemaphore);
        }
        xSemaphoreGive(sdSemaphore);
    }
}

void handleDisplay(void *pvParameters) {
  DISPLAY_COMMAND cmd;
  // สร้าง enum สำหรับจัดการสถานะ
  enum class STATE { IDLE, SHOW, CLEAR } state = STATE::IDLE;
  
  // ตัวแปรสำหรับเก็บ Path ล่าสุดที่ส่งมาจาก Queue
  String lastPath = "";

  for (;;) {
    // 1. รับคำสั่งจาก Queue (Timeout 10ms)
    if(xQueueReceive(display_command, &cmd, 10) == pdPASS) {
      if(cmd.module == DISPLAY_COMMAND::MODULE::DIS) {
        
        // สถานะแสดงผลรูปภาพ
        if(cmd.display_state == DISPLAY_COMMAND::DISPLAY_STATE::SHOW) {
          lastPath = cmd.path; // เก็บ path ไว้ใช้ใน switch-case
          state = STATE::SHOW;
          Serial.println("SHOW OK. Path: " + lastPath);
        }

        // สถานะล้างหน้าจอ
        if(cmd.display_state == DISPLAY_COMMAND::DISPLAY_STATE::CLEAR) {
          state = STATE::CLEAR;
          Serial.println("CLEAR OK.");
        }
      }
    }

    // 2. จัดการการแสดงผลตาม State ที่ได้รับ
    switch (state) {
    case STATE::SHOW:
      if (lastPath != "") {
        String pathLower = lastPath;
        DISM.resetDisplay();
        // แยกการวาดตามนามสกุลไฟล์
        if (pathLower.endsWith(".jpg") || pathLower.endsWith(".jpeg")) {
            DISM.drawJpeg(lastPath.c_str(), 0, 40);
        } 
        else if (pathLower.endsWith(".png")) {
            // DISM.drawPng(lastPath.c_str(), 0, 40);
        }
      }
      
      state = STATE::IDLE; // ทำเสร็จแล้วกลับไปสถานะรอ
      vTaskDelay(pdMS_TO_TICKS(10));
      break;

    case STATE::CLEAR:
      // ใช้ฟังก์ชันที่มีอยู่ใน DisplayManager
      DISM.resetDisplay(); 
      
      state = STATE::IDLE; // ทำเสร็จแล้วกลับไปสถานะรอ
      vTaskDelay(pdMS_TO_TICKS(10));
      break;
    
    default:
      // สถานะ IDLE รอรับคำสั่งใหม่
      vTaskDelay(pdMS_TO_TICKS(50));
      break;
    } 
  }

  vTaskDelay(pdMS_TO_TICKS(1));
}

