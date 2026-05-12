#include <jni.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>

#include "DebugLog.h"
#include "GlobalVariable.h"
#include "Tool/Tool.h"
#include "application.h"

#include <unistd.h>
#include <cerrno>
#include <cmath>
#include <mutex>
#include <atomic>

enum AndroidKeyCode {
    AKC_ESC           = 0,
    AKC_CONSOLE       = 1,
    AKC_KEY_1         = 2,
    AKC_KEY_2         = 3,
    AKC_SPACE         = 4,
    AKC_TOGGLE_MOUSE  = 5,
};

static GAME::Application* gApplication = nullptr;
static bool gInitialized = false;
static bool gSurfaceInitialized = false;
static std::string gFilesDir;
static ANativeWindow* gNativeWindow = nullptr;
static std::mutex gTouchMutex;
static std::atomic<bool> gDestroying{false};

static void resetAllMovementKeys() {
    Global::AndroidKey_W = false;
    Global::AndroidKey_S = false;
    Global::AndroidKey_A = false;
    Global::AndroidKey_D = false;
}

static void releaseNativeWindow() {
    if (gNativeWindow) {
        LOGD("Releasing ANativeWindow reference");
        ANativeWindow_release(gNativeWindow);
        gNativeWindow = nullptr;
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_pixelclean_MainActivity_nativeSetFilesDir(
    JNIEnv* env,
    jobject /* this */,
    jstring path) {

    const char* pathStr = env->GetStringUTFChars(path, nullptr);
    if (!pathStr) {
        LOGE("nativeSetFilesDir: GetStringUTFChars returned null");
        return;
    }
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

    if (gDestroying.load()) {
        LOGW("nativeInitBeforeSurface: application is being destroyed");
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

    if (gDestroying.load()) {
        LOGW("nativeInitSurface: application is being destroyed");
        return;
    }

    if (!gApplication) {
        LOGE("nativeInitSurface: Application not initialized");
        return;
    }

    releaseNativeWindow();

    ANativeWindow* window = ANativeWindow_fromSurface(env, surface);
    if (!window) {
        LOGE("nativeInitSurface: failed to get ANativeWindow from surface");
        return;
    }

    int32_t w = ANativeWindow_getWidth(window);
    int32_t h = ANativeWindow_getHeight(window);
    if (w <= 0 || h <= 0) {
        LOGW("nativeInitSurface: window has invalid size (%dx%d), will retry later", w, h);
        ANativeWindow_release(window);
        return;
    }

    LOGD("nativeInitSurface: got ANativeWindow (%dx%d)", w, h);
    gNativeWindow = window;

    try {
        gApplication->initAfterSurface(window);
        gSurfaceInitialized = true;
    } catch (const std::exception& e) {
        LOGE("initAfterSurface failed: %s", e.what());
        TOOL::Error->error(e.what());
        releaseNativeWindow();
        gSurfaceInitialized = false;
    }

    LOGD("PixelClean Android surface init complete");
}

extern "C" JNIEXPORT void JNICALL
Java_com_pixelclean_MainActivity_nativeRenderFrame(
    JNIEnv* env,
    jobject /* this */) {

    if (!gApplication || gDestroying.load() || !gSurfaceInitialized) {
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
    gDestroying.store(true);

    if (gApplication) {
        gApplication->cleanUp();
        delete gApplication;
        gApplication = nullptr;
    }

    releaseNativeWindow();

    TOOL::DeleteThreadPool();
    TOOL::DeletePerlinNoise();
    TOOL::DeleteSpdLog();

    gInitialized = false;
    gSurfaceInitialized = false;
    gDestroying.store(false);
    LOGD("PixelClean Android native destroy complete");
}

extern "C" JNIEXPORT void JNICALL
Java_com_pixelclean_MainActivity_nativeTouchEvent(
    JNIEnv* env,
    jobject /* this */,
    jint action,
    jfloat x,
    jfloat y) {

    if (!gApplication || gDestroying.load()) return;

    if (action == 0 || action == 2) {
        std::lock_guard<std::mutex> lock(gTouchMutex);
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

    if (gDestroying.load()) return;
    if (width <= 0 || height <= 0) {
        LOGW("nativeResize: ignoring invalid size %dx%d", width, height);
        return;
    }

    if (gApplication && gApplication->mWindow) {
        gApplication->mWindow->mWindowResized = true;
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_pixelclean_MainActivity_nativeKeyRequest(
    JNIEnv* env,
    jobject /* this */,
    jint keyCode) {

    if (!gApplication || gDestroying.load()) return;

    switch (keyCode) {
        case AKC_ESC:
            Global::AndroidRequestESC = true;
            break;
        case AKC_CONSOLE:
            Global::AndroidRequestConsole = true;
            break;
        case AKC_KEY_1:
            Global::AndroidRequestKey1 = true;
            break;
        case AKC_KEY_2:
            Global::AndroidRequestKey2 = true;
            break;
        case AKC_SPACE:
            Global::AndroidRequestSpace = true;
            break;
        case AKC_TOGGLE_MOUSE:
            Global::AndroidRequestToggleMouse = true;
            break;
        default:
            LOGW("nativeKeyRequest: unknown keyCode %d", keyCode);
            break;
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_pixelclean_MainActivity_nativeJoystick(
    JNIEnv* env,
    jobject /* this */,
    jfloat dirX,
    jfloat dirY) {

    if (!gApplication || gDestroying.load()) return;

    const float deadZone = 0.2f;
    float mag = std::sqrt(dirX * dirX + dirY * dirY);

    if (mag < deadZone) {
        resetAllMovementKeys();
        return;
    }

    float nx = dirX / mag;
    float ny = dirY / mag;

    float absX = std::fabs(nx);
    float absY = std::fabs(ny);

    if (absX > absY) {
        Global::AndroidKey_W = false;
        Global::AndroidKey_S = false;
        if (nx > 0.0f) {
            Global::AndroidKey_D = true;
            Global::AndroidKey_A = false;
        } else {
            Global::AndroidKey_A = true;
            Global::AndroidKey_D = false;
        }
    } else {
        Global::AndroidKey_A = false;
        Global::AndroidKey_D = false;
        if (ny > 0.0f) {
            Global::AndroidKey_S = true;
            Global::AndroidKey_W = false;
        } else {
            Global::AndroidKey_W = true;
            Global::AndroidKey_S = false;
        }
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_pixelclean_MainActivity_nativeJoystickRelease(
    JNIEnv* env,
    jobject /* this */) {

    if (!gApplication || gDestroying.load()) return;
    resetAllMovementKeys();
}

extern "C" JNIEXPORT void JNICALL
Java_com_pixelclean_MainActivity_nativeUpdateTouchState(
    JNIEnv* env,
    jobject /* this */,
    jint actionMasked,
    jint pointerCount) {

    if (!gApplication || gDestroying.load()) return;

    if (actionMasked == 1) {
        Global::TouchState = TouchStateEnum::None;
        return;
    }

    if (pointerCount >= 4) {
        Global::TouchState = TouchStateEnum::MultiTouch;
    } else if (pointerCount == 3) {
        Global::TouchState = TouchStateEnum::TertiaryDown;
    } else if (pointerCount == 2) {
        Global::TouchState = TouchStateEnum::SecondaryDown;
    } else if (pointerCount == 1) {
        Global::TouchState = TouchStateEnum::PrimaryDown;
    } else {
        Global::TouchState = TouchStateEnum::None;
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_pixelclean_MainActivity_nativeRequestExit(
    JNIEnv* env,
    jobject /* this */) {

    if (!gApplication || gDestroying.load()) return;
    if (gApplication->mWindow) {
        gApplication->mWindow->setShouldClose(true);
        LOGD("nativeRequestExit: shouldClose flag set");
    }
}