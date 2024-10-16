#pragma once
#include "mqtt_client.h"
#include "ota_handler.h"
#include "apptools/device_config.h"
#include "esp_timer.h"
#include <memory>
#include <functional>

class LogCollector;

class mqtt_handler_ota {
public:
    struct ControlConfig {
        const char *type;
        const char *name;
        const char *value_key;
        const char *unit;
        float min;
        float max;
        float step;
        const char *mode;
        const char *device_class;
        const char **options;
        int options_count;
    };

    class SensorWrapper {
    public:
        using DiscoveryFunc = std::function<std::vector<mqtt_handler_ota::ControlConfig>()>;
        using PayloadFunc = std::function<std::string()>;

        SensorWrapper(const std::string& name, DiscoveryFunc discovery, PayloadFunc payload)
            : name_(name), discoveryFunc_(discovery), payloadFunc_(payload) {}

        std::string getName() const { return name_; }
        std::vector<mqtt_handler_ota::ControlConfig> getDiscoveryConfigs() const { return discoveryFunc_(); }
        std::string getPayload() const { return payloadFunc_(); }


    private:
        std::string name_;
        DiscoveryFunc discoveryFunc_;
        PayloadFunc payloadFunc_;
    };

    struct SubdeviceInfo {
        std::string id;
        std::string name;
        std::string model;
        std::vector<std::shared_ptr<SensorWrapper>> sensors;
    };

    mqtt_handler_ota(const esp_mqtt_client_config_t *mqtt_config,  const device_config_t* config, ota_handler *ota_handler);

    virtual ~mqtt_handler_ota();

    inline bool reboot_pending() const { return reboot_pending_; }

    void enable_logging();

    void addSensor(std::shared_ptr<SensorWrapper> sensor) {
        sensors_.push_back(sensor);
    }

    void add_subdevice(const SubdeviceInfo& subdevice);
    void publish_subdevice_discovery(const SubdeviceInfo& subdevice);

    void start();

protected:
    void publish_auto_discovery();
    void publish_discovery(const ControlConfig &config);

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

    std::vector<std::shared_ptr<SensorWrapper>> sensors_;
    std::vector<SubdeviceInfo> subdevices_;

    esp_timer_handle_t state_timer_ = nullptr;
    bool reboot_pending_ = false;
};
