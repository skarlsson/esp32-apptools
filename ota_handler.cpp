#include <apptools/ota_handler.h>
#include <esp_ota_ops.h>
#include "esp_log.h"
#include "cJSON.h"

static const char* TAG = "ota_handler";

ota_handler::ota_handler(const char* manufacturer, const char* model, const char* hardware_revision):
manufacturer_(manufacturer),
model_(model),
hardware_revision_(hardware_revision)
{
    // Check if there's a pending verify
    const esp_partition_t* running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;
    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK)
    {
        verify_pending_ = (ota_state == ESP_OTA_IMG_PENDING_VERIFY);
    }
}

esp_err_t ota_handler::confirm_update()
{
    esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
    if (err == ESP_OK)
    {
        verify_pending_ = false;
    }
    return err;
}


bool ota_handler::handle_subdevice_ota(const ha_discovery::device_info_t* device, const char* json) {
    if (!device) {
        ESP_LOGE(TAG, "Invalid device pointer");
        return false;
    }

    if (!json) {
        ESP_LOGE(TAG, "Invalid JSON data");
        return false;
    }

    ESP_LOGI(TAG, "Processing OTA request for device %s (model: %s)", device->eid(), device->model());

    // Validate JSON before passing to handlers
    cJSON* root = cJSON_Parse(json);
    if (!root) {
        ESP_LOGE(TAG, "Failed to parse OTA JSON: %s", cJSON_GetErrorPtr());
        return false;
    }

    // Basic JSON validation
    const char* required_fields[] = {
        "manufacturer", "model", "hardware_version", 
        "firmware_version", "firmware_file", "sha256"
    };

    bool json_valid = true;
    for (const char* field : required_fields) {
        if (!cJSON_GetObjectItem(root, field)) {
            ESP_LOGE(TAG, "Missing required field in OTA JSON: %s", field);
            json_valid = false;
            break;
        }
    }

    cJSON_Delete(root);

    if (!json_valid) {
        return false;
    }

    // Try each registered handler
    for (const auto& handler : device_handlers_) {
        if (!handler) {
            ESP_LOGW(TAG, "Encountered null handler, skipping");
            continue;
        }

        if (handler->can_handle(device)) {
            ESP_LOGI(TAG, "Found compatible handler for device %s", device->eid());
            
            if (handler->handle_update(device, json)) {
                ESP_LOGI(TAG, "Successfully initiated OTA update for device %s", device->eid());
                return true;
            }
            
            ESP_LOGW(TAG, "Handler failed to process update for device %s", device->eid());
            // Continue trying other handlers
        }
    }

    if (device_handlers_.empty()) {
        ESP_LOGW(TAG, "No device handlers registered");
    } else {
        ESP_LOGE(TAG, "No compatible handler found for device %s", device->eid());
    }

    return false;
}
