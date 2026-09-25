#pragma once

#include <android/log.h>
#include <errno.h>
#include <string.h>

#ifdef __cplusplus
#include <format>
#include <string>
#include <utility>
#endif

#ifndef LS_LOG_TAG
#define LS_LOG_TAG "ls"
#endif

#ifndef LS_DEBUG_LOG
#define LS_DEBUG_LOG 0
#endif

#define LS_LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, LS_LOG_TAG, __VA_ARGS__))
#define LS_LOGW(...) ((void)__android_log_print(ANDROID_LOG_WARN, LS_LOG_TAG, __VA_ARGS__))
#define LS_LOGE(...) ((void)__android_log_print(ANDROID_LOG_ERROR, LS_LOG_TAG, __VA_ARGS__))

#define LS_LOGI_TAG(tag, ...) ((void)__android_log_print(ANDROID_LOG_INFO, LS_LOG_TAG, "[" tag "] " __VA_ARGS__))
#define LS_LOGW_TAG(tag, ...) ((void)__android_log_print(ANDROID_LOG_WARN, LS_LOG_TAG, "[" tag "] " __VA_ARGS__))
#define LS_LOGE_TAG(tag, ...) ((void)__android_log_print(ANDROID_LOG_ERROR, LS_LOG_TAG, "[" tag "] " __VA_ARGS__))

#ifdef __cplusplus
template <typename... Args> inline void ls_log_format(android_LogPriority priority, std::format_string<Args...> format, Args &&...args)
{
    const std::string message = std::format(format, std::forward<Args>(args)...);
    __android_log_write(priority, LS_LOG_TAG, message.c_str());
}

template <typename... Args> inline void ls_log_format_tag(android_LogPriority priority, const char *tag, std::format_string<Args...> format, Args &&...args)
{
    const std::string message = std::format(format, std::forward<Args>(args)...);
    __android_log_print(priority, LS_LOG_TAG, "[%s] %s", tag, message.c_str());
}

#define LS_LOGI_FMT(...)          ls_log_format(ANDROID_LOG_INFO, __VA_ARGS__)
#define LS_LOGW_FMT(...)          ls_log_format(ANDROID_LOG_WARN, __VA_ARGS__)
#define LS_LOGE_FMT(...)          ls_log_format(ANDROID_LOG_ERROR, __VA_ARGS__)
#define LS_LOGI_TAG_FMT(tag, ...) ls_log_format_tag(ANDROID_LOG_INFO, tag, __VA_ARGS__)
#define LS_LOGW_TAG_FMT(tag, ...) ls_log_format_tag(ANDROID_LOG_WARN, tag, __VA_ARGS__)
#define LS_LOGE_TAG_FMT(tag, ...) ls_log_format_tag(ANDROID_LOG_ERROR, tag, __VA_ARGS__)
#endif

#define LS_LOGE_ERRNO(message)                                               \
    do                                                                       \
    {                                                                        \
        const int ls_errno = errno;                                          \
        LS_LOGE("%s: errno=%d (%s)", message, ls_errno, strerror(ls_errno)); \
    } while (0)

#if LS_DEBUG_LOG
#define LS_DEBUG_ONLY(...)    __VA_ARGS__
#define LS_LOGD(...)          ((void)__android_log_print(ANDROID_LOG_DEBUG, LS_LOG_TAG, __VA_ARGS__))
#define LS_LOGD_TAG(tag, ...) ((void)__android_log_print(ANDROID_LOG_DEBUG, LS_LOG_TAG, "[" tag "] " __VA_ARGS__))
#ifdef __cplusplus
#define LS_LOGD_FMT(...)          ls_log_format(ANDROID_LOG_DEBUG, __VA_ARGS__)
#define LS_LOGD_TAG_FMT(tag, ...) ls_log_format_tag(ANDROID_LOG_DEBUG, tag, __VA_ARGS__)
#endif
#else
#define LS_DEBUG_ONLY(...)
#define LS_LOGD(...)          ((void)0)
#define LS_LOGD_TAG(tag, ...) ((void)0)
#ifdef __cplusplus
#define LS_LOGD_FMT(...)          ((void)0)
#define LS_LOGD_TAG_FMT(tag, ...) ((void)0)
#endif
#endif