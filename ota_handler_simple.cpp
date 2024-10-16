#include <apptools/ota_handler_simple.h>
#include "cJSON.h"
#include "esp_log.h"
#include <esp_ota_ops.h>
#include "esp_netif.h"

static const char* TAG = "ota_handler_simple";

ota_handler_simple::ota_handler_simple(const char* manufacturer, const char* model, const char* hardware_revision) : ota_handler(manufacturer, model, hardware_revision)
{
}

void ota_handler_simple::handle_ota_update(const char* json)
{
    cJSON* root = cJSON_Parse(json);
    if (root)
    {
        cJSON* manufacturer = cJSON_GetObjectItem(root, "manufacturer");
        cJSON* model = cJSON_GetObjectItem(root, "model");
        cJSON* hardware_version = cJSON_GetObjectItem(root, "hardware_version");
        cJSON* firmware_version = cJSON_GetObjectItem(root, "firmware_version");
        cJSON* firmware_file = cJSON_GetObjectItem(root, "firmware_file");
        cJSON* release_date = cJSON_GetObjectItem(root, "release_date");
        cJSON* sha256 = cJSON_GetObjectItem(root, "sha256");

        if (manufacturer && manufacturer->valuestring &&
            model && model->valuestring &&
            hardware_version && hardware_version->valuestring &&
            firmware_version && firmware_version->valuestring &&
            firmware_file && firmware_file->valuestring &&
            release_date && release_date->valuestring &&
            sha256 && sha256->valuestring)
        {
            // Verify device information
            if (strcmp(manufacturer_, manufacturer->valuestring) != 0)
            {
                ESP_LOGE(TAG, "Manufacturer mismatch. Expected: %s, Received: %s",
                         manufacturer_, manufacturer->valuestring);
                cJSON_Delete(root);
                return;
            }

            if (strcmp(model_, model->valuestring) != 0)
            {
                ESP_LOGE(TAG, "Model mismatch. Expected: %s, Received: %s",
                         model_, model->valuestring);
                cJSON_Delete(root);
                return;
            }

            if (strcmp(hardware_revision_, hardware_version->valuestring) != 0)
            {
                ESP_LOGE(TAG, "Hardware version mismatch. Expected: %s, Received: %s",
                         hardware_revision_, hardware_version->valuestring);
                cJSON_Delete(root);
                return;
            }

            // Check software version
            if (strcmp(FIRMWARE_VERSION, firmware_version->valuestring) == 0)
            {
                ESP_LOGI(TAG, "Software version is already up to date");
                cJSON_Delete(root);
                return;
            }

            // todo we need to get certificate and verify that as well...
            //

            // If we've reached here, the versions are different
            ESP_LOGI(TAG, "New version available. Current: %s, New: %s", FIRMWARE_VERSION, firmware_version->valuestring);

            // Start the update
            esp_err_t err = do_firmware_upgrade(firmware_file->valuestring, sha256->valuestring);
            if (err != ESP_OK)
            {
                ESP_LOGE(TAG, "Failed to start OTA update: %s", esp_err_to_name(err));
            }
            else
            {
                ESP_LOGI(TAG, "OTA update started");
            }
        }
        else
        {
            ESP_LOGE(TAG, "Missing required fields in JSON");
        }
        cJSON_Delete(root);
    }
    else
    {
        ESP_LOGE(TAG, "Failed to parse JSON");
    }
}

esp_err_t ota_handler_simple::do_firmware_upgrade(const char* url, const char* expected_sha)
{
    struct ifreq ifr;
    strncpy(ifr.ifr_name, "en1", sizeof(ifr.ifr_name));

    esp_http_client_config_t config = {};
    config.url = url;
    config.keep_alive_enable = true;
    config.timeout_ms = 5000;
    //config.if_name = &ifr;

    esp_https_ota_config_t ota_config = {};
    ota_config.http_config = &config;
    ota_config.partial_http_download = true;

    ESP_LOGI(TAG, "OTA update started");
    esp_err_t ret = esp_https_ota(&ota_config);

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "OTA update OK - reboot pending");
        //esp_restart();
        reboot_pending_ = true;
    } else {
        ESP_LOGI(TAG, "OTA update failed");
        return ESP_FAIL;
    }
    return ESP_OK;
}




