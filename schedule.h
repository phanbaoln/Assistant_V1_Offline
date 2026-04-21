#ifndef SCHEDULE_H
#define SCHEDULE_H

#include <Preferences.h>
#include <ArduinoJson.h>

static Preferences preferences;

struct Task {
  int hour;
  int minute;
  String note;
  bool isEnabled;
};

const int MAX_TASKS = 10; 
static Task dailySchedule[MAX_TASKS];
static int taskCount = 0;

// Các biến extern để đồng bộ với main.ino
extern volatile bool isAlarmActive;
extern String currentAlarmNote;
extern unsigned long lastClockUpdate;

// 1. Đọc bộ nhớ khi khởi động
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
  Serial.printf(">>> Da tai %d lich trinh tu Flash.\n", taskCount);
}

// 2. Xóa toàn bộ lịch
inline void clearAllSchedule() {
  preferences.begin("clock_data", false);
  preferences.clear(); 
  preferences.putInt("taskCount", 0); 
  preferences.end();

  taskCount = 0;
  for (int i = 0; i < MAX_TASKS; i++) {
    dailySchedule[i].isEnabled = false;
  }
  lastClockUpdate = 0; // Ve lai man hinh
  Serial.println(">>> Da xoa sach lich trinh!");
}

// 3. Hàm cốt lõi để thêm lịch (Dùng chung cho cả Mic và Web)
inline void addSchedule(int h, int m, String note) {
  if (taskCount >= MAX_TASKS) {
    Serial.println(">>> Bo nho lich day! Dang ghi de len lich cu nhat...");
    // Neu day, ta xoa lich cu nhat de ghi de (Tuy chon logic)
    taskCount = 0; 
    preferences.begin("clock_data", false);
    preferences.clear();
    preferences.end();
  }
  
  // Luu vao mang tam thoi
  dailySchedule[taskCount] = {h, m, note, true};
  
  // Luu vao Flash (Permanent Memory)
  preferences.begin("clock_data", false);
  preferences.putInt(("h_" + String(taskCount)).c_str(), h);
  preferences.putInt(("m_" + String(taskCount)).c_str(), m);
  preferences.putString(("n_" + String(taskCount)).c_str(), note);
  
  taskCount++; 
  preferences.putInt("taskCount", taskCount);
  preferences.end();
  
  lastClockUpdate = 0; // Cap nhat giao dien de hien lich moi
  Serial.printf(">>> Da luu lich: %02d:%02d - %s\n", h, m, note.c_str());
}

// 4. Kiểm tra giờ để kích hoạt báo thức
inline void checkSchedule(int currentH, int currentM) {
  if (isAlarmActive) return; 

  for (int i = 0; i < taskCount; i++) {
    if (dailySchedule[i].isEnabled && 
        dailySchedule[i].hour == currentH && 
        dailySchedule[i].minute == currentM) {
          
      isAlarmActive = true;
      currentAlarmNote = dailySchedule[i].note;
      
      // Sau khi bao thuc xong thi tam thoi tat nhiem vu nay
      dailySchedule[i].isEnabled = false; 
      Serial.println(">>> KICH HOAT BAO THUC: " + currentAlarmNote);
      break;
    }
  }
}

// 5. Liệt kê danh sách (Cho AI hoac Debug)
inline String getListSchedule() {
  if (taskCount == 0) return "Ban chua co lich trinh nao.";
  String list = "Lich trinh:\n";
  for (int i = 0; i < taskCount; i++) {
    char buf[50];
    sprintf(buf, "- %02d:%02d: %s\n", dailySchedule[i].hour, dailySchedule[i].minute, dailySchedule[i].note.c_str());
    list += String(buf);
  }
  return list;
}

#endif
