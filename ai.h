#ifndef AI_H
#define AI_H

#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <NTPClient.h>

#include "config.h"
#include "MICI2S.h"
#include "schedule.h"

// Tham chiếu các biến toàn cục từ main.ino
extern int16_t *audioBuffer;
extern size_t audioIndex;
extern float currentTemp;
extern NTPClient timeClient;
extern int currentEnglishLevel; 
extern String currentEmotion;
extern volatile bool isAlarmActive;
extern uint16_t* psramChatBG;
extern uint16_t* psramClockBG;
extern unsigned long lastClockUpdate;

// Biến lưu chế độ nền
extern String clockThemeMode; 
extern String chatThemeMode;

// --- 1. Whisper (STT) (Xử lý chuyển giọng nói thành văn bản) ---
inline String speechToText() {
  if (WiFi.status() != WL_CONNECTED) return "";
  WiFiClientSecure client;
  client.setInsecure();  
  HTTPClient http;
  http.begin(client, "https://api.groq.com/openai/v1/audio/transcriptions");
  http.addHeader("Authorization", "Bearer " + String(groq_key));
  
  String boundary = "----ESP32";
  http.addHeader("Content-Type", "multipart/form-data; boundary=" + boundary);
  String head = "--" + boundary + "\r\n"
                "Content-Disposition: form-data; name=\"file\"; filename=\"speech.wav\"\r\n"
                "Content-Type: audio/wav\r\n\r\n";
  String tail = "\r\n--" + boundary + "\r\n"
                "Content-Disposition: form-data; name=\"model\"\r\n\r\n"
                "whisper-large-v3\r\n"
                "--" + boundary + "--\r\n";

  int dataSize = audioIndex * sizeof(int16_t);
  size_t totalLen = head.length() + 44 + dataSize + tail.length();
  
  // 👉 Cấp phát PSRAM an toàn cho Payload STT
  uint8_t* payload = (uint8_t*)ps_malloc(totalLen);
  if (!payload) {
    Serial.println(">>> LỖI: PSRAM đầy!");
    return "";
  }

  size_t offset = 0;
  memcpy(payload + offset, head.c_str(), head.length()); offset += head.length();
  uint8_t wavHeader[44];
  addWavHeader(wavHeader, dataSize);
  memcpy(payload + offset, wavHeader, 44); offset += 44;
  memcpy(payload + offset, (uint8_t*)audioBuffer, dataSize); offset += dataSize;
  memcpy(payload + offset, tail.c_str(), tail.length());

  int httpCode = http.sendRequest("POST", payload, totalLen);
  
  // Giải phóng ngay bộ nhớ sau khi gửi
  if (payload != NULL) {
    free(payload); 
    payload = NULL;
  }

  String result = "";
  if (httpCode == 200) {
    DynamicJsonDocument doc(2048);
    deserializeJson(doc, http.getString());
    result = doc["text"].as<String>();
  } else {
    Serial.printf(">>> LỖI STT HTTP CODE: %d\n", httpCode);
  }
  
  http.end();
  return result;
}

// --- 2. Gửi dữ liệu lên Replit để AI xử lý (LLM) ---
inline String askAI(String userText) {
  if (WiFi.status() != WL_CONNECTED) return "Lỗi WiFi";
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;

  http.begin(client, "https://9yr9fy-8080.csb.app/api/chat");
  http.addHeader("Content-Type", "application/json");

  time_t rawtime = timeClient.getEpochTime();
  struct tm * ti = localtime(&rawtime);
  String formattedTime = String(ti->tm_hour) + ":" + String(ti->tm_min);

  DynamicJsonDocument sendDoc(1024);
  sendDoc["text"] = userText;
  sendDoc["source"] = "esp32";
  sendDoc["time"] = formattedTime;
  sendDoc["weather"] = String(currentTemp) + "C";
  sendDoc["level"] = currentEnglishLevel;

  String requestBody;
  serializeJson(sendDoc, requestBody);
  int httpCode = http.POST(requestBody);
  
  if (httpCode == 200) {
    String response = http.getString();
    DynamicJsonDocument res(2048);
    deserializeJson(res, response);
    
    // 👉 CẬP NHẬT CẢM XÚC: vui, gian, buon, lo_lang, yeu_thuong, nghi_ngo
    String emotion = res["emotion"].as<String>();
    if (emotion != "null" && emotion != "") {
        currentEmotion = emotion;
    }
    
    String replyText = res["reply"].as<String>();
    http.end();
    return replyText;
  }
  http.end(); 
  return "MÈO hơi choáng, Bảo gọi lại sau nhé!";
}

