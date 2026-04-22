#ifndef DISPLAY_H
#define DISPLAY_H

#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <U8g2_for_Adafruit_GFX.h>
#include <WiFi.h>

// --- KHAI BÁO NGOẠI VI & BIẾN EXTERN ---
extern Adafruit_ST7789 tft;
extern GFXcanvas16 canvas; 
extern U8G2_FOR_ADAFRUIT_GFX u8g2;
extern int currentY; 
extern uint16_t* psramChatBG;
extern uint16_t* psramClockBG;
extern volatile bool isProcessingAI; 

extern String currentEmotion;
extern float currentTemp;
extern int weatherCode;
extern int isDay; 
extern String currentLocation;
extern NTPClient timeClient;

extern String clockThemeMode;
extern String chatThemeMode;
extern int currentEnglishLevel;

// 👉 Đã xóa struct Particle ở đây vì main.ino đã định nghĩa rồi
extern Particle particles[];
extern const int MAX_PARTICLES;
void updateParticles(); 

extern String getSoftAdvice(float temp, int code);

#define MSG_USER_BG      tft.color565(0, 132, 255)
#define MSG_AI_BG        tft.color565(240, 240, 240)
#define LINE_HEIGHT      20

inline void initDisplay() {
  pinMode(7, OUTPUT); digitalWrite(7, HIGH); 
  tft.init(240, 320); 
  tft.setRotation(1);
  tft.fillScreen(ST77XX_BLACK); 
  tft.invertDisplay(false);
  u8g2.begin(tft);
  u8g2.setFontMode(1); // Chế độ nền trong suốt (Rất quan trọng!)
  u8g2.setFont(u8g2_font_unifont_t_vietnamese1);
}

// --- 1. HÀM VẼ NỀN ---
inline void renderThemeToCanvas(String mode, uint16_t* psramBuffer) {
  if (mode == "solid_black") {
    // 1. Phông nền đen tuyền sâu thẳm
    canvas.fillScreen(ST77XX_BLACK); 
    for (int i = 0; i < 60; i++) {
        // Thuật toán giả lập vị trí sao ngẫu nhiên nhưng không đổi
        int x = (i * 17) % 320; 
        int y = (i * 23) % 240;
        uint16_t starColor;
        if (i % 3 == 0) starColor = tft.color565(255, 255, 255); // Sao sáng nhất
        else if (i % 3 == 1) starColor = tft.color565(150, 150, 150); // Sao mờ
        else starColor = tft.color565(0, 150, 200); // Sao xanh nhạt
        
        canvas.drawPixel(x, y, starColor);
    }
    
    // 3. Vẽ một "đường chân trời" mờ ảo ở cạnh dưới (tùy chọn)
    canvas.drawFastHLine(0, 239, 320, tft.color565(20, 20, 40));
}
  else if (mode == "solid_blue") canvas.fillScreen(tft.color565(15, 23, 42));
  else if (mode == "pattern_grid") {
    canvas.fillScreen(ST77XX_BLACK);
    for (int i=0; i<320; i+=20) canvas.drawFastVLine(i, 0, 240, tft.color565(40, 60, 80));
    for (int j=0; j<240; j+=20) canvas.drawFastHLine(0, j, 320, tft.color565(40, 60, 80));
  }
  else {
    if (psramBuffer != NULL) canvas.drawRGBBitmap(0, 0, psramBuffer, 320, 240);
    else canvas.fillScreen(ST77XX_BLACK);
  }
}

// --- 2. VẼ THANH TRẠNG THÁI ---
inline void drawStatusBarUI() {
  u8g2.begin(canvas);
  u8g2.setFont(u8g2_font_unifont_t_vietnamese1);
  u8g2.setFontMode(1); 
  
  u8g2.setForegroundColor(ST77XX_BLACK); u8g2.drawUTF8(11, 16, "Phan Bảo"); 
  u8g2.setForegroundColor(tft.color565(200, 100, 255)); u8g2.drawUTF8(10, 15, "Phan Bảo"); 

  int rssi = WiFi.RSSI();
  int bars = (rssi > -55) ? 4 : (rssi > -70) ? 3 : (rssi > -85) ? 2 : 1;
  for(int i=0; i<4; i++) {
    canvas.fillRect(250 + (i*4), 14 - (i*3), 2, 4+(i*3), (bars > i) ? ST77XX_WHITE : tft.color565(80, 80, 80));
  }

  canvas.drawRect(280, 6, 20, 10, ST77XX_WHITE);
  canvas.fillRect(300, 9, 3, 4, ST77XX_WHITE);
  canvas.fillRect(282, 8, 16, 6, ST77XX_GREEN); 
}

