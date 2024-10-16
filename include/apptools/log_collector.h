#pragma once

#include <cstddef>
#include <functional>
#include "esp_timer.h"

class LogCollector {
public:
    using LogSendCallback = std::function<void(const char*, size_t)>;

    LogCollector(size_t buffer_size, LogSendCallback callback);
    ~LogCollector();

    void enable();
    void disable();

private:
    static int log_vprintf_wrapper(const char *fmt, va_list args);
    int log_vprintf(const char *fmt, va_list args);
    static void send_logs_wrapper(void* arg);
    void send_logs();

    char* log_buffer_;
    size_t log_buffer_size_;
    size_t log_buffer_pos_;
    esp_timer_handle_t log_timer_;
    LogSendCallback send_callback_;

    static LogCollector* instance_;
};