#include "CameraService.h"

#include <cstring>

#include "Config.h"
#include "Logger.h"

namespace {
constexpr uint8_t CAMERA_PIN_PWDN = 32;
constexpr int8_t CAMERA_PIN_RESET = -1;
constexpr uint8_t CAMERA_PIN_XCLK = 0;
constexpr uint8_t CAMERA_PIN_SIOD = 26;
constexpr uint8_t CAMERA_PIN_SIOC = 27;
constexpr uint8_t CAMERA_PIN_D7 = 35;
constexpr uint8_t CAMERA_PIN_D6 = 34;
constexpr uint8_t CAMERA_PIN_D5 = 39;
constexpr uint8_t CAMERA_PIN_D4 = 36;
constexpr uint8_t CAMERA_PIN_D3 = 21;
constexpr uint8_t CAMERA_PIN_D2 = 19;
constexpr uint8_t CAMERA_PIN_D1 = 18;
constexpr uint8_t CAMERA_PIN_D0 = 5;
constexpr uint8_t CAMERA_PIN_VSYNC = 25;
constexpr uint8_t CAMERA_PIN_HREF = 23;
constexpr uint8_t CAMERA_PIN_PCLK = 22;
constexpr uint32_t CAMERA_XCLK_FREQUENCY = 2000000;
constexpr uint8_t CAMERA_LEDC_CHANNEL = 0;
constexpr uint8_t CAMERA_LEDC_TIMER = 0;
constexpr uint8_t CAMERA_JPEG_QUALITY = 10;
constexpr uint8_t CAMERA_FRAME_BUFFER_COUNT = 1;
}

void CameraBurst::release() {
  for (CameraFrame& frame : frames) {
    if (frame.data != nullptr) {
      free(frame.data);
      frame.data = nullptr;
      frame.length = 0;
    }
  }
}

bool CameraService::begin() {
  ready_ = false;
  pinMode(CAMERA_PIN_PWDN, OUTPUT);
  digitalWrite(CAMERA_PIN_PWDN, LOW);
  delay(CameraService::POWER_SETTLE_MS);
  esp_camera_deinit();

  Logger::info("Camera start: PSRAM=%s, XCLK=%u Hz", psramFound() ? "yes" : "no",
               CAMERA_XCLK_FREQUENCY);
  camera_config_t config{};
  config.ledc_channel = static_cast<ledc_channel_t>(CAMERA_LEDC_CHANNEL);
  config.ledc_timer = static_cast<ledc_timer_t>(CAMERA_LEDC_TIMER);
  config.pin_d0 = CAMERA_PIN_D0;
  config.pin_d1 = CAMERA_PIN_D1;
  config.pin_d2 = CAMERA_PIN_D2;
  config.pin_d3 = CAMERA_PIN_D3;
  config.pin_d4 = CAMERA_PIN_D4;
  config.pin_d5 = CAMERA_PIN_D5;
  config.pin_d6 = CAMERA_PIN_D6;
  config.pin_d7 = CAMERA_PIN_D7;
  config.pin_xclk = CAMERA_PIN_XCLK;
  config.pin_pclk = CAMERA_PIN_PCLK;
  config.pin_vsync = CAMERA_PIN_VSYNC;
  config.pin_href = CAMERA_PIN_HREF;
  config.pin_sccb_sda = CAMERA_PIN_SIOD;
  config.pin_sccb_scl = CAMERA_PIN_SIOC;
  config.pin_pwdn = CAMERA_PIN_PWDN;
  config.pin_reset = CAMERA_PIN_RESET;
  config.xclk_freq_hz = CAMERA_XCLK_FREQUENCY;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_SVGA;
  config.jpeg_quality = CAMERA_JPEG_QUALITY;
  config.fb_count = CAMERA_FRAME_BUFFER_COUNT;
  config.grab_mode = CAMERA_GRAB_LATEST;
  config.fb_location = CAMERA_FB_IN_PSRAM;

  esp_err_t result = ESP_FAIL;
  for (uint8_t attempt = 0; attempt < CameraService::INIT_RETRIES; ++attempt) {
    result = esp_camera_init(&config);
    if (result == ESP_OK) {
      break;
    }
    Logger::error("Camera initialization attempt %u failed: 0x%X", attempt + 1, result);
    esp_camera_deinit();
    digitalWrite(CAMERA_PIN_PWDN, HIGH);
    delay(20);
    digitalWrite(CAMERA_PIN_PWDN, LOW);
    delay(CameraService::POWER_SETTLE_MS);
  }
  if (result != ESP_OK) {
    Logger::error("Camera initialization failed after %u attempts: 0x%X",
                  CameraService::INIT_RETRIES, result);
    return false;
  }

  sensor_t* sensor = esp_camera_sensor_get();
  if (sensor != nullptr) {
    sensor->set_lenc(sensor, 1);
    sensor->set_bpc(sensor, 1);
    sensor->set_wpc(sensor, 1);
    sensor->set_whitebal(sensor, 1);
    sensor->set_exposure_ctrl(sensor, 1);
  }
  Logger::info("Camera initialized");
  ready_ = true;
  return true;
}

bool CameraService::isReady() const {
  return ready_;
}

bool CameraService::testCapture(size_t& frameSize) {
  frameSize = 0;
  if (!ready_) {
    return false;
  }
  camera_fb_t* buffer = esp_camera_fb_get();
  if (buffer == nullptr) {
    return false;
  }
  frameSize = buffer->len;
  esp_camera_fb_return(buffer);
  return frameSize > 0;
}

bool CameraService::captureBurst(CameraBurst& burst) {
  if (!ready_) {
    return false;
  }
  burst.release();
  for (uint8_t index = 0; index < CameraBurst::COUNT; ++index) {
    CameraFrame& frame = burst.frames[index];
    camera_fb_t* buffer = esp_camera_fb_get();
    if (buffer == nullptr) {
      Logger::error("Camera capture failed");
      burst.release();
      return false;
    }
    frame.data = static_cast<uint8_t*>(ps_malloc(buffer->len));
    if (frame.data == nullptr) {
      esp_camera_fb_return(buffer);
      Logger::error("Camera frame allocation failed");
      burst.release();
      return false;
    }
    memcpy(frame.data, buffer->buf, buffer->len);
    frame.length = buffer->len;
    esp_camera_fb_return(buffer);
  }
  return true;
}