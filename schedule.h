#ifndef SCHEDULE_H
#define SCHEDULE_H
#include <Preferences.h>
#include <ArduinoJson.h>

// --- CẤU TRÚC ---
struct Task {
  int hour;
  int minute;
  String note;
  String audioUrl;
  bool isEnabled;
};

const int MAX_TASKS = 10; 

// 👉 ĐỔI THÀNH EXTERN: Để main.ino và ai.h dùng chung một túi tiền
extern Task dailySchedule[MAX_TASKS];
extern int taskCount;

extern Preferences preferences;

extern volatile bool isAlarmActive;
extern String currentAlarmNote;
extern unsigned long lastClockUpdate;

// 1. Tải dữ liệu (Giữ nguyên)
inline void initSchedule() {
  preferences.begin("clock_data", false);
  taskCount = preferences.getInt("taskCount", 0);
  if (taskCount > MAX_TASKS) taskCount = MAX_TASKS;

  for (int i = 0; i < taskCount; i++) {
    dailySchedule[i].hour = preferences.getInt(("h_" + String(i)).c_str(), 0);
    dailySchedule[i].minute = preferences.getInt(("m_" + String(i)).c_str(), 0);
    dailySchedule[i].note = preferences.getString(("n_" + String(i)).c_str(), "");
    dailySchedule[i].isEnabled = true;
  }
  preferences.end();
  Serial.printf(">>> MÈO đã nạp %d lịch trình.\n", taskCount);
}

// 2. Thêm lịch (Ghi đè thông minh)
inline void addSchedule(int h, int m, String note) {
  // Nếu đầy, ghi đè vào vị trí cũ nhất (vị trí 0)
  int writeIdx = taskCount % MAX_TASKS; 
  
  dailySchedule[writeIdx].hour = h;
  dailySchedule[writeIdx].minute = m;
  dailySchedule[writeIdx].note = note;
  dailySchedule[writeIdx].isEnabled = true;
  
  preferences.begin("clock_data", false);
  preferences.putInt(("h_" + String(writeIdx)).c_str(), h);
  preferences.putInt(("m_" + String(writeIdx)).c_str(), m);
  preferences.putString(("n_" + String(writeIdx)).c_str(), note);
  
  if (taskCount < MAX_TASKS) {
    taskCount++;
    preferences.putInt("taskCount", taskCount);
  }
  preferences.end();
  
  lastClockUpdate = 0; 
  Serial.printf(">>> Đã lưu: %02d:%02d\n", h, m);
}

// 3. Kiểm tra báo thức (Chống reo lặp)
inline void checkSchedule(int currentH, int currentM) {
  if (isAlarmActive) return; 
  static int lastTriggerMin = -1; // Chặn Zombie Alarm

  if (currentM == lastTriggerMin) return;

  for (int i = 0; i < taskCount; i++) {
    if (dailySchedule[i].isEnabled && 
        dailySchedule[i].hour == currentH && 
        dailySchedule[i].minute == currentM) {
          
      isAlarmActive = true;
      currentAlarmNote = dailySchedule[i].note;
      lastTriggerMin = currentM; 
      
      // Nếu muốn mai reo tiếp thì đừng set isEnabled = false ở đây
      // Hoặc reset lại isEnabled vào lúc nửa đêm.
      Serial.println(">>> [ALARM] KÍCH HOẠT: " + currentAlarmNote);
      break;
    }
  }
}

// 4. Xóa sạch (Giữ nguyên)
inline void clearAllSchedule() {
  preferences.begin("clock_data", false);
  preferences.clear(); 
  preferences.putInt("taskCount", 0); 
  preferences.end();
  taskCount = 0;
  lastClockUpdate = 0;
  Serial.println(">>> Đã dọn dẹp sạch lịch trình.");
}

#endif
