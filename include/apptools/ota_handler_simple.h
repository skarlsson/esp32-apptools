#pragma once
#include "esp_err.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
//#include "config_utils.h"
#include "ota_handler.h"

class ota_handler_simple : public ota_handler {
public:
    ota_handler_simple(const char* manufacturer, const char* model, const char* hardware_revision);
    ~ota_handler_simple() override=default;

    void handle_ota_update(const char* json) override;

private:
    esp_err_t do_firmware_upgrade(const char* url, const char* expeced_sha);
    bool update_in_progress_=false;
};