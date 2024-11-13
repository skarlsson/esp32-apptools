#pragma once

#include <cstddef>
#include <functional>
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

class LogCollector {
public:
    using LogSendCallback = std::function<void(const char*, size_t)>;

    static LogCollector& instance();

    // Delete copy and assignment operators
    LogCollector(const LogCollector&) = delete;
    LogCollector& operator=(const LogCollector&) = delete;

    // Set callback for sending logs (called when MQTT is ready)
    void set_callback(LogSendCallback callback);

    // Stop sending logs (e.g., if MQTT disconnects)
    void detach_callback();

private:
    // Private constructor - called automatically before main
    LogCollector();
    ~LogCollector();

    static int log_vprintf_wrapper(const char *fmt, va_list args);
    int log_vprintf(const char *fmt, va_list args);
    static void send_logs_wrapper(void* arg);
    void send_logs();
    bool initialize_timer();

    // Static instance for early initialization
    static LogCollector instance_;

    // Buffer management
    static constexpr size_t LOG_BUFFER_SIZE = 32768; // 32KB buffer
    char log_buffer_[LOG_BUFFER_SIZE];
    size_t log_buffer_pos_;

    // Thread safety
    SemaphoreHandle_t buffer_mutex_;

    // Timer for periodic sending
    esp_timer_handle_t log_timer_;

    // Callback
    LogSendCallback send_callback_;
    bool timer_initialized_;
};