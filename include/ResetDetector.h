#pragma once
#include <Arduino.h>
#include <Preferences.h>

class ResetDetector {
public:
  bool check();
  void clear();
  void update();

private:
  static void expiryTask(void* context);
  void expire();

  Preferences prefs_;
  uint32_t armTime_ = 0;
  bool armed_ = false;
  TaskHandle_t expiryTaskHandle_ = nullptr;
};
