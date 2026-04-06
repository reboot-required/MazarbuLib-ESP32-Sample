// Copyright (c) 2026 Lukas Kraft
// https://github.com/reboot-required
//
// MazarbuLib-ESP32-Sample — advanced demo: system metrics and WiFi scan.
// Named after the Book of Mazarbul from J.R.R. Tolkien's writings.
//
// SPDX-License-Identifier: MIT

#include <Arduino.h>
#include <WiFi.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "mazarbulib.h"

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

static constexpr uint8_t kWifiMaxNets = 8;
static constexpr uint32_t kScanIntervalMs = 15000UL;
static constexpr uint32_t kScanRetryMs = 5000UL;
static constexpr uint32_t kTickIntervalMs = 1000UL;

// ---------------------------------------------------------------------------
// Library context
// ---------------------------------------------------------------------------

static mazarbulib_t g_lib;

// ---------------------------------------------------------------------------
// Screen 0 — System Info (14 rows)
// ---------------------------------------------------------------------------

static uint32_t g_uptime_s = 0;
static uint32_t g_free_heap = 0;
static uint32_t g_min_free_heap = 0;
static uint32_t g_lgst_free_blk = 0;
static float g_chip_temp = 0.0f;
static uint32_t g_cpu_freq_mhz = 0;
static uint32_t g_flash_kb = 0;
static uint32_t g_psram_kb = 0;
static uint32_t g_free_psram_kb = 0;
static uint32_t g_sketch_kb = 0;
static uint32_t g_free_sketch_kb = 0;
static char g_reset_reason[24] = {};
static const char *g_idf_version = nullptr; // set in setup()
static uint32_t g_rtos_tasks = 0;

// ---------------------------------------------------------------------------
// Screens 1 & 2 — WiFi Networks (up to kWifiMaxNets, 4 per screen)
// ---------------------------------------------------------------------------
// Each buffer is registered by address; updates are written in-place so
// MazarbuLib always reads current data without re-registration.

static char g_net_ssid[kWifiMaxNets][33] = {};
static int32_t g_net_rssi[kWifiMaxNets] = {};
static int32_t g_net_chan[kWifiMaxNets] = {};
static char g_net_sec[kWifiMaxNets][14] = {};

// ---------------------------------------------------------------------------
// Screen 3 — WiFi Scanner status (4 rows)
// ---------------------------------------------------------------------------

static char g_scan_status[20] = "initialising";
static int32_t g_net_count = 0;
static uint32_t g_scan_count = 0;
static uint32_t g_last_scan_s = 0;

// ---------------------------------------------------------------------------
// WiFi scan FSM state
// ---------------------------------------------------------------------------

static bool s_scan_running = false;
static uint32_t s_last_scan_ms = 0;
static uint32_t s_scan_interval_ms = kScanIntervalMs;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static const char *reset_reason_str(esp_reset_reason_t r) {
  switch (r) {
  case ESP_RST_POWERON:
    return "power-on";
  case ESP_RST_EXT:
    return "external pin";
  case ESP_RST_SW:
    return "software";
  case ESP_RST_PANIC:
    return "panic/exception";
  case ESP_RST_INT_WDT:
    return "interrupt wdt";
  case ESP_RST_TASK_WDT:
    return "task wdt";
  case ESP_RST_WDT:
    return "other wdt";
  case ESP_RST_DEEPSLEEP:
    return "deep sleep";
  case ESP_RST_BROWNOUT:
    return "brownout";
  case ESP_RST_SDIO:
    return "SDIO";
  default:
    return "unknown";
  }
}

static const char *enc_type_str(wifi_auth_mode_t m) {
  switch (m) {
  case WIFI_AUTH_OPEN:
    return "open";
  case WIFI_AUTH_WEP:
    return "WEP";
  case WIFI_AUTH_WPA_PSK:
    return "WPA";
  case WIFI_AUTH_WPA2_PSK:
    return "WPA2";
  case WIFI_AUTH_WPA_WPA2_PSK:
    return "WPA+WPA2";
  case WIFI_AUTH_WPA2_ENTERPRISE:
    return "WPA2-EAP";
  case WIFI_AUTH_WPA3_PSK:
    return "WPA3";
  case WIFI_AUTH_WPA2_WPA3_PSK:
    return "WPA2+WPA3";
  case WIFI_AUTH_WAPI_PSK:
    return "WAPI";
  default:
    return "unknown";
  }
}

