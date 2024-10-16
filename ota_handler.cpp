#include <apptools/ota_handler.h>
#include <esp_ota_ops.h>

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