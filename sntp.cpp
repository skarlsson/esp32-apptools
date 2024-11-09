#include "sntp.h"
#include "esp_sntp.h"
#include <esp_log.h>


static const char* TAG = "sntp";

static void time_sync_notification_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "Notification of a time synchronization event");
}

esp_err_t initialize_sntp(int timeout_s) {
    ESP_LOGI(TAG, "Initializing SNTP");
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);

    // Add multiple NTP servers for redundancy
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_setservername(1, "time.google.com");
    esp_sntp_setservername(2, "time.windows.com");

    // Set sync interval and timeout
    sntp_set_sync_interval(150000);  // 15 seconds between retries

    // Optional: Use IP address instead of hostname to bypass DNS
    // esp_sntp_setservername(0, "216.239.35.0");  // time.google.com IP

    sntp_set_time_sync_notification_cb(time_sync_notification_cb);

    ESP_LOGI(TAG, "Starting SNTP");
    esp_sntp_init();

    // Add error checking
    if (esp_sntp_enabled()) {
        ESP_LOGI(TAG, "SNTP service started successfully");
    } else {
        ESP_LOGE(TAG, "Failed to start SNTP service");
    }

    // wait for time to be set
    int retry = 0;
    const int retry_count = timeout_s / 2;
    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retry_count) {
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }

    if (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET) {
        return ESP_OK;
    }
    return ESP_FAIL;
}