// --- 3. MÀN HÌNH ĐỒNG HỒ ---
inline void drawClockScreen() {
  updateParticles(); 
  renderThemeToCanvas(clockThemeMode, psramClockBG);

  for (int i = 0; i < MAX_PARTICLES; i++) {
    canvas.fillCircle((int)particles[i].x, (int)particles[i].y, particles[i].size, particles[i].color);
  }

  drawStatusBarUI();
  u8g2.begin(canvas);

  // A. NGÀY THÁNG
  u8g2.setFont(u8g2_font_unifont_t_vietnamese1);
  u8g2.setFontMode(1); 
  time_t rawtime = timeClient.getEpochTime();
  struct tm * ti = localtime(&rawtime);
  char dateBuf[25];
  const char* days[] = {"Chủ Nhật", "Thứ Hai", "Thứ Ba", "Thứ Tư", "Thứ Năm", "Thứ Sáu", "Thứ Bảy"};
  sprintf(dateBuf, "%s, %02d/%02d", days[ti->tm_wday], ti->tm_mday, ti->tm_mon + 1);
  u8g2.setForegroundColor(ST77XX_BLACK); u8g2.drawUTF8(62, 52, dateBuf); 
  u8g2.setForegroundColor(ST77XX_WHITE); u8g2.drawUTF8(60, 50, dateBuf); 

  // B. GIỜ PHÚT
  u8g2.setFont(u8g2_font_7Segments_26x42_mn);
  u8g2.setFontMode(1); 
  String hm = timeClient.getFormattedTime().substring(0, 5);
  u8g2.setForegroundColor(ST77XX_BLACK); u8g2.drawUTF8(37, 117, hm.c_str()); 
  u8g2.setForegroundColor(ST77XX_WHITE); u8g2.drawUTF8(35, 115, hm.c_str()); 

  // C. ICON THỜI GIAN
  u8g2.setFont(u8g2_font_open_iconic_weather_4x_t);
  u8g2.setFontMode(1); 
  int hour = timeClient.getHours();
  int tIcon = 69; uint16_t tCol = ST77XX_YELLOW;
  if (hour >= 5 && hour < 9) { tIcon = 65; tCol = tft.color565(255, 200, 100); }
  else if (hour >= 9 && hour < 15) { tIcon = 69; tCol = ST77XX_YELLOW; }
  else if (hour >= 15 && hour < 18) { tIcon = 68; tCol = tft.color565(255, 140, 0); }
  else if (hour >= 18 && hour < 22) { tIcon = 66; tCol = tft.color565(200, 200, 255); }
  else { tIcon = 66; tCol = tft.color565(100, 100, 255); }
  u8g2.setForegroundColor(ST77XX_BLACK); u8g2.drawGlyph(232, 107, tIcon);
  u8g2.setForegroundColor(tCol); u8g2.drawGlyph(230, 105, tIcon);

  // D. NHIỆT ĐỘ & ICON THỜI TIẾT
  u8g2.setFont(u8g2_font_helvB14_tf);
  u8g2.setFontMode(1); 
  String tempStr = String(currentTemp, 1) + "°C";
  u8g2.setForegroundColor(ST77XX_BLACK); u8g2.drawUTF8(12, 167, tempStr.c_str()); 
  u8g2.setForegroundColor(ST77XX_WHITE); u8g2.drawUTF8(10, 165, tempStr.c_str()); 

  u8g2.setFont(u8g2_font_open_iconic_weather_2x_t);
  u8g2.setFontMode(1); 
  int wIco = 69; uint16_t wCol = ST77XX_YELLOW;
  if (weatherCode == 0) { wIco = 69; wCol = ST77XX_YELLOW; }
  else if (weatherCode >= 51 && weatherCode <= 67) { wIco = 67; wCol = tft.color565(100, 200, 255); }
  else { wIco = 65; wCol = ST77XX_WHITE; }
  u8g2.setForegroundColor(ST77XX_BLACK); u8g2.drawGlyph(82, 167, wIco);
  u8g2.setForegroundColor(wCol); u8g2.drawGlyph(80, 165, wIco);

  // E. VỊ TRÍ & LỜI KHUYÊN
  u8g2.setFont(u8g2_font_unifont_t_vietnamese1);
  u8g2.setFontMode(1); 
  u8g2.setForegroundColor(ST77XX_BLACK); u8g2.drawUTF8(116, 166, currentLocation.c_str());
  u8g2.setForegroundColor(tft.color565(150, 200, 255)); u8g2.drawUTF8(115, 165, currentLocation.c_str());

  String advice = getSoftAdvice(currentTemp, weatherCode);
  u8g2.setForegroundColor(tft.color565(255, 255, 150)); u8g2.drawUTF8(10, 195, advice.c_str());

  // F. LEVEL TIẾNG ANH
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.setFontMode(1); 
  String lvlStr = "LV: " + String(currentEnglishLevel);
  u8g2.setForegroundColor(ST77XX_CYAN); u8g2.drawUTF8(270, 235, lvlStr.c_str());
}

