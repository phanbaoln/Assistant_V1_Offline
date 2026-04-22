#include "config.h"
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <U8g2_for_Adafruit_GFX.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <ArduinoJson.h>
#include "avatar_ui.h"

// --- KHỞI TẠO PHẦN CỨNG ---
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);
GFXcanvas16 canvas(320, 240);
U8G2_FOR_ADAFRUIT_GFX u8g2;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "vn.pool.ntp.org", 25200, 60000);
TrucAvatar avatar(&tft);

// --- BIẾN TOÀN CỤC ---
int currentY = 10;
int16_t *audioBuffer = NULL;
size_t audioIndex = 0;
volatile bool isChattingMode = false;
volatile bool isAlarmActive = false;
volatile unsigned long lastNTPUpdate = 0;
volatile bool isProcessingAI = false;
volatile bool hasNewAnswer = false;
String userTranscript = "";
String aiAnswer = "";

volatile bool hasWebMessage = false;
String webMessageText = "";
unsigned long lastWebCheck = 0;

String clockThemeMode = "default"; 
String chatThemeMode = "default";
unsigned long lastClockUpdate = 0;
unsigned long lastSoundTime = 0;
unsigned long ignoreMicUntil = 0;
bool isWaitingForTrigger = false;
unsigned long clockStartTime = 0; 

// Biến quản lý trạng thái thức dậy
bool isWakingUp = false;         
unsigned long wakeUpStartTime = 0; 
bool isDisplayOff = false; 

const int CHAT_THRESHOLD = 5000; 
const int NEXT_STEP_THRESHOLD = 3000;
const int ALARM_OFF_THRESHOLD = 10000;

int currentEnglishLevel = 0;
float currentTemp = 28.5;
String currentAlarmNote = "";
String currentLocation = "Thu Duc, Ho Chi Minh";
int weatherCode = 0;
int isDay = 1;
String currentEmotion = "binh_thuong";

uint16_t* psramChatBG = NULL;
uint16_t* psramClockBG = NULL;

Particle particles[25];           
const int MAX_PARTICLES = 25;      
bool particlesInitialized = false; 

#include "display.h"   
#include "schedule.h"  
#include "weather.h"   
#include "wifi.h"
#include "MICI2S.h"
#include "ai.h"        

TaskHandle_t TaskCore0;

// --- HÀM HIỆU ỨNG PHẦN CỨNG ---
void updateHardwareEffects() {
  if (isDisplayOff) return;
  
  if (isWakingUp) {
    // Hiệu ứng đèn nền sáng dần khi tỉnh dậy
    int fade = map(millis() - wakeUpStartTime, 0, 5000, 100, 255);
    analogWrite(TFT_BL, constrain(fade, 100, 255));
  }
  else if (currentEmotion == "gian") {
    // 🔴 [GIAN]: Nháy gắt liên tục (Strobe)
    digitalWrite(TFT_BL, (millis() / 50) % 2); 
  } 
  else if (currentEmotion == "vui") {
    // 🟡 [VUI]: Nhịp thở chậm (Breathing)
    float val = (exp(sin(millis() / 2000.0 * PI)) - 0.36787944) * 108.0;
    analogWrite(TFT_BL, map(val, 0, 255, 130, 255));
  } 
  // 👉 ĐÃ THÊM LOGIC CHO LO LẮNG VÀO ĐÂY:
  else if (currentEmotion == "lo_lang") {
    // 🟣 [LO LẮNG]: Nháy loạn (Random Flickering)
    // Cứ mỗi vòng lặp, có 30% cơ hội thay đổi độ sáng đột ngột
    if (random(10) > 7) { 
        analogWrite(TFT_BL, random(40, 255)); 
    }
  }
  // 🟢 [NGHI NGỜ]: Sáng mượt dần rồi tối dần (Fade In-Out)
  else if (currentEmotion == "nghi_ngo") {
    int brightness = (millis() / 10) % 510;
    if (brightness > 255) brightness = 510 - brightness;
    analogWrite(TFT_BL, map(brightness, 0, 255, 60, 255));
  }
  else {
    // Bình thường: Sáng tĩnh
    digitalWrite(TFT_BL, HIGH);
  }
}

void networkTask(void * pvParameters) {
  for(;;) {
    if (!isProcessingAI) {
      if (!isChattingMode && (millis() - lastNTPUpdate > 900000 || lastNTPUpdate == 0)) {
        if (WiFi.status() == WL_CONNECTED) { timeClient.update(); lastNTPUpdate = millis(); }
      }
      if (millis() - lastWebCheck > 2000) {
        String msg = checkWebChat(); 
        if (msg != "" && msg.indexOf("SYSTEM:") == -1) { webMessageText = msg; hasWebMessage = true; }
        lastWebCheck = millis();
      }
      vTaskDelay(100 / portTICK_PERIOD_MS); 
    } else {
      userTranscript = speechToText();
      aiAnswer = (userTranscript.length() > 1) ? askAI(userTranscript) : "Trúc chưa nghe rõ ý Bảo.";
      isProcessingAI = false;
      hasNewAnswer = true;
      vTaskDelay(10 / portTICK_PERIOD_MS);
    }
  }
}