// --- 3. Tải ảnh RAW từ Server vào PSRAM ---
inline bool downloadRawBG(String imageUrl, uint16_t* destBuffer) {
  if (WiFi.status() != WL_CONNECTED || destBuffer == NULL) return false;
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  String serverUrl = "https://9yr9fy-8080.csb.app/api/get_bg_raw?url=" + imageUrl;
  http.begin(client, serverUrl);

  int httpCode = http.GET();
  if (httpCode == 200) {
    int expectedSize = 320 * 240 * 2;
    WiFiClient* stream = http.getStreamPtr();
    int bytesRead = 0;
    uint8_t* buffer = (uint8_t*)destBuffer;
    while (http.connected() && bytesRead < expectedSize) {
      if (stream->available()) {
        int c = stream->readBytes(buffer + bytesRead, expectedSize - bytesRead);
        bytesRead += c;
      }
    }
    http.end();
    return (bytesRead == expectedSize);
  }
  http.end();
  return false;
}

// --- 4. Kiểm tra Lệnh (CMD) và Tin nhắn từ Web Console ---
inline String checkWebChat() {
  if (WiFi.status() != WL_CONNECTED) return "";
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  
  // Bản ổn định: Sử dụng endpoint polling tiêu chuẩn
  http.begin(client, "https://9yr9fy-8080.csb.app/api/check_msg");
  
  int httpCode = http.GET();
  if (httpCode == 200) {
    String response = http.getString();
    if (response.indexOf("no_msg") >= 0) {
      http.end();
      return "";
    }
    
    DynamicJsonDocument doc(2048);
    deserializeJson(doc, response);
    
    // XỬ LÝ CÁC LỆNH HỆ THỐNG (CMD)
    if (doc.containsKey("cmd")) {
      String command = doc["cmd"].as<String>();
      
      // 1. Sửa lại tên lệnh và phím nhận dữ liệu cho khớp với main.py
      if (command == "set_english_level") {
        currentEnglishLevel = doc["level"].as<int>(); 
        return "SYSTEM: Level English -> " + String(currentEnglishLevel);
      }

      // 2. Hỗ trợ cả add_alarm và set_alarm (tùy theo nút bấm trên Web gửi gì)
      if (command == "add_alarm" || command == "set_alarm") {
          String timeStr = doc["time"].as<String>(); 
          String note = doc["note"].as<String>();
          uint8_t repeat = doc.containsKey("repeat") ? doc["repeat"].as<uint8_t>() : 0;
          int h = timeStr.substring(0, 2).toInt();
          int m = timeStr.substring(3, 5).toInt();
          addSchedule(h, m, note);
          return "SYSTEM: Da ghi lich " + timeStr;
      }
      
      if (command == "set_basic_theme") {
        String theme = doc["theme"].as<String>();
        String target = doc["target"].as<String>();
        if (target == "chat") chatThemeMode = theme;
        else clockThemeMode = theme;
        lastClockUpdate = 0;
        return "SYSTEM: Theme " + target + " -> " + theme;
      }
      
      if (command == "update_bg") {
        String url = doc["url"].as<String>();
        String target = doc["target"].as<String>();
        uint16_t* targetBuffer = (target == "chat") ? psramChatBG : psramClockBG;
        if (downloadRawBG(url, targetBuffer)) {
            if (target == "chat") chatThemeMode = "url";
            else clockThemeMode = "url";
            lastClockUpdate = 0;
            return "SYSTEM: Da tai xong Background " + target;
        } else {
            return "SYSTEM: Loi tai Background " + target;
        }
      }
      
      if (command == "stop_alarm") { 
        isAlarmActive = false; 
        i2s_zero_dma_buffer(I2S_OUT_PORT);
        return "stop_alarm"; }
      if (command == "clear") { clearAllSchedule(); return "SYSTEM: Da xoa sach lich!"; }
    }
    
    // XỬ LÝ PHẢN HỒI AI TỪ WEB
    if (doc.containsKey("reply")) {
      // 👉 CẬP NHẬT CẢM XÚC TỪ WEB:
      String emo = doc["emotion"].as<String>();
      if (emo != "null" && emo != "") {
          currentEmotion = emo;
      }
      
      String r = doc["reply"].as<String>();
      http.end();
      return r;
    }
  }
  http.end();
  return "";
}

#endif
