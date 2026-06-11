// ============================================================================
// PixelClean - Android JNI Native Layer (native-lib.cpp)
// 功能：作为 Android Java/Kotlin 层与 C++ 游戏引擎之间的 JNI 桥接层，
//       负责生命周期管理、输入事件传递、渲染循环调度等核心交互。
// ============================================================================

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

// ----------------------------------------------------------------------------
// JNI_OnLoad - Java VM 加载原生库时的入口点
// 当 Kotlin 侧调用 System.loadLibrary("pixelclean") 时自动触发。
// 在此验证 JNI 版本兼容性，确保 Java 1.6+ 运行环境。
// ----------------------------------------------------------------------------
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

// ----------------------------------------------------------------------------
// Android 虚拟按键码定义
// Kotlin 侧的 ControlOverlayView 通过触摸将按键事件传入，
// 这里映射为游戏引擎可识别的枚举值，通过 Global 变量传递给游戏逻辑。
// 枚举值对应关系在 Kotlin 侧 ControlOverlayView 的 getButtonAt 中定义。
// ----------------------------------------------------------------------------
enum AndroidKeyCode {
    AKC_ESC           = 0,   // ESC 键（返回上级菜单 / 退出当前界面）
    AKC_CONSOLE       = 1,   // 控制台键（切换游戏内调试控制台的显隐）
    AKC_KEY_1         = 2,   // 数字键 1（游戏自定义功能，各 Mode 可复用）
    AKC_KEY_2         = 3,   // 数字键 2（游戏自定义功能，各 Mode 可复用）
    AKC_SPACE         = 4,   // 空格键（跳跃 / 确认 / 交互等通用操作）
    AKC_TOGGLE_MOUSE  = 5,   // 切换鼠标模式（指针锁定 / 释放切换）
    AKC_SHIFT         = 6,   // Shift键（下降 / 潜行等）
};

// ----------------------------------------------------------------------------
// 全局静态状态变量（文件内可见性，static 链接）
// 在整个 JNI 生命周期内维护引擎的状态，避免重复初始化。
// ----------------------------------------------------------------------------
static GAME::Application* gApplication = nullptr;   // 主游戏应用单例指针
static bool gInitialized = false;                    // 标记：前置初始化（不依赖 Surface）是否完成
static bool gSurfaceInitialized = false;             // 标记：Surface 及 GPU 上下文是否就绪
static std::string gFilesDir;                        // 缓存的应用文件目录路径
static ANativeWindow* gNativeWindow = nullptr;       // 当前有效的原生窗口指针
static std::mutex gTouchMutex;                       // 触摸坐标写入互斥锁（渲染线程读，输入线程写）
static std::atomic<bool> gDestroying{false};         // 原子销毁标记，防止并发访问已析构资源

// ----------------------------------------------------------------------------
// resetAllMovementKeys - 重置所有 WASD 方向移动键
// 当摇杆回中（死区内）或用户抬离摇杆时调用，
// 防止角色因按键状态残留而持续移动。
// ----------------------------------------------------------------------------
static void resetAllMovementKeys() {
    Global::AndroidKey_W = false;
    Global::AndroidKey_S = false;
    Global::AndroidKey_A = false;
    Global::AndroidKey_D = false;
}

// ----------------------------------------------------------------------------
// releaseNativeWindow - 安全释放 ANativeWindow 引用
// 在 Surface 重建（如屏幕旋转、Activity 恢复）或销毁时调用，
// 需要先释放旧引用再创建新引用，否则会导致 Android 底层的 BufferQueue 泄漏。
// ----------------------------------------------------------------------------
static void releaseNativeWindow() {
    if (gNativeWindow) {
        LOGD("Releasing ANativeWindow reference");
        ANativeWindow_release(gNativeWindow);
        gNativeWindow = nullptr;
    }
}

// ============================================================================
// JNI 导出函数 - 由 Kotlin 端 MainActivity 调用
// 函数命名遵循 JNI 规范: Java_包名_类名_方法名
// ============================================================================

// ----------------------------------------------------------------------------
// nativeSetFilesDir - 设置并切换工作目录至应用文件目录
// Android 的 assets 资源会在首次运行时被解压到 filesDir，
// 引擎后续通过相对路径即可访问这些资源文件（纹理、模型、纹理、音频等）。
// 调用时机：surfaceCreated() -> extractAssets() 完成后立即调用。
// ----------------------------------------------------------------------------
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

