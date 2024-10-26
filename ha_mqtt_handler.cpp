#include "apptools/ha_mqtt_handler.h"
#include <algorithm>
#include "esp_log.h"
#include "apptools/log_collector.h"
#include "apptools/system_stats.h"

static const char *TAG = "mqtt_handler_ota";

#define MAX_TOPIC_LEN 128
#define MAX_PAYLOAD_LEN 1024
#define MQTT_ROOT_TOPIC "huzza32"

ha_mqtt_handler::ha_mqtt_handler(const esp_mqtt_client_config_t *mqtt_config, const device_config_t *config,
                                   ota_handler *ota_handler) : mqtt_client_(esp_mqtt_client_init(mqtt_config)),
                                                               config_(config),
                                                               ota_handler_(ota_handler) {
}

void ha_mqtt_handler::start() {
    auto err = esp_mqtt_client_start(mqtt_client_);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start MQTT client: %s", esp_err_to_name(err));
    }

    err = esp_mqtt_client_register_event(mqtt_client_, MQTT_EVENT_ANY, event_handler_wrapper, this);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register MQTT event handler: %s", esp_err_to_name(err));
    }

    // it seems that if we start to quick here - kernel crashes...
    vTaskDelay(2000 / portTICK_PERIOD_MS);

    log_collector_ = new LogCollector(4096, [this](const char* logs, size_t size) {
          this->send_logs(logs, size);
    });

    // Start a timer to publish uptime every minute
    esp_timer_create_args_t timer_args = {
        .callback = &publish_state_wrapper,
        .arg = this,
        .name = "state_timer"
    };
    esp_timer_create(&timer_args, &state_timer_);
    esp_timer_start_periodic(state_timer_, 100000); // 10hz
}

ha_mqtt_handler::~ha_mqtt_handler() {
    log_collector_->disable();
    // Stop and delete the timer
    if (state_timer_) {
        esp_timer_stop(state_timer_);
        esp_timer_delete(state_timer_);
        state_timer_ = nullptr;
    }

    // Clean up MQTT client
    if (mqtt_client_) {
        esp_mqtt_client_stop(mqtt_client_);
        esp_mqtt_client_destroy(mqtt_client_);
        mqtt_client_ = nullptr;
    }
    delete log_collector_;
}

void ha_mqtt_handler::enable_logging() {
    log_collector_->enable();
}

void ha_mqtt_handler::send_logs(const char* logs, size_t size) {
    char topic[MAX_TOPIC_LEN];
    snprintf(topic, sizeof(topic), "%s/%s/logs", MQTT_ROOT_TOPIC, config_->eid);

    esp_mqtt_client_publish(mqtt_client_, topic, logs, size, 0, 0);
}

void ha_mqtt_handler::addDevice(std::shared_ptr<ha_discovery::device_info_t> p) {
    // TODO SHOULD WE BE ABLE TO REDISCOVER UPDATED SENSORS - IE UPDATED VERSIONS?
    sub_devices_.push_back(p);
    publish_discovery(p);
}

/*void mqtt_handler_ota::add_device_sensor(const char* eid, std::shared_ptr<SensorWrapper> sensor) {
    for (const auto& config : sensor->getDiscoveryConfigs()) {
        publish_subdevice_discovery(eid, config);
    }
}
*/

void
ha_mqtt_handler::event_handler_wrapper(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    auto handler = static_cast<ha_mqtt_handler *>(handler_args);
    handler->event_handler(base, event_id, event_data);
}

void ha_mqtt_handler::event_handler(esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t) event_data;
    //esp_mqtt_client_handle_t client = event->client;
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED: {
            ESP_LOGI(TAG, "MQTT Connected");
            publish_auto_discovery();
            publish_state();
        }
        break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT Disconnected");
            break;
        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT Subscribed");
            break;
        case MQTT_EVENT_DATA: {
            ESP_LOGI(TAG, "MQTT Data Received");
            if (strstr(event->topic, "/set") != nullptr) {
                handle_control_message(event->topic, event->topic_len, event->data, event->data_len);
            }
        }
        break;
        default:
            break;
    }
}

