#pragma once

#include <cstdint>

// Настройки, которые имеет смысл менять пользователю при настройке платы:
// распиновка, полярность подключения и тайминги работы. Внутренние настройки
// сервисов живут в их собственных заголовках.
namespace Config {
// Распиновка платы
constexpr uint8_t PN532_SDA_PIN = 14;
constexpr uint8_t PN532_SCL_PIN = 15;
constexpr uint8_t PN532_IRQ_PIN = 13;
constexpr uint8_t PN532_RESET_PIN = 255;
constexpr uint8_t RELAY_PIN = 12;
constexpr uint8_t RGB_RED_PIN = 2;
constexpr uint8_t RGB_GREEN_PIN = 3;
constexpr uint8_t RGB_BLUE_PIN = 1;
constexpr uint8_t STATUS_LED_PIN = 33;

// Подключение светодиодов
constexpr bool RGB_COMMON_ANODE = false;
constexpr bool STATUS_LED_ACTIVE_LOW = true;
constexpr uint32_t RGB_PWM_FREQUENCY = 5000;
constexpr uint8_t RGB_PWM_RESOLUTION = 8;
constexpr uint8_t LEDC_CH_RED = 4;
constexpr uint8_t LEDC_CH_GREEN = 5;
constexpr uint8_t LEDC_CH_BLUE = 6;

// Точка доступа для первоначальной настройки
constexpr const char* AP_SSID = "DOOR-ACCESS-SETUP";
constexpr const char* AP_PASSWORD = "access-setup";
constexpr uint8_t AP_CHANNEL = 6;
constexpr const char* AP_IP = "192.168.4.1";

// Тайминги работы
constexpr uint32_t ACCESS_GRANTED_TIME_MS = 3000;
constexpr uint32_t ACCESS_DENIED_TIME_MS = 1500;
constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 20000;
constexpr uint32_t WIFI_RETRY_INTERVAL_MS = 10000;
constexpr uint32_t SERVER_CHECK_INTERVAL_MS = 30000;
constexpr uint32_t DRD_TIMEOUT_MS = 2500;
constexpr uint32_t SERIAL_BAUD = 115200;
}