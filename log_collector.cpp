#include "apptools/log_collector.h"
#include "esp_log.h"
#include <cstdarg>
#include <cstring>
#include <ctime>
#include <sys/time.h>

#define LOG_SEND_INTERVAL_US 10000000  // 10 seconds in microseconds

// Static instance initialization - happens before main()
LogCollector LogCollector::instance_;

LogCollector& LogCollector::instance() {
    return instance_;
}

LogCollector::LogCollector()
    : log_buffer_pos_(0)
    , buffer_mutex_(nullptr)
    , log_timer_(nullptr)
    , timer_initialized_(false) {

    // Create mutex early
    buffer_mutex_ = xSemaphoreCreateMutex();

    // Set our log handler immediately to capture early logs
    esp_log_set_vprintf(log_vprintf_wrapper);
}

bool LogCollector::initialize_timer() {
    if (timer_initialized_) {
        return true;
    }

    esp_timer_create_args_t log_timer_args = {
        .callback = &send_logs_wrapper,
        .arg = this,
        .name = "log_timer"
    };

    if (esp_timer_create(&log_timer_args, &log_timer_) != ESP_OK) {
        return false;
    }

    if (esp_timer_start_periodic(log_timer_, LOG_SEND_INTERVAL_US) != ESP_OK) {
        esp_timer_delete(log_timer_);
        return false;
    }

    timer_initialized_ = true;
    return true;
}

void LogCollector::set_callback(LogSendCallback callback) {
    if (buffer_mutex_) {
        xSemaphoreTake(buffer_mutex_, portMAX_DELAY);
    }

    send_callback_ = callback;

    // Initialize timer if this is the first callback set
    if (!timer_initialized_) {
        initialize_timer();
    }

    if (buffer_mutex_) {
        xSemaphoreGive(buffer_mutex_);
    }
}

void LogCollector::detach_callback() {
    if (buffer_mutex_) {
        xSemaphoreTake(buffer_mutex_, portMAX_DELAY);
    }

    send_callback_ = nullptr;

    if (buffer_mutex_) {
        xSemaphoreGive(buffer_mutex_);
    }
}

static void get_current_time(char* time_str, size_t max_len) {
    struct timeval tv;
    struct tm timeinfo;
    gettimeofday(&tv, NULL);
    localtime_r(&tv.tv_sec, &timeinfo);
    strftime(time_str, max_len, "%Y-%m-%d %H:%M:%S", &timeinfo);

    char ms_str[10];
    snprintf(ms_str, sizeof(ms_str), ".%03d", (int)(tv.tv_usec / 1000));
    strncat(time_str, ms_str, max_len - strlen(time_str) - 1);
}

int LogCollector::log_vprintf_wrapper(const char *fmt, va_list args) {
    return instance_.log_vprintf(fmt, args);
}

int LogCollector::log_vprintf(const char *fmt, va_list args) {
    // First, print to stdout for immediate debug visibility
    int stdout_ret = vprintf(fmt, args);

    // Now handle buffering
    int ret = 0;
    if (buffer_mutex_ && xSemaphoreTake(buffer_mutex_, pdMS_TO_TICKS(100)) == pdTRUE) {
        char time_str[32];
        get_current_time(time_str, sizeof(time_str));

        // Try to add timestamp
        int time_len = snprintf(log_buffer_ + log_buffer_pos_,
                              LOG_BUFFER_SIZE - log_buffer_pos_,
                              "%s ", time_str);

        if (time_len > 0) {
            log_buffer_pos_ += time_len;

            // Try to add log message
            va_list args_copy;
            va_copy(args_copy, args);
            ret = vsnprintf(log_buffer_ + log_buffer_pos_,
                          LOG_BUFFER_SIZE - log_buffer_pos_,
                          fmt, args_copy);
            va_end(args_copy);

            if (ret >= 0) {
                // Advance position only by what actually fit
                size_t space_left = LOG_BUFFER_SIZE - log_buffer_pos_ - 1;
                log_buffer_pos_ += (ret > space_left) ? space_left : ret;

                // Add newline if there's space
                if (log_buffer_pos_ < LOG_BUFFER_SIZE - 1) {
                    log_buffer_[log_buffer_pos_++] = '\n';
                }
            }
        }
        xSemaphoreGive(buffer_mutex_);
    }

    // Return stdout result to maintain compatibility
    return stdout_ret;
}

void LogCollector::send_logs_wrapper(void* arg) {
    static_cast<LogCollector*>(arg)->send_logs();
}

void LogCollector::send_logs() {
    if (!buffer_mutex_ || !send_callback_) return;

    if (xSemaphoreTake(buffer_mutex_, pdMS_TO_TICKS(100)) == pdTRUE) {
        if (log_buffer_pos_ > 0) {
            if (send_callback_) {
                // todo maybe add return value here to allow for not clearing buffer on reconnects
                send_callback_(log_buffer_, log_buffer_pos_);
                log_buffer_pos_ = 0;
            }
        }
        xSemaphoreGive(buffer_mutex_);
    }
}

LogCollector::~LogCollector() {
    if (timer_initialized_) {
        esp_timer_stop(log_timer_);
        esp_timer_delete(log_timer_);
    }
    if (buffer_mutex_) {
        vSemaphoreDelete(buffer_mutex_);
    }
}