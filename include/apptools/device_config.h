#pragma once
#include <esp_err.h>

struct device_config_t {
    enum {
        MAX_MANUFACTURER_LENGTH = 32,
        MAX_MODEL_LENGTH = 32,
        MAX_EQUIPMENT_ID_LENGTH = 37, // UUID string length (36) + null terminator
        MAX_HW_VERSION_LENGTH = 32,
        MAX_SW_VERSION_LENGTH = 96, // Room for version string + SHA256 hash + separators
        SHA_LENGTH=65, // 64 + null
    };

    device_config_t() {
    }

    char manufacturer[MAX_MANUFACTURER_LENGTH] = {};
    char model[MAX_MODEL_LENGTH] = {};
    char eid[MAX_EQUIPMENT_ID_LENGTH] = {};
    char hardware_revision[MAX_HW_VERSION_LENGTH] = {};
    char software_revision[MAX_SW_VERSION_LENGTH] = {}; // Not saved to disk
    char sw_sha256[SHA_LENGTH] = {}; // Not saved to disk
};

esp_err_t device_config_init(const char *manufacturer, const char *model, const char *hardware_revision,
                             device_config_t *config);

esp_err_t device_config_init(device_config_t *config);

//esp_err_t load_or_init_device_config(const char* manufacturer, const char* model, const char* hardware_revision, device_config_t* config);
