#include "ConfigManager.h"

bool ConfigManager::load() {
  Preferences preferences;
  if (!preferences.begin("config", true)) {
    return false;
  }
  ssid_ = preferences.getString("ssid", "");
  password_ = preferences.getString("password", "");
  serverUrl_ = preferences.getString("server", "");
  adminUsername_ = preferences.getString("admin_user", "");
  adminPassword_ = preferences.getString("admin_pass", "");
  apSsid_ = preferences.getString("ap_ssid", "DOOR-ACCESS-SETUP");
  apPassword_ = preferences.getString("ap_pass", "access-setup");
  preferences.end();
  return isValid();
}

bool ConfigManager::save(const String& ssid, const String& password, const String& serverUrl,
                         const String& adminUsername, const String& adminPassword,
                         const String& apSsid, const String& apPassword) {
  Preferences preferences;
  if (!preferences.begin("config", false)) {
    return false;
  }
  preferences.putString("ssid", ssid);
  preferences.putString("password", password);
  preferences.putString("server", serverUrl);
  preferences.putString("admin_user", adminUsername);
  preferences.putString("admin_pass", adminPassword);
  preferences.putString("ap_ssid", apSsid);
  preferences.putString("ap_pass", apPassword);
  preferences.end();
  ssid_ = ssid;
  password_ = password;
  serverUrl_ = serverUrl;
  adminUsername_ = adminUsername;
  adminPassword_ = adminPassword;
  apSsid_ = apSsid;
  apPassword_ = apPassword;
  return isValid();
}

bool ConfigManager::isValid() const {
    return ssid_.length() > 0 && serverUrl_.length() > 0 && adminUsername_.length() > 0 &&
      adminPassword_.length() > 0;
}

const String& ConfigManager::ssid() const { return ssid_; }
const String& ConfigManager::password() const { return password_; }
const String& ConfigManager::serverUrl() const { return serverUrl_; }
const String& ConfigManager::adminUsername() const { return adminUsername_; }
const String& ConfigManager::adminPassword() const { return adminPassword_; }
const String& ConfigManager::apSsid() const { return apSsid_; }
const String& ConfigManager::apPassword() const { return apPassword_; }