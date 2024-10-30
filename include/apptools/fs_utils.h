#pragma once
#include "esp_littlefs.h"

esp_err_t utils_littlefs_init(const char* partition_label);
esp_err_t compute_firmware_sha256(char* sha256_buf, size_t sha256_buf_size);
