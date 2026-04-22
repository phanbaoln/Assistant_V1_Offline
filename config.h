#ifndef CONFIG_H
#define CONFIG_H
struct Particle { 
  float x, y, speed; 
  uint16_t color; 
  int size; 
};
// --- Chân màn hình TFT ---
#define TFT_MOSI  11
#define TFT_SCLK  12
#define TFT_CS    10
#define TFT_DC     9
#define TFT_RST    8
#define TFT_BL     7
// ===== WIFI =====
static const char* ssid = "Alpenliebe";
static const char* password = "31082010";
// ===== API KEYS =====
const char* replitApiUrl = "https://8323aa70-12ba-4bc8-84e4-81dc7127a309-00-2ow6iyqz5fdrl.picard.replit.dev/api/chat";
static const char* groq_key = "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";
// ===== MIC I2S (INMP441) =====
#define I2S_WS   4
#define I2S_SCK  5
#define I2S_SD   6

#endif
