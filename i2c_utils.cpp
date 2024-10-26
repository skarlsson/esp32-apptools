#include <apptools/i2c_utils.h>
#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "esp_log.h"

#define TAG "i2c_utils"

/*
esp_err_t reset_i2c_bus(gpio_num_t sda_gpio, gpio_num_t scl_gpio) {
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT_OD;
    io_conf.pin_bit_mask = (1ULL << sda_gpio) | (1ULL << scl_gpio);
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);

    // Toggle SCL 9 times
    for(int i = 0; i < 9; i++) {
        gpio_set_level(scl_gpio, 0);
        esp_rom_delay_us(100);
        gpio_set_level(scl_gpio, 1);
        esp_rom_delay_us(100);
    }

    // Generate STOP condition
    gpio_set_level(sda_gpio, 0);
    esp_rom_delay_us(100);
    gpio_set_level(scl_gpio, 1);
    esp_rom_delay_us(100);
    gpio_set_level(sda_gpio, 1);
    esp_rom_delay_us(100);

    // Reconfigure pins for I2C driver
    io_conf.mode = GPIO_MODE_INPUT_OUTPUT_OD;
    gpio_config(&io_conf);

    return ESP_OK;
}
*/

esp_err_t i2c_bus_init(gpio_num_t sda_io_num, gpio_num_t scl_io_num, bool internal_pullup_enable, i2c_master_bus_handle_t* i2c_bus)
{
    ESP_LOGI(TAG, "Initialize I2C bus");

    i2c_master_bus_config_t bus_config = {};
    bus_config.clk_source = I2C_CLK_SRC_DEFAULT;
    bus_config.glitch_ignore_cnt = 7;
    bus_config.i2c_port = I2C_NUM_0;
    bus_config.sda_io_num = sda_io_num;
    bus_config.scl_io_num = scl_io_num;
    bus_config.flags.enable_internal_pullup = internal_pullup_enable;
    auto ret = i2c_new_master_bus(&bus_config, i2c_bus);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize I2C bus");
        return ret;
    }
    return ESP_OK;
}

void i2c_scan(i2c_master_bus_handle_t i2c_bus) {
    ESP_LOGI(TAG, "Scanning I2C bus...");
    for (uint8_t i = 1; i < 128; i++) {
        if (i2c_master_probe(i2c_bus,i, 1000) == ESP_OK) {
            ESP_LOGI(TAG, "Device found at address 0x%02X", i);
        }
    }
    ESP_LOGI(TAG, "Scan completed");
}
