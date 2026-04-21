#ifndef WEATHER_H
#define WEATHER_H

#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFi.h> 
#include <NTPClient.h>

// --- BIẾN EXTERN: Trỏ sang main.ino ---
extern float currentTemp;
extern String currentLocation;
extern NTPClient timeClient;
extern int weatherCode; 
extern int isDay; 
extern String currentEmotion; 
extern String clockThemeMode;

extern Particle particles[];
extern const int MAX_PARTICLES;
extern bool particlesInitialized;

// --- 1. LOGIC VẬT LÝ HẠT THEO CẢM XÚC ---
inline void setParticleEmotion(int i) {
    // 🔴 GIẬN DỮ: Hạt rơi cực nhanh, màu đỏ rực
    if (currentEmotion == "gian") {
        particles[i].speed = random(100, 250) / 10.0;
        particles[i].color = tft.color565(255, 0, 0); 
    } 
    // 🟡 VUI VẺ: Hạt rơi nhanh vừa phải, màu vàng sáng
    else if (currentEmotion == "vui") {
        particles[i].speed = random(60, 150) / 10.0;
        particles[i].color = tft.color565(255, 0, 255); 
    } 
    // 🔵 BUỒN BÃ: Hạt rơi rất chậm (như mưa phùn), màu xanh dương
    else if (currentEmotion == "buon") {
        particles[i].speed = random(10, 40) / 10.0;
        particles[i].color = tft.color565(0, 100, 255); 
    } 
    // 🟣 LO LẮNG: Tốc độ hỗn loạn, màu tím
    else if (currentEmotion == "lo_lang") {
        particles[i].speed = random(40, 180) / 10.0;
        particles[i].color = tft.color565(180, 0, 255); 
    } 
    // 🌸 YÊU THƯƠNG: Rơi nhẹ nhàng, màu hồng cánh sen
    else if (currentEmotion == "yeu_thuong") {
        particles[i].speed = random(20, 60) / 10.0;
        particles[i].color = tft.color565(255, 100, 200); 
    } 
    // 🟢 NGHI NGỜ: Tốc độ ổn định, màu xanh lá
    else if (currentEmotion == "nghi_ngo") {
        particles[i].speed = random(40, 80) / 10.0;
        particles[i].color = tft.color565(0, 255, 0); 
    } 
    // 💎 BÌNH THƯỜNG / TỈNH TÁO: Màu Cyan đặc trưng của Trúc
    else {
        particles[i].speed = random(40, 100) / 10.0;
        particles[i].color = tft.color565(0, 243, 255); 
    }
}

inline void initParticles() {
  for (int i = 0; i < MAX_PARTICLES; i++) {
    particles[i].x = random(0, 320);
    particles[i].y = random(0, 240);
    particles[i].size = random(1, 3);
    setParticleEmotion(i);
  }
  particlesInitialized = true;
}

inline void updateParticles() {
  if (!particlesInitialized) initParticles();
  
  static String lastEmotion = "binh_thuong";
  if (currentEmotion != lastEmotion) {
      for (int i = 0; i < MAX_PARTICLES; i++) setParticleEmotion(i);
      lastEmotion = currentEmotion;
  }

  for (int i = 0; i < MAX_PARTICLES; i++) {
    // --- HIỆU ỨNG VẬT LÝ ĐẶC BIỆT ---
    if (currentEmotion == "gian") {
        particles[i].x += random(-3, 4); // Hạt rung lắc dữ dội
    } 
    else if (currentEmotion == "lo_lang") {
        particles[i].x += random(-1, 2); // Hạt giật giật bất an
    }
    else if (currentEmotion == "buon" || currentEmotion == "yeu_thuong") {
        particles[i].x += sin(millis() / 1000.0 + i) * 0.4; // Hạt trôi dạt mềm mại
    }
    
    particles[i].y += particles[i].speed;
    
    if (particles[i].y > 240) {
      particles[i].y = -10; 
      particles[i].x = random(0, 320);
      setParticleEmotion(i); 
    }
  }
}

// --- 2. LOGIC LỜI KHUYÊN SỨC KHỎE ---
static unsigned long timerAdvice = 0;
static String memoryAdvice = "";
inline String getSoftAdvice(float temp, int code) {
  if (millis() - timerAdvice > 600000 || memoryAdvice == "") {
    String hot[] = {"Nóng quá Bảo ơi, uống nước đi!", "Làm ly nước mát thôi!", "Nắng gắt, hạn chế ra ngoài nha!"};
    String rain[] = {"Trời sắp mưa, nhớ mang ô nhé!", "Đừng để bị ướt nghe Bảo.", "Mưa lâm thâm, coi chừng cảm đó."};
    String nice[] = {"Thời tiết đẹp, học thôi Bảo!", "Một ngày tuyệt vời để cố gắng!", "Tận hưởng không khí nhé!"};
    
    int r = random(0, 3);
    if (temp >= 33) memoryAdvice = hot[r];
    else if (code >= 51 && code <= 67) memoryAdvice = rain[r];
    else memoryAdvice = nice[r];
    
    timerAdvice = millis();
  }
  return memoryAdvice;
}

// --- 3. CẬP NHẬT THỜI TIẾT TỪ API ---
inline void updateWeather() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin("https://api.open-meteo.com/v1/forecast?latitude=10.8231&longitude=106.6297&current_weather=true");
    if (http.GET() == 200) {
      DynamicJsonDocument doc(512);
      deserializeJson(doc, http.getString());
      currentTemp = doc["current_weather"]["temperature"];
      weatherCode = doc["current_weather"]["weathercode"].as<int>(); 
      isDay = doc["current_weather"]["is_day"].as<int>(); 
      
      // Tự đổi theme nếu trời mưa
      if (weatherCode >= 51 && weatherCode <= 67) clockThemeMode = "solid_blue"; 
      else clockThemeMode = "default";
      
      Serial.printf(">>> Weather Updated: %.1fC, Code: %d\n", currentTemp, weatherCode);
    }
    http.end();
  }
}

#endif