void ha_mqtt_handler::handle_control_message(const char *topic, int topic_len, const char *data, int data_len) {
    // I want this allocation to go on the heap
    static char topic_str[MAX_TOPIC_LEN];
    static char value[MAX_PAYLOAD_LEN];

    // Safely copy the topic
    int safe_topic_len = std::min(topic_len, (int) sizeof(topic_str) - 1);
    strncpy(topic_str, topic, safe_topic_len);
    topic_str[safe_topic_len] = '\0';

    // Safely copy the value
    int safe_data_len = std::min(data_len, (int) sizeof(value) - 1);
    strncpy(value, data, safe_data_len);
    value[safe_data_len] = '\0';

    ESP_LOGI(TAG, "Received test control message - Topic: %s, Value: %s", topic_str, value);


    if (strstr(topic_str, "ota_string") != nullptr) {
        ESP_LOGI(TAG, "OTA string received: %s", value);
        ota_handler_->handle_ota_update(value);
    } else if (strstr(topic_str, "config_string") != nullptr) {
        ESP_LOGI(TAG, "config string received: %s", value);
    } else if (strstr(topic_str, "reboot_button") != nullptr) {
        ESP_LOGI(TAG, "Reboot button pressed");
        reboot_pending_ = true;
    }
}

void ha_mqtt_handler::publish_auto_discovery() {
    // we could move this to a better structure since we dont export to ha
    ha_discovery::control_config_t subscriptions[] = {
        {"text", "config", "config_string", nullptr, 0, 0, 0, nullptr, nullptr, nullptr, 0},
        {"text", "ota", "ota_string", nullptr, 0, 0, 0, nullptr, nullptr, nullptr, 0}
        };

    // default ha config
    ha_discovery::control_config_t builtin_controls[] = {
        {"button", "reboot", "reboot_button", nullptr, 0, 0, 0, nullptr, nullptr, nullptr, 0},
    };

    ha_discovery::control_config_t builtin_sensors[] = {
        {"sensor", "uptime", "uptime", "s", 0, 0, 0, nullptr, "duration", nullptr, 0},
        {"sensor", "cpu_load", "cpu_load", "%", 0, 100, 0.1, nullptr, nullptr, nullptr, 0},
        {"sensor", "free_memory", "free_memory", "bytes", 0, 0, 0, nullptr, nullptr, nullptr, 0}
    };

    for (const auto &control: builtin_controls) {
        publish_discovery(control);
    }

    for (const auto &sensor: builtin_sensors) {
        publish_discovery(sensor);
    }

    for (const auto& sensor : sensors_) {
        for (const auto& config : sensor->get_control_config()) {
            publish_discovery(config);
        }
    }

    for (auto& sub_device : sub_devices_ )
       publish_discovery(sub_device);

    for (const auto &subscription: subscriptions) {
            char topic[128];
            snprintf(topic, sizeof(topic), "%s/%s/%s/set", MQTT_ROOT_TOPIC, config_->eid, subscription.value_key);
            esp_mqtt_client_subscribe(mqtt_client_, topic, 0);
            ESP_LOGI(TAG, "Subscribed to topic: %s", topic);
    }

    for (const auto &control: builtin_controls) {
            char topic[128];
            snprintf(topic, sizeof(topic), "%s/%s/%s/set", MQTT_ROOT_TOPIC, config_->eid, control.value_key);
            esp_mqtt_client_subscribe(mqtt_client_, topic, 0);
            ESP_LOGI(TAG, "Subscribed to topic: %s", topic);
    }

    publish_state();
}

