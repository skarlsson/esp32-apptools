#pragma once
#include <apptools/ha_discovery.h>

class device_update_handler {
public:
    virtual ~device_update_handler() = default;
    
    // Return true if this handler can handle updates for the given device
    virtual bool can_handle(const ha_discovery::device_info_t* device) const = 0;
    
    // Handle the actual update. Returns true if update was handled successfully
    virtual bool handle_update(const ha_discovery::device_info_t* device, const char* json) = 0;
};
