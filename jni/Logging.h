#pragma once

// Useful for filtering logcat
#define LIB_TAG "SignatureHook"

#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, LIB_TAG, __VA_ARGS__))
#define LOGW(...) ((void)__android_log_print(ANDROID_LOG_WARN, LIB_TAG, __VA_ARGS__))

#ifdef __DEBUG__
#define LOGD(...) ((void)__android_log_print(ANDROID_LOG_DEBUG, LIB_TAG, __VA_ARGS__))
#else
#define LOGD(...)
#endif

#define LOG_RETURN_ADDRESS(level) LOGI("%p", (uintptr_t)__builtin_return_address(level) - g_libAddress);