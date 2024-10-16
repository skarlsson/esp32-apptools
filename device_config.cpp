#include <apptools/device_config.h>
#include "esp_log.h"
#include "esp_random.h"
#include <string.h>
#include "cJSON.h"

#define CONFIG_FILE_PATH "/mnt/config.json"
#define MOUNT_POINT "/mnt"

static const char* TAG = "device_config";

static char buffer_s[1024];

static void generate_uuid(char* uuid_str) {
    uint32_t random_values[4];
    for (int i = 0; i < 4; i++) {
        random_values[i] = esp_random();
    }

    snprintf(uuid_str, MAX_EQUIPMENT_ID_LENGTH,
             "%08" PRIx32 "-%04" PRIx16 "-%04" PRIx16 "-%04" PRIx16 "-%04" PRIx16 "%08" PRIx32,
             random_values[0],
             (uint16_t)(random_values[1] >> 16),
             ((uint16_t)random_values[1] & 0xffff) | 0x4000,  // Version 4
             ((uint16_t)(random_values[2] >> 16) & 0x3fff) | 0x8000,  // Variant 1
             (uint16_t)random_values[2],
             random_values[3]);
}

esp_err_t device_config_init(const char* manufacturer,  const char* model, const char* hardware_revision, device_config_t* config) {
    config->eid[0] = 0;
    // Check if config file already exists
    FILE* file = fopen(CONFIG_FILE_PATH, "r");
    if (file != nullptr) {
        ESP_LOGI(TAG, "Config file already exists - overwriting");

        //lets try to keep eid
        size_t file_size = fread(buffer_s, 1, sizeof(buffer_s), file);
        ESP_LOGI(TAG, "read %d bytes", file_size);
        fclose(file);

        cJSON *root = cJSON_Parse(buffer_s);
        if (root != nullptr) {
            cJSON *eid1 = cJSON_GetObjectItemCaseSensitive(root, "equipment_id");
            cJSON *eid2 = cJSON_GetObjectItemCaseSensitive(root, "eid");
            if (cJSON_IsString(eid1)) {
                strncpy(config->eid, eid1->valuestring, MAX_EQUIPMENT_ID_LENGTH - 1);
                config->eid[MAX_EQUIPMENT_ID_LENGTH - 1] = '\0';
            } else if (cJSON_IsString(eid2)) {
                strncpy(config->eid, eid2->valuestring, MAX_EQUIPMENT_ID_LENGTH - 1);
                config->eid[MAX_EQUIPMENT_ID_LENGTH - 1] = '\0';
            } else {
            }
            cJSON_Delete(root);
        }
    }

    // Generate new config
    // generate if empty
    if (strlen(config->eid)==0) {
        generate_uuid(config->eid);
        ESP_LOGI(TAG, "generated new eid %s", config->eid);
    } else {
        ESP_LOGI(TAG, "eid is %s", config->eid);
    }

    strncpy(config->manufacturer, manufacturer, MAX_MANUFACTURER_LENGTH - 1);
    config->manufacturer[MAX_MANUFACTURER_LENGTH - 1] = '\0';
    strncpy(config->model, model, MAX_MODEL_LENGTH - 1);
    config->model[MAX_MODEL_LENGTH - 1] = '\0';
    strncpy(config->hardware_revision, hardware_revision, MAX_VERSION_LENGTH - 1);
    config->hardware_revision[MAX_VERSION_LENGTH - 1] = '\0';

    // Create JSON object
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "manufacturer", config->manufacturer);
    cJSON_AddStringToObject(root, "model", config->model);
    cJSON_AddStringToObject(root, "eid", config->eid);
    cJSON_AddStringToObject(root, "hardware_revision", config->hardware_revision);

    // Write JSON to file
    file = fopen(CONFIG_FILE_PATH, "w");
    if (file == NULL) {
        ESP_LOGE(TAG, "Failed to open config file for writing");
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    char *json_str = cJSON_Print(root);
    fprintf(file, "%s", json_str);
    fclose(file);

    cJSON_free(json_str);
    cJSON_Delete(root);

    ESP_LOGI(TAG, "Created new config file with UUID: %s", config->eid);
    return ESP_OK;
}

esp_err_t device_config_init(device_config_t* config) {
    FILE* file = fopen(CONFIG_FILE_PATH, "r");
    if (file == NULL) {
        ESP_LOGE(TAG, "Failed to open config file");
        return ESP_FAIL;
    }

    size_t file_size = fread(buffer_s, 1, sizeof(buffer_s), file);
    fclose(file);

    cJSON *root = cJSON_Parse(buffer_s);
    if (root == NULL) {
        ESP_LOGE(TAG, "Failed to parse config file");
        return ESP_FAIL;
    }

    cJSON *manufacturer = cJSON_GetObjectItemCaseSensitive(root, "manufacturer");
    cJSON *model = cJSON_GetObjectItemCaseSensitive(root, "model");
    cJSON *eid = cJSON_GetObjectItemCaseSensitive(root, "eid");
    cJSON *hw_revision = cJSON_GetObjectItemCaseSensitive(root, "hardware_revision");

    if (!cJSON_IsString(manufacturer) || !cJSON_IsString(model) || !cJSON_IsString(eid) || !cJSON_IsString(hw_revision)) {
        ESP_LOGE(TAG, "Invalid config file format");
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    strncpy(config->manufacturer, manufacturer->valuestring, MAX_MANUFACTURER_LENGTH - 1);
    strncpy(config->model, model->valuestring, MAX_MODEL_LENGTH - 1);
    strncpy(config->eid, eid->valuestring, MAX_EQUIPMENT_ID_LENGTH - 1);
    strncpy(config->hardware_revision, hw_revision->valuestring, MAX_VERSION_LENGTH - 1);

    config->manufacturer[MAX_MANUFACTURER_LENGTH - 1] = '\0';
    config->model[MAX_MODEL_LENGTH - 1] = '\0';
    config->eid[MAX_EQUIPMENT_ID_LENGTH - 1] = '\0';
    config->hardware_revision[MAX_VERSION_LENGTH - 1] = '\0';

    cJSON_Delete(root);
    ESP_LOGI(TAG, "Loaded config: ID=%s, HW=%s", config->eid, config->hardware_revision);
    return ESP_OK;
}