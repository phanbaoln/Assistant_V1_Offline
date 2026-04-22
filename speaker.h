#ifndef SPEAKER_H
#define SPEAKER_H

#include <driver/i2s.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

inline void initSpeaker() {
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX), // Chế độ Phát
    .sample_rate = 16000,                                // ElevenLabs xuất 16kHz
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,         // Khớp với việc nối SD = 3.3V
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 16,
    .dma_buf_len = 1024,                                  // Buffer lớn để tránh vấp tiếng
    .use_apll = false,
    .tx_desc_auto_clear = true
  };

  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_OUT_BCLK,
    .ws_io_num = I2S_OUT_LRCK,
    .data_out_num = I2S_OUT_DIN,
    .data_in_num = I2S_PIN_NO_CHANGE
  };

  i2s_driver_install(I2S_OUT_PORT, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_OUT_PORT, &pin_config);
  i2s_zero_dma_buffer(I2S_OUT_PORT);
  Serial.println(">>> Da khoi tao MAX98357A (Loa)");
}

// Hàm mã hóa URL để gửi câu nói lên Server
inline String urlEncode(String str) {
  String encodedString = "";
  char c; char code0; char code1;
  for (int i = 0; i < str.length(); i++){
    c = str.charAt(i);
    if (c == ' ') encodedString += '+';
    else if (isalnum(c)) encodedString += c;
    else {
      code1 = (c & 0xf) + '0';
      if ((c & 0xf) > 9) code1 = (c & 0xf) - 10 + 'A';
      c = (c >> 4) & 0xf;
      code0 = c + '0';
      if (c > 9) code0 = c - 10 + 'A';
      encodedString += '%'; encodedString += code0; encodedString += code1;
    }
  }
  return encodedString;
}

// Hàm kết nối Server, tải Audio Stream và phát ra loa
// Hàm kết nối Server, tải Audio Stream và phát ra loa
inline void playTTS(String text) {
  if (WiFi.status() != WL_CONNECTED || text.length() == 0) return;
  
  Serial.println(">>> Đang tải giọng nói...");
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  
  String url = "https://8323aa70-12ba-4bc8-84e4-81dc7127a309-00-2ow6iyqz5fdrl.picard.replit.dev/api/tts?text=" + urlEncode(text);
  
  http.begin(client, url);
  int httpCode = http.GET();
  
  if (httpCode == 200) {
    int len = http.getSize(); // 👉 Lấy tổng dung lượng file audio
    WiFiClient *stream = http.getStreamPtr();
    
    uint8_t *audioBuf = (uint8_t *)malloc(4096); 
    
    if (audioBuf) {
      size_t bytesWritten;
      int timeout = 0; // 👉 Cài đặt chốt chặn an toàn (Timeout)

      // 👉 Vòng lặp an toàn: Thoát khi tải đủ dung lượng 'len' hoặc bị Timeout
      while (http.connected() && (len > 0 || len == -1)) {
        size_t available = stream->available();
        
        if (available > 0) {
          timeout = 0; // Có data chảy về -> Reset lại chốt chặn
          
          size_t bytesToRead = (available > 4096) ? 4096 : available;
          size_t bytesRead = stream->readBytes(audioBuf, bytesToRead);

          if (bytesRead > 0) {
             i2s_write(I2S_OUT_PORT, audioBuf, bytesRead, &bytesWritten, portMAX_DELAY);
             if (len > 0) len -= bytesRead; // Đã đọc xong thì trừ dần dung lượng
          }
        } else {
          delay(2); 
          timeout += 2; // Tăng dần thời gian chờ
          
          // 👉 Nếu đợi 2000ms (2 giây) mà không có data -> Hết file, kết thúc luồng!
          if (timeout > 2000) {
            Serial.println(">>> Đã hết luồng dữ liệu, tự động thoát tải!");
            break; 
          }
        }
      }
      free(audioBuf);
    }
    Serial.println(">>> Đã phát xong!");
  } else {
    Serial.printf(">>> LỖI ElevenLabs API: %d\n", httpCode);
  }
  
  http.end();
  i2s_zero_dma_buffer(I2S_OUT_PORT); 
}
#endif
