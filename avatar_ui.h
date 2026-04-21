#ifndef AVATAR_UI_H
#define AVATAR_UI_H

#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>

// Định nghĩa màu sắc (Đã chỉnh sửa cho màn hình ngược kênh G-B của Bảo)
#define TRUC_CLR_YELLOW  0xF81F // Vàng (Dưới máy Bảo gõ 255, 0, 255)
#define TRUC_CLR_RED     0xF800 // Đỏ
#define TRUC_CLR_BLUE    0x07E0 // Xanh dương (Dưới máy Bảo gõ 0, 255, 0)
#define TRUC_CLR_PINK    0xF810 // Hồng
#define TRUC_CLR_PURPLE  0x801F // Tím
#define TRUC_CLR_CYAN    0x07FF // Xanh lơ (Bình thường)
#define TRUC_SLEEP_CLR   0x3192 // Xanh thép tối

class TrucAvatar {
  private:
    Adafruit_ST7789* tft;
    GFXcanvas16* canvas;
    GFXcanvas16* compositingCanvas;

  public:
    TrucAvatar(Adafruit_ST7789* _tft) {
      tft = _tft;
      canvas = new GFXcanvas16(40, 40);
      compositingCanvas = new GFXcanvas16(40, 40); 
    }

    // [SYS:vui] - Mắt cười nhún nhảy (Sửa lỗi drawArc)
    void drawJoy() {
      canvas->fillScreen(0);
      int jump = sin(millis() / 150.0) * 3;
      // Dùng drawCircleHelper để vẽ cung tròn phía trên (góc 1 và 2)
      canvas->drawCircleHelper(12, 22 + jump, 6, 1, TRUC_CLR_YELLOW); // Mắt trái
      canvas->drawCircleHelper(12, 22 + jump, 6, 2, TRUC_CLR_YELLOW);
      canvas->drawCircleHelper(28, 22 + jump, 6, 1, TRUC_CLR_YELLOW); // Mắt phải
      canvas->drawCircleHelper(28, 22 + jump, 6, 2, TRUC_CLR_YELLOW);
      canvas->fillRoundRect(15, 28 + jump, 10, 4, 2, TRUC_CLR_YELLOW);
    }

    // [SYS:gian] - Mắt xếch dữ dằn
    void drawAngry() {
      canvas->fillScreen(0);
      canvas->drawLine(8, 12, 18, 18, TRUC_CLR_RED); 
      canvas->drawLine(32, 12, 22, 18, TRUC_CLR_RED);
      canvas->fillCircle(12, 18, 3, TRUC_CLR_RED);
      canvas->fillCircle(28, 18, 3, TRUC_CLR_RED);
      canvas->fillRect(16, 28, 8, 2, TRUC_CLR_RED); 
    }

    // [SYS:buon] - Mắt trĩu xuống
    void drawSad() {
      canvas->fillScreen(0);
      canvas->fillRoundRect(8, 22, 10, 3, 1, TRUC_CLR_BLUE);
      canvas->fillRoundRect(22, 22, 10, 3, 1, TRUC_CLR_BLUE);
      canvas->drawFastHLine(16, 30, 8, TRUC_CLR_BLUE); 
    }

    // [SYS:lo_lang] - Mắt vuông chớp nháy
    void drawAnxious() {
      canvas->fillScreen(0);
      if ((millis() / 200) % 2) {
        canvas->drawRect(10, 15, 8, 8, TRUC_CLR_PURPLE);
        canvas->drawRect(22, 15, 8, 8, TRUC_CLR_PURPLE);
      }
      canvas->drawCircle(20, 28, 3, TRUC_CLR_PURPLE);
    }

    // [SYS:yeu_thuong] - Mắt hình tim (hoặc tròn hồng)
    void drawLove() {
      canvas->fillScreen(0);
      int pulse = sin(millis() / 200.0) * 2;
      canvas->fillCircle(12, 18, 5 + pulse, TRUC_CLR_PINK);
      canvas->fillCircle(28, 18, 5 + pulse, TRUC_CLR_PINK);
      canvas->fillTriangle(20, 32, 16, 28, 24, 28, TRUC_CLR_PINK);
    }

