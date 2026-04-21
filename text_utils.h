#ifndef TEXT_UTILS_H
#define TEXT_UTILS_H

#include <Arduino.h>

// Hàm dọn dẹp văn bản và chặn Emoji (byte > 240)
inline String cleanText(String text) {
  text.replace("\r", "");
  text.replace("\t", " ");
  text.replace("\n", " ");
  while (text.indexOf("  ") != -1) text.replace("  ", " ");

  String filtered = "";
  for (int i = 0; i < text.length(); i++) {
    unsigned char c = (unsigned char)text[i];
    if (c >= 32 && c < 240) filtered += (char)c;
  }
  filtered.trim();
  return filtered;
}

// Hàm tách phản hồi và tự động làm sạch nội dung
inline void parseResponse(String response, String &transcript, String &answer) {
  int idxNghe = response.indexOf("NGHE:");
  int idxTraLoi = response.indexOf("TRẢ LỜI:");

  if (idxNghe >= 0 && idxTraLoi > idxNghe) {
    transcript = cleanText(response.substring(idxNghe + 5, idxTraLoi));
    answer = cleanText(response.substring(idxTraLoi + 8));
  } else {
    transcript = "";
    answer = cleanText(response);
  }
}

#endif
