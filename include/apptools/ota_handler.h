#include <esp_err.h>
#pragma once

#if !defined(CONFIG_ESP_HTTPS_OTA_ALLOW_HTTP) && \
    !defined(CONFIG_ESP_HTTPS_OTA_VERIFY_CERT_BUNDLE) && \
    !defined(CONFIG_ESP_HTTPS_OTA_VERIFY_CERT_CHAIN)

#error "No server verification option is enabled for ESP HTTPS OTA. Please enable at least one of the following in menuconfig: CONFIG_ESP_HTTPS_OTA_ALLOW_HTTP, CONFIG_ESP_HTTPS_OTA_VERIFY_CERT_BUNDLE, or CONFIG_ESP_HTTPS_OTA_VERIFY_CERT_CHAIN"

#elif !defined(CONFIG_ESP_HTTPS_OTA_VERIFY_CERT_BUNDLE) && \
      !defined(CONFIG_ESP_HTTPS_OTA_VERIFY_CERT_CHAIN)

#warning "HTTPS OTA is configured without server certificate verification. This is not recommended for production use."

#endif


// Check if OTA support is enabled
#if !defined(CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE)
    #error "OTA rollback support is not enabled. Please enable CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE in menuconfig."
#endif

// Check if the bootloader SHA256 check is enabled (recommended for secure boot)
#if !defined(CONFIG_BOOTLOADER_APP_ANTI_ROLLBACK)
    #warning "Bootloader SHA256 check is not enabled. It's recommended to enable CONFIG_BOOTLOADER_APP_ANTI_ROLLBACK for better security."
#endif

// Check if diagnostic application run is enabled after OTA update failure
#if !defined(CONFIG_BOOTLOADER_APP_TEST)
    #warning "Running diagnostic application after OTA update failure is not enabled. Consider enabling CONFIG_BOOTLOADER_APP_TEST for better error handling."
#endif

class ota_handler
{
public:
    virtual ~ota_handler() = default;
    virtual void handle_ota_update(const char* json_manifest) =0;

    esp_err_t confirm_update();
    bool is_reboot_pending() const { return reboot_pending_; }
    bool is_verify_pending() const { return verify_pending_; }

protected:
    void set_reboot_pending() { reboot_pending_ = true; }
    ota_handler(const char* manufacturer, const char* model, const char* hardware_revision);
    bool reboot_pending_ = false;
    const char* manufacturer_ = nullptr;
    const char* model_ = nullptr;
    const char* hardware_revision_ = nullptr;
private:
    bool verify_pending_ = false;
};
