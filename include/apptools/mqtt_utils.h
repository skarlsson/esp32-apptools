#include "mqtt_client.h"
#pragma once

void mqtt_init_default(esp_mqtt_client_config_t *mqtt_config, const char *uri, int port);