// --- 4. LOGIC CHAT ---
inline void drawChatBackground(String emotion = "binh_thuong") {
  if (emotion == "gian") tft.fillScreen(0x7800);
  else if (emotion == "buon") tft.fillScreen(0x000F);
  else if (emotion == "vui") tft.fillScreen(0xFCB2);
  else if (emotion == "ngac_nhien") tft.fillScreen(0xFFE0);
  else if (emotion == "yeu_thuong") tft.fillScreen(tft.color565(255, 105, 180));
  else if (emotion == "lo_lang") tft.fillScreen(tft.color565(255, 140, 0));
  else if (emotion == "tinh_nghich") tft.fillScreen(tft.color565(0, 255, 255));
  else if (emotion == "nghi_ngo") tft.fillScreen(tft.color565(128, 0, 128));
  else {
    if (psramChatBG != NULL) tft.drawRGBBitmap(0, 0, psramChatBG, 320, 240);
    else tft.fillScreen(ST77XX_BLACK);
  }
}

inline void drawBubble(String text, bool isUser) {
  u8g2.begin(tft); 
  u8g2.setFont(u8g2_font_unifont_t_vietnamese1); 
  u8g2.setFontMode(1); 
  
  const int MAX_WIDTH = 250; 
  String lines[15]; 
  int lineCount = 0, startPos = 0;

  while (startPos < text.length() && lineCount < 15) {
    int sp = text.indexOf(' ', startPos); 
    if (sp == -1) sp = text.length();
    String word = text.substring(startPos, sp);
    String test = (lines[lineCount].length() > 0) ? lines[lineCount] + " " + word : word;
    if (u8g2.getUTF8Width(test.c_str()) > MAX_WIDTH - 20) { 
        lineCount++; 
        lines[lineCount] = word; 
    }
    else lines[lineCount] = test;
    startPos = sp + 1;
  }
  if (lines[lineCount].length() > 0) lineCount++;

  int bW = (lineCount > 1) ? MAX_WIDTH : (u8g2.getUTF8Width(lines[0].c_str()) + 20);
  int bH = (lineCount * LINE_HEIGHT) + 15;
  int xPos = isUser ? (270 - bW) : 10;
  
  if (currentY < 60) currentY = 60;
  if (currentY + bH + 20 > 220) { 
      drawChatBackground(currentEmotion); 
      currentY = 60; 
  }

  uint16_t bubbleColor = isUser ? MSG_USER_BG : MSG_AI_BG;
  if (!isUser) {
      if (currentEmotion == "yeu_thuong") bubbleColor = tft.color565(255, 220, 230);
      else if (currentEmotion == "lo_lang") bubbleColor = tft.color565(255, 230, 200);
  }

  tft.fillRoundRect(xPos, currentY, bW, bH, 8, bubbleColor);
  u8g2.setForegroundColor(isUser ? ST77XX_WHITE : ST77XX_BLACK);
  for (int i = 0; i < lineCount; i++) {
      u8g2.drawUTF8(xPos + 10, currentY + 18 + (i * LINE_HEIGHT), lines[i].c_str());
  }
  currentY += bH + 15;
}

inline void printStatus(String status, uint16_t color) {
  tft.fillRect(0, 220, 320, 20, tft.color565(20, 20, 20)); 
  tft.drawFastHLine(0, 220, 320, tft.color565(80, 80, 80));
  tft.fillCircle(10, 230, 4, color); // Đèn trạng thái
  u8g2.setForegroundColor(color);
  u8g2.setFont(u8g2_font_unifont_t_vietnamese1); 
  u8g2.drawUTF8(25, 235, status.c_str()); 
}

inline void printUserChat(String text) { drawBubble(text, true); }
inline void printAIChat(String text) { drawBubble(text, false); }

#endif
