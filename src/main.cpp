#include <Arduino.h>
#include <WiFi.h>
#include <esp_camera.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cstring>

#include "CameraService.h"
#include "Config.h"
#include "ConfigManager.h"
#include "LedService.h"
#include "Logger.h"
#include "NetworkService.h"
#include "NfcService.h"
#include "ResetDetector.h"
#include "WebPortal.h"

namespace {
enum class RuntimeMode : uint8_t { HOTSPOT, NORMAL };
enum class NormalState : uint8_t { CONNECTING, INITIALIZING, READY, PROCESSING, COOLDOWN };

ConfigManager configManager;
ResetDetector resetDetector;
LedService ledService;
CameraService cameraService;
NfcService nfcService;
NetworkService networkService;
WebPortal webPortal;
CameraBurst cameraBurst;

RuntimeMode runtimeMode = RuntimeMode::HOTSPOT;
NormalState normalState = NormalState::CONNECTING;
uint32_t wifiStartedAt = 0;
uint32_t cooldownStartedAt = 0;
uint32_t relayStartedAt = 0;
bool relayActive = false;
bool debugPortalStarted = false;
bool serverCheckCompleted = false;
bool serverAvailable = false;
volatile bool serverCheckRequested = false;
volatile bool serverCheckRunning = false;
volatile bool serverCheckResultReady = false;
portMUX_TYPE serverCheckMux = portMUX_INITIALIZER_UNLOCKED;
char serverCheckResult[192] = "Проверка ещё не выполнялась";
uint32_t lastServerCheckAt = 0;

portMUX_TYPE wifiTestMux = portMUX_INITIALIZER_UNLOCKED;
char wifiTestSsid[ConfigManager::MAX_SSID_LENGTH + 1] = "";
char wifiTestPassword[ConfigManager::MAX_PASSWORD_LENGTH + 1] = "";
char wifiTestMessage[192] = "";
volatile bool wifiTestRequested = false;
volatile bool wifiTestRunning = false;
volatile bool wifiTestFinished = false;

void closeRelay() {
  if (!relayActive) {
    return;
  }
  digitalWrite(Config::RELAY_PIN, LOW);
  relayActive = false;
  Logger::info("Relay closed, was open for %u ms",
               static_cast<unsigned>(millis() - relayStartedAt));
}

void updateRelay() {
  if (relayActive && millis() - relayStartedAt >= Config::ACCESS_GRANTED_TIME_MS) {
    closeRelay();
  }
}

void restoreReadyLed() {
  if (!serverCheckCompleted) {
    ledService.setState(LedService::State::WAITING_SERVER);
  } else if (!serverAvailable) {
    ledService.setState(LedService::State::DENIED);
  } else {
    ledService.setState(LedService::State::IDLE);
  }
}

void serverHealthTask(void*) {
  for (;;) {
    if (serverCheckRequested && !serverCheckRunning) {
      serverCheckRequested = false;
      serverCheckRunning = true;
      Logger::info("Server health check started");
      String result = networkService.checkServer(configManager.serverUrl());
      portENTER_CRITICAL(&serverCheckMux);
      strncpy(serverCheckResult, result.c_str(), sizeof(serverCheckResult) - 1);
      serverCheckResult[sizeof(serverCheckResult) - 1] = '\0';
      serverCheckResultReady = true;
      serverCheckRunning = false;
      portEXIT_CRITICAL(&serverCheckMux);
    }
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

void requestServerHealthCheck() {
  if (!serverCheckRunning) {
    serverCheckRequested = true;
  }
}

void wifiTestTask(void*) {
  for (;;) {
    if (wifiTestRequested && !wifiTestRunning) {
      wifiTestRequested = false;
      wifiTestRunning = true;
      char ssid[ConfigManager::MAX_SSID_LENGTH + 1];
      char password[ConfigManager::MAX_PASSWORD_LENGTH + 1];
      portENTER_CRITICAL(&wifiTestMux);
      strncpy(ssid, wifiTestSsid, sizeof(ssid) - 1);
      ssid[sizeof(ssid) - 1] = '\0';
      strncpy(password, wifiTestPassword, sizeof(password) - 1);
      password[sizeof(password) - 1] = '\0';
      portEXIT_CRITICAL(&wifiTestMux);
      String result = networkService.testConnection(ssid, password, configManager.ssid(),
                                                   configManager.password());
      portENTER_CRITICAL(&wifiTestMux);
      strncpy(wifiTestMessage, result.c_str(), sizeof(wifiTestMessage) - 1);
      wifiTestMessage[sizeof(wifiTestMessage) - 1] = '\0';
      portEXIT_CRITICAL(&wifiTestMux);
      wifiTestFinished = true;
      wifiTestRunning = false;
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void requestWifiTest(const String& ssid, const String& password) {
  if (wifiTestRunning) {
    return;
  }
  portENTER_CRITICAL(&wifiTestMux);
  strncpy(wifiTestSsid, ssid.c_str(), sizeof(wifiTestSsid) - 1);
  wifiTestSsid[sizeof(wifiTestSsid) - 1] = '\0';
  strncpy(wifiTestPassword, password.c_str(), sizeof(wifiTestPassword) - 1);
  wifiTestPassword[sizeof(wifiTestPassword) - 1] = '\0';
  portEXIT_CRITICAL(&wifiTestMux);
  wifiTestFinished = false;
  wifiTestRequested = true;
}

size_t readWifiTestStatus(String& state, String& message) {
  char buffer[sizeof(wifiTestMessage)];
  bool requested = wifiTestRequested;
  bool running = wifiTestRunning;
  bool finished = wifiTestFinished;
  portENTER_CRITICAL(&wifiTestMux);
  strncpy(buffer, wifiTestMessage, sizeof(buffer) - 1);
  buffer[sizeof(buffer) - 1] = '\0';
  portEXIT_CRITICAL(&wifiTestMux);
  if (finished) {
    state = "done";
    message = buffer;
  } else if (running || requested) {
    state = "running";
    message = "Проверяем подключение к сети...";
  } else {
    state = "idle";
    message = "Проверка не запускалась";
  }
  return 0;
}

void pollCardForDebug() {
  static uint32_t lastPollAt = 0;
  static String lastUid;
  if (millis() - lastPollAt < NfcService::DEBUG_POLL_INTERVAL_MS) {
    return;
  }
  lastPollAt = millis();
  String uid;
  if (!nfcService.isReady() || !nfcService.readUid(uid) || uid == lastUid) {
    return;
  }
  lastUid = uid;
  webPortal.setCardState("UID " + uid + ", " + String(millis() / 1000) + " с");
  Logger::info("Debug card detected: %s", uid.c_str());
}

void applyServerHealthResult() {
  if (!serverCheckResultReady) {
    return;
  }
  char resultBuffer[sizeof(serverCheckResult)];
  portENTER_CRITICAL(&serverCheckMux);
  strncpy(resultBuffer, serverCheckResult, sizeof(resultBuffer) - 1);
  resultBuffer[sizeof(resultBuffer) - 1] = '\0';
  serverCheckResultReady = false;
  portEXIT_CRITICAL(&serverCheckMux);

  String result(resultBuffer);
  serverAvailable = result.startsWith("Сервер доступен");
  serverCheckCompleted = true;
  lastServerCheckAt = millis();
  webPortal.setServerStatus(result);
  if (serverAvailable) {
    Logger::info("Server health check passed: %s", result.c_str());
  } else {
    Logger::error("Server health check failed: %s", result.c_str());
  }
  if (normalState == NormalState::READY) {
    restoreReadyLed();
  }
}

String readCardForDebug() {
  String uid;
  if (!nfcService.isReady() && !nfcService.begin()) {
    return "PN532 не обнаружен";
  }
  if (!nfcService.readUid(uid)) {
    return "Карта не обнаружена или UID не поддерживается";
  }
  Logger::info("Debug NFC read UID: %s", uid.c_str());
  return "UID: " + uid;
}

void streamCameraImage(WebServer& server) {
  if (!cameraService.isReady()) {
    server.send(503, "text/plain; charset=utf-8", "Камера не инициализирована");
    return;
  }
  camera_fb_t* frame = esp_camera_fb_get();
  if (frame == nullptr) {
    Logger::error("Debug camera capture failed");
    server.send(503, "text/plain; charset=utf-8", "Не удалось получить изображение");
    return;
  }
  size_t length = frame->len;
  server.setContentLength(length);
  server.send(200, "image/jpeg");
  server.sendContent(reinterpret_cast<const char*>(frame->buf), length);
  esp_camera_fb_return(frame);
  Logger::info("Debug camera capture completed, frame size: %u bytes", length);
}

bool streamCameraFrame(WebServer& server) {
  if (!cameraService.isReady()) {
    return false;
  }
  camera_fb_t* frame = esp_camera_fb_get();
  if (frame == nullptr) {
    Logger::error("Stream capture failed");
    return server.client().connected();
  }
  String part = "--frame\r\nContent-Type: image/jpeg\r\nContent-Length: " + String(frame->len) + "\r\n\r\n";
  server.sendContent(part.c_str());
  server.sendContent(reinterpret_cast<const char*>(frame->buf), frame->len);
  server.sendContent("\r\n");
  esp_camera_fb_return(frame);
  return server.client().connected();
}

String checkHardware() {
  bool nfcAvailable = nfcService.checkConnection();
  size_t frameSize = 0;
  bool cameraAvailable = cameraService.isReady() && cameraService.testCapture(frameSize);
  Logger::info("Hardware test: PN532=%s, camera=%s", nfcAvailable ? "yes" : "no",
               cameraAvailable ? "yes" : "no");
  return String("PN532: ") + (nfcAvailable ? "подключен" : "не обнаружен") +
         "; камера: " + (cameraAvailable ? "работает" : "не работает");
}

void updateDebugStatus() {
  switch (normalState) {
    case NormalState::CONNECTING:
      webPortal.setRuntimeStatus("Подключение к Wi-Fi");
      break;
    case NormalState::INITIALIZING:
      webPortal.setRuntimeStatus("Инициализация PN532 и камеры");
      break;
    case NormalState::READY:
      if (!nfcService.isReady()) {
        webPortal.setRuntimeStatus("Ошибка: PN532 не обнаружен");
      } else if (!cameraService.isReady()) {
        webPortal.setRuntimeStatus("Ошибка: камера не инициализирована");
      } else if (!serverCheckCompleted) {
        webPortal.setRuntimeStatus("Ожидание ответа сервера");
      } else if (!serverAvailable) {
        webPortal.setRuntimeStatus("Ошибка: сервер недоступен");
      } else {
        webPortal.setRuntimeStatus("Ожидание NFC-карты");
      }
      break;
    case NormalState::PROCESSING:
      webPortal.setRuntimeStatus("Обработка доступа");
      break;
    case NormalState::COOLDOWN:
      webPortal.setRuntimeStatus(relayActive ? "Доступ разрешён, реле активно" : "Пауза после операции");
      break;
  }
}

size_t scanWifiNetworks(NetworkService::WifiNetwork* networks, size_t maxCount) {
  return networkService.scanNetworks(networks, maxCount);
}

void startHotspotMode() {
  Serial.begin(Config::SERIAL_BAUD);
  Logger::setSerialOutputEnabled(true);
  webPortal.beginHotspot(configManager);
  webPortal.setHardwareCheckHandler(checkHardware);
  webPortal.setCardReadHandler(readCardForDebug);
  webPortal.setCameraHandler(streamCameraImage);
  webPortal.setCameraFrameHandler(streamCameraFrame);
  webPortal.setWifiScanHandler(scanWifiNetworks);
  webPortal.setWifiTestHandler(requestWifiTest);
  webPortal.setWifiTestStatusHandler(readWifiTestStatus);
  Logger::info("Hotspot hardware initialization started");
  nfcService.begin();
  cameraService.begin();
  Logger::info("Hotspot hardware initialization finished");
}

void startNormalMode() {
  Logger::setSerialOutputEnabled(false);
  ledService.init(true);
  ledService.startAnimationTask();
  ledService.setState(LedService::State::INITIALIZING);
  pinMode(Config::RELAY_PIN, OUTPUT);
  digitalWrite(Config::RELAY_PIN, LOW);
  nfcService.begin();
  cameraService.begin();
  ledService.setState(LedService::State::CONNECTING);
  WiFi.mode(WIFI_STA);
  WiFi.begin(configManager.ssid().c_str(), configManager.password().c_str());
  wifiStartedAt = millis();
  normalState = NormalState::CONNECTING;
  xTaskCreatePinnedToCore(serverHealthTask, "serverHealth", 6144, nullptr, 1, nullptr, 0);
}

void updateNormalMode() {
  resetDetector.update();
  updateRelay();
  applyServerHealthResult();
  if (normalState == NormalState::CONNECTING) {
    if (WiFi.status() == WL_CONNECTED) {
      webPortal.beginNormal(configManager);
      webPortal.setHardwareCheckHandler(checkHardware);
      webPortal.setCardReadHandler(readCardForDebug);
      webPortal.setCameraHandler(streamCameraImage);
      webPortal.setCameraFrameHandler(streamCameraFrame);
          webPortal.setWifiScanHandler(scanWifiNetworks);
      webPortal.setWifiTestHandler(requestWifiTest);
      webPortal.setWifiTestStatusHandler(readWifiTestStatus);
      debugPortalStarted = true;
      serverCheckCompleted = false;
      serverAvailable = false;
      lastServerCheckAt = millis();
      ledService.setState(LedService::State::WAITING_SERVER);
      normalState = NormalState::READY;
    } else if (millis() - wifiStartedAt >= Config::WIFI_CONNECT_TIMEOUT_MS) {
      WiFi.disconnect();
      WiFi.begin(configManager.ssid().c_str(), configManager.password().c_str());
      wifiStartedAt = millis();
    }
    return;
  }
  if (normalState == NormalState::COOLDOWN) {
    if (millis() - cooldownStartedAt >= (relayActive ? Config::ACCESS_GRANTED_TIME_MS
                                                     : Config::ACCESS_DENIED_TIME_MS)) {
      restoreReadyLed();
      normalState = NormalState::READY;
    }
    return;
  }
  if (normalState != NormalState::READY || WiFi.status() != WL_CONNECTED) {
    closeRelay();
    return;
  }
  if (!serverCheckRunning && !serverCheckRequested &&
      millis() - lastServerCheckAt >= Config::SERVER_CHECK_INTERVAL_MS) {
    requestServerHealthCheck();
  }

  String uid;
  if (!nfcService.readUid(uid)) {
    return;
  }
  Logger::info("NFC card detected: %s", uid.c_str());
  webPortal.setCardState("UID " + uid + ", " + String(millis() / 1000) + " с");
  normalState = NormalState::PROCESSING;
  ledService.setState(LedService::State::CAPTURING);
  if (!cameraService.captureBurst(cameraBurst)) {
    Logger::error("Access request aborted because capture failed");
    ledService.setState(LedService::State::DENIED);
    cooldownStartedAt = millis();
    normalState = NormalState::COOLDOWN;
    return;
  }
  bool granted = networkService.sendAccessRequest(configManager.serverUrl(), uid, cameraBurst);
  cameraBurst.release();
  if (granted) {
    digitalWrite(Config::RELAY_PIN, HIGH);
    relayStartedAt = millis();
    relayActive = true;
    cooldownStartedAt = relayStartedAt;
    ledService.setState(LedService::State::GRANTED);
  } else {
    cooldownStartedAt = millis();
    ledService.setState(LedService::State::DENIED);
  }
  normalState = NormalState::COOLDOWN;
}
}

void setup() {
  bool hasConfig = configManager.load();
  bool doubleReset = hasConfig ? resetDetector.check() : false;
  if (!hasConfig) {
    resetDetector.clear();
  }
  runtimeMode = (!hasConfig || doubleReset) ? RuntimeMode::HOTSPOT : RuntimeMode::NORMAL;
  if (runtimeMode == RuntimeMode::HOTSPOT) {
    startHotspotMode();
  } else {
    startNormalMode();
  }
  xTaskCreatePinnedToCore(wifiTestTask, "wifiTest", 6144, nullptr, 1, nullptr, 0);
}

void loop() {
  resetDetector.update();
  if (runtimeMode == RuntimeMode::HOTSPOT) {
    pollCardForDebug();
    webPortal.update();
  } else {
    if (debugPortalStarted) {
      webPortal.update();
    }
    updateDebugStatus();
    updateNormalMode();
  }
  yield();
}
