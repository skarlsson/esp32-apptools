#include <apptools/device_config.h>
#include <cstring>
#include "esp_log.h"
#include "esp_random.h"
#include "cJSON.h"
#include "esp_ota_ops.h"
#include <esp_app_format.h>
#include <sys/param.h>
#include <apptools/fs_utils.h>



#define CONFIG_FILE_PATH "/mnt/config.json"
#define MOUNT_POINT "/mnt"

static const char* TAG = "device_config";

static char buffer_s[1024];

/*
esp_err_t dump_flash_contents() {
    const esp_partition_t* running = esp_ota_get_running_partition();
    if (!running) {
        ESP_LOGE(TAG, "Failed to get running partition");
        return ESP_FAIL;
    }

    // Read through entire binary and dump structure
    size_t offset = 0;
    uint8_t buffer[32];
    esp_err_t ret;

    // Read and dump header
    esp_image_header_t header;
    ret = esp_partition_read(running, 0, &header, sizeof(header));
    if (ret != ESP_OK) {
        return ret;
    }

    ESP_LOGI(TAG, "Image Header at offset 0x%x:", offset);
    ESP_LOGI(TAG, "  Magic: 0x%02x", header.magic);
    ESP_LOGI(TAG, "  Segment Count: %d", header.segment_count);
    ESP_LOGI(TAG, "  SHA256: %s", header.hash_appended ? "Yes" : "No");
    ESP_LOGI(TAG, "  Entry: 0x%08lx", header.entry_addr);

    offset += sizeof(header);

    // Process each segment
    size_t total_size = sizeof(header);
    for (int i = 0; i < header.segment_count; i++) {
        esp_image_segment_header_t seg_header;
        ret = esp_partition_read(running, offset, &seg_header, sizeof(seg_header));
        if (ret != ESP_OK) break;

        ESP_LOGI(TAG, "Segment %d at offset 0x%x:", i, offset);
        ESP_LOGI(TAG, "  Load Address: 0x%08lx", seg_header.load_addr);
        ESP_LOGI(TAG, "  Size: %ld", seg_header.data_len);

        offset += sizeof(seg_header);
        total_size += sizeof(seg_header);

        // Show first and last bytes of segment
        if (seg_header.data_len > 0) {
            // First bytes
            ret = esp_partition_read(running, offset, buffer,
                                   MIN(32, seg_header.data_len));
            if (ret == ESP_OK) {
                ESP_LOGI(TAG, "  First bytes:");
                ESP_LOG_BUFFER_HEX(TAG, buffer, MIN(32, seg_header.data_len));
            }

            // Last bytes if segment is large enough
            if (seg_header.data_len > 32) {
                ret = esp_partition_read(running,
                                       offset + seg_header.data_len - 32,
                                       buffer, 32);
                if (ret == ESP_OK) {
                    ESP_LOGI(TAG, "  Last bytes:");
                    ESP_LOG_BUFFER_HEX(TAG, buffer, 32);
                }
            }
        }

        offset += seg_header.data_len;
        total_size += seg_header.data_len;
    }

    // Check for padding to 16-byte boundary
    size_t padding = (16 - (total_size % 16)) % 16;
    if (padding > 0) {
        ESP_LOGI(TAG, "Padding bytes at offset 0x%x: %d bytes", offset, padding);
        ret = esp_partition_read(running, offset, buffer, padding);
        if (ret == ESP_OK) {
            ESP_LOG_BUFFER_HEX(TAG, buffer, padding);
        }
        total_size += padding;
    }

    // Check for appended SHA256
    if (header.hash_appended) {
        ESP_LOGI(TAG, "SHA256 at offset 0x%x:", offset + padding);
        ret = esp_partition_read(running, offset + padding, buffer, 32);
        if (ret == ESP_OK) {
            ESP_LOG_BUFFER_HEX(TAG, buffer, 32);
        }
        total_size += 32;
    }

    ESP_LOGI(TAG, "Total calculated size: %d", total_size);

    return ESP_OK;
}




esp_err_t compute_firmware_sha256_0() {
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

    // Calculate total size including padding
    esp_image_header_t header;
    esp_err_t ret = esp_partition_read(running, 0, &header, sizeof(header));
    if (ret != ESP_OK) {
        free(buffer);
        return ret;
    }

    ESP_LOGI(TAG, "Header: magic=0x%02x segments=%d has_sha256=%d",
             header.magic, header.segment_count, header.hash_appended);

    // Calculate size up to end of segments
    size_t total_size = sizeof(header);
    size_t offset = total_size;

    for (int i = 0; i < header.segment_count; i++) {
        esp_image_segment_header_t seg_header;
        ret = esp_partition_read(running, offset, &seg_header, sizeof(seg_header));
        if (ret != ESP_OK) break;

        ESP_LOGI(TAG, "Segment %d: offset=0x%x size=%ld", i, offset, seg_header.data_len);

        total_size += sizeof(seg_header) + seg_header.data_len;
        offset += sizeof(seg_header) + seg_header.data_len;
    }

    // Calculate padding size (align to 16 bytes)
    size_t padding_size = (16 - (total_size % 16)) % 16;
    if (padding_size > 0) {
        ESP_LOGI(TAG, "Padding size: %d bytes", padding_size);
        total_size += padding_size;
    }

    // Add SHA-256 size
    if (header.hash_appended) {
        total_size += 32;
    }

    ESP_LOGI(TAG, "Total binary size: %d bytes", total_size);

    // Now hash everything except the appended SHA-256
    size_t bytes_to_hash = total_size - 32;  // Don't include appended SHA
    size_t remaining = bytes_to_hash;
    offset = 0;

    while (remaining > 0) {
        size_t to_read = (remaining < BUFFER_SIZE) ? remaining : BUFFER_SIZE;
        ret = esp_partition_read(running, offset, buffer, to_read);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to read at offset 0x%x", offset);
            break;
        }

        // Debug output for first and last blocks
        if (offset == 0) {
            ESP_LOGI(TAG, "First %d bytes:", MIN(32, to_read));
            ESP_LOG_BUFFER_HEX(TAG, buffer, MIN(32, to_read));
        }

        if (remaining <= BUFFER_SIZE) {
            ESP_LOGI(TAG, "Last %d bytes:", MIN(32, to_read));
            ESP_LOG_BUFFER_HEX(TAG, buffer + (to_read - MIN(32, to_read)),
                             MIN(32, to_read));
        }

        mbedtls_sha256_update(&sha_ctx, buffer, to_read);
        remaining -= to_read;
        offset += to_read;
    }

    ESP_LOGI(TAG, "Hashed %d bytes", bytes_to_hash);

    // Read the appended SHA for comparison
    uint8_t appended_hash[32];
    ret = esp_partition_read(running, bytes_to_hash, appended_hash, 32);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Appended SHA-256:");
        ESP_LOG_BUFFER_HEX(TAG, appended_hash, 32);
    }

    // Calculate final hash
    uint8_t computed_hash[32];
    mbedtls_sha256_finish(&sha_ctx, computed_hash);
    mbedtls_sha256_free(&sha_ctx);

    printf("\nSHA256: ");
    for (int i = 0; i < 32; i++) {
        printf("%02x", computed_hash[i]);
    }
    printf("\n");

    ESP_LOGI(TAG, "Computed SHA-256:");
    ESP_LOG_BUFFER_HEX(TAG, computed_hash, 32);

    free(buffer);
    return ESP_OK;
}
*/

