 #pragma once
#include "mqtt_client.h"
#include "esp_timer.h"
#include <memory>
//#include <functional>
#include <apptools/ota_handler.h>
#include "apptools/device_config.h"
#include <apptools/ha_discovery.h>

class LogCollector;

/*
 * implements home assistant sensors and registration in a somewhat generic way
 */
class ha_mqtt_handler {
public:

    ha_mqtt_handler(const esp_mqtt_client_config_t *mqtt_config,  const device_config_t* config, ota_handler *ota_handler);

    virtual ~ha_mqtt_handler();

    inline bool reboot_pending() const { return reboot_pending_; }

    void enable_logging();

    void addSensor(std::shared_ptr<ha_discovery::sensor_wrapper_t> sensor) {
        sensors_.push_back(sensor);
    }

    void add_managed_device(std::shared_ptr<ha_discovery::device_info_t>);
    void update_managed_device(const char* eid, const char* sw_tag, const char* sw_sha256);

    void start();

    void publish_auto_discovery();
protected:
    void publish_discovery(const ha_discovery::control_config_t &config);

    void subscribe_topics(std::shared_ptr<ha_discovery::device_info_t>);
    void publish_discovery(std::shared_ptr<ha_discovery::device_info_t>);

    void event_handler(esp_event_base_t base, int32_t event_id, void *event_data);
    static void event_handler_wrapper(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);
    void handle_control_message(const char* topic,  int topic_len, const char* data, int data_len);

    static void publish_state_wrapper(void* arg);
    void publish_state();

    void send_logs(const char* logs, size_t size);

    esp_mqtt_client_handle_t mqtt_client_ = nullptr;
    const device_config_t *config_ = nullptr;
    ota_handler *ota_handler_ = nullptr;
    LogCollector* log_collector_ = nullptr;

    int64_t built_in_sensor_next_ts_ = 0;

    std::vector<std::shared_ptr<ha_discovery::sensor_wrapper_t>> sensors_;
    std::vector<std::shared_ptr<ha_discovery::device_info_t>> sub_devices_;

    esp_timer_handle_t state_timer_ = nullptr;
    bool reboot_pending_ = false;
};
