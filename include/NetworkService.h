#pragma once

#include <WiFiClient.h>

#include "CameraService.h"

class NetworkService {
public:
  static constexpr uint32_t SCAN_CACHE_MS = 15000;
  static constexpr uint8_t SCAN_MAX_NETWORKS = 20;
  static constexpr uint32_t HTTP_TIMEOUT_MS = 10000;
  static constexpr size_t HTTP_CHUNK_SIZE = 1024;
  static constexpr const char* UPLOAD_PATH = "/api/v1/access";
  static constexpr const char* FIELD_UID = "uid";
  static constexpr uint32_t TEST_TIMEOUT_MS = 15000;
  static constexpr uint32_t TEST_RESTORE_TIMEOUT_MS = 8000;

  struct WifiNetwork {
    String ssid;
    int32_t rssi = 0;
    bool secure = false;
  };

  bool sendAccessRequest(const String& serverUrl, const String& uid, const CameraBurst& burst);
  String checkServer(const String& serverUrl);
  size_t scanNetworks(WifiNetwork* networks, size_t maxCount);
  String testConnection(const String& ssid, const String& password,
                        const String& currentSsid, const String& currentPassword);

private:
  bool parseUrl(const String& url, String& host, uint16_t& port, String& path) const;
  bool writePart(WiFiClient& client, const String& boundary, const String& uid,
                const CameraFrame& frame) const;
};