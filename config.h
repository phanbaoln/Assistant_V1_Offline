#ifndef CONFIG_H
#define CONFIG_H
#include "secrets.h"
struct Particle {
  float    x, y, speed;
  uint16_t color;
  int      size;
};
// CHÂN MÀN HÌNH TFT ST7789
#define TFT_MOSI  11
#define TFT_SCLK  12
#define TFT_CS    10
#define TFT_DC     9
#define TFT_RST    8
#define TFT_BL     7   // Backlight
// MIC I2S — INMP441
// Chân: WS=4, SCK=5, SD=6
#define I2S_MIC_WS    4
#define I2S_MIC_SCK   5
#define I2S_MIC_SD    6
#define I2S_MIC_PORT  I2S_NUM_0   // Port dành riêng cho mic
// SPEAKER I2S — MAX98357A
#define I2S_OUT_LRCK  18   // LRC  → WS
#define I2S_OUT_BCLK  17   // BCLK → BCK
#define I2S_OUT_DIN   15   // DIN  → DATA IN
#define I2S_OUT_PORT  I2S_NUM_1   // Port dành riêng cho speaker

#endif // CONFIG_H
