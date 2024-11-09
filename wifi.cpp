#include <apptools/wifi.h>

#if defined(CONFIG_ESP32_WIFI_ENABLED) || defined(CONFIG_ESP_WIFI_ENABLED)

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "freertos/event_groups.h"
#include <cstring>

#define TAG "utils_wifi"

static EventGroupHandle_t s_wifi_event_group;
static int s_retry_num = 0;

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
#define MAXIMUM_RETRY      10
#define SCAN_LIST_SIZE     15

static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG, "connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_PSK

esp_err_t utils_wifi_init(const char *ssid, const char *password) {
    s_wifi_event_group = xEventGroupCreate();

    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();

    // Optimize TCP/IP stack
    /*
    esp_netif_ip_info_t ip_info;
    esp_netif_get_ip_info(sta_netif, &ip_info);

    // Set TCP/IP stack parameters
    esp_netif_set_tx_delay(sta_netif, 0);  // Minimize TX delay
    esp_netif_set_hostname(sta_netif, "ESP32");
    */



    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    cfg.ampdu_tx_enable = 1;
    cfg.ampdu_rx_enable = 1;
    cfg.nano_enable = 0;  // Disable nanosecond timer
    //cfg.cache_tx_buf_enable = 1;

    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
        ESP_EVENT_ANY_ID,
        &event_handler,
        nullptr,
        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
        IP_EVENT_STA_GOT_IP,
        &event_handler,
        nullptr,
        &instance_got_ip));
    wifi_config_t wifi_config = {};
    wifi_config.sta.channel = 0;  // Auto select channel
    wifi_config.sta.listen_interval = 1;
    strncpy((char *) wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *) wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);
    wifi_config.sta.threshold.authmode = ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD;
    wifi_config.sta.pmf_cfg = {
        .capable = true,
        .required = false
    };
    wifi_config.sta.rm_enabled = 1;  // Enable rate adaptation
    wifi_config.sta.btm_enabled = 1;  // Enable BTM
    wifi_config.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
    wifi_config.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;



    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

    esp_wifi_set_bandwidth(WIFI_IF_STA, WIFI_BW_HT40);
    ESP_ERROR_CHECK(esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N));
    esp_wifi_set_ps(WIFI_PS_NONE);  // Disable power saving


    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s", ssid, password);

        wifi_ap_record_t ap_info;
        esp_wifi_sta_get_ap_info(&ap_info);
        ESP_LOGI(TAG, "Connected to AP: RSSI=%d, Channel=%d",  ap_info.rssi, ap_info.primary);

        uint8_t protocol_bitmap;
        esp_wifi_get_protocol(WIFI_IF_STA, &protocol_bitmap);
        ESP_LOGI(TAG, "Protocol bitmap: 0x%x", protocol_bitmap);

        wifi_bandwidth_t bw;
        esp_wifi_get_bandwidth(WIFI_IF_STA, &bw);
        ESP_LOGI(TAG, "Bandwidth: %s",  bw == WIFI_BW_HT40 ? "40MHz" : "20MHz");

        // Log TCP/IP stack info
        //ESP_LOGI(TAG, "IP: " IPSTR ", GW: " IPSTR,
        //         IP2STR(&ip_info.ip), IP2STR(&ip_info.gw));

    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s", ssid, password);
        return ESP_FAIL;
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
        return ESP_FAIL;
    }
    return ESP_OK;
}

#endif
