/**
 * @file logger.hpp
 * @author stroll (116356647@qq.com)
 * @brief 日志模块
 * @version 0.1
 * @date 2025-09-13
 *
 * @copyright Copyright (c) 2025
 *
 */

#pragma once
#include <cstdio>
#include <ctime>

namespace stroll {

enum LogLevel {
    kError = 0,
    kWarn,
    kInfo,
    kDebug,
};

static const char *const level_text[] = {
    [kError] = "Error",
    [kWarn] = "Warn",
    [kInfo] = "Info",
    [kDebug] = "Debug",
};

#define TAG "tag"
#define PATH_SEP '/'

static inline const char *log_base_file_name(const char *file_name) {
    unsigned index = 0;
    for (auto i = 0u; file_name[i] != '\0'; ++i) {
        if (file_name[i] == PATH_SEP) {
            index = i;
        }
    }
    return file_name[index] == PATH_SEP ? file_name + index + 1 : file_name;
}

#define TRACE_INFO(TAG, LEVEL, FMT, args...)                                                     \
    do {                                                                                         \
        struct timespec ts;                                                                      \
        clock_gettime(CLOCK_REALTIME, &ts);                                                      \
        struct tm datetime;                                                                      \
        localtime_r(&ts.tv_sec, &datetime);                                                      \
        std::printf("[%04d-%02d-%02d %02d:%02d:%02d.%03d][%s][%s][%s:%s:%d] " FMT,               \
                    datetime.tm_year + 1900, datetime.tm_mon + 1, datetime.tm_mday,              \
                    datetime.tm_hour, datetime.tm_min, datetime.tm_sec,                          \
                    (int)(ts.tv_nsec / 1000 / 1000), (TAG), level_text[(LEVEL)],                 \
                    log_base_file_name(__FILE__), __func__, __LINE__, ##args);                   \
    } while (0)

#define sl_error(fmt, args...) TRACE_INFO(TAG, kError, fmt, ##args)

#define sl_warn(fmt, args...) TRACE_INFO(TAG, kWarn, fmt, ##args)

#define sl_info(fmt, args...) TRACE_INFO(TAG, kInfo, fmt, ##args)

#define sl_debug(fmt, args...) TRACE_INFO(TAG, kDebug, fmt, ##args)

}  // namespace stroll
