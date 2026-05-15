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
#include <chrono>
#include <thread>

extern "C" JNIEXPORT jint JNICALL
JNI_OnLoad(JavaVM* vm, void* reserved) {
    LOGI("========== PixelClean JNI_OnLoad called ==========");
    JNIEnv* env;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        LOGE("Failed to get JNIEnv");
        return -1;
    }
    LOGI("PixelClean JNI_OnLoad successful");
    return JNI_VERSION_1_6;
}

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

    LOGD("========== nativeInitBeforeSurface called ==========");
    
    if (gInitialized) {
        LOGD("nativeInitBeforeSurface: already initialized");
        return;
    }

    if (gDestroying.load()) {
        LOGW("nativeInitBeforeSurface: application is being destroyed");
        return;
    }

    LOGI("PixelClean Android native init starting...");

    try {
        LOGD("Calling Global::Read()...");
        Global::Read();
        LOGD("Global::Read() completed");

        LOGD("Calling TOOL::InitThreadPool()...");
        TOOL::InitThreadPool();
        LOGD("TOOL::InitThreadPool() completed");

        LOGD("Calling TOOL::InitPerlinNoise()...");
        TOOL::InitPerlinNoise();
        LOGD("TOOL::InitPerlinNoise() completed");

        LOGD("Calling TOOL::InitSpdLog()...");
        TOOL::InitSpdLog();
        LOGD("TOOL::InitSpdLog() completed");

        LOGD("Calling TOOL::InitTimer()...");
        TOOL::InitTimer();
        LOGD("TOOL::InitTimer() completed");

        LOGD("Creating Application instance...");
        gApplication = new GAME::Application();
        LOGD("Application instance created");

        LOGD("Calling gApplication->initBeforeSurface()...");
        gApplication->initBeforeSurface();
        LOGD("initBeforeSurface() completed");

        gInitialized = true;
        LOGI("PixelClean Android native init complete (before surface)");
    } catch (const std::exception& e) {
        LOGE("nativeInitBeforeSurface exception: %s", e.what());
    } catch (...) {
        LOGE("nativeInitBeforeSurface unknown exception");
    }
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
        if (TOOL::Error) TOOL::Error->error(e.what());
        releaseNativeWindow();
        gSurfaceInitialized = false;

        gApplication->cleanUp();
        delete gApplication;
        gApplication = nullptr;
        gInitialized = false;
    } catch (...) {
        LOGE("initAfterSurface failed: unknown exception");
        releaseNativeWindow();
        gSurfaceInitialized = false;

        gApplication->cleanUp();
        delete gApplication;
        gApplication = nullptr;
        gInitialized = false;
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

    if (!gApplication->isInitialized()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
        return;
    }

    try {
        gApplication->frameStep();
    } catch (const std::exception& e) {
        LOGE("frameStep failed: %s", e.what());
        if (TOOL::Error) TOOL::Error->error(e.what());
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
	if (action == 0) {
		gApplication->mPendingMouseDown.store(true);
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
        gApplication->mPendingMouseUp.store(true);
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
Java_com_pixelclean_MainActivity_nativeMouseScroll(
	JNIEnv* env,
	jobject /* this */,
	jint delta) {

	if (!gApplication || gDestroying.load()) return;
	int prev = gApplication->mPendingScroll.exchange(delta);
	if (prev != 0) {
		gApplication->mPendingScroll.store(prev + delta);
	}
}

extern "C" JNIEXPORT jint JNICALL
Java_com_pixelclean_MainActivity_nativeGetGameMode(
    JNIEnv* env,
    jobject /* this */) {

    if (!gApplication || gDestroying.load()) return -1;
    return static_cast<jint>(GetGameModeStableId(Global::GameMode));
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_pixelclean_MainActivity_nativeIsInGame(
    JNIEnv* env,
    jobject /* this */) {

    if (!gApplication || gDestroying.load()) return JNI_FALSE;
    return gApplication->InterFace ? !gApplication->InterFace->GetInterFaceBool() : JNI_FALSE;
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