static void clear_net_slots() {
  for (int i = 0; i < kWifiMaxNets; ++i) {
    strlcpy(g_net_ssid[i], "---", sizeof(g_net_ssid[i]));
    g_net_rssi[i] = 0;
    g_net_chan[i] = 0;
    strlcpy(g_net_sec[i], "---", sizeof(g_net_sec[i]));
  }
}

static void uart_send(const char *data, size_t len) {
  Serial.write(reinterpret_cast<const uint8_t *>(data), len);
}

static void terminal_clear(void) { Serial.print("\033[2J\033[H"); }

// ---------------------------------------------------------------------------
// setup
// ---------------------------------------------------------------------------

void setup() {
  Serial.begin(115200);

  // One-time static values.
  g_flash_kb = ESP.getFlashChipSize() / 1024;
  g_psram_kb = ESP.getPsramSize() / 1024;
  g_sketch_kb = ESP.getSketchSize() / 1024;
  g_free_sketch_kb = ESP.getFreeSketchSpace() / 1024;
  strlcpy(g_reset_reason, reset_reason_str(esp_reset_reason()),
          sizeof(g_reset_reason));
  clear_net_slots();

  g_idf_version = esp_get_idf_version();

  // Row labels for WiFi screens — static local guarantees program lifetime.
  static char net_labels[kWifiMaxNets][4][21];
  for (int i = 0; i < kWifiMaxNets; ++i) {
    snprintf(net_labels[i][0], 21, "Net %d SSID", i + 1);
    snprintf(net_labels[i][1], 21, "Net %d RSSI (dBm)", i + 1);
    snprintf(net_labels[i][2], 21, "Net %d Channel", i + 1);
    snprintf(net_labels[i][3], 21, "Net %d Security", i + 1);
  }

  mazarbulib_init(&g_lib, uart_send, terminal_clear);

  // Screen 0 — System Info (14 rows).
  int s0 = mazarbulib_register_screen(&g_lib, "System Info");
  mazarbulib_register_row(&g_lib, s0, "Uptime (s)", MAZARBULIB_TYPE_UINT32,
                          &g_uptime_s);
  mazarbulib_register_row(&g_lib, s0, "Free Heap (B)", MAZARBULIB_TYPE_UINT32,
                          &g_free_heap);
  mazarbulib_register_row(&g_lib, s0, "Min Free Heap (B)",
                          MAZARBULIB_TYPE_UINT32, &g_min_free_heap);
  mazarbulib_register_row(&g_lib, s0, "Lgst Free Blk (B)",
                          MAZARBULIB_TYPE_UINT32, &g_lgst_free_blk);
  mazarbulib_register_row(&g_lib, s0, "Chip Temp (C)", MAZARBULIB_TYPE_FLOAT,
                          &g_chip_temp);
  mazarbulib_register_row(&g_lib, s0, "CPU Freq (MHz)", MAZARBULIB_TYPE_UINT32,
                          &g_cpu_freq_mhz);
  mazarbulib_register_row(&g_lib, s0, "Flash (KB)", MAZARBULIB_TYPE_UINT32,
                          &g_flash_kb);
  mazarbulib_register_row(&g_lib, s0, "PSRAM (KB)", MAZARBULIB_TYPE_UINT32,
                          &g_psram_kb);
  mazarbulib_register_row(&g_lib, s0, "Free PSRAM (KB)", MAZARBULIB_TYPE_UINT32,
                          &g_free_psram_kb);
  mazarbulib_register_row(&g_lib, s0, "Sketch (KB)", MAZARBULIB_TYPE_UINT32,
                          &g_sketch_kb);
  mazarbulib_register_row(&g_lib, s0, "Free Sketch (KB)",
                          MAZARBULIB_TYPE_UINT32, &g_free_sketch_kb);
  mazarbulib_register_row(&g_lib, s0, "IDF Version", MAZARBULIB_TYPE_STRING,
                          g_idf_version);
  mazarbulib_register_row(&g_lib, s0, "Reset Reason", MAZARBULIB_TYPE_STRING,
                          g_reset_reason);
  mazarbulib_register_row(&g_lib, s0, "FreeRTOS Tasks", MAZARBULIB_TYPE_UINT32,
                          &g_rtos_tasks);

  // Screens 1 & 2 — WiFi Networks (16 rows each = 4 networks × 4 rows).
  for (int s = 0; s < 2; ++s) {
    const char *title = (s == 0) ? "WiFi Networks 1-4" : "WiFi Networks 5-8";
    int sid = mazarbulib_register_screen(&g_lib, title);
    for (int n = 0; n < 4; ++n) {
      int i = s * 4 + n;
      mazarbulib_register_row(&g_lib, sid, net_labels[i][0],
                              MAZARBULIB_TYPE_STRING, g_net_ssid[i]);
      mazarbulib_register_row(&g_lib, sid, net_labels[i][1],
                              MAZARBULIB_TYPE_INT32, &g_net_rssi[i]);
      mazarbulib_register_row(&g_lib, sid, net_labels[i][2],
                              MAZARBULIB_TYPE_INT32, &g_net_chan[i]);
      mazarbulib_register_row(&g_lib, sid, net_labels[i][3],
                              MAZARBULIB_TYPE_STRING, g_net_sec[i]);
    }
  }

  // Screen 3 — WiFi Scanner status (4 rows).
  int s3 = mazarbulib_register_screen(&g_lib, "WiFi Scanner");
  mazarbulib_register_row(&g_lib, s3, "Status", MAZARBULIB_TYPE_STRING,
                          g_scan_status);
  mazarbulib_register_row(&g_lib, s3, "Networks Found", MAZARBULIB_TYPE_INT32,
                          &g_net_count);
  mazarbulib_register_row(&g_lib, s3, "Scans Done", MAZARBULIB_TYPE_UINT32,
                          &g_scan_count);
  mazarbulib_register_row(&g_lib, s3, "Last Scan (s)", MAZARBULIB_TYPE_UINT32,
                          &g_last_scan_s);

  // Start WiFi in station mode and kick the first async scan.
  WiFi.mode(WIFI_STA);
  WiFi.scanNetworks(/*async=*/true);
  s_scan_running = true;
  s_last_scan_ms = millis();
  s_scan_interval_ms = kScanIntervalMs;
  strlcpy(g_scan_status, "scanning", sizeof(g_scan_status));
}

