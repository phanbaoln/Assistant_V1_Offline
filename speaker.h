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

// Hàm kết nối Server, tải Audio Stream và phát ra loa (Dùng PSRAM)
inline void playTTS(String text) {
  if (WiFi.status() != WL_CONNECTED || text.length() == 0) return;
  
  Serial.println(">>> Đang tải TOÀN BỘ giọng nói vào PSRAM...");
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  
  String url = "https://9yr9fy-8080.csb.app/api/tts?text=" + urlEncode(text);
  
  http.begin(client, url);
  http.setTimeout(30000);
  int httpCode = http.GET();
  
  if (httpCode == 200) {
    int len = http.getSize(); 
    WiFiClient *stream = http.getStreamPtr();
    
    const size_t MAX_AUDIO_SIZE = 512 * 1024; 
    uint8_t *psramAudioBuf = (uint8_t *)ps_malloc(MAX_AUDIO_SIZE); 
    
    if (psramAudioBuf) {
      size_t totalBytesDownloaded = 0;
      int timeout = 0; 

      // ==========================================
      // GIAI ĐOẠN 1: KÉO TOÀN BỘ FILE VỀ PSRAM (Giữ nguyên)
      // ==========================================
      while (http.connected() && (len > 0 || len == -1) && totalBytesDownloaded < MAX_AUDIO_SIZE) {
        size_t available = stream->available();
        
        if (available > 0) {
           timeout = 0; 
           size_t bytesToRead = available;
           if (totalBytesDownloaded + bytesToRead > MAX_AUDIO_SIZE) {
               bytesToRead = MAX_AUDIO_SIZE - totalBytesDownloaded;
           }
           
           size_t bytesRead = stream->readBytes(psramAudioBuf + totalBytesDownloaded, bytesToRead);
           totalBytesDownloaded += bytesRead;
           
           if (len > 0) len -= bytesRead; 
        } else {
           delay(10); 
           timeout++; 
           if (timeout > 300) break; 
        }
      }
      
      Serial.printf(">>> Đã tải xong %d bytes. BẮT ĐẦU PHÁT!\n", totalBytesDownloaded);
      // GIAI ĐOẠN 2: CHỐNG RÈ BẰNG CÁCH GIẢM ÂM LƯỢNG (DIGITAL VOLUME)
      size_t bytesWritten;
      int16_t *monoSamples = (int16_t *)psramAudioBuf;
      size_t totalSamples = totalBytesDownloaded / 2; // Mỗi mẫu âm là 2 byte

      int16_t audioChunk[1024]; // Khay chứa tạm
      size_t chunkIdx = 0;

      for (size_t i = 0; i < totalSamples; i++) {
          // Nếu vẫn còn hơi rè, Bảo có thể đổi thành / 3 hoặc / 4 nhé!
          audioChunk[chunkIdx++] = monoSamples[i] / 4; 

          // Khi khay đầy, tống ra I2S
          if (chunkIdx == 1024) {
              i2s_write(I2S_OUT_PORT, audioChunk, sizeof(audioChunk), &bytesWritten, portMAX_DELAY);
              chunkIdx = 0;
              vTaskDelay(pdMS_TO_TICKS(1));
          }
      }

      // Đẩy nốt phần lẻ còn lại
      if (chunkIdx > 0) {
          i2s_write(I2S_OUT_PORT, audioChunk, chunkIdx * 2, &bytesWritten, portMAX_DELAY);
      }

      // ==========================================
      // GIAI ĐOẠN 3: BƠM "IM LẶNG" CHỐNG MẤT CHỮ CUỐI
      // ==========================================
      memset(audioChunk, 0, sizeof(audioChunk));
      for(int i = 0; i < 16; i++) {
          i2s_write(I2S_OUT_PORT, audioChunk, sizeof(audioChunk), &bytesWritten, portMAX_DELAY);
      }

      free(psramAudioBuf); // Xong xuôi thì dọn dẹp RAM
    } else {
      Serial.println(">>> LỖI: PSRAM của ESP32 không đủ dung lượng!");
    }
    Serial.println(">>> Đã phát xong toàn bộ!");
  } else {
    Serial.printf(">>> LỖI API: %d\n", httpCode);
  }
  
  http.end();
  i2s_zero_dma_buffer(I2S_OUT_PORT); 
} 
// Hàm tạo tiếng Bíp siêu chói, tối ưu để gọi liên tục trong loop
inline void playAlarmBeep(int durationMs, int freqHz = 3000) {
    size_t bytesWritten;
    int16_t maxSample = 32767;  // Max biên độ dương
    int16_t minSample = -32768; // Max biên độ âm

    // Tính toán số mẫu dựa trên tần số (Sample Rate mặc định 16000)
    int samplesPerHalfCycle = (16000 / freqHz) / 2; 
    if (samplesPerHalfCycle < 1) samplesPerHalfCycle = 1;

    unsigned long start = millis();
    // Chạy liên tục trong khoảng durationMs
    while (millis() - start < (unsigned long)durationMs) {
        // Nửa chu kỳ dương (Sóng vuông)
        for (int i = 0; i < samplesPerHalfCycle; i++) {
            i2s_write(I2S_OUT_PORT, &maxSample, sizeof(int16_t), &bytesWritten, portMAX_DELAY);
        }
        // Nửa chu kỳ âm
        for (int i = 0; i < samplesPerHalfCycle; i++) {
            i2s_write(I2S_OUT_PORT, &minSample, sizeof(int16_t), &bytesWritten, portMAX_DELAY);
        }
    }
    // Dọn dẹp buffer để tránh tiếng rè sau khi bíp
    i2s_zero_dma_buffer(I2S_OUT_PORT);
}
#endif
