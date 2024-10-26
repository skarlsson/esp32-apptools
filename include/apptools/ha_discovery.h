#pragma once
#include <cstdint>
#include <memory>
#include <functional>
#include <vector>

// this is based on home assistants device structure
namespace ha_discovery {
      constexpr size_t MAX_EID_LENGTH = 37; //UUID
      constexpr size_t MAX_NAME_LENGTH = 32;
      constexpr size_t MAX_MODEL_LENGTH = 20;
      constexpr size_t MAX_VERSION_LENGTH = 32;

      struct control_config_t {
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
            int64_t next_update_ms_=0;
      };

      struct device_info_t {
            static std::shared_ptr<device_info_t> make_shared(const char *eid,
            const char *name,
            const char *model,
            const char *hw_version,
            const char *sw_version);

            inline const char* eid() const { return eid_; }
            inline const char* name() const { return name_; }
            inline const char* model() const { return model_; }
            inline const char* hw_version() const { return hw_version_; }
            inline const char* sw_version() const { return sw_version_; }

            void add_sensor(std::shared_ptr<sensor_wrapper_t> sensor);

            inline const std::vector<std::shared_ptr<sensor_wrapper_t>>& sensors() const { return sensors_; }

private:
            char eid_[MAX_EID_LENGTH];
            char name_[MAX_NAME_LENGTH];
            char model_[MAX_MODEL_LENGTH];
            char hw_version_[MAX_VERSION_LENGTH];
            char sw_version_[MAX_VERSION_LENGTH];
            std::vector<std::shared_ptr<sensor_wrapper_t>> sensors_;
      };
}

