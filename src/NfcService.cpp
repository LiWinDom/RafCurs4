#include "NfcService.h"

#include <Wire.h>

#include "Config.h"
#include "Logger.h"

NfcService::NfcService() : nfc_(Config::PN532_IRQ_PIN, Config::PN532_RESET_PIN, &Wire1) {}

bool NfcService::begin() {
  Wire1.begin(Config::PN532_SDA_PIN, Config::PN532_SCL_PIN);
  Wire1.setClock(100000);
  Wire1.setTimeOut(100);

  nfc_.begin();
  Wire1.end();
  Wire1.begin(Config::PN532_SDA_PIN, Config::PN532_SCL_PIN);
  Wire1.setClock(100000);
  Wire1.setTimeOut(100);

  uint32_t version = nfc_.getFirmwareVersion();
  if (version == 0) {
    Logger::error("PN532 was not detected");
    ready_ = false;
    return false;
  }

  Logger::info("Found chip PN5%02X, Firmware: %d.%d",
               (version >> 24) & 0xFF, (version >> 16) & 0xFF, (version >> 8) & 0xFF);

  if (!nfc_.SAMConfig()) {
    Logger::error("SAMConfig failed");
    return false;
  }

  nfc_.setPassiveActivationRetries(1);

  ready_ = true;
  Logger::info("PN532 initialized successfully");
  return true;
}

bool NfcService::isReady() const {
  return ready_;
}

bool NfcService::checkConnection() {
  uint32_t version = nfc_.getFirmwareVersion();
  ready_ = version != 0;
  return ready_;
}

void NfcService::update() {}

bool NfcService::readUid(String& uid) {
  if (!ready_ || millis() - lastPollAt_ < NfcService::POLL_INTERVAL_MS) {
    return false;
  }
  lastPollAt_ = millis();

  uint8_t rawUid[NfcService::MAX_UID_LENGTH] = {};
  uint8_t uidLength = 0;

  if (!nfc_.readPassiveTargetID(PN532_MIFARE_ISO14443A, rawUid, &uidLength, 50)) {
    return false;
  }

  if (uidLength != NfcService::REQUIRED_UID_LENGTH) {
    return false;
  }

  uid = "";
  for (uint8_t index = 0; index < uidLength; ++index) {
    if (rawUid[index] < 0x10) uid += "0";
    uid += String(rawUid[index], HEX);
  }
  uid.toUpperCase();
  return true;
}
