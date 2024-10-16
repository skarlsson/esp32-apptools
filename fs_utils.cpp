#include <apptools/fs_utils.h>
#include "esp_littlefs.h"
#include <esp_log.h>
#include <string.h>


static const char* TAG = "fs_utils";

#define MOUNT_POINT "/mnt"
#define MAX_PARTITION_LABEL 16
static char partition_label_s[MAX_PARTITION_LABEL];

esp_err_t utils_littlefs_init(const char* partition_label)
{
    if (strlen(partition_label) > MAX_PARTITION_LABEL-1) {
        ESP_LOGE(TAG, "partition label is too long");
        return ESP_ERR_INVALID_ARG;
    }
    strncpy(partition_label_s, partition_label, MAX_PARTITION_LABEL);
    partition_label_s[MAX_PARTITION_LABEL-1] = '\0';

    ESP_LOGI(TAG, "Initializing LittleFS");
    esp_vfs_littlefs_conf_t conf = {
        .base_path = MOUNT_POINT,
        .partition_label = partition_label_s,
        .format_if_mount_failed = true,
        .dont_mount = false
    };
    ESP_ERROR_CHECK(esp_vfs_littlefs_register(&conf));

    size_t total = 0, used = 0;
    ESP_ERROR_CHECK(esp_littlefs_info(conf.partition_label, &total, &used));
    ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);

    return ESP_OK;
}