void setup() {
  Serial.begin(115200);
  initDisplay(); 
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
  tft.setSPISpeed(80000000);
  
  if (psramInit()) {
    audioBuffer = (int16_t *)ps_malloc(BUFFER_SIZE * sizeof(int16_t));
    psramChatBG = (uint16_t*) ps_malloc(320 * 240 * 2);
    psramClockBG = (uint16_t*) ps_malloc(320 * 240 * 2);
    if (psramChatBG != NULL) memset(psramChatBG, 0, 320 * 240 * 2);
    if (psramClockBG != NULL) memset(psramClockBG, 0, 320 * 240 * 2);
  }

  initMic();
  connectWiFi();
  timeClient.begin();
  updateWeather();
  initSchedule();
  
  tft.fillScreen(ST77XX_BLACK);
  clockStartTime = millis(); 
  xTaskCreatePinnedToCore(networkTask, "TaskCore0", 20000, NULL, 1, &TaskCore0, 0); 
}

void startChatSession() {
  isChattingMode = true;
  clockStartTime = millis(); 
  drawChatBackground(currentEmotion); 
  currentY = 40;
  printAIChat("Trúc đây! Bảo cần gì?");
  printStatus("SẴN SÀNG: Bảo hãy gọi Trúc...", ST77XX_CYAN);
  lastSoundTime = millis();
}