void ha_mqtt_handler::publish_state() {
    char topic[MAX_TOPIC_LEN];
    char payload[MAX_PAYLOAD_LEN];

    snprintf(topic, sizeof(topic), "%s/%s/state", MQTT_ROOT_TOPIC, config_->eid);

    int payload_len=0;

    int64_t now = esp_timer_get_time()/1000;

    if (built_in_sensor_next_ts_ < now){
      built_in_sensor_next_ts_ = now + 10000; // 10s on builtin stuff

      int64_t uptime_seconds = now / 1000;
      float cpu_load = get_cpu_load().total;
      uint32_t free_memory = esp_get_free_heap_size();

      payload_len = snprintf(payload, sizeof(payload),
             "{\"uptime\": %lld, \"cpu_load\": %.1f, \"free_memory\": %lu",
             uptime_seconds, cpu_load, free_memory);
    }

    for (const auto& sensor : sensors_) {
        std::string sensor_payload = sensor->get_payload();
        if (!sensor_payload.empty()) {
            //first?
            if (payload_len==0){
              payload_len += snprintf(payload + payload_len, sizeof(payload) - payload_len,
                                    "{%s", sensor_payload.c_str());
            } else {
               payload_len += snprintf(payload + payload_len, sizeof(payload) - payload_len,
                                    ", %s", sensor_payload.c_str());
            }
        }
    }
    if (payload_len>0){
      // Close the JSON object
      payload_len += snprintf(payload + payload_len, sizeof(payload) - payload_len, "}");
      esp_mqtt_client_publish(mqtt_client_, topic, payload, 0, 0, 0);
      //ESP_LOGI(TAG, "Published state: %s", payload);
    }


     // Now publish each subdevice state separately
    for (const auto& subdevice : sub_devices_) {
        // Create unique topic for each subdevice
        snprintf(topic, sizeof(topic), "%s/%s/%s/state",
                MQTT_ROOT_TOPIC, config_->eid, subdevice->eid());

        // Reset payload buffer
        payload_len = snprintf(payload, sizeof(payload), "{");
        bool first_sensor = true;

        // Add all sensors for this subdevice
        for (const auto& sensor : subdevice->sensors()) {
            std::string sensor_payload = sensor->get_payload();
            if (!sensor_payload.empty()) {
                if (!first_sensor) {
                    payload_len += snprintf(payload + payload_len, sizeof(payload) - payload_len, ", ");
                }
                payload_len += snprintf(payload + payload_len, sizeof(payload) - payload_len,
                                      "%s", sensor_payload.c_str());
                first_sensor = false;
            }
        }

        // Close the subdevice JSON
        payload_len += snprintf(payload + payload_len, sizeof(payload) - payload_len, "}");

        // Publish subdevice state if we have any sensor data
        if (payload_len > 2) {  // More than just "{}"
            esp_mqtt_client_publish(mqtt_client_, topic, payload, 0, 0, 0);
            //ESP_LOGI(TAG, "Published subdevice state for %s: %s", subdevice->eid(), payload);
        }
    }


}

void ha_mqtt_handler::publish_discovery(const ha_discovery::control_config_t &config) {
    char discovery_topic[MAX_TOPIC_LEN];
    char payload[MAX_PAYLOAD_LEN];

    snprintf(discovery_topic, sizeof(discovery_topic), "homeassistant/%s/%s_%s/config",
             config.type, config_->eid, config.value_key);

    int payload_len = snprintf(payload, sizeof(payload),
                               "{"
                               "\"name\":\"%s\","
                               "\"state_topic\":\"%s/%s/state\","
                               "\"unique_id\":\"%s_%s\","
                               "\"device\":{"
                               "\"identifiers\":[\"%s\"],"
                               "\"name\":\"%s %s\","
                               "\"model\":\"%s\","
                               "\"manufacturer\":\"%s\","
                               "\"hw_version\":\"%s\","
                               "\"sw_version\":\"%s\""
                               "},"
                               "\"value_template\":\"{{ value_json.%s }}\","
                               "\"command_topic\":\"%s/%s/%s/set\"",
                               config.name,
                               MQTT_ROOT_TOPIC, config_->eid, // state_topic
                               config_->eid, config.value_key, // unique_id
                               config_->eid, // identifiers
                               config_->model, config_->eid,  // name
                               config_->model, // model
                               config_->manufacturer,
                               config_->hardware_revision,
                               FIRMWARE_VERSION,
                               config.value_key,
                               MQTT_ROOT_TOPIC, config_->eid, config.value_key
    );

    if (strcmp(config.type, "number") == 0) {
        payload_len += snprintf(payload + payload_len, sizeof(payload) - payload_len,
                                R"(,"min":%.1f,"max":%.1f,"step":%.2f,"mode":"%s")",
                                config.min, config.max, config.step, config.mode);
    } else if (strcmp(config.type, "select") == 0 && config.options != nullptr) {
        payload_len += snprintf(payload + payload_len, sizeof(payload) - payload_len, ",\"options\":[");
        for (int i = 0; i < config.options_count; i++) {
            payload_len += snprintf(payload + payload_len, sizeof(payload) - payload_len,
                                    "\"%s\"%s", config.options[i], (i < config.options_count - 1) ? "," : "");
        }
        payload_len += snprintf(payload + payload_len, sizeof(payload) - payload_len, "]");
    } else if (strcmp(config.type, "switch") == 0) {
        payload_len += snprintf(payload + payload_len, sizeof(payload) - payload_len,
                                ",\"payload_on\":\"ON\",\"payload_off\":\"OFF\","
                                "\"state_on\":\"ON\",\"state_off\":\"OFF\"");
    }

    if (config.unit) {
        payload_len += snprintf(payload + payload_len, sizeof(payload) - payload_len,
                                R"(,"unit_of_measurement":"%s")", config.unit);
    }

    if (config.device_class) {
        payload_len += snprintf(payload + payload_len, sizeof(payload) - payload_len,
                                R"(,"device_class":"%s")", config.device_class);
    }

    // hardcode expire for now
    payload_len += snprintf(payload + payload_len, sizeof(payload) - payload_len,
                            ",\"expire_after\":30");

    payload_len += snprintf(payload + payload_len, sizeof(payload) - payload_len, "}");

    esp_mqtt_client_publish(mqtt_client_, discovery_topic, payload, 0, 1, 0);
    ESP_LOGI(TAG, "Published discovery for %s: sz=%d", config.name, payload_len);
}