// ----------------------------------------------------------------------------
// nativeInitBeforeSurface - Surface 创建前的引擎初始化
// 本函数负责初始化所有不依赖 GPU 上下文的子系统，按以下顺序执行：
//   1. Global::Read()        - 读取全局配置文件（Info.ini）
//   2. InitThreadPool()      - 初始化线程池（异步任务调度）
//   3. InitPerlinNoise()    - 初始化柏林噪声生成器（程序化地形）
//   4. InitSpdLog()         - 初始化高性能日志系统
//   5. InitTimer()          - 初始化高精度计时器
//   6. new Application()    - 创建主游戏应用实例
//   7. initBeforeSurface()  - 执行引擎内部的无窗口初始化（着色器预编译等）
// 调用时机：surfaceCreated() 中，在 nativeInitSurface 之前。
// ----------------------------------------------------------------------------
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

// ----------------------------------------------------------------------------
// nativeInitSurface - 基于 Surface 的初始化（创建 GPU 上下文）
// 将 Java 层的 Surface（由 SurfaceView 提供）转为 ANativeWindow，
// 交给引擎创建 OpenGL/Vulkan 渲染上下文及交换链。
// 调用时机：surfaceCreated() 中，在独立线程中调用以避免阻塞 UI 线程。
// 异常处理：若初始化失败，会自动清理已创建的资源并重置状态。
// ----------------------------------------------------------------------------
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

// ----------------------------------------------------------------------------
// nativeRenderFrame - 渲染一帧画面
// 由 Kotlin 侧的 RenderThread 在循环中不断调用，构成游戏主循环。
// 引擎内部通过 frameStep() 完成一帧逻辑更新 + 渲染提交。
// 如果引擎尚未就绪，以 ~60FPS 速率忙等待，避免空转消耗 CPU。
// 调用时机：渲染线程 while 循环中每帧调用一次。
// ----------------------------------------------------------------------------
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

// ----------------------------------------------------------------------------
// nativeDestroy - 销毁引擎并释放所有资源
// 执行顺序与初始化相反，确保依赖关系正确：
//   1. 设置 gDestroying 标记，阻塞后续操作
//   2. 清理 Application（cleanUp + delete）
//   3. 释放 ANativeWindow
//   4. 清理工具子系统（线程池、噪声、日志）
//   5. 重置所有状态标记
// 调用时机：Activity.onDestroy() 中。
// ----------------------------------------------------------------------------
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

// ----------------------------------------------------------------------------
// nativeTouchEvent - 触摸事件传递（光标位置 / 鼠标按下）
// action 取值: 0=ACTION_DOWN(按下), 2=ACTION_MOVE(移动)
// 按下或移动时更新光标位置，使用互斥锁确保渲染线程读取一致性。
// 按下动作额外标记 mPendingMouseDown，供引擎在下一帧处理点击逻辑。
// 调用时机：用户触摸屏幕非摇杆/按钮区域时。
// ----------------------------------------------------------------------------
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

// ----------------------------------------------------------------------------
// nativeResize - 窗口尺寸变化通知
// 当 Surface 尺寸改变（屏幕旋转/分屏/键盘弹出）时通知引擎，
// 引擎在下一帧检测到 mWindowResized 标记后重新创建交换链/视口。
// 调用时机：SurfaceHolder.Callback.surfaceChanged() 中。
// ----------------------------------------------------------------------------
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

// ----------------------------------------------------------------------------
// nativeKeyRequest - 虚拟按键请求处理
// Kotlin ControlOverlayView 的用户触摸按钮时触发，
// 将按键码映射为 Global 命名空间中的请求标记。引擎在每帧处理这些标记，
// 处理完成后复位（消费模式），因此不会出现按键重复触发的竞争问题。
// 支持按键：ESC, Console, Key1, Key2, Space, ToggleMouse
// 调用时机：用户点击控制层按钮时。
// ----------------------------------------------------------------------------
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
        case AKC_SHIFT:
            Global::AndroidRequestMoveDown = true;
            break;
        default:
            LOGW("nativeKeyRequest: unknown keyCode %d", keyCode);
            break;
    }
}

