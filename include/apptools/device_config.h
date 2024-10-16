#pragma once
#include <esp_err.h>

#define MAX_MANUFACTURER_LENGTH 16
#define MAX_MODEL_LENGTH 20
#define MAX_EQUIPMENT_ID_LENGTH 37  // UUID string length (36) + null terminator
#define MAX_VERSION_LENGTH 16

struct device_config_t {
    device_config_t() {}
    char manufacturer[MAX_MANUFACTURER_LENGTH] = {};
    char model[MAX_MODEL_LENGTH] = {};
    char eid[MAX_EQUIPMENT_ID_LENGTH] = {};
    char hardware_revision[MAX_VERSION_LENGTH] = {};
};

esp_err_t device_config_init(const char* manufacturer, const char* model, const char* hardware_revision, device_config_t* config);
esp_err_t device_config_init(device_config_t* config);

//esp_err_t load_or_init_device_config(const char* manufacturer, const char* model, const char* hardware_revision, device_config_t* config);