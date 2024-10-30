#include <apptools/fs_utils.h>
#include "esp_littlefs.h"
#include <esp_log.h>
#include <string.h>
#include <mbedtls/sha256.h>
#include <esp_partition.h>
#include "esp_ota_ops.h"
#include <esp_app_format.h>
#include <sys/param.h>

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


esp_err_t compute_firmware_sha256(char* sha256_buf, size_t sha256_buf_siz) {
    if (sha256_buf_siz<65) {
        ESP_LOGE(TAG, "buffer must be >= 65 bytes");
        return ESP_FAIL;
    }

    const esp_partition_t* running = esp_ota_get_running_partition();
    if (!running) {
        ESP_LOGE(TAG, "Failed to get running partition");
        return ESP_FAIL;
    }

    // Initialize SHA-256
    mbedtls_sha256_context sha_ctx;
    mbedtls_sha256_init(&sha_ctx);
    mbedtls_sha256_starts(&sha_ctx, 0);

    const size_t BUFFER_SIZE = 4096;
    uint8_t* buffer = (uint8_t*)malloc(BUFFER_SIZE);
    if (!buffer) {
        ESP_LOGE(TAG, "Failed to allocate buffer");
        return ESP_ERR_NO_MEM;
    }

    // Read header and calculate exact file size
    esp_image_header_t header;
    esp_err_t ret = esp_partition_read(running, 0, &header, sizeof(header));
    if (ret != ESP_OK) {
        free(buffer);
        return ret;
    }

    // Calculate exact binary size
    uint32_t binary_size = sizeof(esp_image_header_t);  // Start with header size
    size_t offset = binary_size;

    // Read all segment headers first to get total size
    for (int i = 0; i < header.segment_count; i++) {
        esp_image_segment_header_t seg_header;
        ret = esp_partition_read(running, offset, &seg_header, sizeof(seg_header));
        if (ret != ESP_OK) {
            free(buffer);
            return ret;
        }

        ESP_LOGI(TAG, "Segment %d: offset=0x%x, size=%ld", i, offset, seg_header.data_len);

        binary_size += sizeof(esp_image_segment_header_t) + seg_header.data_len;
        offset += sizeof(seg_header) + seg_header.data_len;
    }

    // Check if size needs alignment to 16 bytes and add SHA-256 size
    uint32_t aligned_size = (binary_size + 15) & ~15;  // Align to 16 bytes
    if (header.hash_appended) {
        aligned_size += 32;  // Add SHA-256 size
    }

    ESP_LOGI(TAG, "Image size: %ld, Aligned: %ld bytes", binary_size, aligned_size);

    // Now hash the exact file size
    auto remaining = aligned_size;
    offset = 0;
    size_t bytes_hashed = 0;

    // Hash in chunks
    while (remaining > 0) {
        size_t to_read = (remaining < BUFFER_SIZE) ? remaining : BUFFER_SIZE;
        ret = esp_partition_read(running, offset, buffer, to_read);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to read at offset 0x%x", offset);
            break;
        }

        // Print first chunk
        if (offset == 0) {
            ESP_LOGI(TAG, "First 32 bytes:");
            ESP_LOG_BUFFER_HEX(TAG, buffer, MIN(32, to_read));
        }

        // Print last chunk
        if (remaining <= BUFFER_SIZE) {
            ESP_LOGI(TAG, "Last %ld bytes:", MIN(32, remaining));
            ESP_LOG_BUFFER_HEX(TAG, buffer + (to_read - MIN(32, remaining)),
                             MIN(32, remaining));
        }

        mbedtls_sha256_update(&sha_ctx, buffer, to_read);
        remaining -= to_read;
        offset += to_read;
        bytes_hashed += to_read;
    }

    ESP_LOGI(TAG, "Hashed %d bytes", bytes_hashed);

    // Calculate final hash
    uint8_t computed_hash[32];
    mbedtls_sha256_finish(&sha_ctx, computed_hash);
    mbedtls_sha256_free(&sha_ctx);

    // Format hash string
    for(int i = 0; i < 32; i++) {
        sprintf(&sha256_buf[i*2], "%02x", computed_hash[i]);
    }
    sha256_buf[64] = '\0';

    ESP_LOGI(TAG, "firmware SHA-256: %s", sha256_buf);

    free(buffer);
    return ESP_OK;
}


