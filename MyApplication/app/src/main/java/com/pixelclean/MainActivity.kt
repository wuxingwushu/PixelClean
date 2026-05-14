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

class MainActivity : Activity(), SurfaceHolder.Callback {

    private val TAG = "PixelClean"
    private var nativeInitialized = false
    private var surfaceInitialized = false
    private var renderThread: RenderThread? = null
    private val destroyed = AtomicBoolean(false)

    private lateinit var containerLayout: FrameLayout
    private lateinit var surfaceView: SurfaceView
    private lateinit var controlOverlay: ControlOverlayView

    private val currentGameMode = AtomicInteger(-1)
    private val currentInGame = AtomicBoolean(false)

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        setupFullScreen()

        containerLayout = FrameLayout(this)

        surfaceView = SurfaceView(this)
        surfaceView.holder.addCallback(this)
        containerLayout.addView(surfaceView, FrameLayout.LayoutParams(
            FrameLayout.LayoutParams.MATCH_PARENT,
            FrameLayout.LayoutParams.MATCH_PARENT
        ))

        controlOverlay = ControlOverlayView(this)
        controlOverlay.setOnTouchListener { _, event ->
            handleControlTouch(event)
        }
        containerLayout.addView(controlOverlay, FrameLayout.LayoutParams(
            FrameLayout.LayoutParams.MATCH_PARENT,
            FrameLayout.LayoutParams.MATCH_PARENT
        ))

        setContentView(containerLayout)

