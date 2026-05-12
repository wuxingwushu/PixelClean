#pragma once

#ifdef PIXEL_ENABLE_LOG

    #if defined(__ANDROID__)
        #include <android/log.h>
        #define PC_LOG_TAG "PixelClean"
        #define LOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, PC_LOG_TAG, __VA_ARGS__)
        #define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG,   PC_LOG_TAG, __VA_ARGS__)
        #define LOGI(...) __android_log_print(ANDROID_LOG_INFO,    PC_LOG_TAG, __VA_ARGS__)
        #define LOGW(...) __android_log_print(ANDROID_LOG_WARN,    PC_LOG_TAG, __VA_ARGS__)
        #define LOGE(...) __android_log_print(ANDROID_LOG_ERROR,   PC_LOG_TAG, __VA_ARGS__)
        #define LOGF(...) __android_log_print(ANDROID_LOG_FATAL,   PC_LOG_TAG, __VA_ARGS__)
    #elif defined(_WIN32)
        #include <cstdio>
        #define LOGV(...) printf("[VERBOSE] " __VA_ARGS__); printf("\n")
        #define LOGD(...) printf("[DEBUG] "   __VA_ARGS__); printf("\n")
        #define LOGI(...) printf("[INFO] "    __VA_ARGS__); printf("\n")
        #define LOGW(...) printf("[WARN] "    __VA_ARGS__); printf("\n")
        #define LOGE(...) fprintf(stderr, "[ERROR] " __VA_ARGS__); fprintf(stderr, "\n")
        #define LOGF(...) fprintf(stderr, "[FATAL] " __VA_ARGS__); fprintf(stderr, "\n")
    #endif

#else

    #define LOGV(...) ((void)0)
    #define LOGD(...) ((void)0)
    #define LOGI(...) ((void)0)
    #define LOGW(...) ((void)0)
    #define LOGE(...) ((void)0)
    #define LOGF(...) ((void)0)

#endif