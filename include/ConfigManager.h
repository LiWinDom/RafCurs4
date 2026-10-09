#pragma once

#include <Arduino.h>
#include <Preferences.h>

class ConfigManager {
public:
  static constexpr size_t MAX_SSID_LENGTH = 32;
  static constexpr size_t MAX_PASSWORD_LENGTH = 64;
  static constexpr size_t MAX_SERVER_URL_LENGTH = 192;
  static constexpr size_t MAX_ADMIN_USERNAME_LENGTH = 32;
  static constexpr size_t MAX_ADMIN_PASSWORD_LENGTH = 64;
  static constexpr size_t MAX_AP_SSID_LENGTH = 32;
  static constexpr size_t MAX_AP_PASSWORD_LENGTH = 64;

  bool load();
  bool save(const String& ssid, const String& password, const String& serverUrl,
            const String& adminUsername, const String& adminPassword,
            const String& apSsid, const String& apPassword);
  bool isValid() const;
  const String& ssid() const;
  const String& password() const;
  const String& serverUrl() const;
  const String& adminUsername() const;
  const String& adminPassword() const;
  const String& apSsid() const;
  const String& apPassword() const;

private:
  String ssid_;
  String password_;
  String serverUrl_;
  String adminUsername_;
  String adminPassword_;
  String apSsid_;
  String apPassword_;
};