static void generate_uuid(char* uuid_str) {
    uint32_t random_values[4];
    for (int i = 0; i < 4; i++) {
        random_values[i] = esp_random();
    }

    snprintf(uuid_str, device_config_t::MAX_EQUIPMENT_ID_LENGTH,
             "%08" PRIx32 "-%04" PRIx16 "-%04" PRIx16 "-%04" PRIx16 "-%04" PRIx16 "%08" PRIx32,
             random_values[0],
             (uint16_t)(random_values[1] >> 16),
             ((uint16_t)random_values[1] & 0xffff) | 0x4000,  // Version 4
             ((uint16_t)(random_values[2] >> 16) & 0x3fff) | 0x8000,  // Variant 1
             (uint16_t)random_values[2],
             random_values[3]);
}

esp_err_t device_config_set_sw_version(device_config_t* config, const char* hash) {
    // FIRMWARE_VERSION comes from CMake
    snprintf(config->software_revision,
             device_config_t::MAX_SW_VERSION_LENGTH,
             "%s;SHA256:%s",
             FIRMWARE_VERSION,
             hash);
    return ESP_OK;
}


esp_err_t device_config_init(const char* manufacturer,  const char* model, const char* hardware_revision, device_config_t* config) {
    bool needs_save = false;
    config->eid[0] = 0;

    // Check if config file already exists
    FILE* file = fopen(CONFIG_FILE_PATH, "r");
    if (file != nullptr) {
        ESP_LOGI(TAG, "Config file exists - checking for changes");

        size_t file_size = fread(buffer_s, 1, sizeof(buffer_s), file);
        ESP_LOGI(TAG, "read %d bytes", file_size);
        fclose(file);

        cJSON *root = cJSON_Parse(buffer_s);
        if (root != nullptr) {
            cJSON *eid1 = cJSON_GetObjectItemCaseSensitive(root, "equipment_id");
            cJSON *eid2 = cJSON_GetObjectItemCaseSensitive(root, "eid");
            if (cJSON_IsString(eid1)) {
                strncpy(config->eid, eid1->valuestring, device_config_t::MAX_EQUIPMENT_ID_LENGTH - 1);
                config->eid[device_config_t::MAX_EQUIPMENT_ID_LENGTH - 1] = '\0';
            } else if (cJSON_IsString(eid2)) {
                strncpy(config->eid, eid2->valuestring, device_config_t::MAX_EQUIPMENT_ID_LENGTH - 1);
                config->eid[device_config_t::MAX_EQUIPMENT_ID_LENGTH - 1] = '\0';
            }

            // Read existing values to compare
            cJSON *mfr = cJSON_GetObjectItemCaseSensitive(root, "manufacturer");
            cJSON *mdl = cJSON_GetObjectItemCaseSensitive(root, "model");
            cJSON *hw_rev = cJSON_GetObjectItemCaseSensitive(root, "hardware_revision");

            needs_save = !cJSON_IsString(mfr) || strcmp(mfr->valuestring, manufacturer) != 0 ||
                        !cJSON_IsString(mdl) || strcmp(mdl->valuestring, model) != 0 ||
                        !cJSON_IsString(hw_rev) || strcmp(hw_rev->valuestring, hardware_revision) != 0;

            cJSON_Delete(root);
        } else {
            needs_save = true;
        }
    } else {
        needs_save = true;
    }

    // Generate new eid if empty
    if (strlen(config->eid)==0) {
        generate_uuid(config->eid);
        ESP_LOGI(TAG, "generated new eid %s", config->eid);
        needs_save = true;
    } else {
        ESP_LOGI(TAG, "eid is %s", config->eid);
    }

    strncpy(config->manufacturer, manufacturer, device_config_t::MAX_MANUFACTURER_LENGTH - 1);
    config->manufacturer[device_config_t::MAX_MANUFACTURER_LENGTH - 1] = '\0';
    strncpy(config->model, model, device_config_t::MAX_MODEL_LENGTH - 1);
    config->model[device_config_t::MAX_MODEL_LENGTH - 1] = '\0';
    strncpy(config->hardware_revision, hardware_revision, device_config_t::MAX_HW_VERSION_LENGTH - 1);
    config->hardware_revision[device_config_t::MAX_HW_VERSION_LENGTH - 1] = '\0';

    //verify_firmware_sha256(config->sw_sha256);
    //dump_flash_contents();
    //compute_firmware_sha256();
    config->sw_sha256[60] = '\0';
    compute_firmware_sha256(config->sw_sha256, device_config_t::SHA_LENGTH);

    /*
    const esp_app_desc_t* app_desc = esp_app_get_description();
    for(int i = 0; i < 32; i++) {
        sprintf(&config->sw_sha256[i*2], "%02x", app_desc->app_elf_sha256[i]);
    }

    */
    // Set software version with hash
    device_config_set_sw_version(config, config->sw_sha256);

    if (needs_save) {
        ESP_LOGI(TAG, "Config changed - saving to file");
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

        ESP_LOGI(TAG, "Saved new config file with UUID: %s", config->eid);
    } else {
        ESP_LOGI(TAG, "Config unchanged - not saving");
    }

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

    strncpy(config->manufacturer, manufacturer->valuestring, device_config_t::MAX_MANUFACTURER_LENGTH - 1);
    strncpy(config->model, model->valuestring, device_config_t::MAX_MODEL_LENGTH - 1);
    strncpy(config->eid, eid->valuestring, device_config_t::MAX_EQUIPMENT_ID_LENGTH - 1);
    strncpy(config->hardware_revision, hw_revision->valuestring, device_config_t::MAX_HW_VERSION_LENGTH - 1);

    config->manufacturer[device_config_t::MAX_MANUFACTURER_LENGTH - 1] = '\0';
    config->model[device_config_t::MAX_MODEL_LENGTH - 1] = '\0';
    config->eid[device_config_t::MAX_EQUIPMENT_ID_LENGTH - 1] = '\0';
    config->hardware_revision[device_config_t::MAX_HW_VERSION_LENGTH - 1] = '\0';

    // Get hash from app description instead of partition
    const esp_app_desc_t* app_desc = esp_app_get_description();
    for(int i = 0; i < 32; i++) {
        sprintf(&config->sw_sha256[i*2], "%02x", app_desc->app_elf_sha256[i]);
    }
    config->sw_sha256[64] = '\0';

    device_config_set_sw_version(config, config->sw_sha256);

    cJSON_Delete(root);
    ESP_LOGI(TAG, "Loaded config: ID=%s, HW=%s", config->eid, config->hardware_revision);
    return ESP_OK;
}