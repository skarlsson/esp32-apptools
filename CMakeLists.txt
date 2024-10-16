file(GLOB UTIL_FILES *)
file(GLOB SENSOR_FILES sensors/*)
idf_component_register(SRCS ${UTIL_FILES} ${SENSOR_FILES}
                  INCLUDE_DIRS "include"
                  REQUIRES mqtt esp_wifi app_update esp_https_ota json esp_driver_i2c esp_driver_gpio esp_eth)