esp_err_t compute_firmware_sha256V0(char* sha256_buf, size_t sha256_buf_siz) {
    if (sha256_buf_siz<65) {
        ESP_LOGE(TAG, "buffer must be >= 65 bytes");
        return ESP_FAIL;
    }

    const esp_partition_t* running = esp_ota_get_running_partition();
    if (!running) {
        ESP_LOGE(TAG, "Failed to get running partition");
        return ESP_FAIL;
    }

    // Initialize SHA-256
    mbedtls_sha256_context sha_ctx;
    mbedtls_sha256_init(&sha_ctx);
    mbedtls_sha256_starts(&sha_ctx, 0);

    const size_t BUFFER_SIZE = 4096;
    uint8_t* buffer = (uint8_t*)malloc(BUFFER_SIZE);
    if (!buffer) {
        ESP_LOGE(TAG, "Failed to allocate buffer");
        return ESP_ERR_NO_MEM;
    }

    // Calculate total size including padding and SHA256
    esp_image_header_t header;
    esp_err_t ret = esp_partition_read(running, 0, &header, sizeof(header));
    if (ret != ESP_OK) {
        free(buffer);
        return ret;
    }

    // Calculate size up to end of segments
    size_t total_size = sizeof(header);
    size_t offset = total_size;

    for (int i = 0; i < header.segment_count; i++) {
        esp_image_segment_header_t seg_header;
        ret = esp_partition_read(running, offset, &seg_header, sizeof(seg_header));
        if (ret != ESP_OK) break;

        ESP_LOGI(TAG, "Segment %d: size=%ld", i, seg_header.data_len);

        total_size += sizeof(seg_header) + seg_header.data_len;
        offset += sizeof(seg_header) + seg_header.data_len;
    }

    // Add padding size (align to 16 bytes)
    size_t padding_size = (16 - (total_size % 16)) % 16;
    if (padding_size > 0) {
        ESP_LOGI(TAG, "Padding: %d bytes", padding_size);
        total_size += padding_size;
    }

    // Add SHA-256
    if (header.hash_appended) {
        total_size += 32;
        ESP_LOGI(TAG, "Including 32 bytes SHA256");
    }

    ESP_LOGI(TAG, "Total binary size: %d bytes", total_size);

    // Now hash EVERYTHING - including the appended SHA-256
    size_t remaining = total_size;  // Hash everything
    offset = 0;
    size_t bytes_hashed = 0;

    while (remaining > 0) {
        size_t to_read = (remaining < BUFFER_SIZE) ? remaining : BUFFER_SIZE;
        ret = esp_partition_read(running, offset, buffer, to_read);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to read at offset 0x%x", offset);
            break;
        }

        // Print first and last blocks for verification
        if (offset == 0) {
            ESP_LOGI(TAG, "First 32 bytes:");
            ESP_LOG_BUFFER_HEX(TAG, buffer, MIN(32, to_read));
        }
        if (remaining <= BUFFER_SIZE) {
            ESP_LOGI(TAG, "Last 32 bytes:");
            ESP_LOG_BUFFER_HEX(TAG, buffer + (to_read - MIN(32, to_read)),
                             MIN(32, to_read));
        }

        mbedtls_sha256_update(&sha_ctx, buffer, to_read);
        remaining -= to_read;
        offset += to_read;
        bytes_hashed += to_read;
    }

    ESP_LOGI(TAG, "Hashed %d bytes", bytes_hashed);

    // Calculate final hash
    uint8_t computed_hash[32];
    mbedtls_sha256_finish(&sha_ctx, computed_hash);
    mbedtls_sha256_free(&sha_ctx);

    for(int i = 0; i < 32; i++) {
        sprintf(&sha256_buf[i*2], "%02x", computed_hash[i]);
    }
    sha256_buf[64] = '\0';

    ESP_LOGI(TAG, "firmware SHA-256: %s", sha256_buf);

    free(buffer);
    return ESP_OK;
}