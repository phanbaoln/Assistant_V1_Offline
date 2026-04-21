#ifndef SCHEDULE_H
#define SCHEDULE_H

#include <Preferences.h>
#include <ArduinoJson.h>

static Preferences preferences;

struct Task {
  int hour;
  int minute;
  String note;
  uint8_t repeatDays; // 0: Một lần, bit 1-7: T2-CN
  bool isEnabled;
};

const int MAX_TASKS = 15; // Nâng lên 15 nhiệm vụ
static Task dailySchedule[MAX_TASKS];
static int taskCount = 0;

extern volatile bool isAlarmActive;
extern String currentAlarmNote;
extern unsigned long lastClockUpdate;

// 1. Khởi tạo và tải dữ liệu
inline void initSchedule() {
  preferences.begin("clock_data", false);
  taskCount = preferences.getInt("taskCount", 0);
  
  for (int i = 0; i < taskCount; i++) {
    dailySchedule[i].hour = preferences.getInt(("h_" + String(i)).c_str(), 0);
    dailySchedule[i].minute = preferences.getInt(("m_" + String(i)).c_str(), 0);
    dailySchedule[i].note = preferences.getString(("n_" + String(i)).c_str(), "");
    dailySchedule[i].repeatDays = preferences.getUChar(("r_" + String(i)).c_str(), 0);
    dailySchedule[i].isEnabled = true;
  }
  preferences.end();
}

// 2. Thêm lịch thông minh
// repeatDays: 0 (Once), 62 (T2-T6), 127 (Hàng ngày), 65 (Cuối tuần)...
inline void addSchedule(int h, int m, String note, uint8_t repeatDays) {
  if (taskCount >= MAX_TASKS) taskCount = 0; // Ghi đè vòng lặp

  dailySchedule[taskCount] = {h, m, note, repeatDays, true};

  preferences.begin("clock_data", false);
  preferences.putInt(("h_" + String(taskCount)).c_str(), h);
  preferences.putInt(("m_" + String(taskCount)).c_str(), m);
  preferences.putString(("n_" + String(taskCount)).c_str(), note);
  preferences.putUChar(("r_" + String(taskCount)).c_str(), repeatDays);
  
  taskCount++;
  preferences.putInt("taskCount", taskCount);
  preferences.end();
  
  lastClockUpdate = 0;
}

// 3. Kiểm tra thông minh theo ngày trong tuần
inline void checkSchedule(int currentH, int currentM, int currentDay) {
  // currentDay lấy từ timeClient.getDay() (0: CN, 1: T2...)
  if (isAlarmActive) return;

  for (int i = 0; i < taskCount; i++) {
    if (!dailySchedule[i].isEnabled) continue;

    if (dailySchedule[i].hour == currentH && dailySchedule[i].minute == currentM) {
      
      bool trigger = false;
      if (dailySchedule[i].repeatDays == 0) {
        // Báo thức một lần
        trigger = true;
        dailySchedule[i].isEnabled = false; // Tắt luôn sau khi reo
      } else {
        // Báo thức lặp lại: Kiểm tra bit tương ứng với ngày hiện tại
        if ((dailySchedule[i].repeatDays >> currentDay) & 1) {
          trigger = true;
        }
      }

      if (trigger) {
        isAlarmActive = true;
        currentAlarmNote = dailySchedule[i].note;
        Serial.println(">>> TRÚC REO: " + currentAlarmNote);
        break;
      }
    }
  }
}
#endif
