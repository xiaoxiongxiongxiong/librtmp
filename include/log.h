#ifndef _OS_RTMP_LOG_H_
#define _OS_RTMP_LOG_H_

#include <stdarg.h>

#if defined(WIN32) || defined(_WIN32)
#if defined(OS_RTMP_API_EXPORT)
#define OS_RTMP_API __declspec(dllexport)
#else
#define OS_RTMP_API __declspec(dllimport)
#endif
#else
#define OS_RTMP_API
#endif

typedef enum _RTMP_LOG_LEVEL_TYPE
{
    RTMP_LOG_LEVEL_DEBUG,
    RTMP_LOG_LEVEL_INFO,
    RTMP_LOG_LEVEL_WARN,
    RTMP_LOG_LEVEL_ERROR,
    RTMP_LOG_LEVEL_FATAL
} RTMP_LOG_LEVEL_TYPE;

typedef void(*rtmp_log_callback)(int level, const char * func, const char * fmt, va_list vl);

/**
 * 
 */
OS_RTMP_API void os_rtmp_log_set_cb(const rtmp_log_callback cb);

/**
 * 
 */
OS_RTMP_API void os_rtmp_log_set_level(int level);

/**
 * 
 */
OS_RTMP_API int os_rtmp_log_get_level();

/**
 * 
 */
OS_RTMP_API void os_rtmp_log(int level, const char * func, const char * fmt, ...);

#define os_rtmp_log_debug(...) os_rtmp_log(RTMP_LOG_LEVEL_DEBUG, __FUNCTION__, __VA_ARGS__)
#define os_rtmp_log_info(...) os_rtmp_log(RTMP_LOG_LEVEL_INFO, __FUNCTION__, __VA_ARGS__)
#define os_rtmp_log_warn(...) os_rtmp_log(RTMP_LOG_LEVEL_WARN, __FUNCTION__, __VA_ARGS__)
#define os_rtmp_log_error(...) os_rtmp_log(RTMP_LOG_LEVEL_ERROR, __FUNCTION__, __VA_ARGS__)
#define os_rtmp_log_fatal(...) os_rtmp_log(RTMP_LOG_LEVEL_FATAL, __FUNCTION__, __VA_ARGS__)

#endif
