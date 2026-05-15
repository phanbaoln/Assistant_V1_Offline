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

inline String checkWebChat() {
  if (WiFi.status() != WL_CONNECTED) return "";
  
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  
  // Endpoint polling
  http.begin(client, "https://9yr9fy-8080.csb.app/api/check_msg");
  
  int httpCode = http.GET();
  if (httpCode == 200) {
    String response = http.getString();
    if (response.indexOf("no_msg") >= 0) {
      http.end();
      return "";
    }
    
    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, response);
    if (error) { 
      http.end(); 
      return ""; 
    }
    
    String command = "";
    // Đồng bộ cả 'cmd' và 'action' cho an toàn
    if (doc.containsKey("cmd")) command = doc["cmd"].as<String>();
    else if (doc.containsKey("action")) command = doc["action"].as<String>();

    if (command != "") {
      String result = ""; // Biến tạm để lưu kết quả trả về

      // --- XỬ LÝ THEME (TRỌNG TÂM) ---
      if (command == "set_basic_theme") {
        String theme = doc["theme"].as<String>();
        String target = doc["target"].as<String>();

        if (target == "clock") {
          clockThemeMode = theme;
          
          // ĐÁNH THỨC MÀN HÌNH: Nếu đang tắt thì bật lên ngay
          if (isDisplayOff) {
            isDisplayOff = false;
            analogWrite(TFT_BL, 255); 
            tft.fillScreen(ST77XX_BLACK);
          }
          
          lastClockUpdate = 0; // Ép vẽ lại ở loop()
          result = "SYSTEM: Clock theme updated to " + theme;
        } 
        else if (target == "chat") {
          chatThemeMode = theme;
          result = "SYSTEM: Chat theme updated.";
        }
      }

      // --- XỬ LÝ BACKGROUND URL ---
      else if (command == "update_bg") {
        String url = doc["url"].as<String>();
        String target = doc["target"].as<String>();
        uint16_t* targetBuffer = (target == "chat") ? psramChatBG : psramClockBG;

        if (downloadRawBG(url, targetBuffer)) {
          if (target == "clock") clockThemeMode = "url";
          else chatThemeMode = "url";
          
          lastClockUpdate = 0;
          result = "SYSTEM: Da tai xong Background " + target;
        } else {
          result = "SYSTEM: Loi tai Background " + target;
        }
      }
      
      // --- CÁC LỆNH KHÁC ---
      else if (command == "set_english_level") {
        currentEnglishLevel = doc["level"].as<int>();
        result = "SYSTEM: Level English -> " + String(currentEnglishLevel);
      }
      else if (command == "add_alarm" || command == "set_alarm") {
        String timeStr = doc["time"].as<String>();
        String note = doc["note"].as<String>();
        addSchedule(timeStr.substring(0, 2).toInt(), timeStr.substring(3, 5).toInt(), note);
        result = "SYSTEM: Da ghi lich " + timeStr;
      }
      else if (command == "stop_alarm") { isAlarmActive = false; result = "stop_alarm"; }
      else if (command == "clear") { clearAllSchedule(); result = "SYSTEM: Da xoa sach lich!"; }

      http.end(); // QUAN TRỌNG: Đóng kết nối trước khi return
      return result;
    }
    
    // XỬ LÝ PHẢN HỒI AI
    if (doc.containsKey("reply")) {
      String emo = doc["emotion"].as<String>();
      if (emo != "null" && emo != "") currentEmotion = emo;
      String r = doc["reply"].as<String>();
      http.end();
      return r;
    }
  }
  
  http.end();
  return "";
}

#endif
