#ifndef MIC_H
#define MIC_H

#include <driver/i2s.h>

// ==========================================
// 1. PIN CONFIGURATION
// ==========================================
#define I2S_WS    4
#define I2S_SCK   5
#define I2S_SD    6
#define I2S_PORT  I2S_NUM_0

// ==========================================
// 2. AUDIO SETTINGS
// ==========================================
const int SAMPLE_RATE = 16000;
const int RECORD_TIME = 4; // Thời gian thu âm (giây)
const size_t BUFFER_SIZE = SAMPLE_RATE * RECORD_TIME;

extern int16_t *audioBuffer;
extern size_t audioIndex;

// ==========================================
// 3. GENERATE WAV HEADER (44 Bytes)
inline void addWavHeader(uint8_t* header, int dataSize) {
  int byteRate   = SAMPLE_RATE * 1 * 16 / 8;
  int blockAlign = 1 * 16 / 8;
  int chunkSize  = dataSize + 36;

  // Chunk ID & Size
  memcpy(header,      "RIFF", 4);
  memcpy(header + 4,  &chunkSize, 4);
  memcpy(header + 8,  "WAVE", 4);
  
  // Format Subchunk
  memcpy(header + 12, "fmt ", 4);
  int subChunk1 = 16;   
  memcpy(header + 16, &subChunk1, 4);
  short fmt = 1;        
  memcpy(header + 20, &fmt, 2);
  short ch = 1;         
  memcpy(header + 22, &ch, 2);
  memcpy(header + 24, &SAMPLE_RATE, 4);
  memcpy(header + 28, &byteRate, 4);
  memcpy(header + 32, &blockAlign, 2);
  short bits = 16;      
  memcpy(header + 34, &bits, 2);

  // Data Subchunk
  memcpy(header + 36, "data", 4);
  memcpy(header + 40, &dataSize, 4);
}

// ==========================================
// 4. INITIALIZE MICROPHONE (I2S)
// ==========================================
inline void initMic() {
  i2s_config_t i2s_config = {
    .mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate          = SAMPLE_RATE,
    .bits_per_sample      = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format       = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_STAND_I2S),
    .intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count        = 8,
    .dma_buf_len          = 512,
    .use_apll             = false
  };

  i2s_pin_config_t pin_config = {
    .bck_io_num   = I2S_MIC_SCK,
    .ws_io_num    = I2S_MIC_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num  = I2S_MIC_SD
  };

  i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_PORT, &pin_config);

  // Cấp phát bộ nhớ PSRAM cho Audio Buffer
  if (audioBuffer == NULL) {
    audioBuffer = (int16_t *)ps_malloc(BUFFER_SIZE * sizeof(int16_t));
    if (audioBuffer == NULL) {
      Serial.println(">>> LỖI CHÍ MẠNG: Không đủ PSRAM để cấp phát bộ nhớ!");
    } // ĐÃ THÊM DẤU ĐÓNG NGOẶC ĐÚNG CHUẨN Ở ĐÂY
  }
}
// 5. TEST PEAK AMPLITUDE (WAKE-UP)
// ==========================================
inline int32_t getPeakAmplitude() {
  size_t bytes_read;
  int32_t raw_sample = 0;
  int32_t max_amp = 0;

  for (int i = 0; i < 64; i++) {
    i2s_read(I2S_PORT, &raw_sample, sizeof(raw_sample), &bytes_read, 0);
    if (bytes_read > 0) {
      int16_t sample = (int16_t)(raw_sample >> 16);
      if (abs(sample) > max_amp) {
        max_amp = abs(sample);
      }
    }
  }
  return max_amp;
}

// 6. RECORD AUDIO (TỐI ƯU CHO WHISPER AI)
inline void recordAudio() {
  audioIndex = 0;
  size_t bytes_read;
  int32_t raw_sample;

  // Xóa sạch buffer trước khi thu
  memset(audioBuffer, 0, BUFFER_SIZE * sizeof(int16_t));

  // --- 1. LẤY MẪU KHỬ DC OFFSET ---
  int32_t offset = 0;
  for (int i = 0; i < 100; i++) {
    i2s_read(I2S_PORT, &raw_sample, sizeof(raw_sample), &bytes_read, 0);
    offset += (int16_t)(raw_sample >> 16);
  }
  offset /= 100;

  // --- 2. FILTER STATES ---
  float prev_input = 0;
  float prev_output = 0;

  // --- 3. VÒNG LẶP THU ÂM CHÍNH ---
  while (audioIndex < BUFFER_SIZE) {
    i2s_read(I2S_PORT, &raw_sample, sizeof(raw_sample), &bytes_read, portMAX_DELAY);

    if (bytes_read > 0) {
      // B1: Khử DC Offset (Đưa tín hiệu về trục 0)
      float x = (int16_t)(raw_sample >> 16) - offset;

      // B2: High Pass Filter NHẸ (Chỉ lọc tiếng ù nền 50Hz của điện lưới, giữ nguyên dải âm giọng nói)
      float hp = 0.99 * (prev_output + x - prev_input);
      prev_input = x;
      prev_output = hp;

      // B3: Khuếch đại TĨNH (Static Gain)
      // Whisper thích âm lượng ổn định. Nhân 4 lần là đủ to rõ.
      float s = hp * 4.0;

      // B4: Hard Limiter (Chống vỡ tiếng)
      if (s > 32767) s = 32767;
      if (s < -32768) s = -32768;

      audioBuffer[audioIndex++] = (int16_t)s;
    }
  }

  // --- 4. DEBUG INFO ---
  int16_t maxAmp = 0;
  for (size_t i = 0; i < BUFFER_SIZE; i++) {
    if (abs(audioBuffer[i]) > maxAmp) maxAmp = abs(audioBuffer[i]);
  }

  Serial.printf(">>> THU ÂM XONG | MAX AMP: %d\n", maxAmp);
}
#endif
