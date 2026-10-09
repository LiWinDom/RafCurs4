#pragma once

#include <Adafruit_PN532.h>

class NfcService {
public:
  static constexpr uint32_t POLL_INTERVAL_MS = 120;
  static constexpr uint32_t DEBUG_POLL_INTERVAL_MS = 250;
  static constexpr uint8_t MAX_UID_LENGTH = 10;
  static constexpr uint8_t REQUIRED_UID_LENGTH = 7;

  NfcService();
  bool begin();
  bool checkConnection();
  bool isReady() const;
  bool readUid(String& uid);
  void update();

private:
  Adafruit_PN532 nfc_;
  uint32_t lastPollAt_ = 0;
  bool ready_ = false;
};