// ----------------------------------------------------------------------------
// nativeJoystick - 虚拟摇杆方向输入
// 将触摸摇杆的偏移量归一化后，按正交优先策略映射为 WASD 方向键状态。
//
// 处理策略：
//   1. 死区过滤：mag < 0.2f 时无视，防止手指微颤导致误触
//   2. 向量归一化：消除距离对方向判断的影响
//   3. 正交优先：比较 |nx| 与 |ny|，选择偏移较大的方向轴
//
// 此设计模仿了传统游戏手柄的 DPAD（方向键）行为，
// 适用于场景中的角色移动（如 Maze/TankTrouble 模式）。
// 调用时机：用户触摸并拖动摇杆时（ACTION_MOVE）。
// ----------------------------------------------------------------------------
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

// ----------------------------------------------------------------------------
// nativeJoystickRelease - 摇杆释放处理
// 用户抬离摇杆区域时清除所有方向键状态，停止角色移动。
// 调用时机：触摸事件 ACTION_UP/POINTER_UP 且 pointerId 匹配摇杆。
// ----------------------------------------------------------------------------
extern "C" JNIEXPORT void JNICALL
Java_com_pixelclean_MainActivity_nativeJoystickRelease(
    JNIEnv* env,
    jobject /* this */) {

    if (!gApplication || gDestroying.load()) return;
    resetAllMovementKeys();
}

// ----------------------------------------------------------------------------
// nativeUpdateTouchState - 更新全局触摸状态
// 根据当前活动的触控点数向引擎报告触摸模式，实现多指手势识别：
//   0 (PrimaryDown)   - 单指触摸（标准游戏交互，如射击/选择）
//   1 (SecondaryDown) - 双指触摸（特殊操作或视角缩放）
//   2 (TertiaryDown)  - 三指触摸（系统级手势兼容）
//   3 (MultiTouch)    - 四指及以上触摸
//
// actionMasked == 1 (ACTION_UP) 时重置为 None 并触发鼠标释放。
// 调用时机：每次触摸事件处理完成后，统一更新状态。
// ----------------------------------------------------------------------------
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

// ----------------------------------------------------------------------------
// nativeMouseScroll - 鼠标滚轮模拟（双指捏合）
// 在无物理滚轮的 Android 设备上，使用双指捏合手势模拟滚轮滚动。
// 用原子操作累加滚动值，确保渲染线程与输入线程无锁安全交互。
// 调用时机：检测到双指捏合距离变化超过阈值时。
// ----------------------------------------------------------------------------
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

// ----------------------------------------------------------------------------
// nativeGetGameMode - 获取当前游戏模式 ID
//  Kotlin 侧每隔 15 帧轮询此函数，同步当前游戏模式。
// 返回的稳定模式 ID 用于动态调整 UI 覆盖层布局（显示哪些按钮和摇杆）。
// 使用 GetGameModeStableId() 确保返回稳定的模式标识，避免瞬态值干扰。
// 返回值：-1 表示无效，0~N 为各游戏模式 ID。
// 调用时机：RenderThread 的 syncGameMode() 中每 15 帧轮询一次。
// ----------------------------------------------------------------------------
extern "C" JNIEXPORT jint JNICALL
Java_com_pixelclean_MainActivity_nativeGetGameMode(
    JNIEnv* env,
    jobject /* this */) {

    if (!gApplication || gDestroying.load()) return -1;
    return static_cast<jint>(GetGameModeStableId(Global::GameMode));
}

// ----------------------------------------------------------------------------
// nativeIsInGame - 查询游戏是否处于"游玩中"状态
// 区分玩家当前处于菜单界面还是实际游戏中。
// 通过 InterFace->GetInterFaceBool() 判断：true=正在显示菜单/UI，false=游戏中。
// Kotlin 侧据此控制控制按钮的显隐，仅在游戏中显示虚拟按键。
// 调用时机：RenderThread 的 syncGameMode() 中每 15 帧轮询一次。
// ----------------------------------------------------------------------------
extern "C" JNIEXPORT jboolean JNICALL
Java_com_pixelclean_MainActivity_nativeIsInGame(
    JNIEnv* env,
    jobject /* this */) {

    if (!gApplication || gDestroying.load()) return JNI_FALSE;
    return gApplication->InterFace ? !gApplication->InterFace->GetInterFaceBool() : JNI_FALSE;
}

// ----------------------------------------------------------------------------
// nativeRequestExit - 请求游戏引擎优雅退出
// 在 Activity 销毁前调用，先通知引擎的窗口系统设置 shouldClose，
// 让引擎在下一帧检测到该标记后自行释放 GPU 资源并退出渲染循环，
// 然后才调用 nativeDestroy 做最终清理。
// 调用时机：onDestroy() 中，早于 nativeDestroy()。
// ----------------------------------------------------------------------------
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