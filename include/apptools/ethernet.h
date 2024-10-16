#pragma once
#include "esp_err.h"
#include "sdkconfig.h"
#include "soc/soc_caps.h"

//#if defined(SOC_EMAC_SUPPORTED) || defined(CONFIG_ETH_ENABLED) || defined(CONFIG_ESP32_EMAC_SUPPORTED)
#if defined(SOC_EMAC_SUPPORTED) ||  defined(CONFIG_ESP32_EMAC_SUPPORTED)
esp_err_t utils_ethernet_init(void);
#endif
