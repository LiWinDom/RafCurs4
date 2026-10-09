#include "NetworkService.h"

#include <ArduinoJson.h>
#include <WiFi.h>

#include "Config.h"
#include "Logger.h"

namespace {
// Тело multipart собирается теми же функциями, по которым считается
// Content-Length: так заголовок и тело запроса не могут разойтись.
String uidPart(const String& boundary, const String& uid) {
  return "--" + boundary + "\r\nContent-Disposition: form-data; name=\"" +
         String(NetworkService::FIELD_UID) + "\"\r\n\r\n" + uid + "\r\n";
}

String photoHeader(const String& boundary) {
  return "--" + boundary +
         "\r\nContent-Disposition: form-data; name=\"photo\"; filename=\"capture.jpg\"\r\n"
         "Content-Type: image/jpeg\r\n\r\n";
}

String closingPart(const String& boundary) {
  return "--" + boundary + "--\r\n";
}
}

bool NetworkService::parseUrl(const String& url, String& host, uint16_t& port, String& path) const {
  String value = url;
  if (value.startsWith("https://")) {
    return false;
  }
  if (value.startsWith("http://")) {
    value.remove(0, 7);
  }
  int slash = value.indexOf('/');
  String authority = slash < 0 ? value : value.substring(0, slash);
  path = slash < 0 ? String("/") : value.substring(slash);
  int colon = authority.lastIndexOf(':');
  port = 80;
  if (colon > 0) {
    port = static_cast<uint16_t>(authority.substring(colon + 1).toInt());
    authority = authority.substring(0, colon);
  }
  host = authority;
  return host.length() > 0 && port > 0;
}

bool NetworkService::writePart(WiFiClient& client, const String& boundary, const String& uid,
                               const CameraFrame& frame) const {
  client.print(uidPart(boundary, uid));
  client.print(photoHeader(boundary));
  size_t sent = 0;
  while (sent < frame.length) {
    size_t chunk = frame.length - sent;
    if (chunk > NetworkService::HTTP_CHUNK_SIZE) {
      chunk = NetworkService::HTTP_CHUNK_SIZE;
    }
    if (client.write(frame.data + sent, chunk) != chunk) {
      return false;
    }
    sent += chunk;
  }
  client.print("\r\n" + closingPart(boundary));
  return true;
}

bool NetworkService::sendAccessRequest(const String& serverUrl, const String& uid,
                                       const CameraBurst& burst) {
  String host;
  String path;
  uint16_t port = 0;
  if (!parseUrl(serverUrl, host, port, path)) {
    Logger::error("Only HTTP server URLs are supported");
    return false;
  }
  if (!path.endsWith("/")) {
    path += "/";
  }
  path += String(NetworkService::UPLOAD_PATH).substring(1);

  String boundary = "DoorAccessBoundary" + String(millis());
  const CameraFrame& frame = burst.frames[0];
  size_t contentLength = uidPart(boundary, uid).length() + photoHeader(boundary).length() +
                         frame.length + closingPart(boundary).length() + 2;

  WiFiClient client;
  client.setTimeout(NetworkService::HTTP_TIMEOUT_MS);
  if (!client.connect(host.c_str(), port)) {
    Logger::error("Could not connect to access server");
    return false;
  }
  client.print("POST " + path + " HTTP/1.1\r\nHost: " + host + "\r\nConnection: close\r\n");
  client.print("Content-Type: multipart/form-data; boundary=" + boundary + "\r\n");
  client.print("Content-Length: " + String(contentLength) + "\r\n\r\n");
  if (!writePart(client, boundary, uid, frame)) {
    client.stop();
    return false;
  }

  String statusLine = client.readStringUntil('\n');
  while (client.connected() && client.readStringUntil('\n') != "\r") {}
  String response;
  uint32_t startedAt = millis();
  while (client.connected() && millis() - startedAt < NetworkService::HTTP_TIMEOUT_MS) {
    while (client.available()) {
      response += static_cast<char>(client.read());
      if (response.length() > 4096) {
        response.remove(4096);
      }
      startedAt = millis();
    }
  }
  client.stop();

  int statusCode = statusLine.substring(9, 12).toInt();
  if (statusCode < 200 || statusCode >= 300) {
    Logger::error("Access server returned HTTP %d", statusCode);
    return false;
  }
  JsonDocument document;
  if (deserializeJson(document, response)) {
    Logger::error("Invalid JSON response from access server");
    return false;
  }
  return document["access"] | false;
}

