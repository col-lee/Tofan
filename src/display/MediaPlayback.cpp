// Decodes JPEG/GIF files and services the display command queue.
#include "DisplayManager.hpp"
#include "JpegDimensions.hpp"
#include "VideoPlayback.hpp"
#include "../storage/FileManager.hpp"

AnimatedGIF gif;
File gifFile;
bool isGifPlaying = false;

static int iXOff = 0;
static int iYOff = 0;

// ฟังก์ชันสำหรับแอบอ่านขนาดกว้าง/สูง จาก Header ของไฟล์ JPEG
bool DisplayManager::getJpegSize(const char* filename, uint16_t &width, uint16_t &height) {
    File f = SD.open(filename, FILE_READ);
    if (!f) return false;

    const bool valid = media::readJpegDimensions(f, width, height);
    f.close();
    return valid;
}

void DisplayManager::drawJpeg(const char *filename) {
    // Media renders directly to the panel; the UI sprite has been released.
    if (xSemaphoreTake(sdSemaphore, pdMS_TO_TICKS(1000)) != pdTRUE) return;
    if (xSemaphoreTake(displaySemaphore, pdMS_TO_TICKS(1000)) == pdTRUE) {
        uint16_t width = 0, height = 0;
        getJpegSize(filename, width, height);
        int x = (tft.width() - width) / 2;
        int y = (tft.height() - height) / 2;
        tft.fillScreen(TFT_BLACK);
        if (!tft.drawJpgFile(SD, filename, x < 0 ? 0 : x, y < 0 ? 0 : y)) {
            tft.setTextDatum(MC_DATUM);
            tft.setTextColor(TFT_WHITE);
            tft.drawString("Unable to open image", tft.width()/2, tft.height()/2, 2);
        }
        xSemaphoreGive(displaySemaphore);
    }
    xSemaphoreGive(sdSemaphore);
}

// ==========================================
// Callback Functions สำหรับ AnimatedGIF
// ==========================================
void * GIFOpenFile(const char *fname, int32_t *pSize) {
  if(xSemaphoreTake(sdSemaphore, portMAX_DELAY) == pdTRUE) {
      gifFile = SD.open(fname);
      if (gifFile) {
        *pSize = gifFile.size();
        xSemaphoreGive(sdSemaphore);
        return (void *)&gifFile;
      }
      xSemaphoreGive(sdSemaphore);
  }
  return NULL;
}

void GIFCloseFile(void *pHandle) {
  if(xSemaphoreTake(sdSemaphore, portMAX_DELAY) == pdTRUE) {
      gifFile.close();
      xSemaphoreGive(sdSemaphore);
  }
}

int32_t GIFReadFile(GIFFILE *pFile, uint8_t *pBuf, int32_t iLen) {
    int32_t bytesRead = 0;
    if(xSemaphoreTake(sdSemaphore, portMAX_DELAY) == pdTRUE) {
        // ให้ ESP32 จัดการเรื่อง EOF เองล้วนๆ ไม่ต้องมี Work-around
        bytesRead = gifFile.read(pBuf, iLen);
        pFile->iPos = gifFile.position();
        xSemaphoreGive(sdSemaphore);
    }
    return bytesRead;
}

int32_t GIFSeekFile(GIFFILE *pFile, int32_t iPosition) {
    if(xSemaphoreTake(sdSemaphore, portMAX_DELAY) == pdTRUE) {
        gifFile.seek(iPosition);
        pFile->iPos = gifFile.position();
        xSemaphoreGive(sdSemaphore);
    }
    return pFile->iPos;
}

#define BUFFER_SIZE 256
uint16_t usTemp[1][BUFFER_SIZE];