        if (Build.VERSION.SDK_INT >= 33) {
            registerBackInvokedCallback()
        }
    }

    @Deprecated("Deprecated in Java")
    override fun onBackPressed() {
        if (nativeInitialized) {
            nativeKeyRequest(0)
        }
    }

    @SuppressLint("NewApi")
    private fun registerBackInvokedCallback() {
        onBackInvokedDispatcher.registerOnBackInvokedCallback(
            android.window.OnBackInvokedDispatcher.PRIORITY_DEFAULT
        ) {
            if (nativeInitialized) {
                nativeKeyRequest(0)
            }
        }
    }

    override fun dispatchKeyEvent(event: KeyEvent): Boolean {
        if (event.keyCode == KeyEvent.KEYCODE_BACK && event.action == KeyEvent.ACTION_UP) {
            if (nativeInitialized) {
                nativeKeyRequest(0)
            }
            return true
        }
        return super.dispatchKeyEvent(event)
    }

    override fun onPause() {
        super.onPause()
        stopRenderThread()
    }

    override fun onResume() {
        super.onResume()
        if (!destroyed.get()) {
            startRenderThread()
        }
    }

    override fun onDestroy() {
        destroyed.set(true)
        stopRenderThread()
        if (nativeInitialized) {
            nativeRequestExit()
            try {
                Thread.sleep(100)
            } catch (_: InterruptedException) { }
            nativeDestroy()
            nativeInitialized = false
            surfaceInitialized = false
        }
        super.onDestroy()
    }

    override fun onWindowFocusChanged(hasFocus: Boolean) {
        super.onWindowFocusChanged(hasFocus)
        if (hasFocus) {
            hideSystemUI()
        }
    }

    private fun setupFullScreen() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            window.attributes.layoutInDisplayCutoutMode =
                WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            window.setDecorFitsSystemWindows(false)
        }
    }

    private fun hideSystemUI() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            window.insetsController?.hide(
                WindowInsets.Type.statusBars() or WindowInsets.Type.navigationBars()
            )
        } else {
            @Suppress("DEPRECATION")
            window.decorView.systemUiVisibility = (
                View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                    or View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                    or View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                    or View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                    or View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                    or View.SYSTEM_UI_FLAG_FULLSCREEN
                )
        }
    }

    private var joystickPointerId = -1
    private var joystickBaseX = 0f
    private var joystickBaseY = 0f
    private var joystickCurrDirX = 0f
    private var joystickCurrDirY = 0f
    private var prevPinchDistance = 0f
    private val activePointers = mutableSetOf<Int>()
    private val buttonPointerIds = mutableSetOf<Int>()
    private val gameTouchPointerIds = mutableSetOf<Int>()

    @SuppressLint("ClickableViewAccessibility")
    private fun handleControlTouch(event: MotionEvent): Boolean {
        if (destroyed.get() || !nativeInitialized || !surfaceInitialized) return false

        val action = event.actionMasked
        val pointerIndex = event.actionIndex
        val pointerId = event.getPointerId(pointerIndex)
        val rawX = event.getX(pointerIndex)
        val rawY = event.getY(pointerIndex)

        when (action) {
            MotionEvent.ACTION_DOWN,
            MotionEvent.ACTION_POINTER_DOWN -> {
                activePointers.add(pointerId)

                if (controlOverlay.showJoystick && isInJoystickZone(rawX, rawY)) {
                    joystickPointerId = pointerId
                    joystickBaseX = rawX
                    joystickBaseY = rawY
                    joystickCurrDirX = rawX
                    joystickCurrDirY = rawY
                } else if (isInButtonZone(rawX, rawY)) {
                    handleButtonPress(rawX, rawY)
                    buttonPointerIds.add(pointerId)
                } else {
                    gameTouchPointerIds.add(pointerId)
                    nativeTouchEvent(0, rawX, rawY)
                }

                if (activePointers.size == 2) {
                    prevPinchDistance = pinchDist(event)
                }

                updateTouchStateForGame()
            }

            MotionEvent.ACTION_MOVE -> {
                val pointerCount = event.pointerCount
                if (pointerCount == 2) {
                    val curDist = pinchDist(event)
                    if (prevPinchDistance > 0f) {
                        val delta = curDist - prevPinchDistance
                        if (kotlin.math.abs(delta) > 12f) {
                            nativeMouseScroll(if (delta > 0) -1 else 1)
                            prevPinchDistance = curDist
                        }
                    } else {
                        prevPinchDistance = curDist
                    }
                }
                if (pointerCount == 1) {
                    val px = event.getX(0)
                    val py = event.getY(0)
                    val pid = event.getPointerId(0)
                    if (pid == joystickPointerId) {
                        updateJoystick(px, py)
                    } else if (pid !in buttonPointerIds) {
                        nativeTouchEvent(2, px, py)
                    }
                } else {
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

            MotionEvent.ACTION_UP,
            MotionEvent.ACTION_POINTER_UP -> {
                activePointers.remove(pointerId)
                buttonPointerIds.remove(pointerId)
                val hadGameTouch = gameTouchPointerIds.isNotEmpty()
                gameTouchPointerIds.remove(pointerId)

                if (activePointers.size < 2) {
                    prevPinchDistance = 0f
                }

                if (pointerId == joystickPointerId) {
                    joystickPointerId = -1
                    nativeJoystickRelease()
                }

                if (hadGameTouch && gameTouchPointerIds.isEmpty()) {
                    nativeUpdateTouchState(1, 0)
                } else if (gameTouchPointerIds.isNotEmpty()) {
                    updateTouchStateForGame()
                }
            }

            MotionEvent.ACTION_CANCEL -> {
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

        controlOverlay.invalidate()
        return true
    }

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
            val scale = maxR / Math.sqrt(dist.toDouble()).toFloat()
            dirX = dx * scale / maxR
            dirY = dy * scale / maxR
        } else {
            dirX = dx / maxR
            dirY = dy / maxR
        }
        nativeJoystick(dirX, dirY)
    }

    private fun updateTouchStateForGame() {
        nativeUpdateTouchState(0, gameTouchPointerIds.size)
    }

    private fun handleButtonPress(x: Float, y: Float) {
        val code = controlOverlay.getButtonAt(x, y)
        if (code >= 0) {
            nativeKeyRequest(code)
        }
    }

    private fun pinchDist(event: MotionEvent): Float {
        val dx = event.getX(0) - event.getX(1)
        val dy = event.getY(0) - event.getY(1)
        return kotlin.math.sqrt(dx * dx + dy * dy)
    }

    private fun isInJoystickZone(x: Float, y: Float): Boolean {
        if (!controlOverlay.showJoystick) return false
        val h = controlOverlay.height.toFloat()
        val w = controlOverlay.width.toFloat()
        return x < w * 0.33f && y > h * 0.4f
    }

    private fun isInButtonZone(x: Float, y: Float): Boolean {
        return controlOverlay.getButtonAt(x, y) >= 0
    }

    override fun surfaceCreated(holder: SurfaceHolder) {
        Log.d(TAG, "surfaceCreated")
        if (destroyed.get()) return

        if (!nativeInitialized) {
            extractAssets()
            nativeSetFilesDir(filesDir.absolutePath)
            nativeInitBeforeSurface()
            nativeInitialized = true
        }

        if (holder.surface.isValid) {
            val surface = holder.surface
            Thread({
                Log.d(TAG, "initSurface thread started")
                try {
                    nativeInitSurface(surface)
                    surfaceInitialized = true
                    Log.d(TAG, "initSurface thread completed, starting render thread")
                    startRenderThread()
                } catch (e: Exception) {
                    Log.e(TAG, "initSurface thread failed", e)
                }
            }, "PixelClean-InitSurface").start()
        } else {
            Log.w(TAG, "surfaceCreated: surface is not valid, deferring init")
        }
    }

    override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
        Log.d(TAG, "surfaceChanged: ${width}x${height}")
        controlOverlay.setScreenSize(width, height)
        if (!destroyed.get() && surfaceInitialized && width > 0 && height > 0) {
            nativeResize(width, height)
        }
    }

    override fun surfaceDestroyed(holder: SurfaceHolder) {
        Log.d(TAG, "surfaceDestroyed")
        stopRenderThread()
        surfaceInitialized = false
        activePointers.clear()
        buttonPointerIds.clear()
        gameTouchPointerIds.clear()
        joystickPointerId = -1
        nativeUpdateTouchState(1, 0)
        nativeJoystickRelease()
    }

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

    private fun copyAssetDir(assetPath: String, targetDir: File) {
        val assetManager = assets
        val children = assetManager.list(assetPath)
        if (children == null || children.isEmpty()) {
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
                    targetFile.mkdirs()
                }
            }
            return
        }
        if (assetPath.isNotEmpty()) {
            File(targetDir, assetPath).mkdirs()
        }
        for (child in children) {
            val childPath = if (assetPath.isEmpty()) child else "$assetPath/$child"
            copyAssetDir(childPath, targetDir)
        }
    }

    @Synchronized
    private fun startRenderThread() {
        if (destroyed.get()) return
        if (renderThread == null || renderThread?.isRunning?.get() == false) {
            renderThread?.stopRendering()
            renderThread = RenderThread()
            renderThread?.start()
        }
    }

    @Synchronized
    private fun stopRenderThread() {
        val thread = renderThread
        if (thread != null) {
            thread.stopRendering()
            thread.interrupt()
            try {
                thread.join(2000)
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

    private inner class RenderThread : Thread("PixelClean-Render") {
        private val running = AtomicBoolean(false)
        val isRunning: AtomicBoolean get() = running
        private var pollCounter = 0

        override fun run() {
            running.set(true)
            android.os.Process.setThreadPriority(android.os.Process.THREAD_PRIORITY_DISPLAY)
            while (running.get() && !isInterrupted && !destroyed.get()) {
                try {
                    nativeRenderFrame()
                } catch (e: Exception) {
                    Log.e(TAG, "RenderFrame exception", e)
                }

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

    private data class ButtonSpec(
        val label: String,
        val keyCode: Int,
        val color: Int
    )

    private data class ModeConfig(
        val showJoystick: Boolean,
        val buttons: List<ButtonSpec>
    )

    private val modeConfigs: Map<Int, ModeConfig> = mapOf(
        -1 to ModeConfig(
            showJoystick = false,
            buttons = emptyList()
        ),
        0 to ModeConfig(
            showJoystick = true,
            buttons = listOf(
                ButtonSpec("SP", 4, Color.argb(120, 60, 140, 200)),
                ButtonSpec("1", 2, Color.argb(120, 180, 100, 60)),
                ButtonSpec("2", 3, Color.argb(120, 180, 140, 60)),
                ButtonSpec("/", 1, Color.argb(120, 100, 180, 100)),
                ButtonSpec("`", 5, Color.argb(120, 100, 100, 200))
            )
        ),
        1 to ModeConfig(
            showJoystick = true,
            buttons = listOf(
                ButtonSpec("SP", 4, Color.argb(120, 60, 140, 200)),
                ButtonSpec("/", 1, Color.argb(120, 100, 180, 100))
            )
        ),
        2 to ModeConfig(
            showJoystick = false,
            buttons = emptyList()
        ),
        3 to ModeConfig(
            showJoystick = true,
            buttons = emptyList()
        ),
        4 to ModeConfig(
            showJoystick = false,
            buttons = emptyList()
        ),
        5 to ModeConfig(
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

    private data class ButtonDef(
        val label: String,
        val cx: Float,
        val cy: Float,
        val radius: Float,
        val color: Int,
        val keyCode: Int
    )

    inner class ControlOverlayView(context: Context) : View(context) {

        var screenWidth = 0
        var screenHeight = 0

        var showJoystick = false
            private set

        private var activeButtons = mutableListOf<ButtonDef>()
        private var gameModeIndex = -1
        private var inGame = false

        val joystickRadius get() = (screenHeight * 0.07f)

        private val circlePaint = Paint(Paint.ANTI_ALIAS_FLAG)
        private val textPaint = Paint(Paint.ANTI_ALIAS_FLAG)

        init {
            circlePaint.style = Paint.Style.FILL
            circlePaint.alpha = 120

            textPaint.color = Color.WHITE
            textPaint.textAlign = Paint.Align.CENTER
            textPaint.alpha = 180
        }

        fun setScreenSize(w: Int, h: Int) {
            screenWidth = w
            screenHeight = h
            applyModeConfig()
            invalidate()
        }

        fun onGameModeChanged(mode: Int, inGame: Boolean) {
            if (gameModeIndex == mode && this.inGame == inGame) return
            gameModeIndex = mode
            this.inGame = inGame
            applyModeConfig()
            post { invalidate() }
        }

        private fun applyModeConfig() {
            showJoystick = false
            activeButtons.clear()
            if (!inGame || screenWidth == 0 || screenHeight == 0) return

            val config = modeConfigs[gameModeIndex] ?: modeConfigs[-1]!!

            showJoystick = config.showJoystick

            activeButtons.clear()
            if (config.buttons.isEmpty()) return

            val btnCount = config.buttons.size
            val pad = 80f
            val btnW = screenWidth.toFloat()
            val btnH = screenHeight.toFloat()

            val baseRadius = when {
                btnCount <= 2 -> 65f
                btnCount <= 4 -> 55f
                else -> 48f
            }

            when {
                btnCount == 1 -> {
                    val cx = btnW - pad - baseRadius
                    val cy = btnH - pad - baseRadius * 2.5f
                    activeButtons.add(ButtonDef(
                        config.buttons[0].label, cx, cy,
                        baseRadius * 1.3f, config.buttons[0].color, config.buttons[0].keyCode
                    ))
                }
                btnCount == 2 -> {
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

        private fun drawJoystick(canvas: Canvas) {
            val cx = screenWidth * 0.165f
            val cy = screenHeight * 0.65f
            val r = joystickRadius
            val r2 = r * 0.4f

            circlePaint.color = Color.argb(60, 255, 255, 255)
            canvas.drawCircle(cx, cy, r, circlePaint)

            circlePaint.color = Color.argb(80, 255, 255, 255)
            canvas.drawCircle(cx, cy, r2, circlePaint)

            if (joystickPointerId != -1) {
                val dx = joystickCurrDirX - joystickBaseX
                val dy = joystickCurrDirY - joystickBaseY
                val dist = Math.sqrt((dx * dx + dy * dy).toDouble()).toFloat()
                val knobX: Float
                val knobY: Float
                if (dist > r) {
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

        private fun drawButtons(canvas: Canvas) {
            for (btn in activeButtons) {
                circlePaint.color = btn.color
                canvas.drawCircle(btn.cx, btn.cy, btn.radius, circlePaint)

                val textY = btn.cy + btn.radius * 0.25f
                if (btn.radius < 50f) {
                    textPaint.textSize = 20f
                } else {
                    textPaint.textSize = 28f
                }
                canvas.drawText(btn.label, btn.cx, textY, textPaint)
            }
        }
    }

    private external fun nativeSetFilesDir(path: String)
    private external fun nativeInitBeforeSurface()
    private external fun nativeInitSurface(surface: Any)
    private external fun nativeRenderFrame()
    private external fun nativeDestroy()
    private external fun nativeTouchEvent(action: Int, x: Float, y: Float)
    private external fun nativeResize(width: Int, height: Int)
    private external fun nativeKeyRequest(keyCode: Int)
    private external fun nativeJoystick(dirX: Float, dirY: Float)
    private external fun nativeJoystickRelease()
    private external fun nativeUpdateTouchState(actionMasked: Int, pointerCount: Int)
    private external fun nativeRequestExit()
    private external fun nativeGetGameMode(): Int
    private external fun nativeIsInGame(): Boolean
    private external fun nativeMouseScroll(delta: Int)

    companion object {
        init {
            System.loadLibrary("pixelclean")
        }
    }
}