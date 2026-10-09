#include <Arduino.h>
#include "Config.h"
#include "ResetDetector.h"

bool ResetDetector::check() {
  prefs_.begin("drd", false);

  bool isArmed = prefs_.getBool("armed", false);

  if (isArmed) {
    prefs_.putBool("armed", false);
    prefs_.end();
    armed_ = false;
    return true;
  }

  prefs_.putBool("armed", true);
  prefs_.end();

  armTime_ = millis();
  armed_ = true;
  if (expiryTaskHandle_ == nullptr) {
    xTaskCreatePinnedToCore(expiryTask, "drdExpiry", 2048, this, 1,
                            &expiryTaskHandle_, 0);
  }
  return false;
}

void ResetDetector::expiryTask(void* context) {
  ResetDetector* detector = static_cast<ResetDetector*>(context);
  vTaskDelay(pdMS_TO_TICKS(Config::DRD_TIMEOUT_MS));
  detector->expire();
  detector->expiryTaskHandle_ = nullptr;
  vTaskDelete(nullptr);
}

void ResetDetector::expire() {
  if (!armed_) {
    return;
  }
  if (millis() - armTime_ >= Config::DRD_TIMEOUT_MS) {
    clear();
  }
}

void ResetDetector::clear() {
  if (!prefs_.begin("drd", false)) {
    armed_ = false;
    return;
  }
  prefs_.putBool("armed", false);
  prefs_.end();
  armed_ = false;
}

void ResetDetector::update() {
  expire();
}
