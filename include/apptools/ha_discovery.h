#pragma once
#include <cstdint>
#include <memory>
#include <functional>
#include <vector>

// this is based on home assistants device structure
namespace ha_discovery {
    constexpr size_t MAX_EID_LENGTH = 37; //UUID
    constexpr size_t MAX_NAME_LENGTH = 32;
    constexpr size_t MAX_MODEL_LENGTH = 32;
    constexpr size_t MAX_VERSION_LENGTH = 32;
    constexpr size_t SHA256_LENGTH = 65;

    struct control_config_t {
        const char *type; // Required: sensor, switch, number, etc.
        const char *name; // Required: display name
        const char *value_key; // Required: key in the JSON state message
        const char *unit; // Optional: unit of measurement
        float min; // Optional: for number type
        float max; // Optional: for number type
        float step; // Optional: for number type
        const char *mode; // Optional: for number type
        const char *device_class; // Optional: device class for HA
        const char **options; // Optional: for select type
        int options_count; // Optional: for select type
        bool is_controllable; // Optional: indicates if command topic should be included
        const char *state_on; // Optional: for switch/binary_sensor
        const char *state_off; // Optional: for switch/binary_sensor
        const char *payload_on; // Optional: for switch
        const char *payload_off; // Optional: for switch

        // Constructor with all fields, providing defaults
        control_config_t(
            const char *type_,
            const char *name_,
            const char *value_key_,
            const char *unit_ = nullptr,
            float min_ = 0,
            float max_ = 0,
            float step_ = 0,
            const char *mode_ = nullptr,
            const char *device_class_ = nullptr,
            const char **options_ = nullptr,
            int options_count_ = 0,
            bool is_controllable_ = false,
            const char *state_on_ = nullptr,
            const char *state_off_ = nullptr,
            const char *payload_on_ = nullptr,
            const char *payload_off_ = nullptr
        ) : type(type_),
            name(name_),
            value_key(value_key_),
            unit(unit_),
            min(min_),
            max(max_),
            step(step_),
            mode(mode_),
            device_class(device_class_),
            options(options_),
            options_count(options_count_),
            is_controllable(is_controllable_),
            state_on(state_on_),
            state_off(state_off_),
            payload_on(payload_on_),
            payload_off(payload_off_) {
        }

        // Static helper methods for common configurations
        static control_config_t make_sensor(
            const char *name,
            const char *value_key,
            const char *unit = nullptr,
            const char *device_class = nullptr
        ) {
            return control_config_t(
                "sensor", name, value_key,
                unit, 0, 0, 0, nullptr,
                device_class
            );
        }

        static control_config_t make_switch(
            const char *name,
            const char *value_key,
            bool is_controllable = true,
            const char *state_on = "ON",
            const char *state_off = "OFF"
        ) {
            return control_config_t(
                "switch", name, value_key,
                nullptr, 0, 0, 0, nullptr, nullptr,
                nullptr, 0, is_controllable,
                state_on, state_off, state_on, state_off
            );
        }

        static control_config_t make_number(
            const char *name,
            const char *value_key,
            float min,
            float max,
            float step = 1,
            const char *unit = nullptr,
            bool is_controllable = true
        ) {
            return control_config_t(
                "number", name, value_key,
                unit, min, max, step, "slider",
                nullptr, nullptr, 0, is_controllable
            );
        }

        static control_config_t make_subscribed_text(const char *name, const char *value_key) {
            return control_config_t(
                "text", name, value_key, nullptr, 0, 0, 0, nullptr, nullptr, nullptr, 0, true
            );
        }

        static control_config_t make_button(const char *name, const char *value_key) {
            return control_config_t(
                "button", name, value_key, nullptr, 0, 0, 0, nullptr, nullptr, nullptr, 0, true
            );
        }
    };

    class sensor_wrapper_t {
    public:
        using DiscoveryFunc = std::function<std::vector<control_config_t>()>;
        using PayloadFunc = std::function<std::string()>;

        sensor_wrapper_t(uint32_t min_intervall_ms, DiscoveryFunc discovery, PayloadFunc payload);

        std::vector<control_config_t> get_control_config() const { return discoveryFunc_(); }
        std::string get_payload() const { return payloadFunc_(); }

    private:
        DiscoveryFunc discoveryFunc_;
        PayloadFunc payloadFunc_;
        uint32_t min_intervall_ms_;
        int64_t next_update_ms_ = 0;
    };

    struct device_info_t {
        static std::shared_ptr<device_info_t> make_shared(
            const char *eid,
            const char *name,
            const char *model,
            const char *hw_version,
            const char *sw_tag,
            const char *sha256);

        inline const char *eid() const { return eid_; }
        inline const char *name() const { return name_; }
        inline const char *model() const { return model_; }
        inline const char *hw_version() const { return hw_version_; }
        inline const char *sw_tag() const { return sw_tag_; }
        inline const char *sha256() const { return sha256_; }

        void add_sensor(std::shared_ptr<sensor_wrapper_t> sensor);

        void set_sw_tag(const char*);
        void set_sw_sha256(const char*);

        inline const std::vector<std::shared_ptr<sensor_wrapper_t> > &sensors() const { return sensors_; }

    private:
        char eid_[MAX_EID_LENGTH];
        char name_[MAX_NAME_LENGTH];
        char model_[MAX_MODEL_LENGTH];
        char hw_version_[MAX_VERSION_LENGTH];
        char sw_tag_[MAX_VERSION_LENGTH];
        char sha256_[SHA256_LENGTH];
        std::vector<std::shared_ptr<sensor_wrapper_t> > sensors_;
    };
}
