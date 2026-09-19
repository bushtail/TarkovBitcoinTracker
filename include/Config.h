#pragma once

#include <Arduino.h>

static constexpr unsigned long REFRESH_INTERVAL_MS = 5UL * 60UL * 1000UL;
static constexpr unsigned long RETRY_INTERVAL_MS = 30UL * 1000UL;
static constexpr unsigned long WIFI_RETRY_INTERVAL_MS = 20UL * 1000UL;
static constexpr uint16_t HTTP_TIMEOUT_MS = 15000;
static constexpr uint8_t I2C_SDA_PIN = 21;
static constexpr uint8_t I2C_SCL_PIN = 22;
static constexpr uint8_t LCD_I2C_ADDRESS = 0x27;
static constexpr uint8_t LCD_COLS = 16;
static constexpr uint8_t LCD_ROWS = 2;