void loop() {
  unsigned long loopStart = millis(); 
  int32_t amp = getPeakAmplitude();
  static unsigned long successTimer = 0;

  // Cập nhật hiệu ứng đèn nền theo cảm xúc (Nháy loạn khi lo lắng, thở khi vui...)
  updateHardwareEffects();

  // --- 0. ƯU TIÊN CAO NHẤT: XỬ LÝ TIN NHẮN & LỆNH TỪ WEB ---
  if (hasWebMessage) {
    if (isDisplayOff) { 
        isDisplayOff = false; 
        digitalWrite(TFT_BL, HIGH); 
        tft.fillScreen(ST77XX_BLACK);
    }
    
    isWakingUp = false;        // Hủy trạng thái chờ 5s nếu đang thức dậy dở dang
    clockStartTime = millis(); 

    if (webMessageText.indexOf("SYSTEM:") != -1) {
        // Nếu là lệnh hệ thống (Đổi level, đổi theme), chỉ hiện trên thanh status
        printStatus(webMessageText, ST77XX_MAGENTA);
    } 
    else {
        // Nếu là tin nhắn chat bình thường từ giao diện Web
        if (!isChattingMode) {
            startChatSession(); 
        } else {
            lastSoundTime = millis(); 
        }
        printUserChat("Bảo nhắn từ Web...");
        printAIChat(webMessageText);
    }
    hasWebMessage = false; 
  }

  // --- 1. XỬ LÝ KÍCH HOẠT VÀ THỨC DẬY (DO TIẾNG ĐỘNG) ---
  if (amp > CHAT_THRESHOLD && !isChattingMode && !isAlarmActive && !isWakingUp) {
    if (isDisplayOff) { 
        isDisplayOff = false; 
        digitalWrite(TFT_BL, HIGH); 
        tft.fillScreen(ST77XX_BLACK);
    }
    isWakingUp = true;
    wakeUpStartTime = millis();
    lastClockUpdate = 0; 
  }

  // Chờ đủ 5 giây để tỉnh hẳn rồi mới vào Chat (Tránh kích hoạt nhầm)
  if (isWakingUp && (millis() - wakeUpStartTime > 5000)) {
      isWakingUp = false;
      if (!isChattingMode) {
          startChatSession();               
          isWaitingForTrigger = true;      
          ignoreMicUntil = millis() + 800; 
      }
  }

  // --- 2. BÁO THỨC (ĐÃ FIX FONT TIẾNG VIỆT) ---
  if (isAlarmActive) {
    if (isDisplayOff) { isDisplayOff = false; digitalWrite(TFT_BL, HIGH); }
    
    static unsigned long lastRedraw = 0;
    if (millis() - lastRedraw > 500) {
      static bool toggle = false;
      toggle = !toggle;
      tft.fillScreen(toggle ? ST77XX_RED : ST77XX_BLACK);
      
      u8g2.setFontMode(1); 
      u8g2.setFont(u8g2_font_unifont_t_vietnamese1); 
      u8g2.setForegroundColor(ST77XX_WHITE);
      u8g2.drawUTF8(40, 100, "BÁO THỨC ĐANG REO!"); 
      
      if (currentAlarmNote != "") u8g2.drawUTF8(40, 140, currentAlarmNote.c_str());
      else u8g2.drawUTF8(40, 140, "Đến giờ rồi Bảo ơi!");
      
      lastRedraw = millis();
    }
    // Tắt báo thức bằng tiếng động lớn (Hét to hoặc vỗ tay)
    if (amp > ALARM_OFF_THRESHOLD) {
      isAlarmActive = false;
      tft.fillScreen(ST77XX_BLACK);
      clockStartTime = millis();
      lastClockUpdate = 0;
    }
    return; // Khi reo báo thức thì không làm việc khác
  }

  // --- 3. CHẾ ĐỘ CHAT ---
  if (isChattingMode) {
    if (isProcessingAI) {
      avatar.drawThinking();
      printStatus("ĐANG NGHĨ: Đợi Trúc xíu...", tft.color565(200, 100, 255));
    }
    else if (successTimer > 0 && millis() < successTimer) avatar.drawSuccess();
    else if (hasNewAnswer) avatar.drawSpeaking();
    else {
      // Hiển thị biểu cảm theo trạng thái AI gửi về
      if (currentEmotion == "vui") avatar.drawJoy();
      else if (currentEmotion == "gian") avatar.drawAngry();
      else if (currentEmotion == "buon") avatar.drawSad();
      else if (currentEmotion == "lo_lang") avatar.drawAnxious();
      else if (currentEmotion == "yeu_thuong") avatar.drawLove();
      else if (currentEmotion == "nghi_ngo") avatar.drawSkeptical();
      else avatar.drawIdle();
    }
    
    int shake = (currentEmotion == "gian") ? random(-5, 5) : 0;
    avatar.renderChat(290 + shake, 25 + shake, psramChatBG); 

    if (hasNewAnswer && millis() > successTimer) {
        if (successTimer == 0) { 
            successTimer = millis() + 1200; 
            printStatus("ĐANG TRẢ LỜI...", ST77XX_GREEN); 
        } else {
            successTimer = 0;
            printUserChat(userTranscript);
            if (aiAnswer.indexOf("stop_alarm") >= 0) isAlarmActive = false;
            else printAIChat(aiAnswer);
            hasNewAnswer = false;
            isWaitingForTrigger = true;
            ignoreMicUntil = millis() + 1500;
            lastSoundTime = millis();
            printStatus("XONG! Bảo gọi Trúc tiếp đi...", ST77XX_CYAN);
        }
    } else if (!isProcessingAI) {
      // Tự động thoát chat sau 30s im lặng
      if (millis() - lastSoundTime > 30000) { 
        isChattingMode = false;
        tft.fillScreen(ST77XX_BLACK);
        clockStartTime = millis();
        lastClockUpdate = 0; 
      } else if (isWaitingForTrigger && millis() > ignoreMicUntil && amp > NEXT_STEP_THRESHOLD) {
        isWaitingForTrigger = false;
        printStatus("ĐANG NGHE: Bảo nói đi...", ST77XX_YELLOW);
        recordAudio();
        isProcessingAI = true;
      }
    }
  }
  
  // --- 4. MÀN HÌNH CHỜ & LỊCH TRÌNH THÔNG MINH ---
  if (!isChattingMode && !isAlarmActive) {
    // 👉 CẬP NHẬT: Kiểm tra lịch có tính đến thứ trong tuần (Bitmask)
    checkSchedule(timeClient.getHours(), timeClient.getMinutes(), timeClient.getDay());
    
    unsigned long elapsedClock = millis() - (isWakingUp ? wakeUpStartTime : clockStartTime);

    if (!isDisplayOff) {
      if (elapsedClock > 35000 && !isWakingUp) { 
        isDisplayOff = true;
        digitalWrite(TFT_BL, LOW); 
        tft.fillScreen(ST77XX_BLACK);
      } 
      else {
        if (millis() - lastClockUpdate > 150) {
            drawClockScreen(); // Vẽ nền và hạt sao rơi theo cảm xúc
            
            if (isWakingUp) {
                avatar.drawYawn(); 
                printStatus("ĐANG TỈNH DẬY...", ST77XX_ORANGE);
            }
            else if (elapsedClock > 30000) avatar.drawSleep(); 
            else if (elapsedClock > 25000) avatar.drawYawn(); 
            else {
                // Mang cảm xúc hiện tại ra màn hình đồng hồ
                if (currentEmotion == "vui") avatar.drawJoy();
                else if (currentEmotion == "gian") avatar.drawAngry();
                else if (currentEmotion == "buon") avatar.drawSad();
                else if (currentEmotion == "lo_lang") avatar.drawAnxious();
                else if (currentEmotion == "yeu_thuong") avatar.drawLove();
                else if (currentEmotion == "nghi_ngo") avatar.drawSkeptical();
                else avatar.drawIdle();
            }

            int lookX = (currentEmotion == "nghi_ngo") ? 210 : 230;
            avatar.renderToCanvas(&canvas, lookX, 45); 
            tft.drawRGBBitmap(0, 0, canvas.getBuffer(), 320, 240); 
            lastClockUpdate = millis();
        }
      }
    }  
  }
  // Giữ khung hình ổn định khoảng 30fps
  unsigned long loopTime = millis() - loopStart;
  if (loopTime < 30) delay(30 - loopTime);
}