void GIFDraw(GIFDRAW *pDraw) {
  uint8_t *s;
  uint16_t *d, *usPalette;
  int x, y, iWidth, iCount;

  iWidth = pDraw->iWidth;
  if (iWidth + pDraw->iX > tft.width()) iWidth = tft.width() - pDraw->iX;

  usPalette = pDraw->pPalette;
  y = iYOff + pDraw->iY + pDraw->y; // จัดกึ่งกลางแนวตั้ง

  if (y >= tft.height() || (iXOff + pDraw->iX) >= tft.width() || iWidth < 1) return;

  s = pDraw->pPixels;
  if (pDraw->ucDisposalMethod == 2) {
    for (x = 0; x < iWidth; x++) {
      if (s[x] == pDraw->ucTransparent) s[x] = pDraw->ucBackground;
    }
    pDraw->ucHasTransparency = 0;
  }

  // รอคิวหน้าจอ
  if (xSemaphoreTake(displaySemaphore, portMAX_DELAY) == pdTRUE) {
      // --- กรณีที่ 1: ภาพมีพื้นหลังโปร่งใส ---
      if (pDraw->ucHasTransparency) {
        uint8_t *pEnd, c, ucTransparent = pDraw->ucTransparent;
        pEnd = s + iWidth;
        x = 0;
        iCount = 0;

        while (x < iWidth) {
          c = ucTransparent - 1;
          d = &usTemp[0][0];

          while (c != ucTransparent && s < pEnd && iCount < BUFFER_SIZE) {
            c = *s++;
            if (c == ucTransparent) s--;
            else { *d++ = usPalette[c]; iCount++; }
          }

          if (iCount) {
            // Push this opaque pixel run to the display.
            tft.pushImage(iXOff + pDraw->iX + x, y, iCount, 1, usTemp[0]);
            x += iCount;
            iCount = 0;
          }

          c = ucTransparent;
          while (c == ucTransparent && s < pEnd) {
            c = *s++;
            if (c == ucTransparent) x++; else s--;
          }
        }
      }
      // --- กรณีที่ 2: ภาพทึบปกติ (เขียนโค้ดให้สั้นและไวขึ้นมาก) ---
      else {
        s = pDraw->pPixels;
        int currentX = iXOff + pDraw->iX;

        while (iWidth > 0) {
            int toDraw = (iWidth <= BUFFER_SIZE) ? iWidth : BUFFER_SIZE;
            for (iCount = 0; iCount < toDraw; iCount++) {
                usTemp[0][iCount] = usPalette[*s++];
            }
            // Push the decoded scanline to the display.
            tft.pushImage(currentX, y, toDraw, 1, usTemp[0]);
            currentX += toDraw;
            iWidth -= toDraw;
        }
      }
      xSemaphoreGive(displaySemaphore);
  }
}

// ==========================================

bool DisplayManager::openGif(const char *filename) {
    gif.begin(GIF_PALETTE_RGB565_BE);
    if (gif.open(filename, GIFOpenFile, GIFCloseFile, GIFReadFile, GIFSeekFile, GIFDraw)) {
        Serial.printf("Playing GIF: %s\n", filename);

        // คำนวณจัดกึ่งกลางภาพ
        iXOff = (tft.width() - gif.getCanvasWidth()) / 2;
        if (iXOff < 0) iXOff = 0;

        iYOff = (tft.height() - gif.getCanvasHeight()) / 2;
        if (iYOff < 0) iYOff = 0;

        return true;
    } else {
        Serial.println("Failed to open GIF file.");
        return false;
    }
}

int DisplayManager::playGifFrame() {
    // คืนค่ากลับไปให้ handleDisplay รู้ว่าเล่นจบหรือ Error หรือยัง
    return gif.playFrame(true, NULL);
}

void DisplayManager::stopGif() {
    gif.close();
    Serial.println("GIF Stopped.");
}

