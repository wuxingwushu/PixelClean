#include "Platform/PlatformDetection.h"
#include "VulkanApp.h"

#if defined(PIXEL_ANDROID)

#include <android/log.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <jni.h>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>

#define LOG_TAG "PixelClean"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

static DemoApp*                 gVulkanApp = nullptr;
static std::thread*             gRenderThread = nullptr;
static std::atomic<bool>        gRunning{false};
static std::atomic<bool>        gPaused{false};
static ANativeWindow*           gWindow = nullptr;
static std::mutex               gWindowMutex;
static bool                     gInitialized = false;

static void RenderLoop()
{
    LOGD("Render thread started.");

    while (gRunning) {
        if (gPaused) {
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
            continue;
        }

        {
            std::lock_guard<std::mutex> lock(gWindowMutex);
            if (gVulkanApp && gVulkanApp->IsInitialized()) {
                gVulkanApp->DrawFrame();
            }
        }
    }

    LOGD("Render thread exiting.");
}

static bool InitVulkan(ANativeWindow* window)
{
    if (gVulkanApp && gVulkanApp->IsInitialized()) {
        LOGD("Vulkan already initialized.");
        return true;
    }

    if (gVulkanApp) {
        delete gVulkanApp;
        gVulkanApp = nullptr;
    }

    gVulkanApp = new DemoApp();
    if (!gVulkanApp->Initialize(window)) {
        LOGE("Failed to initialize Vulkan.");
        delete gVulkanApp;
        gVulkanApp = nullptr;
        return false;
    }

    gInitialized = true;
    LOGD("Vulkan initialized successfully.");
    return true;
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_pixelclean_MainActivity_stringFromJNI(
        JNIEnv* env,
        jobject /* this */)
{
#if PIXEL_PLATFORM_ANDROID
    std::string hello = "PixelClean Vulkan Engine running on Android";
#elif PIXEL_PLATFORM_WIN
    std::string hello = "PixelClean Vulkan Engine running on Windows";
#else
    std::string hello = "PixelClean Vulkan Engine running on Unknown platform";
#endif
    return env->NewStringUTF(hello.c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_pixelclean_MainActivity_getPlatformInfo(
        JNIEnv* env,
        jobject /* this */)
{
    std::string info;

#if PIXEL_PLATFORM_ANDROID
    info += "Android";
    #if PIXEL_ARCH_ARM64
    info += " / ARM64";
    #elif PIXEL_ARCH_X64
    info += " / x64";
    #endif

    #if PIXEL_BUILD_DEBUG
    info += " / Debug";
    #else
    info += " / Release";
    #endif
#elif PIXEL_PLATFORM_WIN
    info += "Windows";
    #if PIXEL_ARCH_X64
    info += " / x64";
    #endif

    #if PIXEL_BUILD_DEBUG
    info += " / Debug";
    #else
    info += " / Release";
    #endif
#endif

    if (gVulkanApp && gVulkanApp->IsInitialized()) {
        info += " | Vulkan: ACTIVE";
    } else {
        info += " | Vulkan: INIT";
    }

    return env->NewStringUTF(info.c_str());
}

extern "C" JNIEXPORT void JNICALL
Java_com_pixelclean_MainActivity_nativeSurfaceCreate(
        JNIEnv* env,
        jobject /* this */,
        jobject surface)
{
    if (!surface) {
        LOGE("nativeSurfaceCreate: null surface.");
        return;
    }

    std::lock_guard<std::mutex> lock(gWindowMutex);

    if (gWindow) {
        ANativeWindow_release(gWindow);
        gWindow = nullptr;
    }

    gWindow = ANativeWindow_fromSurface(env, surface);
    if (!gWindow) {
        LOGE("ANativeWindow_fromSurface failed.");
        return;
    }

    int32_t w = ANativeWindow_getWidth(gWindow);
    int32_t h = ANativeWindow_getHeight(gWindow);
    int32_t fmt = ANativeWindow_getFormat(gWindow);
    LOGD("Native surface created: %dx%d format=%d", w, h, fmt);

    if (!InitVulkan(gWindow)) {
        LOGE("Vulkan init failed.");
        return;
    }

    gRunning = true;
    gPaused = false;

    if (!gRenderThread) {
        gRenderThread = new std::thread(RenderLoop);
        LOGD("Render thread launched.");
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_pixelclean_MainActivity_nativeSurfaceDestroy(
        JNIEnv* env,
        jobject /* this */)
{
    LOGD("nativeSurfaceDestroy.");
    gRunning = false;

    if (gRenderThread) {
        gRenderThread->join();
        delete gRenderThread;
        gRenderThread = nullptr;
    }

    std::lock_guard<std::mutex> lock(gWindowMutex);

    if (gVulkanApp) {
        gVulkanApp->Shutdown();
        delete gVulkanApp;
        gVulkanApp = nullptr;
    }

    if (gWindow) {
        ANativeWindow_release(gWindow);
        gWindow = nullptr;
    }

    gInitialized = false;
}

extern "C" JNIEXPORT void JNICALL
Java_com_pixelclean_MainActivity_nativeSurfaceChange(
        JNIEnv* env,
        jobject /* this */,
        jint format,
        jint width,
        jint height)
{
    LOGD("nativeSurfaceChange: %dx%d format=%d", width, height, format);

    std::lock_guard<std::mutex> lock(gWindowMutex);
    if (gVulkanApp && gVulkanApp->IsInitialized()) {
        gVulkanApp->OnWindowResize();
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_pixelclean_MainActivity_enginePause(JNIEnv* env, jobject /* this */)
{
    LOGD("enginePause.");
    gPaused = true;
}

extern "C" JNIEXPORT void JNICALL
Java_com_pixelclean_MainActivity_engineResume(JNIEnv* env, jobject /* this */)
{
    LOGD("engineResume.");
    gPaused = false;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_pixelclean_MainActivity_isVulkanReady(JNIEnv* env, jobject /* this */)
{
    std::lock_guard<std::mutex> lock(gWindowMutex);
    return (gVulkanApp && gVulkanApp->IsInitialized()) ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL
Java_com_pixelclean_MainActivity_nativeTouchEvent(
        JNIEnv* env,
        jobject /* this */,
        jint action,
        jfloat x,
        jfloat y,
        jint pointerId)
{
    if (gVulkanApp && gVulkanApp->IsInitialized()) {
        gVulkanApp->OnTouchEvent(action, x, y, pointerId);
    }
}

#endif // PIXEL_ANDROID