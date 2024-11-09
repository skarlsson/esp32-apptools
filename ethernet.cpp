#include "apptools/ethernet.h"

//#if defined(SOC_EMAC_SUPPORTED) || defined(CONFIG_ETH_ENABLED) || defined(CONFIG_ESP32_EMAC_SUPPORTED)
#if defined(SOC_EMAC_SUPPORTED) ||  defined(CONFIG_ESP32_EMAC_SUPPORTED)
#include "esp_netif.h"
#include "esp_eth.h"
#include "esp_log.h"
#include "esp_event.h"

#define TAG "utils_ethernet"

static esp_netif_t *eth_netif = NULL;
static esp_eth_handle_t eth_handle = NULL;
static EventGroupHandle_t s_connection_event_group;


void prioritize_ethernet_routing(esp_netif_t* eth_netif) {
    esp_netif_ip_info_t ip_info;
    if (esp_netif_get_ip_info(eth_netif, &ip_info) == ESP_OK) {
        // Set ethernet as default interface with higher priority
        esp_netif_set_default_netif(eth_netif);
        ESP_LOGI(TAG, "Set ethernet as default interface");
    }
}


// Ethernet event handler
static void eth_event_handler(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data) {
    uint8_t mac_addr[6] = {0};
    esp_eth_handle_t eth_handle = *(esp_eth_handle_t *) event_data;

    switch (event_id) {
        case ETHERNET_EVENT_CONNECTED:
            esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, mac_addr);
            ESP_LOGI(TAG, "Ethernet Link Up");
            ESP_LOGI(TAG, "Ethernet HW Addr %02x:%02x:%02x:%02x:%02x:%02x",
                     mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
            break;
        case ETHERNET_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "Ethernet Link Down");
            break;
        case ETHERNET_EVENT_START:
            ESP_LOGI(TAG, "Ethernet Started");
            break;
        case ETHERNET_EVENT_STOP:
            ESP_LOGI(TAG, "Ethernet Stopped");
            break;
        default:
            break;
    }
}


// IP event handler
static void ip_event_handler(void *arg, esp_event_base_t event_base,
                             int32_t event_id, void *event_data) {
    ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
    const esp_netif_ip_info_t *ip_info = &event->ip_info;

    ESP_LOGI(TAG, "Ethernet Got IP Address");
    ESP_LOGI(TAG, "~~~~~~~~~~~");
    ESP_LOGI(TAG, "ETHIP:" IPSTR, IP2STR(&ip_info->ip));
    ESP_LOGI(TAG, "ETHMASK:" IPSTR, IP2STR(&ip_info->netmask));
    ESP_LOGI(TAG, "ETHGW:" IPSTR, IP2STR(&ip_info->gw));
    ESP_LOGI(TAG, "~~~~~~~~~~~");


    // Set ethernet as default interface
    esp_netif_set_default_netif(eth_netif);
    ESP_LOGI(TAG, "Set ethernet as default interface");

    // For SNTP, bind to ethernet IP
    /*esp_sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);
    esp_sntp_set_local_ip4(ip_info->ip.addr);
    ESP_LOGI(TAG, "SNTP bound to ethernet IP");
    */

    xEventGroupSetBits(s_connection_event_group, BIT0);
}

esp_err_t utils_ethernet_init(void) {
    s_connection_event_group = xEventGroupCreate();
    if (s_connection_event_group == NULL) {
        ESP_LOGE(TAG, "Failed to create event group");
        return ESP_FAIL;
    }
    esp_netif_config_t netif_config = ESP_NETIF_DEFAULT_ETH();
    eth_netif = esp_netif_new(&netif_config);

    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    phy_config.phy_addr = 0;
    phy_config.reset_gpio_num = -1;
    phy_config.autonego_timeout_ms = 5000;
    phy_config.reset_timeout_ms = 1000; // Increase reset timeout

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 4, 0)
    eth_esp32_emac_config_t esp32_emac_config = ETH_ESP32_EMAC_DEFAULT_CONFIG();
#else
    //this does not work for all eth chips...
    eth_esp32_emac_config_t esp32_emac_config =
    {
        .smi_gpio =
        {
            .mdc_num = 23,
            .mdio_num = 18
        },
        .interface = EMAC_DATA_INTERFACE_RMII,
        .clock_config =
        {
            .rmii =
            {
                .clock_mode = DEFAULT_RMII_CLK_MODE,
                .clock_gpio = static_cast<emac_rmii_clock_gpio_t>(DEFAULT_RMII_CLK_GPIO)
            }
        },
        .dma_burst_len = ETH_DMA_BURST_LEN_32,
        .intr_priority = 0,
    };
#endif

    esp_eth_mac_t *mac = esp_eth_mac_new_esp32(&esp32_emac_config, &mac_config);
    esp_eth_phy_t *phy = esp_eth_phy_new_lan87xx(&phy_config);

    esp_eth_config_t config = ETH_DEFAULT_CONFIG(mac, phy);
    config.check_link_period_ms = 5000; // Increase link check period

    ESP_ERROR_CHECK(esp_eth_driver_install(&config, &eth_handle));
    ESP_ERROR_CHECK(esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handle)));
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &ip_event_handler, NULL));
    ESP_ERROR_CHECK(esp_eth_start(eth_handle));
    EventBits_t bits  = xEventGroupWaitBits(s_connection_event_group, BIT0, pdFALSE, pdTRUE, portMAX_DELAY);
    if (bits & BIT0) {
        ESP_LOGI(TAG, "Ethernet initialized and connected");
    } else {
        ESP_LOGE(TAG, "Ethernet initialization timeout");
        return ESP_FAIL;
    }
    return ESP_OK;
}
#endif
