#pragma once
#include "esp_err.h"

esp_err_t initialize_sntp(int timeout_s = 60);