#pragma once

#include <Arduino.h>
#include "esp_camera.h"

struct CameraFrame {
  uint8_t* data = nullptr;
  size_t length = 0;
};

struct CameraBurst {
  static constexpr uint8_t COUNT = 1;

  CameraFrame frames[COUNT];

  void release();
};

class CameraService {
public:
  static constexpr uint32_t POWER_SETTLE_MS = 100;
  static constexpr uint8_t INIT_RETRIES = 2;

  bool begin();
  bool isReady() const;
  bool testCapture(size_t& frameSize);
  bool captureBurst(CameraBurst& burst);

private:
  bool ready_ = false;
};