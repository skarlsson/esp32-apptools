#pragma once

struct cpu_usage_t {
    float total;
    float core0;
    float core1;
};

cpu_usage_t get_cpu_load();

