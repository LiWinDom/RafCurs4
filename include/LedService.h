#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>

class LedService {
public:
  static constexpr uint32_t UPDATE_INTERVAL_MS = 20;
  static constexpr uint32_t BREATH_PERIOD_MS = 2200;
  static constexpr uint8_t BREATH_MIN_BRIGHTNESS = 12;
  static constexpr uint8_t BREATH_MAX_BRIGHTNESS = 180;

  static constexpr uint32_t IDLE_COLOR = 0xFFFFFF;
  static constexpr uint32_t INITIALIZING_COLOR = 0x00FFFF;
  static constexpr uint32_t CONNECTING_COLOR = 0x0000FF;
  static constexpr uint32_t SERVER_WAIT_COLOR = 0xFF00FF;
  static constexpr uint32_t CAPTURING_COLOR = 0xFFFF00;
  static constexpr uint32_t GRANTED_COLOR = 0x00C800;
  static constexpr uint32_t DENIED_COLOR = 0xDC0000;

  enum class State : uint8_t { IDLE, INITIALIZING, CONNECTING, WAITING_SERVER, CAPTURING, GRANTED, DENIED };

  void init(bool useHardwareRgb);
  void setState(State state);
  void update();
  void startAnimationTask();

private:
  void writeRgb(uint8_t red, uint8_t green, uint8_t blue);
  void writeColor(uint32_t color, uint8_t brightness = 255);
  void writeStatusLed(uint8_t brightness);
  uint8_t breathe(uint32_t now) const;

  bool useHardwareRgb_ = false;
  State state_ = State::IDLE;
  uint32_t stateStartedAt_ = 0;
  uint32_t lastUpdateAt_ = 0;
  TaskHandle_t animationTask_ = nullptr;

  static void animationTaskEntry(void* context);
};