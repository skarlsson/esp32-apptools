#pragma once
#include "esp_err.h"
#include "sdkconfig.h"

#if defined(CONFIG_ESP32_WIFI_ENABLED) || defined(CONFIG_ESP_WIFI_ENABLED)

esp_err_t utils_wifi_init(const char* ssid, const char* password);

#endif