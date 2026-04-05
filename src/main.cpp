// Copyright (c) 2026 Lukas Kraft
// https://github.com/reboot-required
//
// MazarbuLib-ESP32-Sample — demonstrates MazarbuLib on ESP32-WROOM-32.
// Named after the Book of Mazarbul from J.R.R. Tolkien's writings.
//
// SPDX-License-Identifier: MIT

#include <Arduino.h>
#include <esp_system.h>

#include "mazarbulib.h"  // NOLINT(build/include_subdir)

namespace {

constexpr uint32_t kSerialBaudRate = 115200;
constexpr uint32_t kScreenRefreshIntervalMs = 1000;
constexpr uint8_t kGpio2Pin = 2;
constexpr uint8_t kGpio4Pin = 4;
constexpr uint8_t kGpio5Pin = 5;

struct AppState {
  uint32_t uptime_s = 0;
  uint32_t free_heap = 0;
  float chip_temp_c = 0.0f;
  bool gpio2 = false;
  bool gpio4 = false;
  bool gpio5 = false;
};

mazarbulib_t g_lib;
AppState g_app_state;
uint32_t g_last_tick_ms = 0;
uint32_t g_last_uptime_sample_ms = 0;
uint64_t g_accumulated_uptime_ms = 0;

bool HasIntervalElapsed(uint32_t now, uint32_t last_tick_ms,
                        uint32_t interval_ms) {
  // Unsigned subtraction keeps elapsed-time checks correct across millis()
  // wrap-around on ESP32.
  return static_cast<uint32_t>(now - last_tick_ms) >= interval_ms;
}

void UartSend(const char* data, size_t len) {
  if (data == nullptr || len == 0U) {
    return;
  }
  Serial.write(reinterpret_cast<const uint8_t *>(data), len);
}

void TerminalClear() {
  Serial.print("\033[2J\033[H");
}

bool RegisterScreenRow(int screen_index, const char* label,
                       mazarbulib_type_t type, const void* value_ptr) {
  return mazarbulib_register_row(&g_lib, screen_index, label, type,
                                 value_ptr) == MAZARBULIB_OK;
}

bool InitializeScreens() {
  if (mazarbulib_init(&g_lib, UartSend, TerminalClear) != MAZARBULIB_OK) {
    return false;
  }

  const int system_info_screen =
      mazarbulib_register_screen(&g_lib, "System Info");
  if (system_info_screen < 0 ||
      !RegisterScreenRow(system_info_screen, "Uptime (s)",
                         MAZARBULIB_TYPE_UINT32, &g_app_state.uptime_s) ||
      !RegisterScreenRow(system_info_screen, "Free Heap (B)",
                         MAZARBULIB_TYPE_UINT32, &g_app_state.free_heap) ||
      !RegisterScreenRow(system_info_screen, "Chip Temp (C)",
                         MAZARBULIB_TYPE_FLOAT, &g_app_state.chip_temp_c)) {
    return false;
  }

  const int gpio_status_screen =
      mazarbulib_register_screen(&g_lib, "GPIO Status");
  return gpio_status_screen >= 0 &&
         RegisterScreenRow(gpio_status_screen, "GPIO2 (LED)",
                           MAZARBULIB_TYPE_BOOL, &g_app_state.gpio2) &&
         RegisterScreenRow(gpio_status_screen, "GPIO4", MAZARBULIB_TYPE_BOOL,
                           &g_app_state.gpio4) &&
         RegisterScreenRow(gpio_status_screen, "GPIO5", MAZARBULIB_TYPE_BOOL,
                           &g_app_state.gpio5);
}

void HaltWithError(const char* error_message) {
  Serial.println(error_message);
  while (true) {
    delay(kScreenRefreshIntervalMs);
  }
}

void UpdateTelemetry() {
  const uint32_t now = millis();
  g_accumulated_uptime_ms += now - g_last_uptime_sample_ms;
  g_last_uptime_sample_ms = now;
  g_app_state.uptime_s = static_cast<uint32_t>(g_accumulated_uptime_ms / 1000U);
  g_app_state.free_heap = esp_get_free_heap_size();
  g_app_state.chip_temp_c = temperatureRead();
  g_app_state.gpio2 = digitalRead(kGpio2Pin) != LOW;
  g_app_state.gpio4 = digitalRead(kGpio4Pin) != LOW;
  g_app_state.gpio5 = digitalRead(kGpio5Pin) != LOW;
}

void ProcessSerialInput() {
  while (Serial.available() > 0) {
    mazarbulib_feed_char(&g_lib, static_cast<char>(Serial.read()));
  }
}

void TickScreenIfDue() {
  const uint32_t now = millis();
  if (!HasIntervalElapsed(now, g_last_tick_ms, kScreenRefreshIntervalMs)) {
    return;
  }

  g_last_tick_ms = now;
  mazarbulib_tick(&g_lib);
}

}  // namespace

void setup() {
  Serial.begin(kSerialBaudRate);
  g_last_uptime_sample_ms = millis();

  pinMode(kGpio2Pin, INPUT);
  pinMode(kGpio4Pin, INPUT);
  pinMode(kGpio5Pin, INPUT);

  if (!InitializeScreens()) {
    HaltWithError("Failed to initialize MazarbuLib screens.");
  }
}

void loop() {
  UpdateTelemetry();
  ProcessSerialInput();
  TickScreenIfDue();
}
