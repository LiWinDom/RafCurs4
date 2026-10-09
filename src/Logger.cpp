#include "Logger.h"

#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>

#include <cstring>

namespace {
portMUX_TYPE logMux = portMUX_INITIALIZER_UNLOCKED;
}

char Logger::logBuffer[Logger::CAPACITY][Logger::ENTRY_CAPACITY] = {};
uint8_t Logger::headIndex = 0;
uint8_t Logger::totalLogs = 0;
bool Logger::serialOutputEnabled = false;

void Logger::addLog(const char* level, const char* format, va_list args) {
  char message[ENTRY_CAPACITY];
  vsnprintf(message, sizeof(message), format, args);

  char entry[ENTRY_CAPACITY];
  int length = snprintf(entry, sizeof(entry), "[%u] [%s] %s",
                        static_cast<unsigned>(millis()), level, message);
  if (length < 0) {
    return;
  }
  if (length >= static_cast<int>(sizeof(entry))) {
    length = static_cast<int>(sizeof(entry)) - 1;
  }

  if (serialOutputEnabled) {
    Serial.println(entry);
  }

  portENTER_CRITICAL(&logMux);
  strncpy(logBuffer[headIndex], entry, ENTRY_CAPACITY - 1);
  logBuffer[headIndex][ENTRY_CAPACITY - 1] = '\0';
  headIndex = (headIndex + 1) % Logger::CAPACITY;
  if (totalLogs < Logger::CAPACITY) {
    totalLogs++;
  }
  portEXIT_CRITICAL(&logMux);
}

void Logger::setSerialOutputEnabled(bool enabled) {
  serialOutputEnabled = enabled;
}

void Logger::info(const char* format, ...) {
  va_list args;
  va_start(args, format);
  addLog("INFO", format, args);
  va_end(args);
}

void Logger::warn(const char* format, ...) {
  va_list args;
  va_start(args, format);
  addLog("WARN", format, args);
  va_end(args);
}

void Logger::error(const char* format, ...) {
  va_list args;
  va_start(args, format);
  addLog("ERROR", format, args);
  va_end(args);
}

String Logger::getAllLogs() {
  char entry[ENTRY_CAPACITY];
  uint8_t head = 0;
  uint8_t count = 0;
  portENTER_CRITICAL(&logMux);
  head = headIndex;
  count = totalLogs;
  portEXIT_CRITICAL(&logMux);

  String result;
  result.reserve(static_cast<size_t>(count) * ENTRY_CAPACITY);
  uint8_t start = count < Logger::CAPACITY ? 0 : head;
  for (uint8_t i = 0; i < count; ++i) {
    uint8_t index = (start + i) % Logger::CAPACITY;
    portENTER_CRITICAL(&logMux);
    strncpy(entry, logBuffer[index], ENTRY_CAPACITY - 1);
    entry[ENTRY_CAPACITY - 1] = '\0';
    portEXIT_CRITICAL(&logMux);
    result += entry;
    result += '\n';
  }
  return result;
}