#include "apptools/log_collector.h"
#include "esp_log.h"
#include <cstdarg>
#include <cstring>
#include <ctime>
#include <sys/time.h>

#define LOG_SEND_INTERVAL 10000000 // 10 seconds in microseconds

LogCollector* LogCollector::instance_ = nullptr;

void get_current_time(char* time_str, size_t max_len) {
    struct timeval tv;
    struct tm timeinfo;
    gettimeofday(&tv, NULL);
    localtime_r(&tv.tv_sec, &timeinfo);
    strftime(time_str, max_len, "%Y-%m-%d %H:%M:%S", &timeinfo);

    // Append milliseconds
    char ms_str[10];
    snprintf(ms_str, sizeof(ms_str), ".%03d", (int)(tv.tv_usec / 1000));
    strncat(time_str, ms_str, max_len - strlen(time_str) - 1);
}

LogCollector::LogCollector(size_t buffer_size, LogSendCallback callback)
    : log_buffer_size_(buffer_size), send_callback_(callback) {
    log_buffer_ = new char[buffer_size];
    log_buffer_pos_ = 0;

    esp_timer_create_args_t log_timer_args = {
        .callback = &send_logs_wrapper,
        .arg = this,
        .name = "log_timer"
    };
    esp_timer_create(&log_timer_args, &log_timer_);

    instance_ = this;
}

LogCollector::~LogCollector() {
    disable();
    delete[] log_buffer_;
}

void LogCollector::enable() {
    esp_log_set_vprintf(log_vprintf_wrapper);
    esp_timer_start_periodic(log_timer_, LOG_SEND_INTERVAL);
}

void LogCollector::disable() {
    esp_log_set_vprintf(vprintf);
    esp_timer_stop(log_timer_);
}

int LogCollector::log_vprintf_wrapper(const char *fmt, va_list args) {
    return instance_->log_vprintf(fmt, args);
}


// int LogCollector::log_vprintf(const char *fmt, va_list args) {
//     int ret = vsnprintf(log_buffer_ + log_buffer_pos_, log_buffer_size_ - log_buffer_pos_, fmt, args);
//     if (ret >= 0) {
//         log_buffer_pos_ += ret;
//         /*if (log_buffer_pos_ < log_buffer_size_ - 1) {
//             log_buffer_[log_buffer_pos_++] = '\n';
//         }
//         */
//     }
//     return ret;
// }


int LogCollector::log_vprintf(const char *fmt, va_list args) {
    char time_str[32];
    get_current_time(time_str, sizeof(time_str));

    int time_len = snprintf(log_buffer_ + log_buffer_pos_, log_buffer_size_ - log_buffer_pos_, "%s ", time_str);
    if (time_len > 0) {
        log_buffer_pos_ += time_len;
    }

    int ret = vsnprintf(log_buffer_ + log_buffer_pos_, log_buffer_size_ - log_buffer_pos_, fmt, args);
    if (ret >= 0) {
        log_buffer_pos_ += ret;
        if (log_buffer_pos_ < log_buffer_size_ - 1) {
            log_buffer_[log_buffer_pos_++] = '\n';
        }
    }
    return ret;
}


void LogCollector::send_logs_wrapper(void* arg) {
    static_cast<LogCollector*>(arg)->send_logs();
}

void LogCollector::send_logs() {
    if (log_buffer_pos_ > 0) {
        send_callback_(log_buffer_, log_buffer_pos_);
        log_buffer_pos_ = 0;
    }
}