package com.pixelclean

import android.app.Activity
import android.os.Bundle
import android.util.Log
import android.view.MotionEvent
import android.view.SurfaceHolder
import android.view.SurfaceView
import java.io.File
import java.io.FileNotFoundException
import java.io.FileOutputStream

class MainActivity : Activity(), SurfaceHolder.Callback {

    private val TAG = "PixelClean"
    private var nativeInitialized = false
    private var renderThread: RenderThread? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        val surfaceView = SurfaceView(this)
        surfaceView.holder.addCallback(this)
        setContentView(surfaceView)
    }

    override fun onPause() {
        super.onPause()
        stopRenderThread()
    }

    override fun onResume() {
        super.onResume()
        startRenderThread()
    }

    override fun onDestroy() {
        stopRenderThread()
        nativeDestroy()
        super.onDestroy()
    }

    override fun onTouchEvent(event: MotionEvent): Boolean {
        if (nativeInitialized) {
            val pointerCount = event.pointerCount
            for (i in 0 until pointerCount) {
                val x = event.getX(i)
                val y = event.getY(i)
                nativeTouchEvent(
                    event.actionMasked,
                    x,
                    y
                )
            }
        }
        return true
    }

    override fun surfaceCreated(holder: SurfaceHolder) {
        Log.d(TAG, "surfaceCreated")

        if (!nativeInitialized) {
            extractAssets()
            nativeSetFilesDir(filesDir.absolutePath)
            nativeInitBeforeSurface()
            nativeInitialized = true
        }

        nativeInitSurface(holder.surface)
        startRenderThread()
    }

    override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
        Log.d(TAG, "surfaceChanged: ${width}x${height}")
        nativeResize(width, height)
    }

    override fun surfaceDestroyed(holder: SurfaceHolder) {
        Log.d(TAG, "surfaceDestroyed")
        stopRenderThread()
    }

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
            targetFile.parentFile?.mkdirs()
            try {
                assetManager.open(assetPath).use { input ->
                    FileOutputStream(targetFile).use { output ->
                        input.copyTo(output)
                    }
                }
            } catch (e: FileNotFoundException) {
                targetFile.mkdirs()
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

    private fun startRenderThread() {
        if (renderThread == null || renderThread?.isRunning == false) {
            renderThread = RenderThread()
            renderThread?.start()
        }
    }

    private fun stopRenderThread() {
        renderThread?.stopRendering()
        try {
            renderThread?.join(1000)
        } catch (e: InterruptedException) {
            Log.e(TAG, "Failed to join render thread", e)
        }
        renderThread = null
    }

    private inner class RenderThread : Thread() {
        @Volatile
        var isRunning = false

        override fun run() {
            isRunning = true
            while (isRunning && !isInterrupted) {
                try {
                    nativeRenderFrame()
                } catch (e: Exception) {
                    Log.e(TAG, "RenderFrame exception", e)
                }
            }
        }

        fun stopRendering() {
            isRunning = false
        }
    }

    private external fun nativeSetFilesDir(path: String)
    private external fun nativeInitBeforeSurface()
    private external fun nativeInitSurface(surface: Any)
    private external fun nativeRenderFrame()
    private external fun nativeDestroy()
    private external fun nativeTouchEvent(action: Int, x: Float, y: Float)
    private external fun nativeResize(width: Int, height: Int)

    companion object {
        init {
            System.loadLibrary("pixelclean")
        }
    }
}