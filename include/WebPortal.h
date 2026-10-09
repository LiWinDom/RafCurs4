#pragma once

#include <DNSServer.h>
#include <WebServer.h>

#include "ConfigManager.h"
#include "NetworkService.h"

class WebPortal {
public:
  static constexpr uint32_t STREAM_INTERVAL_MS = 100;
  static constexpr uint32_t STREAM_MAX_MS = 60000;

  using HardwareCheckHandler = String (*)();
  using CardReadHandler = String (*)();
  using CameraHandler = void (*)(WebServer& server);
  using CameraFrameHandler = bool (*)(WebServer& server);
  using WifiScanHandler = size_t (*)(NetworkService::WifiNetwork* networks, size_t maxCount);
  using WifiTestHandler = void (*)(const String& ssid, const String& password);
  using WifiTestStatusHandler = size_t (*)(String& state, String& message);

  void beginHotspot(ConfigManager& config);
  void beginNormal(ConfigManager& config);
  void setHardwareCheckHandler(HardwareCheckHandler handler);
  void setCardReadHandler(CardReadHandler handler);
  void setCameraHandler(CameraHandler handler);
  void setCameraFrameHandler(CameraFrameHandler handler);
  void setWifiScanHandler(WifiScanHandler handler);
  void setWifiTestHandler(WifiTestHandler handler);
  void setWifiTestStatusHandler(WifiTestStatusHandler handler);
  void setCardState(const String& state);
  void setServerStatus(const String& status);
  void setRuntimeStatus(const String& status);
  void update();

private:
  void registerRoutes();
  void handleRoot();
  void handleStatusApi();
  void handleHardwareCheck();
  void handleCardRead();
  void handleCamera();
  void handleCameraStream();
  void handleSave();
  void handleWifiScan();
  void handleWifiTest();
  void handleWifiTestStatus();
  String renderPage() const;
  String statusMetrics() const;
  String htmlEscape(const String& value) const;
  bool authenticate();

  ConfigManager* config_ = nullptr;
  String runtimeStatus_ = "Не запущено";
  String serverStatus_ = "Проверка ещё не выполнялась";
  String hardwareStatus_ = "Проверка ещё не выполнялась";
  String cardState_ = "Карта не приложена";
  HardwareCheckHandler hardwareCheckHandler_ = nullptr;
  CardReadHandler cardReadHandler_ = nullptr;
  CameraHandler cameraHandler_ = nullptr;
  CameraFrameHandler cameraFrameHandler_ = nullptr;
  WifiScanHandler wifiScanHandler_ = nullptr;
  WifiTestHandler wifiTestHandler_ = nullptr;
  WifiTestStatusHandler wifiTestStatusHandler_ = nullptr;
  String scanCache_;
  uint32_t lastScanAt_ = 0;
  bool captiveMode_ = false;
  DNSServer dnsServer_;
  WebServer webServer_{80};
};
