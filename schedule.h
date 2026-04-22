#ifndef SCHEDULE_H
#define SCHEDULE_H

#include <Preferences.h>

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
  
  // 👉 CHỐT CHẶN: Ngăn tràn bộ nhớ nếu bản cũ lưu nhiều hơn MAX_TASKS
  if (taskCount > MAX_TASKS) taskCount = MAX_TASKS; 

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
inline void addSchedule(int h, int m, String note, uint8_t repeatDays) {
  if (taskCount >= MAX_TASKS) taskCount = 0; // Ghi đè vòng lặp nếu đầy

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
  Serial.printf(">>> Đã lưu lịch: %02d:%02d\n", h, m);
}

// 3. Kiểm tra thông minh theo ngày trong tuần
inline void checkSchedule(int currentH, int currentM, int currentDay) {
  if (isAlarmActive) return;

  // 👉 THÊM: Biến lưu lại phút vừa reo để trị bệnh "Zombie Alarm"
  static int lastTriggerMinute = -1; 
  
  if (currentM == lastTriggerMinute) {
      return; // Nếu vẫn đang trong phút vừa tắt báo thức -> Bỏ qua
  }

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
        lastTriggerMinute = currentM; // 👉 Ghi nhớ phút này
        Serial.println(">>> TRÚC REO: " + currentAlarmNote);
        break; // Chỉ reo 1 báo thức tại một thời điểm
      }
    }
  }
}

// 4. Xóa toàn bộ lịch trình
inline void clearAllSchedule() {
  preferences.begin("clock_data", false);
  preferences.clear(); 
  preferences.putInt("taskCount", 0); 
  preferences.end();

  taskCount = 0;
  for (int i = 0; i < MAX_TASKS; i++) {
    dailySchedule[i].isEnabled = false;
  }
  lastClockUpdate = 0; 
  Serial.println(">>> Đã xóa sạch lịch trình!");
}

// 5. Liệt kê danh sách lịch trình (Cho AI đọc)
inline String getListSchedule() {
  if (taskCount == 0) return "Chưa có lịch trình nào được lưu.";
  
  String list = "DANH SÁCH LỊCH TRÌNH:\n";
  for (int i = 0; i < taskCount; i++) {
    char timeBuf[10];
    sprintf(timeBuf, "- %02d:%02d: ", dailySchedule[i].hour, dailySchedule[i].minute);
    list += String(timeBuf);
    list += dailySchedule[i].note;

    if (dailySchedule[i].repeatDays == 0) {
      list += " (Một lần)";
    } else if (dailySchedule[i].repeatDays == 127) {
      list += " (Hàng ngày)";
    } else {
      list += " (";
      bool first = true;
      const char* dayNames[] = {"CN", "T2", "T3", "T4", "T5", "T6", "T7"};
      for (int d = 0; d < 7; d++) {
        if ((dailySchedule[i].repeatDays >> d) & 1) {
          if (!first) list += ", ";
          list += dayNames[d];
          first = false;
        }
      }
      list += ")";
    }
    list += "\n";
  }
  return list;
}

#endif
