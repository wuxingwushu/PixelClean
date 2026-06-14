#pragma once
#define PIXEL_ENABLE_LOG 1

#if PIXEL_ENABLE_LOG

    #if defined(__ANDROID__)
        #include <android/log.h>
        #define PC_LOG_TAG "PixelClean"
        #define LOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, PC_LOG_TAG, ##__VA_ARGS__)
        #define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG,   PC_LOG_TAG, ##__VA_ARGS__)
        #define LOGI(...) __android_log_print(ANDROID_LOG_INFO,    PC_LOG_TAG, ##__VA_ARGS__)
        #define LOGW(...) __android_log_print(ANDROID_LOG_WARN,    PC_LOG_TAG, ##__VA_ARGS__)
        #define LOGE(...) __android_log_print(ANDROID_LOG_ERROR,   PC_LOG_TAG, ##__VA_ARGS__)
        #define LOGF(...) __android_log_print(ANDROID_LOG_FATAL,   PC_LOG_TAG, ##__VA_ARGS__)
    #elif defined(_WIN32)
        #include <cstdio>
        #define LOGV(...) do { printf("[VERBOSE] "); printf(__VA_ARGS__); printf("\n"); } while(0)
        #define LOGD(...) do { printf("[DEBUG] ");   printf(__VA_ARGS__); printf("\n"); } while(0)
        #define LOGI(...) do { printf("[INFO] ");    printf(__VA_ARGS__); printf("\n"); } while(0)
        #define LOGW(...) do { printf("[WARN] ");    printf(__VA_ARGS__); printf("\n"); } while(0)
        #define LOGE(...) do { fprintf(stderr, "[ERROR] "); fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); } while(0)
        #define LOGF(...) do { fprintf(stderr, "[FATAL] "); fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); } while(0)
    #endif

#else

    #define LOGV(...) ((void)0)
    #define LOGD(...) ((void)0)
    #define LOGI(...) ((void)0)
    #define LOGW(...) ((void)0)
    #define LOGE(...) ((void)0)
    #define LOGF(...) ((void)0)

#endif