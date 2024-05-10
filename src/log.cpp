#include "log.h"
#include <stdio.h>

#define OS_RTMP_LOG_MAX_LEN 1024

static const char * g_log_str[] = { "DEBUG","INFO","WARN","ERROR","FATAL" };

static void os_rtmp_log_default_cb(int level, const char * func, const char * fmt, va_list vl);

static RTMP_LOG_LEVEL_TYPE g_log_level = RTMP_LOG_LEVEL_TYPE::RTMP_LOG_LEVEL_WARN;
static rtmp_log_callback g_log_cb = os_rtmp_log_default_cb;

void os_rtmp_log_set_cb(const rtmp_log_callback cb)
{
    g_log_cb = cb;
}

void os_rtmp_log_set_level(int level)
{
    g_log_level = static_cast<RTMP_LOG_LEVEL_TYPE>(level);
}

int os_rtmp_log_get_level()
{
    return static_cast<int>(g_log_level);
}

void os_rtmp_log(int level, const char * func, const char * fmt, ...)
{
    if (level < g_log_level)
        return;

    va_list vl;
    va_start(vl, fmt);
    if (g_log_cb)
        g_log_cb(level, func, fmt, vl);
    va_end(vl);
}

void os_rtmp_log_default_cb(int level, const char * func, const char * fmt, va_list vl)
{
    char buff[OS_RTMP_LOG_MAX_LEN] = { 0 };
    int len = vsnprintf(buff, OS_RTMP_LOG_MAX_LEN, fmt, vl);
    if (len < OS_RTMP_LOG_MAX_LEN - 1)
        buff[len] = '\n';
    else
        buff[OS_RTMP_LOG_MAX_LEN - 1] = '\n';
    fprintf(stderr, "[%s] %s", g_log_str[level], buff);
}
