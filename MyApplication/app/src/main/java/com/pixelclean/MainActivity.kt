// ============================================================================
// PixelClean - Android MainActivity (MainActivity.kt)
// 功能：Android 主 Activity，管理应用生命周期、SurfaceView 渲染表面、
//       触摸事件分发、虚拟控制覆盖层（摇杆+按钮）、资源提取、渲染线程。
// 架构：Kotlin（UI/事件层） + JNI -> C++（游戏引擎层）
// ============================================================================

package com.pixelclean

import android.annotation.SuppressLint
import android.app.Activity
import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.os.Build
import android.os.Bundle
import android.util.Log
import android.view.KeyEvent
import android.view.MotionEvent
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.view.View
import android.view.WindowInsets
import android.view.WindowInsetsController
import android.view.WindowManager
import android.widget.FrameLayout
import java.io.File
import java.io.FileNotFoundException
import java.io.FileOutputStream
import java.util.concurrent.atomic.AtomicBoolean
import java.util.concurrent.atomic.AtomicInteger

/**
 * MainActivity - PixelClean 游戏的主入口 Activity
 *
 * 职责说明：
 * 1. 全屏沉浸式显示，隐藏系统导航栏和状态栏
 * 2. 通过 SurfaceView 提供原生渲染表面给 C++ 引擎
 * 3. 通过 ControlOverlayView 绘制虚拟摇杆和按钮覆盖层
 * 4. 管理渲染线程（RenderThread），驱动游戏主循环
 * 5. 解压 APK 中的 assets 资源到文件系统
 * 6. 通过 JNI 与原生 C++ 引擎通信
 *
 * 生命周期流程：
 *   onCreate -> setupFullScreen -> 布局初始化 -> 资源解压
 *   surfaceCreated -> extractAssets -> nativeSetFilesDir
 *                   -> nativeInitBeforeSurface -> nativeInitSurface
 *                   -> startRenderThread -> 循环 nativeRenderFrame
 *   surfaceDestroyed -> stopRenderThread
 *   onDestroy -> nativeRequestExit -> nativeDestroy
 */
class MainActivity : Activity(), SurfaceHolder.Callback {

    // 日志标签
    private val TAG = "PixelClean"

    // ------ 状态标记 ------
    private var nativeInitialized = false           // C++ 引擎前置初始化是否完成
    private var surfaceInitialized = false           // Surface 初始化是否完成
    private var renderThread: RenderThread? = null   // 渲染线程实例
    private val destroyed = AtomicBoolean(false)     // Activity 是否已销毁

    // ------ 视图组件 ------
    private lateinit var containerLayout: FrameLayout    // 根容器
    private lateinit var surfaceView: SurfaceView        // 原生渲染表面
    private lateinit var controlOverlay: ControlOverlayView  // 触摸控制覆盖层（摇杆+按钮）

    // ------ 游戏模式同步状态 ------
    private val currentGameMode = AtomicInteger(-1)     // 缓存当前游戏模式 ID
    private val currentInGame = AtomicBoolean(false)    // 缓存当前是否在游戏中

    // ========================================================================
    // Activity 生命周期
    // ========================================================================

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        setupFullScreen()
        containerLayout = FrameLayout(this)

        // Step 2: 创建 SurfaceView——C++ 引擎将通过它进行 OpenGL/Vulkan 渲染
        surfaceView = SurfaceView(this)
        surfaceView.holder.addCallback(this)
        containerLayout.addView(surfaceView, FrameLayout.LayoutParams(
            FrameLayout.LayoutParams.MATCH_PARENT,
            FrameLayout.LayoutParams.MATCH_PARENT
        ))

        // Step 3: 创建控制覆盖层——绘制虚拟摇杆和按钮，并拦截触摸事件
        controlOverlay = ControlOverlayView(this)
        // 将触摸事件委托给 handleControlTouch 进行精细分发
        controlOverlay.setOnTouchListener { _, event ->
            handleControlTouch(event)
        }
        containerLayout.addView(controlOverlay, FrameLayout.LayoutParams(
            FrameLayout.LayoutParams.MATCH_PARENT,
            FrameLayout.LayoutParams.MATCH_PARENT
        ))

        setContentView(containerLayout)

