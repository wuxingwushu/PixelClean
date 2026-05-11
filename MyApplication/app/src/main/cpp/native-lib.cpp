#include <jni.h>
#include <android/log.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>

#include "GlobalVariable.h"
#include "Tool/Tool.h"
#include "application.h"

#include <unistd.h>
#include <cerrno>

#define LOG_TAG "PixelClean"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

static GAME::Application* gApplication = nullptr;
static bool gInitialized = false;
static std::string gFilesDir;

extern "C" JNIEXPORT void JNICALL
Java_com_pixelclean_MainActivity_nativeSetFilesDir(
    JNIEnv* env,
    jobject /* this */,
    jstring path) {

    const char* pathStr = env->GetStringUTFChars(path, nullptr);
    gFilesDir = pathStr;
    env->ReleaseStringUTFChars(path, pathStr);

    if (chdir(gFilesDir.c_str()) != 0) {
        LOGE("Failed to chdir to %s (errno=%d)", gFilesDir.c_str(), errno);
    } else {
        LOGD("Working directory changed to %s", gFilesDir.c_str());
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_pixelclean_MainActivity_nativeInitBeforeSurface(
    JNIEnv* env,
    jobject /* this */) {

    if (gInitialized) {
        LOGD("nativeInitBeforeSurface: already initialized");
        return;
    }

    LOGD("PixelClean Android native init starting...");

    Global::Read();
    TOOL::InitThreadPool();
    TOOL::InitPerlinNoise();
    TOOL::InitSpdLog();
    TOOL::InitTimer();

    gApplication = new GAME::Application();
    gApplication->initBeforeSurface();

    gInitialized = true;
    LOGD("PixelClean Android native init complete (before surface)");
}

extern "C" JNIEXPORT void JNICALL
Java_com_pixelclean_MainActivity_nativeInitSurface(
    JNIEnv* env,
    jobject /* this */,
    jobject surface) {

    if (!gApplication) {
        LOGE("nativeInitSurface: Application not initialized");
        return;
    }

    ANativeWindow* window = ANativeWindow_fromSurface(env, surface);
    if (!window) {
        LOGE("nativeInitSurface: failed to get ANativeWindow from surface");
        return;
    }

    LOGD("nativeInitSurface: got ANativeWindow (%dx%d)",
         ANativeWindow_getWidth(window), ANativeWindow_getHeight(window));

    try {
        gApplication->initAfterSurface(window);
    } catch (const std::exception& e) {
        LOGE("initAfterSurface failed: %s", e.what());
        TOOL::Error->error(e.what());
    }

    LOGD("PixelClean Android surface init complete");
}

extern "C" JNIEXPORT void JNICALL
Java_com_pixelclean_MainActivity_nativeRenderFrame(
    JNIEnv* env,
    jobject /* this */) {

    if (!gApplication) {
        return;
    }

    try {
        gApplication->frameStep();
    } catch (const std::exception& e) {
        LOGE("frameStep failed: %s", e.what());
        TOOL::Error->error(e.what());
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_pixelclean_MainActivity_nativeDestroy(
    JNIEnv* env,
    jobject /* this */) {

    LOGD("PixelClean Android native destroy starting...");

    if (gApplication) {
        gApplication->cleanUp();
        delete gApplication;
        gApplication = nullptr;
    }

    TOOL::DeleteThreadPool();
    TOOL::DeletePerlinNoise();
    TOOL::DeleteSpdLog();

    gInitialized = false;
    LOGD("PixelClean Android native destroy complete");
}

extern "C" JNIEXPORT void JNICALL
Java_com_pixelclean_MainActivity_nativeTouchEvent(
    JNIEnv* env,
    jobject /* this */,
    jint action,
    jfloat x,
    jfloat y) {

    if (!gApplication) return;

    if (action == 0 || action == 2) {
        gApplication->CursorPosX = static_cast<double>(x);
        gApplication->CursorPosY = static_cast<double>(y);
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_pixelclean_MainActivity_nativeResize(
    JNIEnv* env,
    jobject /* this */,
    jint width,
    jint height) {

    if (gApplication && gApplication->mWindow) {
        gApplication->mWindow->mWindowResized = true;
    }
}