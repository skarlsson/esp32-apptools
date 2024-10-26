#pragma once
#include "esp_err.h"
#include "driver/i2c_master.h"

esp_err_t i2c_bus_init(gpio_num_t sda_io_num, gpio_num_t scl_io_num, bool enable_internal_pullup, i2c_master_bus_handle_t* i2c_bus);
void i2c_scan(i2c_master_bus_handle_t i2c_bus);

