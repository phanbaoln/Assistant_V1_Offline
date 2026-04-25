ã #include "config.h"
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

// --- BIẾN TOÀN CỤC (ĐÃ FIX EXTERN CHO SCHEDULE) ---
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
const int ALARM_OFF_THRESHOLD = 15000; // Tăng nhẹ để tránh nhiễu tiếng loa

int currentEnglishLevel = 0;
float currentTemp = 28.5;
String currentAlarmNote = "";
String currentLocation = "Thu Duc, Ho Chi Minh";
int weatherCode = 0;
int isDay = 1;
String currentEmotion = "binh_thuong";

uint16_t* psramChatBG = NULL;
uint16_t* psramClockBG = NULL;

// KHAI BÁO THỰC TẾ CHO SCHEDULE (QUAN TRỌNG)
#include "schedule.h"
Preferences preferences;
Task dailySchedule[MAX_TASKS];
int taskCount = 0; 

// Hạt sao rơi cho UI
Particle particles[25];           
const int MAX_PARTICLES = 25;      
bool particlesInitialized = false; 

// --- INCLUDE CÁC MODULE CON ---
#include "display.h"   
#include "weather.h"   
#include "wifi.h"
#include "MICI2S.h"
#include "speaker.h"
#include "ai.h"        

TaskHandle_t TaskCore0;

// --- HÀM HIỆU ỨNG PHẦN CỨNG (LED BACKLIGHT) ---
void updateHardwareEffects() {
  if (isDisplayOff) return;
  
  if (isWakingUp) {
    int fade = map(millis() - wakeUpStartTime, 0, 5000, 100, 255);
    analogWrite(TFT_BL, constrain(fade, 100, 255));
  }
  else if (currentEmotion == "gian") {
    digitalWrite(TFT_BL, (millis() / 50) % 2); // Chớp đỏ (Strobe)
  } 
  else if (currentEmotion == "vui") {
    float val = (exp(sin(millis() / 2000.0 * PI)) - 0.36787944) * 108.0;
    analogWrite(TFT_BL, map(val, 0, 255, 130, 255)); // Thở vàng
  } 
  else if (currentEmotion == "lo_lang") {
    if (random(10) > 7) analogWrite(TFT_BL, random(40, 255)); // Nháy loạn tím
  }
  else if (currentEmotion == "nghi_ngo") {
    int brightness = (millis() / 10) % 510;
    if (brightness > 255) brightness = 510 - brightness;
    analogWrite(TFT_BL, map(brightness, 0, 255, 60, 255)); // Fade xanh
  }
  else {
    analogWrite(TFT_BL, 255); // Sáng tĩnh
  }
}

// --- LUỒNG XỬ LÝ MẠNG (CORE 0) ---
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
      aiAnswer = (userTranscript.length() > 1) ? askAI(userTranscript) : "Em chưa nghe rõ, anh nói lại nhé.";
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
  analogWrite(TFT_BL, 255);
  tft.setSPISpeed(80000000);
  
  if (psramInit()) {
    audioBuffer = (int16_t *)ps_malloc(BUFFER_SIZE * sizeof(int16_t));
    psramChatBG = (uint16_t*) ps_malloc(320 * 240 * 2);
    psramClockBG = (uint16_t*) ps_malloc(320 * 240 * 2);
    if (psramChatBG != NULL) memset(psramChatBG, 0, 320 * 240 * 2);
    if (psramClockBG != NULL) memset(psramClockBG, 0, 320 * 240 * 2);
  }

  initMic();
  initSpeaker();
  connectWiFi();
  initSchedule(); // Tải lịch từ Flash
  timeClient.begin();
  updateWeather();
  
  tft.fillScreen(ST77XX_BLACK);
  clockStartTime = millis(); 
  xTaskCreatePinnedToCore(networkTask, "TaskCore0", 20000, NULL, 1, &TaskCore0, 0); 
}

void startChatSession() {
  isChattingMode = true;
  clockStartTime = millis(); 
  drawChatBackground(currentEmotion); 
  currentY = 40;
  printAIChat("MÈO đây! Bảo cần em giúp gì?");
  printStatus("SẴN SÀNG: Bảo hãy gọi MÈO...", ST77XX_CYAN);
  lastSoundTime = millis();
}