        // Step 4: Android 13+ 注册返回手势回调（替代过时的 onBackPressed）
        if (Build.VERSION.SDK_INT >= 33) {
            registerBackInvokedCallback()
        }
    }

    // ------------------------------------------------------------------
    // onBackPressed - 返回键处理（API 33 以下已废弃）
    // 按下返回键时向引擎发送 ESC 按键请求
    // ------------------------------------------------------------------
    @Deprecated("Deprecated in Java")
    override fun onBackPressed() {
        if (nativeInitialized) {
            nativeKeyRequest(0)  // 发送 ESC 码
        }
    }

    // ------------------------------------------------------------------
    // registerBackInvokedCallback - Android 13+ 返回手势回调
    // 当用户使用手势导航滑动返回时，向引擎发送 ESC 按键请求
    // ------------------------------------------------------------------
    @SuppressLint("NewApi")
    private fun registerBackInvokedCallback() {
        onBackInvokedDispatcher.registerOnBackInvokedCallback(
            android.window.OnBackInvokedDispatcher.PRIORITY_DEFAULT
        ) {
            if (nativeInitialized) {
                nativeKeyRequest(0)  // 发送 ESC 码
            }
        }
    }

    // ------------------------------------------------------------------
    // dispatchKeyEvent - 物理按键事件分发
    // 拦截物理返回键，优先传给引擎处理
    // ------------------------------------------------------------------
    override fun dispatchKeyEvent(event: KeyEvent): Boolean {
        if (event.keyCode == KeyEvent.KEYCODE_BACK && event.action == KeyEvent.ACTION_UP) {
            if (nativeInitialized) {
                nativeKeyRequest(0)
            }
            return true
        }
        return super.dispatchKeyEvent(event)
    }

    // ------------------------------------------------------------------
    // onPause - Activity 暂停
    // 停止渲染线程，防止后台时继续占用 GPU 资源
    // ------------------------------------------------------------------
    override fun onPause() {
        super.onPause()
        stopRenderThread()
    }

    // ------------------------------------------------------------------
    // onResume - Activity 恢复
    // 如果未销毁，重新启动渲染线程
    // ------------------------------------------------------------------
    override fun onResume() {
        super.onResume()
        if (!destroyed.get()) {
            startRenderThread()
        }
    }

    // ------------------------------------------------------------------
    // onDestroy - Activity 销毁
    // 按正确顺序清理资源：先通知引擎退出，再执行原生销毁
    // ------------------------------------------------------------------
    override fun onDestroy() {
        destroyed.set(true)
        stopRenderThread()  // 先停止渲染线程
        if (nativeInitialized) {
            nativeRequestExit()     // Step 1: 通知引擎优雅退出
            try {
                Thread.sleep(100)   // 等待引擎 GPU 资源释放
            } catch (_: InterruptedException) { }
            nativeDestroy()         // Step 2: 执行最终销毁
            nativeInitialized = false
            surfaceInitialized = false
        }
        super.onDestroy()
    }

    // ------------------------------------------------------------------
    // onWindowFocusChanged - 窗口焦点变化
    // 每次获得焦点时重新隐藏系统 UI（适用于从其他应用切回的场景）
    // ------------------------------------------------------------------
    override fun onWindowFocusChanged(hasFocus: Boolean) {
        super.onWindowFocusChanged(hasFocus)
        if (hasFocus) {
            hideSystemUI()
        }
    }

    // ========================================================================
    // 全屏 & 沉浸模式
    // ========================================================================

    // ------------------------------------------------------------------
    // setupFullScreen - 设置全屏显示
    // Android P+：允许内容绘制到刘海屏区域
    // Android R+：使用 insetsController 实现沉浸式全屏
    // ------------------------------------------------------------------
    @SuppressLint("ObsoleteSdkInt")
    private fun setupFullScreen() {
        // Android 9+：扩展到刘海屏区域
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            window.attributes.layoutInDisplayCutoutMode =
                WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES
        }

        // Android 11+：使用新的 WindowInsets API
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            window.setDecorFitsSystemWindows(false)
        }
    }

    // ------------------------------------------------------------------
    // hideSystemUI - 隐藏状态栏和导航栏
    // Android R+：使用 insetsController API
    // 旧版本：使用 SYSTEM_UI_FLAG 组合标志（沉浸粘性模式）
    // ------------------------------------------------------------------
    @SuppressLint("ObsoleteSdkInt")
    private fun hideSystemUI() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            window.insetsController?.hide(
                WindowInsets.Type.statusBars() or WindowInsets.Type.navigationBars()
            )
        } else {
            @Suppress("DEPRECATION")
            window.decorView.systemUiVisibility = (
                View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY       // 沉浸粘性——滑入时暂时显示
                    or View.SYSTEM_UI_FLAG_LAYOUT_STABLE        // 布局稳定
                    or View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION  // 布局延伸到导航栏区域
                    or View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN    // 布局延伸到状态栏区域
                    or View.SYSTEM_UI_FLAG_HIDE_NAVIGATION      // 隐藏导航栏
                    or View.SYSTEM_UI_FLAG_FULLSCREEN           // 隐藏状态栏
                )
        }
    }

    // ========================================================================
    // 触摸事件处理（核心输入分发器）
    // ========================================================================

    // ------ 触摸状态变量 ------
    private var joystickPointerId = -1          // 当前控制摇杆的触摸点 ID，-1 表示无人操控
    private var joystickBaseX = 0f              // 摇杆基座中心 X 坐标（触摸按下时的位置）
    private var joystickBaseY = 0f              // 摇杆基座中心 Y 坐标
    private var joystickCurrDirX = 0f           // 摇杆当前触摸 X 坐标（用于计算方向）
    private var joystickCurrDirY = 0f              // 摇杆当前触摸 Y 坐标
    private var prevPinchDistance = 0f          // 上一次双指捏合距离（用于计算缩放/滚轮增量）
    private val activePointers = mutableSetOf<Int>()         // 当前所有活动的触摸点 ID 集合
    private val buttonPointerIds = mutableSetOf<Int>()       // 当前按在按钮上的触摸点 ID 集合
    private val gameTouchPointerIds = mutableSetOf<Int>()    // 当前用于游戏触摸交互的点 ID 集合

    /**
     * handleControlTouch - 控制层触摸事件分发器
     *
     * 负责将 ControlOverlayView 上的触摸事件按区域分发给不同的处理逻辑。
     * 触摸区域划分：
     *   1. 摇杆区域（屏幕左侧 1/3，下方 60%） -> nativeJoystick
     *   2. 按钮区域（屏幕右侧） -> nativeKeyRequest
     *   3. 其他区域（游戏交互区域） -> nativeTouchEvent
     *
     * 特殊手势：
     *   - 双指捏合 -> nativeMouseScroll（模拟滚轮）
     *   - 多指状态 -> nativeUpdateTouchState
     */
    @SuppressLint("ClickableViewAccessibility")
    private fun handleControlTouch(event: MotionEvent): Boolean {
        if (destroyed.get() || !nativeInitialized || !surfaceInitialized) return false

        val action = event.actionMasked        // 触摸动作类型（DOWN/UP/MOVE/CANCEL）
        val pointerIndex = event.actionIndex   // 触发事件的触摸点的索引
        val pointerId = event.getPointerId(pointerIndex)  // 触发事件的触摸点的唯一 ID
        val rawX = event.getX(pointerIndex)    // 触摸点 X 坐标
        val rawY = event.getY(pointerIndex)    // 触摸点 Y 坐标

        when (action) {
            // ---- 手指按下 ----
            MotionEvent.ACTION_DOWN,
            MotionEvent.ACTION_POINTER_DOWN -> {
                activePointers.add(pointerId)

                // 判断触摸点落在哪个区域
                if (controlOverlay.showJoystick && isInJoystickZone(rawX, rawY)) {
                    // 区域1：摇杆区域 -> 初始化摇杆
                    joystickPointerId = pointerId
                    joystickBaseX = rawX
                    joystickBaseY = rawY
                    joystickCurrDirX = rawX
                    joystickCurrDirY = rawY
                } else if (isInButtonZone(rawX, rawY)) {
                    // 区域2：按钮区域 -> 触发按键
                    handleButtonPress(rawX, rawY)
                    buttonPointerIds.add(pointerId)
                } else {
                    // 区域3：游戏交互区域 -> 传递触摸坐标给引擎
                    gameTouchPointerIds.add(pointerId)
                    nativeTouchEvent(0, rawX, rawY)
                }

                // 如果恰好有 2 个触摸点，初始化捏合距离
                if (activePointers.size == 2) {
                    prevPinchDistance = pinchDist(event)
                }

                updateTouchStateForGame()
            }

            // ---- 手指滑动 ----
            MotionEvent.ACTION_MOVE -> {
                val pointerCount = event.pointerCount

                // 双指捏合手势 -> 模拟鼠标滚轮
                if (pointerCount == 2) {
                    val curDist = pinchDist(event)
                    if (prevPinchDistance > 0f) {
                        val delta = curDist - prevPinchDistance
                        if (kotlin.math.abs(delta) > 12f) {
                            // delta > 0 = 放大（向外捏）-> 向上滚动
                            // delta < 0 = 缩小（向内捏）-> 向下滚动
                            nativeMouseScroll(if (delta > 0) -1 else 1)
                            prevPinchDistance = curDist
                        }
                    } else {
                        prevPinchDistance = curDist
                    }
                }

                if (pointerCount == 1) {
                    // 单指滑动
                    val px = event.getX(0)
                    val py = event.getY(0)
                    val pid = event.getPointerId(0)
                    if (pid == joystickPointerId) {
                        updateJoystick(px, py)         // 更新摇杆方向
                    } else if (pid !in buttonPointerIds) {
                        nativeTouchEvent(2, px, py)    // 更新游戏触摸位置
                    }
                } else {
                    // 多指滑动，分别处理每个触摸点
                    for (i in 0 until pointerCount) {
                        val pid = event.getPointerId(i)
                        if (pid == joystickPointerId) {
                            updateJoystick(event.getX(i), event.getY(i))
                        } else if (pid !in buttonPointerIds
                            && !isInJoystickZone(event.getX(i), event.getY(i))
                            && !isInButtonZone(event.getX(i), event.getY(i))) {
                            nativeTouchEvent(2, event.getX(i), event.getY(i))
                        }
                    }
                }
                updateTouchStateForGame()
            }

            // ---- 手指抬起 ----
            MotionEvent.ACTION_UP,
            MotionEvent.ACTION_POINTER_UP -> {
                activePointers.remove(pointerId)
                buttonPointerIds.remove(pointerId)
                val hadGameTouch = gameTouchPointerIds.isNotEmpty()
                gameTouchPointerIds.remove(pointerId)

                // 触点少于 2 时重置捏合状态
                if (activePointers.size < 2) {
                    prevPinchDistance = 0f
                }

                // 如果抬起的是摇杆触点，释放摇杆
                if (pointerId == joystickPointerId) {
                    joystickPointerId = -1
                    nativeJoystickRelease()
                }

                // 更新游戏触摸状态
                if (hadGameTouch && gameTouchPointerIds.isEmpty()) {
                    nativeUpdateTouchState(1, 0)  // 所有游戏触摸点都已抬起
                } else if (gameTouchPointerIds.isNotEmpty()) {
                    for (i in 0 until event.pointerCount) {
                        val pid = event.getPointerId(i)
                        if (pid in gameTouchPointerIds) {
                            nativeTouchEvent(2, event.getX(i), event.getY(i))
                            break
                        }
                    }
                    updateTouchStateForGame()
                }
            }

            // ---- 触摸取消（系统中断） ----
            MotionEvent.ACTION_CANCEL -> {
                // 重置所有触摸状态
                activePointers.clear()
                buttonPointerIds.clear()
                gameTouchPointerIds.clear()
                prevPinchDistance = 0f
                if (joystickPointerId != -1) {
                    joystickPointerId = -1
                    nativeJoystickRelease()
                }
                nativeUpdateTouchState(1, 0)
            }
        }

        // 触发重绘，更新摇杆/按钮的视觉状态
        controlOverlay.invalidate()
        return true
    }

    // ------------------------------------------------------------------
    // updateJoystick - 更新摇杆方向
    // 根据触摸点与基座中心的偏移计算归一化方向向量
    // 方向被限制在摇杆半径范围内（超出时按比例缩放）
    // ------------------------------------------------------------------
    private fun updateJoystick(x: Float, y: Float) {
        joystickCurrDirX = x
        joystickCurrDirY = y
        val dx = x - joystickBaseX
        val dy = y - joystickBaseY
        val maxR = controlOverlay.joystickRadius
        val dist = dx * dx + dy * dy
        val dirX: Float
        val dirY: Float
        if (dist > maxR * maxR) {
            // 超出摇杆边界：限制在边缘并归一化
            val scale = maxR / Math.sqrt(dist.toDouble()).toFloat()
            dirX = dx * scale / maxR
            dirY = dy * scale / maxR
        } else {
            // 在范围内：直接归一化
            dirX = dx / maxR
            dirY = dy / maxR
        }
        nativeJoystick(dirX, dirY)  // 通知引擎
    }

    // ------------------------------------------------------------------
    // updateTouchStateForGame - 更新游戏触控状态
    // 根据游戏互触点数量通知引擎当前的触摸模式
    // ------------------------------------------------------------------
    private fun updateTouchStateForGame() {
        nativeUpdateTouchState(0, gameTouchPointerIds.size)
    }

    // ------------------------------------------------------------------
    // handleButtonPress - 处理按钮按下事件
    // 查询 ControlOverlayView 确定哪个按钮被按下，发送对应按键码
    // ------------------------------------------------------------------
    private fun handleButtonPress(x: Float, y: Float) {
        val code = controlOverlay.getButtonAt(x, y)
        if (code >= 0) {
            nativeKeyRequest(code)
        }
    }

    // ------------------------------------------------------------------
    // pinchDist - 计算双指捏合距离
    // 用于检测缩放手势，模拟鼠标滚轮
    // ------------------------------------------------------------------
    private fun pinchDist(event: MotionEvent): Float {
        val dx = event.getX(0) - event.getX(1)
        val dy = event.getY(0) - event.getY(1)
        return kotlin.math.sqrt(dx * dx + dy * dy)
    }

    // ------------------------------------------------------------------
    // isInJoystickZone - 判断是否在摇杆区域内
    // 摇杆区域：屏幕左侧 1/3，下半部分的 60% 位置
    // ------------------------------------------------------------------
    private fun isInJoystickZone(x: Float, y: Float): Boolean {
        if (!controlOverlay.showJoystick) return false
        val h = controlOverlay.height.toFloat()
        val w = controlOverlay.width.toFloat()
        return x < w * 0.33f && y > h * 0.4f
    }

    // ------------------------------------------------------------------
    // isInButtonZone - 判断是否在按钮区域内
    // 委托给 ControlOverlayView 的 hit-test 方法
    // ------------------------------------------------------------------
    private fun isInButtonZone(x: Float, y: Float): Boolean {
        return controlOverlay.getButtonAt(x, y) >= 0
    }

    // ========================================================================
    // SurfaceHolder.Callback 实现——管理 Surface 生命周期
    // ========================================================================

    /**
     * surfaceCreated - Surface 创建完成
     *
     * 初始化顺序：
     *   1. extractAssets()        - 首次运行时将 APK assets 解压到文件系统
     *   2. nativeSetFilesDir()    - 通知引擎工作目录
     *   3. nativeInitBeforeSurface() - 无窗口初始化（读取配置、创建线程池等）
     *   4. nativeInitSurface()    - 创建 GPU 上下文（在独立线程中，避免阻塞 UI）
     *   5. startRenderThread()    - 启动渲染循环
     */
    override fun surfaceCreated(holder: SurfaceHolder) {
        Log.d(TAG, "surfaceCreated")
        if (destroyed.get()) return

        // ----- 前置初始化（仅执行一次） -----
        if (!nativeInitialized) {
            extractAssets()              // 解压资源文件
            nativeSetFilesDir(filesDir.absolutePath)  // 设置工作目录
            nativeInitBeforeSurface()  // 引擎无窗口初始化
            nativeInitialized = true
        }

        // ----- Surface 初始化（需在独立线程，因为涉及 EGL 创建） -----
        if (holder.surface.isValid) {
            val surface = holder.surface
            Thread({
                Log.d(TAG, "initSurface thread started")
                try {
                    nativeInitSurface(surface)   // 在独立线程中创建 GPU 上下文
                    surfaceInitialized = true
                    Log.d(TAG, "initSurface thread completed, starting render thread")
                    startRenderThread()          // 初始化完成后启动渲染循环
                } catch (e: Exception) {
                    Log.e(TAG, "initSurface thread failed", e)
                }
            }, "PixelClean-InitSurface").start()
        } else {
            Log.w(TAG, "surfaceCreated: surface is not valid, deferring init")
        }
    }

    /**
     * surfaceChanged - Surface 尺寸变化
     * 通知控制覆盖层更新尺寸，并通知引擎窗口需要重新创建交换链
     */
    override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
        Log.d(TAG, "surfaceChanged: ${width}x${height}")
        controlOverlay.setScreenSize(width, height)
        if (!destroyed.get() && surfaceInitialized && width > 0 && height > 0) {
            nativeResize(width, height)
        }
    }

    /**
     * surfaceDestroyed - Surface 销毁
     * 停止渲染线程，清除触摸状态，等待 Surface 重新创建
     */
    override fun surfaceDestroyed(holder: SurfaceHolder) {
        Log.d(TAG, "surfaceDestroyed")
        stopRenderThread()
        surfaceInitialized = false
        // 清除所有触摸状态
        activePointers.clear()
        buttonPointerIds.clear()
        gameTouchPointerIds.clear()
        joystickPointerId = -1
        nativeUpdateTouchState(1, 0)
        nativeJoystickRelease()
    }

    // ========================================================================
    // 资源解压
    // ========================================================================

    /**
     * extractAssets - 将 APK 中的 assets 资源解压到文件系统
     *
     * 原因：Android APK 中的 assets 文件无法直接通过文件路径访问，
     * C++ 引擎需要通过文件系统路径读取资源（纹理、模型、配置等）。
     * 通过检查 filesDir/Info.ini 是否存在来判断是否已解压，避免重复操作。
     * 使用 synchronized 防止并发解压导致文件冲突。
     */
    @Synchronized
    private fun extractAssets() {
        val checkFile = File(filesDir, "Info.ini")
        if (checkFile.exists() && checkFile.isFile) {
            Log.d(TAG, "Assets already extracted (Info.ini exists), skipping")
            return
        }
        if (checkFile.isDirectory) {
            checkFile.deleteRecursively()
        }
        Log.d(TAG, "Extracting assets to ${filesDir.absolutePath}...")
        try {
            copyAssetDir("", filesDir)
            Log.d(TAG, "Assets extracted successfully")
        } catch (e: Exception) {
            Log.e(TAG, "Failed to extract assets", e)
        }
    }

    /**
     * copyAssetDir - 递归复制 assets 目录
     *
     * 处理逻辑：
     *   - 如果 assetPath 是文件 -> 直接复制到目标路径
     *   - 如果 assetPath 是目录 -> 创建目录后递归复制子项
     *   - 如果目标文件已存在且为目录，先删除再覆盖
     */
    private fun copyAssetDir(assetPath: String, targetDir: File) {
        val assetManager = assets
        val children = assetManager.list(assetPath)
        if (children == null || children.isEmpty()) {
            // children 为空 -> 当前路径是一个文件，直接复制
            val targetFile = File(targetDir, assetPath)
            if (targetFile.isDirectory) {
                targetFile.deleteRecursively()
            }
            val parent = targetFile.parentFile
            if (parent != null && !parent.exists()) {
                parent.mkdirs()
            }
            if (assetPath.isNotEmpty()) {
                try {
                    assetManager.open(assetPath).use { input ->
                        FileOutputStream(targetFile).use { output ->
                            input.copyTo(output)
                        }
                    }
                } catch (e: FileNotFoundException) {
                    // assets.list 返回空但不一定是文件，也可能是空目录
                    // 在 Android 中空目录无法通过 open 打开，所以创建目录
                    targetFile.mkdirs()
                }
            }
            return
        }
        // children 非空 -> 当前路径是一个目录，递归复制
        if (assetPath.isNotEmpty()) {
            File(targetDir, assetPath).mkdirs()
        }
        for (child in children) {
            val childPath = if (assetPath.isEmpty()) child else "$assetPath/$child"
            copyAssetDir(childPath, targetDir)
        }
    }

    // ========================================================================
    // 渲染线程管理
    // ========================================================================

    /**
     * startRenderThread - 启动渲染线程
     *
     * 渲染线程负责持续调用 nativeRenderFrame 驱动游戏主循环。
     * 在 Surface 初始化完成后或 Activity 恢复时调用。
     * 使用 synchronized 防止重复启动。
     */
    @Synchronized
    private fun startRenderThread() {
        if (destroyed.get()) return
        if (renderThread == null || renderThread?.isRunning?.get() == false) {
            renderThread?.stopRendering()
            renderThread = RenderThread()
            renderThread?.start()
        }
    }

    /**
     * stopRenderThread - 停止渲染线程
     *
     * 在 Surface 销毁或 Activity 暂停/销毁时调用。
     * 先设置停止标记，再等待线程结束（最多 2 秒超时）。
     * 如果线程未能在超时时间内结束，记录警告。
     */
    @Synchronized
    private fun stopRenderThread() {
        val thread = renderThread
        if (thread != null) {
            thread.stopRendering()
            thread.interrupt()
            try {
                thread.join(2000)  // 等待最多 2 秒
            } catch (e: InterruptedException) {
                Log.w(TAG, "Interrupted while waiting for render thread")
                Thread.currentThread().interrupt()
            }
            if (thread.isAlive) {
                Log.w(TAG, "Render thread did not stop within timeout")
            }
        }
        renderThread = null
    }

    /**
     * RenderThread - 游戏渲染线程（内部类）
     *
     * 在独立线程中循环调用 C++ 引擎的 frameStep()，
     * 避免阻塞 Android 主线程（UI 线程）。
     * 每隔 15 帧轮询一次游戏模式状态，用于动态调整 UI 覆盖层。
     * 线程优先级设为 THREAD_PRIORITY_DISPLAY，优化渲染性能。
     */
    private inner class RenderThread : Thread("PixelClean-Render") {
        private val running = AtomicBoolean(false)
        val isRunning: AtomicBoolean get() = running
        private var pollCounter = 0   // 轮询计数器（每 15 帧执行一次模式同步）

        override fun run() {
            running.set(true)
            // 设置线程优先级为"显示"级别，高于普通线程
            android.os.Process.setThreadPriority(android.os.Process.THREAD_PRIORITY_DISPLAY)

            while (running.get() && !isInterrupted && !destroyed.get()) {
                try {
                    nativeRenderFrame()  // 调用引擎渲染一帧
                } catch (e: Exception) {
                    Log.e(TAG, "RenderFrame exception", e)
                }

                // 每 15 帧同步一次游戏模式（用于更新 UI 布局）
                pollCounter++
                if (pollCounter >= 15) {
                    pollCounter = 0
                    syncGameMode()
                }
            }

            running.set(false)
            Log.d(TAG, "Render thread exiting")
        }

        fun stopRendering() {
            running.set(false)
        }
    }

    /**
     * syncGameMode - 同步游戏模式状态
     *
     * 每 15 帧检查一次当前游戏模式和"是否在游戏中"状态。
     * 如果状态发生变化，通知 ControlOverlayView 更新按钮布局。
     * 这是 Kotlin 侧通过"拉取"方式获取引擎状态的唯一步骤，
     * 其他输入事件都是"推送"方式。
     */
    private fun syncGameMode() {
        if (!nativeInitialized || destroyed.get()) return
        try {
            val mode = nativeGetGameMode()
            val inGame = nativeIsInGame()
            val oldMode = currentGameMode.getAndSet(mode)
            val oldInGame = currentInGame.getAndSet(inGame)
            if (oldMode != mode || oldInGame != inGame) {
                Log.d(TAG, "Game mode changed: $oldMode -> $mode, inGame: $oldInGame -> $inGame")
                controlOverlay.onGameModeChanged(mode, inGame)
            }
        } catch (e: Exception) {
            Log.w(TAG, "syncGameMode failed", e)
        }
    }

    // ========================================================================
    // 游戏模式 UI 配置
    // ========================================================================

    /**
     * ButtonSpec - 按钮规格数据类
     * @param label  按钮上显示的文本（如 "SP", "1", "/"）
     * @param keyCode 对应的虚拟按键码（映射到 AndroidKeyCode 枚举）
     * @param color  按钮背景颜色（ARGB）
     */
    private data class ButtonSpec(
        val label: String,
        val keyCode: Int,
        val color: Int
    )

    /**
     * ModeConfig - 游戏模式配置数据类
     * @param showJoystick 该模式下是否显示虚拟摇杆
     * @param buttons      该模式下显示的按钮列表
     */
    private data class ModeConfig(
        val showJoystick: Boolean,
        val buttons: List<ButtonSpec>
    )

    /**
     * 各游戏模式的 UI 配置映射表
     *
     * 每个模式独立配置摇杆显隐和按钮布局：
     *   - Maze（迷宫）：摇杆 + 5 个按钮（SP/1/2//）
     *   - TankTrouble（坦克）：摇杆 + 2 个按钮（SP//）
     *   - PhysicsTest/SoloudTest/RadianceCascades/FruitNinja：
     *     各模式使用不同的交互方式，不需要虚拟按键或只需要触摸
     *   - Infinite（无限模式）：摇杆 + 5 个按钮
     *   - -1（默认/菜单）：无控件
     */
    private val modeConfigs: Map<Int, ModeConfig> = mapOf(
        -1 to ModeConfig(                    // 默认/菜单模式
            showJoystick = false,
            buttons = emptyList()
        ),
        GameModeId.Maze to ModeConfig(      // 迷宫模式（需要方向 + 功能键）
            showJoystick = true,
                listOf(
                    ButtonSpec("SP", 4, Color.argb(120, 60, 140, 200)),  // 空格（跳跃）
                    ButtonSpec("1", 2, Color.argb(120, 180, 100, 60)),   // 键1
                    ButtonSpec("2", 3, Color.argb(120, 180, 140, 60)),   // 键2
                    ButtonSpec("/", 1, Color.argb(120, 100, 180, 100)),  // 控制台
                    ButtonSpec("`", 5, Color.argb(120, 100, 100, 200))   // 切换鼠标
                )
            ),
            GameModeId.TankTrouble to ModeConfig(  // 坦克大战模式
                showJoystick = true,
                buttons = listOf(
                    ButtonSpec("SP", 4, Color.argb(120, 60, 140, 200)),  // 发射
                    ButtonSpec("/", 1, Color.argb(120, 100, 180, 100))   // 控制台
                )
            ),
            GameModeId.PhysicsTest to ModeConfig(  // 物理测试模式
                showJoystick = false,
                buttons = emptyList()
            ),
            GameModeId.SoloudTest to ModeConfig(   // 音频测试模式
                showJoystick = true,
                buttons = emptyList()
            ),
            GameModeId.RadianceCascades to ModeConfig(  // 辐射级联渲染演示
                showJoystick = false,
                buttons = emptyList()
            ),
            GameModeId.FruitNinja to ModeConfig(   // 水果忍者模式
                showJoystick = false,
                buttons = emptyList()
            ),
            GameModeId.WFCTest to ModeConfig(     // WFC 测试模式
                showJoystick = false,
                buttons = emptyList()
            ),
            GameModeId.BlockWorld to ModeConfig(  // 区块世界模式（飞行探索）
                showJoystick = true,
                buttons = listOf(
                    ButtonSpec("↑", 4, Color.argb(120, 60, 140, 200)),    // SPACE → 上升
                    ButtonSpec("↓", 6, Color.argb(120, 200, 140, 60)),    // SHIFT → 下降
                    ButtonSpec("👁", 5, Color.argb(120, 140, 80, 220)),   // 切换视角拖拽
                    ButtonSpec("/", 1, Color.argb(120, 100, 180, 100)),   // 控制台
                    ButtonSpec("✕", 0, Color.argb(120, 200, 80, 80))     // ESC → 菜单
                )
            ),
            GameModeId.Infinite to ModeConfig(     // 无限模式（需要完整控制）
                showJoystick = true,
                buttons = listOf(
                    ButtonSpec("SP", 4, Color.argb(120, 60, 140, 200)),
                    ButtonSpec("1", 2, Color.argb(120, 180, 100, 60)),
                    ButtonSpec("2", 3, Color.argb(120, 180, 140, 60)),
                    ButtonSpec("/", 1, Color.argb(120, 100, 180, 100)),
                    ButtonSpec("`", 5, Color.argb(120, 100, 100, 200))
                )
            )
        )

    /**
     * Button - 按钮定义（包含布局位置信息）
     * 由 ControlOverlayView 根据 ModeConfig 和屏幕尺寸计算得出
     */
    private data class ButtonDef(
        val label: String,
        val cx: Float,       // 圆心 X 坐标（屏幕像素）
        val cy: Float,       // 圆心 Y 坐标
        val radius: Float,   // 按钮半径
        val color: Int,      // 填充色
        val keyCode: Int     // 对应的按键码
    )

    // ========================================================================
    // ControlOverlayView - 触摸控制覆盖层（自定义 View）
    // ========================================================================

    /**
     * ControlOverlayView - 覆盖在 SurfaceView 上的透明视图
     *
     * 功能：
     *   1. 根据当前游戏模式绘制虚拟摇杆和按钮
     *   2. 提供 hit-test 方法（getButtonAt）供 touch 分发器调用
     *   3. 根据屏幕尺寸动态计算按钮位置和大小
     *
     * 设计说明：
     *   - 使用 onDraw 手动绘制（Canvas API），不依赖布局 XML
     *   - 按钮数量影响布局算法：1~5 个按钮有不同的排列方式
     *   - 摇杆仅在 showJoystick=true 的模式下绘制
     */
    inner class ControlOverlayView(context: Context) : View(context) {

        var screenWidth = 0        // 屏幕宽度
        var screenHeight = 0       // 屏幕高度

        var showJoystick = false   // 是否显示摇杆
            private set

        private var activeButtons = mutableListOf<ButtonDef>()  // 当前显示的按钮列表
        private var gameModeIndex = -1     // 当前游戏模式 ID（缓存，避免重复计算）
        private var inGame = false         // 当前是否在游戏中

        /** 摇杆半径：屏幕高度的 7% */
        val joystickRadius get() = (screenHeight * 0.07f)

        // 画笔资源画笔
        private val circlePaint = Paint(Paint.ANTI_ALIAS_FLAG)
        private val textPaint = Paint(Paint.ANTI_ALIAS_FLAG)

        init {
            circlePaint.style = Paint.Style.FILL
            circlePaint.alpha = 120  // 半透明

            textPaint.color = Color.WHITE
            textPaint.textAlign = Paint.Align.CENTER
            textPaint.alpha = 180
        }

        /** 设置屏幕尺寸并重新计算按钮布局 */
        fun setScreenSize(w: Int, h: Int) {
            screenWidth = w
            screenHeight = h
            applyModeConfig()
            invalidate()
        }

        /**
         * 游戏模式变化回调
         * 由 RenderThread 的 syncGameMode() 在检测到模式变化时调用
         */
        fun onGameModeChanged(mode: Int, inGame: Boolean) {
            if (gameModeIndex == mode && this.inGame == inGame) return
            gameModeIndex = mode
            this.inGame = inGame
            applyModeConfig()
            post { invalidate() }
        }

        /**
         * applyModeConfig - 应用游戏模式配置
         *
         * 根据当前游戏模式从 modeConfigs 查找对应的 UI 配置，
         * 然后根据按钮数量自动计算每个按钮的位置和大小。
         *
         * 按钮布局策略：
         *   - 1 个按钮：右下角，1.3 倍大小
         *   - 2 个按钮：右下角垂直排列
         *   - 3 个按钮：右下角水平一行排列
         *   - 4 个按钮：右下角水平一行
         *   - 5 个按钮：右下角水平一行
         */
        private fun applyModeConfig() {
            showJoystick = false
            activeButtons.clear()
            if (!inGame || screenWidth == 0 || screenHeight == 0) return

            val config = modeConfigs[gameModeIndex] ?: modeConfigs[-1]!!
            showJoystick = config.showJoystick

            activeButtons.clear()
            if (config.buttons.isEmpty()) return

            val btnCount = config.buttons.size
            val pad = 80f  // 按钮距屏幕边缘的内边距
            val btnW = screenWidth.toFloat()
            val btnH = screenHeight.toFloat()

            // 根据按钮数量动态调整基础半径
            val baseRadius = when {
                btnCount <= 2 -> 65f
                btnCount <= 4 -> 55f
                else -> 48f
            }

            when {
                btnCount == 1 -> {
                    // 单个按钮：放大显示在右下角
                    val cx = btnW - pad - baseRadius
                    val cy = btnH - pad - baseRadius * 2.5f
                    activeButtons.add(ButtonDef(
                        config.buttons[0].label, cx, cy,
                        baseRadius * 1.3f, config.buttons[0].color, config.buttons[0].keyCode
                    ))
                }
                btnCount == 2 -> {
                    // 两个按钮：垂直堆叠在右侧
                    val cx = btnW - pad - baseRadius
                    val gapY = baseRadius * 3.5f
                    for (i in 0 until btnCount) {
                        val cy = btnH - pad - baseRadius * (2.5f - i * 2.0f)
                        activeButtons.add(ButtonDef(
                            config.buttons[i].label, cx, cy,
                            baseRadius, config.buttons[i].color, config.buttons[i].keyCode
                        ))
                    }
                }
                btnCount == 3 -> {
                    // 三个按钮：水平排列一行
                    val gapX = baseRadius * 2.8f
                    for (i in 0 until btnCount) {
                        val cx = btnW - pad - (btnCount - 1 - i) * gapX
                        val cy = btnH - pad - baseRadius * 2f
                        activeButtons.add(ButtonDef(
                            config.buttons[i].label, cx, cy,
                            baseRadius, config.buttons[i].color, config.buttons[i].keyCode
                        ))
                    }
                }
                btnCount == 4 -> {
                    val gapX = baseRadius * 2.4f
                    for (i in 0 until btnCount) {
                        val cx = btnW - pad - (btnCount - 1 - i) * gapX
                        val cy = btnH - pad - baseRadius * 2f
                        activeButtons.add(ButtonDef(
                            config.buttons[i].label, cx, cy,
                            baseRadius, config.buttons[i].color, config.buttons[i].keyCode
                        ))
                    }
                }
                else -> {
                    val gapX = baseRadius * 2.2f
                    for (i in 0 until btnCount) {
                        val cx = btnW - pad - (btnCount - 1 - i) * gapX
                        val cy = btnH - pad - baseRadius * 2f
                        activeButtons.add(ButtonDef(
                            config.buttons[i].label, cx, cy,
                            baseRadius, config.buttons[i].color, config.buttons[i].keyCode
                        ))
                    }
                }
            }
        }

        override fun onSizeChanged(w: Int, h: Int, oldw: Int, oldh: Int) {
            super.onSizeChanged(w, h, oldw, oldh)
            if (screenWidth == 0) setScreenSize(w, h)
        }

        /**
         * getButtonAt - 命中测试
         * @return 触摸点所在按钮的 keyCode，未命中任何按钮则返回 -1
         */
        fun getButtonAt(x: Float, y: Float): Int {
            for (btn in activeButtons) {
                val dx = x - btn.cx
                val dy = y - btn.cy
                if (dx * dx + dy * dy <= btn.radius * btn.radius) {
                    return btn.keyCode
                }
            }
            return -1
        }

        override fun onDraw(canvas: Canvas) {
            super.onDraw(canvas)

            if (showJoystick) {
                drawJoystick(canvas)
            }
            drawButtons(canvas)
        }

        /**
         * drawJoystick - 绘制虚拟摇杆
         *
         * 摇杆由三部分构成：
         *   1. 外圈（半透明灰白）- 摇杆的最大活动范围
         *   2. 内圈（半透明灰白）- 摇杆中心标记
         *   3. 摇杆头（半透明蓝）- 用户拖动时的指示器（位置受手指位置约束在圈内）
         *
         * 摇杆位置固定在屏幕左侧中心偏下：(16.5% 宽度, 65% 高度)
         */
        private fun drawJoystick(canvas: Canvas) {
            val cx = screenWidth * 0.165f
            val cy = screenHeight * 0.65f
            val r = joystickRadius
            val r2 = r * 0.4f

            // 外圈
            circlePaint.color = Color.argb(60, 255, 255, 255)
            canvas.drawCircle(cx, cy, r, circlePaint)

            // 内圈（中心标记）
            circlePaint.color = Color.argb(80, 255, 255, 255)
            canvas.drawCircle(cx, cy, r2, circlePaint)

            // 摇杆头（仅在拖拽时显示）
            if (joystickPointerId != -1) {
                val dx = joystickCurrDirX - joystickBaseX
                val dy = joystickCurrDirY - joystickBaseY
                val dist = Math.sqrt((dx * dx + dy * dy).toDouble()).toFloat()
                val knobX: Float
                val knobY: Float
                if (dist > r) {
                    // 限制摇杆头在圈内
                    val scale = r / dist
                    knobX = cx + dx * scale
                    knobY = cy + dy * scale
                } else {
                    knobX = cx + dx
                    knobY = cy + dy
                }

                circlePaint.color = Color.argb(120, 120, 180, 255)
                canvas.drawCircle(knobX, knobY, r2 * 1.3f, circlePaint)
            }
        }

        /**
         * drawButtons - 绘制所有按钮
         * 每个按钮由圆形背景 + 居中文本标签组成
         * 根据按钮大小自适应文本尺寸
         */
        private fun drawButtons(canvas: Canvas) {
            for (btn in activeButtons) {
                circlePaint.color = btn.color
                canvas.drawCircle(btn.cx, btn.cy, btn.radius, circlePaint)

                val textY = btn.cy + btn.radius * 0.25f  // 文本垂直居中（偏移校正）
                // 小按钮用小字号
                if (btn.radius < 50f) {
                    textPaint.textSize = 20f
                } else {
                    textPaint.textSize = 28f
                }
                canvas.drawText(btn.label, btn.cx, textY, textPaint)
            }
        }
    }

    // ========================================================================
    // JNI 原生方法声明（与 native-lib.cpp 一一对应）
    // ========================================================================

    private external fun nativeSetFilesDir(path: String)                     // 设置工作目录
    private external fun nativeInitBeforeSurface()                           // 无窗口初始化
    private external fun nativeInitSurface(surface: Any)                     // 基于 Surface 初始化
    private external fun nativeRenderFrame()                                 // 渲染一帧
    private external fun nativeDestroy()                                     // 销毁引擎
    private external fun nativeTouchEvent(action: Int, x: Float, y: Float)   // 触摸事件
    private external fun nativeResize(width: Int, height: Int)               // 窗口尺寸变化
    private external fun nativeKeyRequest(keyCode: Int)                      // 按键请求
    private external fun nativeJoystick(dirX: Float, dirY: Float)            // 摇杆方向
    private external fun nativeJoystickRelease()                              // 摇杆释放
    private external fun nativeUpdateTouchState(actionMasked: Int, pointerCount: Int)  // 触摸状态更新
    private external fun nativeRequestExit()                                  // 请求退出
    private external fun nativeGetGameMode(): Int                             // 获取游戏模式
    private external fun nativeIsInGame(): Boolean                             // 是否在游戏中
    private external fun nativeMouseScroll(delta: Int)                        // 鼠标滚轮

    companion object {
        /**
         * GameModeId - 游戏模式常量定义
         * 与 C++ 侧的 GameMode 枚举一一对应
         */
        object GameModeId {
            const val Maze = 0              // 迷宫模式
            const val TankTrouble = 1       // 坦克大战模式
            const val PhysicsTest = 2       // 物理引擎测试模式
            const val SoloudTest = 3        // 音频引擎（SoLoud）测试模式
            const val RadianceCascades = 4  // 辐射级联（Radiance Cascades）渲染演示
            const val FruitNinja = 5        // 水果忍者模式
            const val WFCTest = 6           // WFC 测试模式
            const val BlockWorld = 7         // 区块世界模式（Minecraft-like）
            const val Infinite = 100        // 无限模式（无尽生存/挑战）
        }

        init {
            // 加载原生库 "libpixelclean.so"
            // 这是 JNI 的入口，System.loadLibrary 会触发 native-lib.cpp 中的 JNI_OnLoad
            System.loadLibrary("pixelclean")
        }
    }
}