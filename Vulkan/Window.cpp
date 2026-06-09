#include "window.h"
#include "../GlobalVariable.h"
#include "../GameMods/GameMods.h"
#if defined(_WIN32)
#define _WINSOCKAPI_
#include <Windows.h>
#elif defined(__ANDROID__)
#include <android/log.h>
#endif
#include <iostream>
#include "../DebugLog.h"

namespace VulKan {

#if defined(_WIN32)
	//获取窗口大小是否改变
	static void windowResized(GLFWwindow* window, int width, int height) {
		auto* win = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));
		if (win) {
			win->mWindowResized = true;
		}
	}

	//用来绑定Camera鼠标事件
	static void cursorPosCallBack(GLFWwindow* window, double xpos, double ypos) {
		auto* win = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));
		if (win && win->mApp) {
			win->mApp->MouseMove(xpos, ypos);
		}
	}

	static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
	{
		auto* win = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));
		if (win) {
			win->MouseScroll = -yoffset;
			if (win->mApp) {
				win->mApp->MouseRoller(-yoffset);
			}
		}
	}
#endif


	Window::Window(const int& width, const int& height, bool MouseBool, bool FullScreen) {
		LOGD("Window::Window(width=%d, height=%d, FullScreen=%d)", width, height, FullScreen);
		mWidth = width;
		mHeight = height;
		MouseDisabled = MouseBool;

#if defined(_WIN32)
		glfwInit();

		//设置环境，关掉opengl API 并且禁止窗口改变大小
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);//关掉opengl API
		glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);//是否禁止窗口改变大小
		GLFWmonitor* pMonitor = FullScreen ? glfwGetPrimaryMonitor() : NULL;
		mWindow = glfwCreateWindow(mWidth, mHeight, "Game_Demo - VulKan", pMonitor, nullptr);//创建一个窗口
		if (!mWindow) {
				LOGE("Window::Window: failed to create window");
				std::cerr << "Error: failed to create window" << std::endl;
		}
		//glfwSetWindowAttrib(mWindow, GLFW_FLOATING, GLFW_TRUE);//窗口置顶
		//glfwSetWindowOpacity(mWindow, 1.0f);//窗口透明度
		glfwSetWindowUserPointer(mWindow, this);
		if (FullScreen) {
			glfwSetWindowMonitor(mWindow, glfwGetPrimaryMonitor(), 0, 0, mWidth, mHeight, GLFW_DONT_CARE);//全屏
		}
		

		
		glfwSetFramebufferSizeCallback(mWindow, windowResized);//绑定窗口大小改变事件

		//GLFW_CURSOR_DISABLED 禁用鼠标
		if (MouseDisabled) {
			glfwSetInputMode(mWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
		}
		else {
			glfwSetInputMode(mWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
		}
#elif defined(__ANDROID__)
		mWindow = nullptr;
#endif
	}

#if defined(__ANDROID__)
	void Window::setAndroidWindow(ANativeWindow* nativeWindow) {
		mWindow = nativeWindow;
		if (mWindow) {
			mWidth = ANativeWindow_getWidth(mWindow);
			mHeight = ANativeWindow_getHeight(mWindow);
		}
	}
#endif

#if defined(_WIN32)
	void Window::setApp(GameMods* app) { 
		mApp = app;
		// 注册鼠标滚轮回调函数
		glfwSetScrollCallback(mWindow, scroll_callback);
		// 注册鼠标移动回调函数
		glfwSetCursorPosCallback(mWindow, cursorPosCallBack);
	}

	void Window::ReleaseApp() {
		// 注册鼠标滚轮回调函数
		glfwSetScrollCallback(mWindow, nullptr);
		// 注册鼠标移动回调函数
		glfwSetCursorPosCallback(mWindow, nullptr);
	}
#elif defined(__ANDROID__)
	void Window::setApp(GameMods* app) { 
		mApp = app;
	}

	void Window::ReleaseApp() {
	}
#endif

	//销毁Window
	Window::~Window() {
#if defined(_WIN32)
		glfwDestroyWindow(mWindow);//回收GLFW的API
		glfwTerminate();
#endif
	}

	void Window::SetWindow(bool FullScreen) {
		if (FullScreen) {
#if defined(_WIN32)
			RECT windowRect;
			GetWindowRect(GetDesktopWindow(), &windowRect);
			mWidth = windowRect.right;
			mHeight = windowRect.bottom;
#elif defined(__ANDROID__)
			// Android 全屏尺寸由 SurfaceView 管理，mWidth/mHeight 由 JNI 层设置
#endif
#if defined(_WIN32)
			glfwSetWindowMonitor(mWindow, glfwGetPrimaryMonitor(), 0, 0, mWidth, mHeight, GLFW_DONT_CARE);//全屏
#endif
		}
		else {
#if defined(_WIN32)
			glfwSetWindowMonitor(mWindow, NULL, mWidth / 4, mHeight / 4, mWidth/2, mHeight/2, 0);
#endif
		}
		mWindowResized = true;
	}

	void Window::WindowClose() {
		Global::Storage();
		exit(0);//用这个退出是直接关闭整个程序，所以要提前储存配置信息
	}

	//判断窗口是否被关闭
	bool Window::shouldClose() {
#if defined(_WIN32)
		return glfwWindowShouldClose(mWindow);
#elif defined(__ANDROID__)
		return mShouldClose;
#endif
	}

	//窗口获取事件
	void Window::pollEvents() {
#if defined(_WIN32)
		glfwPollEvents();
#endif
	}

	void Window::ImGuiKeyBoardEvent() {
#if defined(_WIN32)
		/*if (glfwGetKey(mWindow, GLFW_KEY_ESCAPE) == GLFW_PRESS && glfwGetKey(mWindow, GLFW_KEY_ESCAPE) != KeysRisingEdgeTrigger_Esc) {
			if (mApp->InterFace->GetInterfaceIndexes() == GAME::InterFaceEnum_::ViceInterface_Enum) {
				mApp->InterFace->SetInterFaceBool();
			}
		}
		KeysRisingEdgeTrigger_Esc = glfwGetKey(mWindow, GLFW_KEY_ESCAPE);*/

		//控制鼠标显示和禁用
		if (glfwGetKey(mWindow, GLFW_KEY_GRAVE_ACCENT) == GLFW_PRESS && glfwGetKey(mWindow, GLFW_KEY_GRAVE_ACCENT) != KeysRisingEdgeTrigger) {
			if (MouseDisabled) {
				glfwSetInputMode(mWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
				MouseDisabled = false;
			}
			else {
				glfwSetInputMode(mWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
				MouseDisabled = true;
			}
		}
		KeysRisingEdgeTrigger = glfwGetKey(mWindow, GLFW_KEY_GRAVE_ACCENT);
#endif
	}

	//键盘事件
	void Window::processEvent() {

#if defined(_WIN32)
		//控制鼠标显示和禁用
		if (glfwGetKey(mWindow, GLFW_KEY_GRAVE_ACCENT) == GLFW_PRESS && glfwGetKey(mWindow, GLFW_KEY_GRAVE_ACCENT) != KeysRisingEdgeTrigger) {
			if (MouseDisabled) {
				glfwSetInputMode(mWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
				MouseDisabled = false;
			}
			else {
				glfwSetInputMode(mWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
				MouseDisabled = true;
			}
		}
		KeysRisingEdgeTrigger = glfwGetKey(mWindow, GLFW_KEY_GRAVE_ACCENT);


		if (glfwGetKey(mWindow, GLFW_KEY_SLASH) == GLFW_PRESS && glfwGetKey(mWindow, GLFW_KEY_SLASH) != KeysRisingEdgeTrigger_Console) {
			Global::ConsoleBool = true;
		}
		KeysRisingEdgeTrigger_Console = glfwGetKey(mWindow, GLFW_KEY_SLASH);

		if (glfwGetKey(mWindow, GLFW_KEY_ESCAPE) == GLFW_PRESS && glfwGetKey(mWindow, GLFW_KEY_ESCAPE) != KeysRisingEdgeTrigger_Esc) {
			mApp->KeyDown(GameKeyEnum::ESC);
		}
		KeysRisingEdgeTrigger_Esc = glfwGetKey(mWindow, GLFW_KEY_ESCAPE);

		if (Global::ConsoleBool) {
			return;
		}

		if (glfwGetKey(mWindow, Global::KeyW) == GLFW_PRESS) {
			mApp->KeyDown(GameKeyEnum::MOVE_FRONT);
		}

		if (glfwGetKey(mWindow, Global::KeyS) == GLFW_PRESS) {
			mApp->KeyDown(GameKeyEnum::MOVE_BACK);
		}

		if (glfwGetKey(mWindow, Global::KeyA) == GLFW_PRESS) {
			mApp->KeyDown(GameKeyEnum::MOVE_LEFT);
		}

		if (glfwGetKey(mWindow, Global::KeyD) == GLFW_PRESS) {
			mApp->KeyDown(GameKeyEnum::MOVE_RIGHT);
		}

		// 持续触发：空格 → 上升（自由飞行相机）
		if (glfwGetKey(mWindow, GLFW_KEY_SPACE) == GLFW_PRESS) {
			mApp->KeyDown(GameKeyEnum::MOVE_UP);
		}

		// 持续触发：左Shift → 下降
		if (glfwGetKey(mWindow, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
			mApp->KeyDown(GameKeyEnum::MOVE_DOWN);
		}

		if (glfwGetKey(mWindow, GLFW_KEY_1) == GLFW_PRESS && glfwGetKey(mWindow, GLFW_KEY_1) != KeysRisingEdgeTrigger_0) {
			mApp->KeyDown(GameKeyEnum::Key_1);
		}
		KeysRisingEdgeTrigger_0 = glfwGetKey(mWindow, GLFW_KEY_1);

		if (glfwGetKey(mWindow, GLFW_KEY_2) == GLFW_PRESS && glfwGetKey(mWindow, GLFW_KEY_2) != KeysRisingEdgeTrigger_1) {
			mApp->KeyDown(GameKeyEnum::Key_2);
		}
		KeysRisingEdgeTrigger_1 = glfwGetKey(mWindow, GLFW_KEY_2);

		if (glfwGetKey(mWindow, GLFW_KEY_SPACE) == GLFW_PRESS && glfwGetKey(mWindow, GLFW_KEY_SPACE) != KeysRisingEdgeTrigger_Space) {
			mApp->KeyDown(GameKeyEnum::SPACE);
		}
		KeysRisingEdgeTrigger_Space = glfwGetKey(mWindow, GLFW_KEY_SPACE);
#elif defined(__ANDROID__)
		if (mApp == nullptr) return;

		if (Global::AndroidRequestToggleMouse) {
			MouseDisabled = !MouseDisabled;
			Global::AndroidRequestToggleMouse = false;
		}

		if (Global::AndroidRequestConsole) {
			Global::ConsoleBool = true;
			Global::AndroidRequestConsole = false;
		}

		if (Global::AndroidRequestESC) {
			mApp->KeyDown(GameKeyEnum::ESC);
			Global::AndroidRequestESC = false;
		}

		if (Global::ConsoleBool) {
			return;
		}

		if (Global::AndroidKey_W) {
			mApp->KeyDown(GameKeyEnum::MOVE_FRONT);
		}

		if (Global::AndroidKey_S) {
			mApp->KeyDown(GameKeyEnum::MOVE_BACK);
		}

		if (Global::AndroidKey_A) {
			mApp->KeyDown(GameKeyEnum::MOVE_LEFT);
		}

		if (Global::AndroidKey_D) {
			mApp->KeyDown(GameKeyEnum::MOVE_RIGHT);
		}

		if (Global::AndroidRequestKey1) {
			mApp->KeyDown(GameKeyEnum::Key_1);
			Global::AndroidRequestKey1 = false;
		}

		if (Global::AndroidRequestKey2) {
			mApp->KeyDown(GameKeyEnum::Key_2);
			Global::AndroidRequestKey2 = false;
		}

		if (Global::AndroidRequestSpace) {
			mApp->KeyDown(GameKeyEnum::SPACE);
			Global::AndroidRequestSpace = false;
		}
#endif
	}
}