void loop() {
  unsigned long loopStart = millis(); 
  int32_t amp = getPeakAmplitude();
  static unsigned long successTimer = 0;

  updateHardwareEffects();

  // --- 0. ƯU TIÊN: LỆNH TỪ WEB ---
  if (hasWebMessage) {
    if (isDisplayOff) { 
        isDisplayOff = false; 
        analogWrite(TFT_BL, 255);
        tft.fillScreen(ST77XX_BLACK);
    }
    isWakingUp = false;
    clockStartTime = millis(); 

    if (!isChattingMode) startChatSession();
    printUserChat("Bảo nhắn từ Web...");
    printAIChat(webMessageText);
    playTTS(webMessageText);
    lastSoundTime = millis();
    hasWebMessage = false;
  }

  // --- 1. XỬ LÝ THỨC DẬY ---
  if (amp > CHAT_THRESHOLD && !isChattingMode && !isAlarmActive && !isWakingUp) {
    if (isDisplayOff) { 
        isDisplayOff = false; 
        analogWrite(TFT_BL, 255);
        tft.fillScreen(ST77XX_BLACK);
    }
    isWakingUp = true;
    wakeUpStartTime = millis();
    lastClockUpdate = 0; 
  }

  if (isWakingUp && (millis() - wakeUpStartTime > 5000)) {
      isWakingUp = false;
      if (!isChattingMode) {
          startChatSession();               
          isWaitingForTrigger = true;      
          ignoreMicUntil = millis() + 800; 
      }
  }

  // --- 2. BÁO THỨC (CÓ HỖ TRỢ DỪNG BẰNG ÂM THANH) ---
    if (isAlarmActive) {
    if (isDisplayOff) { isDisplayOff = false; analogWrite(TFT_BL, 255); }
    
    // 🔴 HIỆU ỨNG MÀN HÌNH: Chớp gắt Đỏ - Trắng
    static unsigned long lastRedraw = 0;
    static bool toggle = false;
    if (millis() - lastRedraw > 200) { 
      toggle = !toggle;
      tft.fillScreen(toggle ? ST77XX_RED : ST77XX_WHITE);
      
      u8g2.setFontMode(1); 
      u8g2.setFont(u8g2_font_unifont_t_vietnamese1); 
      u8g2.setForegroundColor(toggle ? ST77XX_WHITE : ST77XX_RED);
      
      u8g2.drawUTF8(40, 100, "MÈO LỆNH:"); 
      // Hiển thị nội dung Bảo đã cài trên Web (ví dụ: "đi học", "đi ngủ")
      if (currentAlarmNote != "") {
          u8g2.drawUTF8(40, 140, currentAlarmNote.c_str());
      } else {
          u8g2.drawUTF8(40, 140, "DẬY NGAY BẢO!");
      }
      lastRedraw = millis();
    }

    // 🔊 ÂM THANH: Kêu tít tít liên tục không nghỉ
    playAlarmBeep(100, 3500); // Kêu 0.1 giây tần số 3.5kHz (siêu chói)
    delay(50);                // Nghỉ cực ngắn để tạo nhịp "tít...tít"

    // 🎙️ Tắt bằng tiếng động (Vỗ tay/Hét)
    if (amp > ALARM_OFF_THRESHOLD) {
      isAlarmActive = false;
      i2s_zero_dma_buffer(I2S_OUT_PORT);
      tft.fillScreen(ST77XX_BLACK);
      playTTS("Đã rõ, em tắt báo thức đây."); // Phản hồi sạch sẽ
      clockStartTime = millis();
      lastClockUpdate = 0;
    }
    return; // Đang báo thức thì không làm việc khác
  }

  // --- 3. CHẾ ĐỘ CHAT ---
  if (isChattingMode) {
    if (isProcessingAI) {
      avatar.drawThinking();
      printStatus("ĐANG NGHĨ: Đợi MÈO xíu...", tft.color565(200, 100, 255));
    }
    else if (successTimer > 0 && millis() < successTimer) {
      avatar.drawSuccess();
    }
    else if (hasNewAnswer) {
      if (successTimer == 0) { 
        successTimer = millis() + 1200; 
        printStatus("ĐANG TRẢ LỜI...", ST77XX_GREEN); 
      } else {
        successTimer = 0;
        printUserChat(userTranscript);

        // --- BƯỚC 1: XỬ LÝ LỆNH NGẦM (LỌC STOP_ALARM) ---
        // Chúng ta xử lý lệnh này TRƯỚC khi in ra màn hình hoặc phát loa
        if (aiAnswer.indexOf("stop_alarm") != -1) { 
            isAlarmActive = false; // Ngắt còi hú lập tức
            aiAnswer.replace("stop_alarm", ""); // Xóa chữ stop_alarm khỏi bộ nhớ
            aiAnswer.trim(); // Dọn dẹp khoảng trắng thừa
            Serial.println(">>> MÈO LỆNH: Đã tắt báo thức.");
        }

        // --- BƯỚC 2: PHẢN HỒI (CHỈ HIỆN VÀ NÓI NỘI DUNG SẠCH) ---
        if (aiAnswer.length() > 0) { 
            avatar.drawSpeaking();
            printAIChat(aiAnswer); // Hiện chữ đã lọc lên màn hình
            playTTS(aiAnswer);     // Nói câu đã lọc ra loa
        }

        hasNewAnswer = false;
        isWaitingForTrigger = true;
        ignoreMicUntil = millis() + 1500;
        lastSoundTime = millis();
      }
    }
    else {
      // Hiển thị biểu cảm theo trạng thái AI gửi về
      if (currentEmotion == "vui") avatar.drawJoy();
      else if (currentEmotion == "gian") avatar.drawAngry();
      else if (currentEmotion == "buon") avatar.drawSad();
      else if (currentEmotion == "lo_lang") avatar.drawAnxious();
      else if (currentEmotion == "yeu_thuong") avatar.drawLove();
      else if (currentEmotion == "nghi_ngo") avatar.drawSkeptical();
      else avatar.drawIdle();
      
      int shake = (currentEmotion == "gian") ? random(-5, 5) : 0;
      avatar.renderChat(290 + shake, 25 + shake, psramChatBG); 

      // Tự động thoát chat sau 30s im lặng
      if (millis() - lastSoundTime > 30000) { 
        isChattingMode = false;
        tft.fillScreen(ST77XX_BLACK);
        clockStartTime = millis();
        lastClockUpdate = 0; 
      } 
      else if (isWaitingForTrigger && millis() > ignoreMicUntil && amp > NEXT_STEP_THRESHOLD) {
        isWaitingForTrigger = false;
        printStatus("ĐANG NGHE: Bảo nói đi...", ST77XX_YELLOW);
        recordAudio();
        isProcessingAI = true;
      }
    }
  }
  
  // --- 4. MÀN HÌNH CHỜ & LỊCH TRÌNH ---
  if (!isChattingMode && !isAlarmActive) {
    checkSchedule(timeClient.getHours(), timeClient.getMinutes());
    
    unsigned long elapsedClock = millis() - (isWakingUp ? wakeUpStartTime : clockStartTime);

    if (!isDisplayOff) {
      if (elapsedClock > 35000 && !isWakingUp) { 
        isDisplayOff = true;
        analogWrite(TFT_BL, 0);
        tft.fillScreen(ST77XX_BLACK);
      } 
      else {
        if (millis() - lastClockUpdate > 150) {
            drawClockScreen(); 
            if (isWakingUp) {
                avatar.drawYawn(); 
                printStatus("ĐANG TỈNH DẬY...", ST77XX_ORANGE);
            }
            else if (elapsedClock > 30000) avatar.drawSleep(); 
            else if (elapsedClock > 25000) avatar.drawYawn(); 
            else {
                if (currentEmotion == "vui") avatar.drawJoy();
                else if (currentEmotion == "gian") avatar.drawAngry();
                else if (currentEmotion == "buon") avatar.drawSad();
                else if (currentEmotion == "lo_lang") avatar.drawAnxious();
                else if (currentEmotion == "yeu_thuong") avatar.drawLove();
                else if (currentEmotion == "nghi_ngo") avatar.drawSkeptical();
                else avatar.drawIdle();
            }
            avatar.renderToCanvas(&canvas, 230, 45); 
            tft.drawRGBBitmap(0, 0, canvas.getBuffer(), 320, 240); 
            lastClockUpdate = millis();
        }
      }
    }  
  }
  unsigned long loopTime = millis() - loopStart;
  if (loopTime < 30) delay(30 - loopTime);
}
