#pragma once

#include <Arduino.h>
#include <cstdarg>
#include <cstddef>
#include <cstdint>

class Logger {
public:
  static constexpr size_t CAPACITY = 50;

  static void info(const char* format, ...);
  static void warn(const char* format, ...);
  static void error(const char* format, ...);
  static void setSerialOutputEnabled(bool enabled);
  static String getAllLogs();

private:
  static constexpr uint8_t ENTRY_CAPACITY = 192;

  static void addLog(const char* level, const char* format, va_list args);
  static char logBuffer[CAPACITY][ENTRY_CAPACITY];
  static uint8_t headIndex;
  static uint8_t totalLogs;
  static bool serialOutputEnabled;
};