// Receive display commands and advance GIF frames while playback is active.
void handleDisplay(void *pvParameters) {
    DISPLAY_COMMAND cmd;
    enum class STATE { IDLE, SHOW, CLEAR, PLAYING_GIF, PLAYING_VIDEO } state = STATE::IDLE;
    String lastPath = "";

    for (;;) {
        // ไม่ Block คิวเวลาเล่น GIF ทำให้เฟรมเรตไม่ตก
        TickType_t waitTime = (state == STATE::PLAYING_GIF || state == STATE::PLAYING_VIDEO) ? pdMS_TO_TICKS(5) : pdMS_TO_TICKS(50);

        if(xQueueReceive(display_command, &cmd, waitTime) == pdPASS) {
            if(cmd.module == DISPLAY_COMMAND::MODULE::DIS) {

                if(cmd.display_state == DISPLAY_COMMAND::DISPLAY_STATE::SHOW) {
                    if (!cmd.path.valid) { Serial.println("Image path too long"); continue; }
                    lastPath = cmd.path.c_str();
                    if(state == STATE::PLAYING_GIF) DISM.stopGif();
                    if(state == STATE::PLAYING_VIDEO) closeVideo();
                    state = STATE::SHOW;
                    Serial.println("SHOW OK. Path: " + lastPath);
                }

                if(cmd.display_state == DISPLAY_COMMAND::DISPLAY_STATE::CLEAR) {
                    if(state == STATE::PLAYING_GIF) DISM.stopGif();
                    if(state == STATE::PLAYING_VIDEO) closeVideo();
                    state = STATE::CLEAR;
                    Serial.println("CLEAR OK.");
                }
            }
        }

        switch (state) {
        case STATE::SHOW:
            if (lastPath != "") {
                String pathLower = lastPath;
                pathLower.toLowerCase();
                DISM.resetDisplay();

                if (pathLower.endsWith(".jpg") || pathLower.endsWith(".jpeg")) {
                    DISM.drawJpeg(lastPath.c_str());
                    state = STATE::IDLE;
                } else if(pathLower.endsWith(".png")) {
                    if (xSemaphoreTake(sdSemaphore,pdMS_TO_TICKS(1000)) == pdTRUE) {
                        if (xSemaphoreTake(displaySemaphore,pdMS_TO_TICKS(1000)) == pdTRUE) {
                            tft.fillScreen(TFT_BLACK);
                            if (!tft.drawPngFile(SD,lastPath.c_str(),tft.width()/2,tft.height()/2,0,0,0,0,1.0f,1.0f,lgfx::datum_t::middle_center)) {
                                tft.setTextColor(TFT_WHITE); tft.drawString("Unable to open PNG",10,20,2);
                            }
                            xSemaphoreGive(displaySemaphore);
                        }
                        xSemaphoreGive(sdSemaphore);
                    }
                    state = STATE::IDLE;
                } else if(pathLower.endsWith(".mjpeg") || pathLower.endsWith(".mjpg")){
                    state=openVideo(lastPath)?STATE::PLAYING_VIDEO:STATE::IDLE;
                } else if(pathLower.endsWith(".gif")){
                    if(DISM.openGif(lastPath.c_str())) {
                        state = STATE::PLAYING_GIF;
                    } else {
                        state = STATE::IDLE;
                    }
                } else {
                     state = STATE::IDLE;
                }
            } else {
                state = STATE::IDLE;
            }
            break;

        case STATE::PLAYING_VIDEO:
            if(!advanceVideo())state=STATE::IDLE;
            vTaskDelay(pdMS_TO_TICKS(1));
            break;

        case STATE::PLAYING_GIF:
        {
            int result = DISM.playGifFrame();

            // ถ้าเล่นจนจบไฟล์แล้ว (<= 0)
            if (result <= 0) {
                DISM.stopGif(); // ปิดไฟล์เก่า

                // พยายามเปิดไฟล์เดิมอีกครั้ง เพื่อเล่นแบบวนลูป
                if (DISM.openGif(lastPath.c_str())) {
                    // เปิดสำเร็จ จะทำงานต่อในลูปหน้า
                } else {
                    // แต่ถ้าไฟล์พัง เปิดไม่ติด ให้กลับไปหน้าจอดำ (IDLE) เพื่อไม่ให้สแปม
                    state = STATE::IDLE;
                }
            }
            vTaskDelay(pdMS_TO_TICKS(1));
            break;
        }

        case STATE::CLEAR:
            DISM.resetDisplay();
            DISM.mediaClearComplete.store(true);
            state = STATE::IDLE;
            vTaskDelay(pdMS_TO_TICKS(10));
            break;

        case STATE::IDLE:
        default:
            vTaskDelay(pdMS_TO_TICKS(100));
            break;
        }
    }
}
