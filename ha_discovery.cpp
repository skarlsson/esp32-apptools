#include <apptools/ha_discovery.h>
#include <string.h>

namespace ha_discovery {
    sensor_wrapper_t::sensor_wrapper_t(uint32_t min_intervall_ms, DiscoveryFunc discovery, PayloadFunc payload)
        : discoveryFunc_(discovery), payloadFunc_(payload), min_intervall_ms_(min_intervall_ms) {
    }

    std::shared_ptr<device_info_t> device_info_t::make_shared(const char *eid,
                                                              const char *name,
                                                              const char *model,
                                                              const char *hw_version,
                                                              const char *sw_tag,
                                                              const char *sha256) {
        auto p = std::make_shared<device_info_t>();
        strncpy(p->eid_, eid, MAX_EID_LENGTH - 1);
        strncpy(p->name_, name, MAX_NAME_LENGTH - 1);
        strncpy(p->model_, model, MAX_MODEL_LENGTH - 1);
        strncpy(p->hw_version_, hw_version, MAX_VERSION_LENGTH - 1);
        strncpy(p->sw_tag_, sw_tag, MAX_VERSION_LENGTH - 1);
        strncpy(p->sha256_, sha256, SHA256_LENGTH - 1);

        // Ensure null termination
        p->eid_[MAX_EID_LENGTH - 1] = '\0';
        p->name_[MAX_NAME_LENGTH - 1] = '\0';
        p->model_[MAX_MODEL_LENGTH - 1] = '\0';
        p->hw_version_[MAX_VERSION_LENGTH - 1] = '\0';
        p->sw_tag_[MAX_VERSION_LENGTH - 1] = '\0';
        p->sha256_[SHA256_LENGTH - 1] = '\0';
        return p;
    }

    void device_info_t::set_sw_tag(const char* sw_tag) {
        strncpy(sw_tag_, sw_tag, MAX_VERSION_LENGTH - 1);
        sw_tag_[MAX_VERSION_LENGTH - 1] = '\0';
    }
    void device_info_t::set_sw_sha256(const char* sha256) {
        strncpy(sha256_, sha256, SHA256_LENGTH - 1);
        sha256_[SHA256_LENGTH - 1] = '\0';
    }

    void device_info_t::add_sensor(std::shared_ptr<sensor_wrapper_t> sensor) {
        sensors_.push_back(sensor);
    }
}