void ha_mqtt_handler::publish_discovery(std::shared_ptr<ha_discovery::device_info_t> device_info){
    char discovery_topic[MAX_TOPIC_LEN];
    char payload[MAX_PAYLOAD_LEN];

    const char* device_manufacturer = "csi";
    for (auto& sensor : device_info->sensors()) {
        for (auto& config : sensor->get_control_config()) {
            snprintf(discovery_topic, sizeof(discovery_topic), "homeassistant/%s/%s_%s/config",
                     config.type, device_info->eid(), config.value_key);

            // todo should we always have a command topic??=
            int payload_len = snprintf(payload, sizeof(payload),
                                       "{"
                                       "\"name\":\"%s\","
                                       "\"state_topic\":\"%s/%s/%s/state\","
                                       "\"unique_id\":\"%s_%s\","
                                       "\"device\":{"
                                       "\"identifiers\":[\"%s\"],"
                                       "\"name\":\"%s\","
                                       "\"model\":\"%s\","
                                       "\"manufacturer\":\"%s\","
                                       "\"hw_version\":\"%s\","
                                       "\"sw_version\":\"%s\","
                                       "\"via_device\":\"%s\""
                                       "},"
                                       "\"value_template\":\"{{ value_json.%s }}\","
                                       "\"command_topic\":\"%s/%s/%s/set\"",
                                       config.name, // meassurement name
                                       MQTT_ROOT_TOPIC, config_->eid, device_info->eid(), // state_topic
                                       device_info->eid(), config.value_key, // unique_id
                                       device_info->eid(), // identifiers
                                       device_info->name(), // device name
                                       device_info->model(), // model
                                       device_manufacturer,
                                       device_info->hw_version(),
                                       device_info->sw_version(),
                                       config_->eid, // via device
                                       config.value_key,
                                       MQTT_ROOT_TOPIC, config_->eid, config.value_key
            );

            if (strcmp(config.type, "number") == 0) {
                payload_len += snprintf(payload + payload_len, sizeof(payload) - payload_len,
                                        R"(,"min":%.1f,"max":%.1f,"step":%.2f,"mode":"%s")",
                                        config.min, config.max, config.step, config.mode);
            } else if (strcmp(config.type, "select") == 0 && config.options != nullptr) {
                payload_len += snprintf(payload + payload_len, sizeof(payload) - payload_len, ",\"options\":[");
                for (int i = 0; i < config.options_count; i++) {
                    payload_len += snprintf(payload + payload_len, sizeof(payload) - payload_len,
                                            "\"%s\"%s", config.options[i], (i < config.options_count - 1) ? "," : "");
                }
                payload_len += snprintf(payload + payload_len, sizeof(payload) - payload_len, "]");
            } else if (strcmp(config.type, "switch") == 0) {
                payload_len += snprintf(payload + payload_len, sizeof(payload) - payload_len,
                                        ",\"payload_on\":\"ON\",\"payload_off\":\"OFF\","
                                        "\"state_on\":\"ON\",\"state_off\":\"OFF\"");
            }

            if (config.unit) {
                payload_len += snprintf(payload + payload_len, sizeof(payload) - payload_len,
                                        R"(,"unit_of_measurement":"%s")", config.unit);
            }

            if (config.device_class) {
                payload_len += snprintf(payload + payload_len, sizeof(payload) - payload_len,
                                        R"(,"device_class":"%s")", config.device_class);
            }

            // hardcode expire for now
            payload_len += snprintf(payload + payload_len, sizeof(payload) - payload_len,
                                    ",\"expire_after\":30");

            payload_len += snprintf(payload + payload_len, sizeof(payload) - payload_len, "}");

            esp_mqtt_client_publish(mqtt_client_, discovery_topic, payload, 0, 1, 0);
            ESP_LOGI(TAG, "Published discovery for subdevice %s : sz=%d", config.name, payload_len);
        }
    }
}

void ha_mqtt_handler::publish_state_wrapper(void* arg) {
    auto handler = static_cast<ha_mqtt_handler*>(arg);
    handler->publish_state();
}