String NetworkService::checkServer(const String& serverUrl) {
  String host;
  String path;
  uint16_t port = 0;
  if (!parseUrl(serverUrl, host, port, path)) {
    return "Некорректный адрес сервера или HTTPS не поддерживается";
  }
  WiFiClient client;
  client.setTimeout(NetworkService::HTTP_TIMEOUT_MS);
  uint32_t startedAt = millis();
  if (!client.connect(host.c_str(), port)) {
    return "Сервер недоступен";
  }
  client.print("GET " + path + " HTTP/1.1\r\nHost: " + host +
               "\r\nConnection: close\r\n\r\n");
  String statusLine = client.readStringUntil('\n');
  client.stop();
  int statusCode = statusLine.substring(9, 12).toInt();
  if (statusCode >= 200 && statusCode < 500) {
    return "Сервер доступен, HTTP " + String(statusCode) +
           ", время " + String(millis() - startedAt) + " мс";
  }
  return "Ошибка ответа сервера, HTTP " + String(statusCode);
}

size_t NetworkService::scanNetworks(WifiNetwork* networks, size_t maxCount) {
  if (maxCount == 0) {
    return 0;
  }
  bool wasConnected = WiFi.status() == WL_CONNECTED;
  int found = WiFi.scanNetworks(false, false, 0);
  size_t count = 0;
  for (int index = 0; index < found; ++index) {
    String ssid = WiFi.SSID(index);
    if (ssid.length() == 0) {
      continue;
    }
    int32_t rssi = WiFi.RSSI(index);
    size_t slot = count;
    bool duplicate = false;
    for (size_t existing = 0; existing < count; ++existing) {
      if (networks[existing].ssid == ssid) {
        slot = existing;
        duplicate = true;
        break;
      }
    }
    if (!duplicate && count >= maxCount) {
      continue;
    }
    networks[slot].ssid = ssid;
    networks[slot].rssi = rssi;
    networks[slot].secure = WiFi.encryptionType(index) != WIFI_AUTH_OPEN;
    if (!duplicate) {
      ++count;
    }
  }
  WiFi.scanDelete();
  for (size_t index = 1; index < count; ++index) {
    WifiNetwork current = networks[index];
    size_t position = index;
    while (position > 0 && networks[position - 1].rssi < current.rssi) {
      networks[position] = networks[position - 1];
      --position;
    }
    networks[position] = current;
  }
  if (wasConnected && WiFi.status() != WL_CONNECTED) {
    WiFi.reconnect();
  }
  return count;
}

String NetworkService::testConnection(const String& ssid, const String& password,
                                      const String& currentSsid, const String& currentPassword) {
  if (ssid.length() == 0) {
    return "Не указано имя сети";
  }
  bool wasConnected = WiFi.status() == WL_CONNECTED;
  WiFi.disconnect();
  delay(250);
  WiFi.begin(ssid.c_str(), password.c_str());
  uint32_t startedAt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startedAt < NetworkService::TEST_TIMEOUT_MS) {
    delay(100);
  }

  String result;
  if (WiFi.status() == WL_CONNECTED) {
    result = "Подключение успешно: IP " + WiFi.localIP().toString() + ", сигнал " +
             String(WiFi.RSSI()) + " dBm";
  } else {
    result = "Не удалось подключиться к «" + ssid +
             "». Проверьте имя сети и пароль, либо что точка доступа включена.";
  }
  Logger::info("Wi-Fi test for '%s': %s", ssid.c_str(), result.c_str());

  WiFi.disconnect();
  if (wasConnected && currentSsid.length() > 0) {
    WiFi.begin(currentSsid.c_str(), currentPassword.c_str());
    uint32_t restoreStartedAt = millis();
    while (WiFi.status() != WL_CONNECTED &&
           millis() - restoreStartedAt < NetworkService::TEST_RESTORE_TIMEOUT_MS) {
      delay(100);
    }
    result += WiFi.status() == WL_CONNECTED
                  ? ". Прежнее подключение восстановлено."
                  : ". Прежнее подключение не восстановилось.";
  }
  return result;
}
