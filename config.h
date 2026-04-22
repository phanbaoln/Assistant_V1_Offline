#ifndef CONFIG_H
#define CONFIG_H
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
// WIFI
static const char* ssid     = "Alpenliebe";
static const char* password = "31082010";
// API
#define AI_URL "https://8323aa70-12ba-4bc8-84e4-81dc7127a309-00-2ow6iyqz5fdrl.picard.replit.dev/api/chat"
static const char* groq_key = "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";
// MIC I2S — INMP441
// Chân: WS=4, SCK=5, SD=6
#define I2S_MIC_WS    4
#define I2S_MIC_SCK   5
#define I2S_MIC_SD    6
#define I2S_MIC_PORT  I2S_NUM_0   // Port dành riêng cho mic
// SPEAKER I2S — MAX98357A
// Chân: LRC=18, BCLK=17, DIN=15
#define I2S_OUT_LRCK  18   // LRC  → WS
#define I2S_OUT_BCLK  17   // BCLK → BCK
#define I2S_OUT_DIN   15   // DIN  → DATA IN
#define I2S_OUT_PORT  I2S_NUM_1   // Port dành riêng cho speaker

#endif // CONFIG_H