// ---------------------------------------------------------------------------
// loop
// ---------------------------------------------------------------------------

void loop() {
  uint32_t now_ms = millis();

  // Update live system values.
  g_uptime_s = now_ms / 1000UL;
  g_free_heap = esp_get_free_heap_size();
  g_min_free_heap = esp_get_minimum_free_heap_size();
  g_lgst_free_blk =
      static_cast<uint32_t>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
  g_chip_temp = temperatureRead();
  g_cpu_freq_mhz = getCpuFrequencyMhz();
  g_free_psram_kb = ESP.getFreePsram() / 1024;
  g_rtos_tasks = static_cast<uint32_t>(uxTaskGetNumberOfTasks());

  // WiFi scan state machine — no blocking delay().
  if (s_scan_running) {
    int n = WiFi.scanComplete();
    if (n >= 0) {
      // Scan finished: copy results into in-place buffers.
      clear_net_slots();
      int count = (n < kWifiMaxNets) ? n : kWifiMaxNets;
      for (int i = 0; i < count; ++i) {
        strlcpy(g_net_ssid[i], WiFi.SSID(i).c_str(), sizeof(g_net_ssid[i]));
        g_net_rssi[i] = static_cast<int32_t>(WiFi.RSSI(i));
        g_net_chan[i] = static_cast<int32_t>(WiFi.channel(i));
        strlcpy(g_net_sec[i], enc_type_str(WiFi.encryptionType(i)),
                sizeof(g_net_sec[i]));
      }
      g_net_count = n;
      g_scan_count += 1;
      g_last_scan_s = now_ms / 1000UL;
      snprintf(g_scan_status, sizeof(g_scan_status), "idle (%d nets)", n);
      WiFi.scanDelete();
      s_scan_running = false;
      s_last_scan_ms = now_ms;
      s_scan_interval_ms = kScanIntervalMs;
    } else if (n == WIFI_SCAN_FAILED) {
      strlcpy(g_scan_status, "scan failed", sizeof(g_scan_status));
      s_scan_running = false;
      s_last_scan_ms = now_ms;
      s_scan_interval_ms = kScanRetryMs;
    }
    // n == WIFI_SCAN_RUNNING: still in progress, nothing to do.
  } else {
    if (now_ms - s_last_scan_ms >= s_scan_interval_ms) {
      WiFi.scanNetworks(/*async=*/true);
      s_scan_running = true;
      s_last_scan_ms = now_ms;
      strlcpy(g_scan_status, "scanning", sizeof(g_scan_status));
    }
  }

  // Feed received serial bytes to the library.
  while (Serial.available()) {
    mazarbulib_feed_char(&g_lib, static_cast<char>(Serial.read()));
  }

  // Render tick every kTickIntervalMs.
  static uint32_t s_last_tick_ms = 0;
  if (now_ms - s_last_tick_ms >= kTickIntervalMs) {
    s_last_tick_ms = now_ms;
    mazarbulib_tick(&g_lib);
  }
}
