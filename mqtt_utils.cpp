#include <apptools/mqtt_utils.h>

void mqtt_init_default(esp_mqtt_client_config_t* mqtt_config, const char* uri, int port)
{
    if (mqtt_config == nullptr)
    {
        return;
    }

    // Zero out the entire structure first
    memset(mqtt_config, 0, sizeof(esp_mqtt_client_config_t));

    // Set the provided URI and port
    mqtt_config->broker.address.uri = uri;
    mqtt_config->broker.address.port = port;

    // Set sane defaults for other fields
    //mqtt_config->broker.address.transport = MQTT_TRANSPORT_OVER_TCP;

    mqtt_config->session.keepalive = 120; // 2 minutes
    mqtt_config->session.protocol_ver = MQTT_PROTOCOL_V_3_1_1;
    mqtt_config->session.last_will.qos = 1;

    mqtt_config->network.reconnect_timeout_ms = 10000; // 10 seconds

    mqtt_config->task.priority = 5;
    mqtt_config->task.stack_size = 6144; // 6 KB

    mqtt_config->buffer.size = 1024; // 1 KB
    mqtt_config->buffer.out_size = 1024; // 1 KB
}
