#include "window.h"
#include "../GlobalVariable.h"
#include "../GameMods/GameMods.h"
#define _WINSOCKAPI_
#include <Windows.h>
#include <iostream>

namespace VulKan {

	//获取窗口大小是否改变
	static void windowResized(GLFWwindow* window, int width, int height) {
		reinterpret_cast<Window*>(glfwGetWindowUserPointer(window))->mWindowResized = true;
	}

	//用来绑定Camera鼠标事件
	static void cursorPosCallBack(GLFWwindow* window, double xpos, double ypos) {
		reinterpret_cast<Window*>(glfwGetWindowUserPointer(window))->mApp->MouseMove(xpos, ypos);
	}

	//绑定鼠标滚轮事件
	static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
	{
		reinterpret_cast<Window*>(glfwGetWindowUserPointer(window))->mApp->MouseRoller(-yoffset);
		//printf("Mouse wheel scrolled: xoffset = %lf, yoffset = %lf\n", xoffset, yoffset);
	}


	Window::Window(const int& width, const int& height, bool MouseBool, bool FullScreen) {
		mWidth = width;
		mHeight = height;
		MouseDisabled = MouseBool;

		glfwInit();

		//设置环境，关掉opengl API 并且禁止窗口改变大小
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);//关掉opengl API
		glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);//是否禁止窗口改变大小
		GLFWmonitor* pMonitor = FullScreen ? glfwGetPrimaryMonitor() : NULL;
		mWindow = glfwCreateWindow(mWidth, mHeight, "Game_Demo - VulKan", pMonitor, nullptr);//创建一个窗口
		if (!mWindow) {//判断窗口是否创建成功
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
	}

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

	//销毁Window
	Window::~Window() {
		glfwDestroyWindow(mWindow);//回收GLFW的API
		glfwTerminate();
	}

	void Window::SetWindow(bool FullScreen) {
		if (FullScreen) {
			RECT windowRect;
			GetWindowRect(GetDesktopWindow(), &windowRect);
			mWidth = windowRect.right;
			mHeight = windowRect.bottom;
			glfwSetWindowMonitor(mWindow, glfwGetPrimaryMonitor(), 0, 0, mWidth, mHeight, GLFW_DONT_CARE);//全屏
		}
		else {
			glfwSetWindowMonitor(mWindow, NULL, mWidth / 4, mHeight / 4, mWidth/2, mHeight/2, 0);
		}
		mWindowResized = true;
	}

	void Window::WindowClose() {
		Global::Storage();
		exit(0);//用这个退出是直接关闭整个程序，所以要提前储存配置信息
	}

	//判断窗口是否被关闭
	bool Window::shouldClose() {
		return glfwWindowShouldClose(mWindow);
	}

	//窗口获取事件
	void Window::pollEvents() {
		glfwPollEvents();
	}

	void Window::ImGuiKeyBoardEvent() {
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
	}

	//键盘事件
	void Window::processEvent() {

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

		if (glfwGetKey(mWindow, GLFW_KEY_1) == GLFW_PRESS && glfwGetKey(mWindow, GLFW_KEY_1) != KeysRisingEdgeTrigger_0) {
			mApp->KeyDown(GameKeyEnum::Key_1);
		}
		KeysRisingEdgeTrigger_0 = glfwGetKey(mWindow, GLFW_KEY_1);

		if (glfwGetKey(mWindow, GLFW_KEY_2) == GLFW_PRESS && glfwGetKey(mWindow, GLFW_KEY_2) != KeysRisingEdgeTrigger_1) {
			mApp->KeyDown(GameKeyEnum::Key_2);
		}
		KeysRisingEdgeTrigger_1 = glfwGetKey(mWindow, GLFW_KEY_2);
	}
}