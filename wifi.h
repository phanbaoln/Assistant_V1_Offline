#ifndef WIFI_H
#define WIFI_H

#include <WiFi.h>
#include "config.h" 

// Kéo các đối tượng màn hình từ main sang
extern Adafruit_ST7789 tft;
extern U8G2_FOR_ADAFRUIT_GFX u8g2;
extern GFXcanvas16 canvas;

inline void connectWiFi() {
  // 1. Màn hình chờ kết nối WiFi (Vẽ trực tiếp lên TFT)
  tft.fillScreen(ST77XX_BLACK);
  u8g2.begin(tft); 
  u8g2.setFontMode(1);
  u8g2.setFont(u8g2_font_unifont_t_vietnamese1);
  
  u8g2.setForegroundColor(ST77XX_YELLOW);
  String msg = "Đang kết nối WiFi...";
  int w = u8g2.getUTF8Width(msg.c_str());
  u8g2.drawUTF8((320 - w) / 2, 120, msg.c_str()); 

  // 2. Bắt đầu kết nối
  WiFi.begin(ssid, password);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  // 3. Xóa màn hình để báo kết quả
  tft.fillScreen(ST77XX_BLACK);

  // 4. Báo kết quả lên màn hình
  if(WiFi.status() != WL_CONNECTED) {
    u8g2.setForegroundColor(ST77XX_RED);
    msg = "Lỗi WiFi! Đang Reset...";
    w = u8g2.getUTF8Width(msg.c_str());
    u8g2.drawUTF8((320 - w) / 2, 120, msg.c_str());
    delay(3000);
    ESP.restart(); 
  } 
  else {
    u8g2.setForegroundColor(ST77XX_GREEN);
    msg = "Đã kết nối mạng!";
    w = u8g2.getUTF8Width(msg.c_str());
    u8g2.drawUTF8((320 - w) / 2, 120, msg.c_str());
    delay(1500); 
  }
  
  tft.fillScreen(ST77XX_BLACK);
}

#endif
