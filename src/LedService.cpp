#include "LedService.h"

#include <cmath>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "Config.h"

void LedService::init(bool useHardwareRgb) {
  useHardwareRgb_ = useHardwareRgb;
  stateStartedAt_ = millis();
  lastUpdateAt_ = 0;
  pinMode(Config::STATUS_LED_PIN, OUTPUT);
  writeStatusLed(0);

  if (useHardwareRgb_) {
    ledcSetup(Config::LEDC_CH_RED, Config::RGB_PWM_FREQUENCY, Config::RGB_PWM_RESOLUTION);
    ledcSetup(Config::LEDC_CH_GREEN, Config::RGB_PWM_FREQUENCY, Config::RGB_PWM_RESOLUTION);
    ledcSetup(Config::LEDC_CH_BLUE, Config::RGB_PWM_FREQUENCY, Config::RGB_PWM_RESOLUTION);
    ledcAttachPin(Config::RGB_RED_PIN, Config::LEDC_CH_RED);
    ledcAttachPin(Config::RGB_GREEN_PIN, Config::LEDC_CH_GREEN);
    ledcAttachPin(Config::RGB_BLUE_PIN, Config::LEDC_CH_BLUE);
    writeRgb(0, 0, 0);
  }
}

void LedService::startAnimationTask() {
  if (animationTask_ == nullptr) {
    xTaskCreatePinnedToCore(animationTaskEntry, "ledAnimation", 2048, this, 1,
                            &animationTask_, 1);
  }
}

void LedService::animationTaskEntry(void* context) {
  LedService* service = static_cast<LedService*>(context);
  for (;;) {
    service->update();
    vTaskDelay(pdMS_TO_TICKS(LedService::UPDATE_INTERVAL_MS));
  }
}

void LedService::setState(State state) {
  if (state_ != state) {
    state_ = state;
    stateStartedAt_ = millis();
  }
}

uint8_t LedService::breathe(uint32_t now) const {
  constexpr float twoPi = 6.28318530718f;
  float phase = static_cast<float>(now % LedService::BREATH_PERIOD_MS) /
                static_cast<float>(LedService::BREATH_PERIOD_MS) * twoPi;
  return static_cast<uint8_t>(LedService::BREATH_MIN_BRIGHTNESS +
                              ((std::sin(phase) + 1.0f) * 0.5f *
                               (LedService::BREATH_MAX_BRIGHTNESS - LedService::BREATH_MIN_BRIGHTNESS)));
}

void LedService::writeRgb(uint8_t red, uint8_t green, uint8_t blue) {
  if (!useHardwareRgb_) {
    return;
  }
  if (Config::RGB_COMMON_ANODE) {
    red = 255 - red;
    green = 255 - green;
    blue = 255 - blue;
  }
  ledcWrite(Config::LEDC_CH_RED, red);
  ledcWrite(Config::LEDC_CH_GREEN, green);
  ledcWrite(Config::LEDC_CH_BLUE, blue);
}

void LedService::writeColor(uint32_t color, uint8_t brightness) {
  uint8_t red = static_cast<uint8_t>((color >> 16) & 0xFF);
  uint8_t green = static_cast<uint8_t>((color >> 8) & 0xFF);
  uint8_t blue = static_cast<uint8_t>(color & 0xFF);
  writeRgb(static_cast<uint8_t>(static_cast<uint16_t>(red) * brightness / 255),
           static_cast<uint8_t>(static_cast<uint16_t>(green) * brightness / 255),
           static_cast<uint8_t>(static_cast<uint16_t>(blue) * brightness / 255));
}

void LedService::writeStatusLed(uint8_t brightness) {
  uint8_t value = Config::STATUS_LED_ACTIVE_LOW ? 255 - brightness : brightness;
  analogWrite(Config::STATUS_LED_PIN, value);
}

void LedService::update() {
  uint32_t now = millis();
  if (now - lastUpdateAt_ < LedService::UPDATE_INTERVAL_MS) {
    return;
  }
  lastUpdateAt_ = now;

  switch (state_) {
    case State::IDLE:
      writeColor(LedService::IDLE_COLOR);
      writeStatusLed(0);
      break;
    case State::INITIALIZING: {
      uint8_t value = breathe(now);
      writeColor(LedService::INITIALIZING_COLOR, value);
      writeStatusLed(0);
      break;
    }
    case State::CONNECTING: {
      uint8_t value = breathe(now);
      writeColor(LedService::CONNECTING_COLOR, value);
      writeStatusLed(value);
      break;
    }
    case State::WAITING_SERVER: {
      uint8_t value = breathe(now);
      writeColor(LedService::SERVER_WAIT_COLOR, value);
      writeStatusLed(0);
      break;
    }
    case State::CAPTURING:
      writeColor(LedService::CAPTURING_COLOR);
      writeStatusLed(0);
      break;
    case State::GRANTED:
      writeColor(LedService::GRANTED_COLOR);
      writeStatusLed(0);
      break;
    case State::DENIED:
      writeColor(LedService::DENIED_COLOR);
      writeStatusLed(0);
      break;
  }
}