    // [SYS:nghi_ngo] - Mặt liếc ngang
    void drawSkeptical() {
      canvas->fillScreen(0);
      canvas->fillRoundRect(5, 18, 12, 3, 1, TRUC_CLR_CYAN); 
      canvas->fillRoundRect(19, 18, 12, 3, 1, TRUC_CLR_CYAN);
      canvas->drawLine(15, 28, 25, 26, TRUC_CLR_CYAN); 
    }

    // Icon Ngáp (Sửa lỗi màu)
    void drawYawn() {
      canvas->fillScreen(0);
      canvas->fillRoundRect(10, 18, 7, 3, 1, TRUC_CLR_CYAN); 
      canvas->fillRoundRect(23, 18, 7, 3, 1, TRUC_CLR_CYAN); 
      int vibrate = random(-1, 2);
      canvas->drawCircle(20 + vibrate, 28, 6, TRUC_CLR_CYAN);
    }

    // --- CÁC HÀM PHỤC VỤ MAIN.INO ---
    void drawSuccess() { drawJoy(); } // Alias cho drawJoy
    
    void drawSpeaking() {
      canvas->fillScreen(0);
      canvas->fillRoundRect(10, 12, 7, 12, 2, TRUC_CLR_CYAN); 
      canvas->fillRoundRect(23, 12, 7, 12, 2, TRUC_CLR_CYAN); 
      if((millis() / 150) % 2) {
        canvas->fillRoundRect(14, 24, 12, 7, 3, TRUC_CLR_CYAN);
      } else {
        canvas->drawFastHLine(16, 26, 8, TRUC_CLR_CYAN); 
      }
    }

    void drawThinking() {
      canvas->fillScreen(0); 
      int offsetX = (sin(millis() / 150.0) * 5); 
      canvas->fillRoundRect(10 + offsetX, 12, 6, 12, 2, TRUC_CLR_CYAN);
      canvas->fillRoundRect(24 + offsetX, 12, 6, 12, 2, TRUC_CLR_CYAN);
      canvas->drawCircle(20 + (offsetX/2), 27, 4, TRUC_CLR_CYAN);
    }

    void drawIdle() {
      canvas->fillScreen(0);
      canvas->fillRoundRect(10, 12, 7, 12, 2, TRUC_CLR_CYAN);
      canvas->fillRoundRect(23, 12, 7, 12, 2, TRUC_CLR_CYAN);
      canvas->drawFastHLine(16, 26, 8, TRUC_CLR_CYAN);
    }

    void drawSleep() {
      canvas->fillScreen(0);
      canvas->fillRoundRect(8, 18, 9, 3, 1, TRUC_SLEEP_CLR);
      canvas->fillRoundRect(23, 18, 9, 3, 1, TRUC_SLEEP_CLR);
      canvas->drawFastHLine(16, 25, 8, TRUC_SLEEP_CLR);
    }

    // --- CÁC HÀM RENDER (GIỮ NGUYÊN) ---
    void renderChat(int x, int y, uint16_t* chatBG) {
      if (x < 20 || x > 300 || y < 20 || y > 220) return; 
      if (chatBG != NULL) {
        for (int i = 0; i < 40; i++) {
          memcpy(compositingCanvas->getBuffer() + (i * 40), 
                 chatBG + ((y - 20 + i) * 320) + (x - 20), 40 * 2);
        }
      }
      uint16_t* animBuf = canvas->getBuffer();
      uint16_t* compBuf = compositingCanvas->getBuffer();
      for (int i = 0; i < 1600; i++) { if (animBuf[i] != 0) compBuf[i] = animBuf[i]; }
      tft->drawRGBBitmap(x - 20, y - 20, compositingCanvas->getBuffer(), 40, 40);
    }

    void renderToCanvas(GFXcanvas16* destCanvas, int x, int y) {
      uint16_t* animBuf = canvas->getBuffer();
      uint16_t* destBuf = destCanvas->getBuffer();
      for (int j = 0; j < 40; j++) {
        for (int i = 0; i < 40; i++) {
          uint16_t color = animBuf[j * 40 + i];
          if (color != 0) {
            int dx = x - 20 + i; int dy = y - 20 + j;
            if (dx >= 0 && dx < 320 && dy >= 0 && dy < 240) destBuf[dy * 320 + dx] = color;
          }
        }
      }
    }
};

#endif
