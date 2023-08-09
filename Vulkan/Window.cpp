#include "window.h"
#include "../application.h"
#include "../Camera.h"

namespace VulKan {

	//获取窗口大小是否改变
	static void windowResized(GLFWwindow* window, int width, int height) {
		reinterpret_cast<Window*>(glfwGetWindowUserPointer(window))->mWindowResized = true;
	}

	//用来绑定Camera鼠标事件
	static void cursorPosCallBack(GLFWwindow* window, double xpos, double ypos) {
		reinterpret_cast<Window*>(glfwGetWindowUserPointer(window))->mApp->onMouseMove(xpos, ypos);
	}

	//绑定鼠标滚轮事件
	static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
	{
		reinterpret_cast<Window*>(glfwGetWindowUserPointer(window))->mApp->SetCameraZ(-yoffset);
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
		glfwSetFramebufferSizeCallback(mWindow, windowResized);//绑定窗口大小改变事件
		//glfwSetCursorPosCallback(mWindow, cursorPosCallBack);//绑定鼠标事件
		if (FullScreen) {
			glfwSetWindowMonitor(mWindow, glfwGetPrimaryMonitor(), 0, 0, mWidth, mHeight, GLFW_DONT_CARE);//全屏
		}
		

		// 注册鼠标滚轮回调函数
		glfwSetScrollCallback(mWindow, scroll_callback);

		//GLFW_CURSOR_DISABLED 禁用鼠标
		if (MouseDisabled) {
			glfwSetInputMode(mWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
		}
		else {
			glfwSetInputMode(mWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
		}
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
		mApp->recreateSwapChain();
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
		if (glfwGetKey(mWindow, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
			while (glfwGetKey(mWindow, GLFW_KEY_ESCAPE) == GLFW_PRESS)
			{
				glfwPollEvents();
			}
			mApp->InterFace->SetInterFaceBool();
		}

		//控制鼠标显示和禁用
		if (glfwGetKey(mWindow, GLFW_KEY_P) == GLFW_PRESS && glfwGetKey(mWindow, GLFW_KEY_P) != KeysRisingEdgeTrigger) {
			if (MouseDisabled) {
				glfwSetInputMode(mWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
				MouseDisabled = false;
			}
			else {
				glfwSetInputMode(mWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
				MouseDisabled = true;
			}
		}
		KeysRisingEdgeTrigger = glfwGetKey(mWindow, GLFW_KEY_P);
	}

	//键盘事件
	void Window::processEvent() {
		if (glfwGetKey(mWindow, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
			while (glfwGetKey(mWindow, GLFW_KEY_ESCAPE) == GLFW_PRESS)
			{
				glfwPollEvents();
			}
			mApp->InterFace->SetInterFaceBool();
		}

		//控制鼠标显示和禁用
		if (glfwGetKey(mWindow, GLFW_KEY_P) == GLFW_PRESS && glfwGetKey(mWindow, GLFW_KEY_P) != KeysRisingEdgeTrigger) {
			if (MouseDisabled) {
				glfwSetInputMode(mWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
				MouseDisabled = false;
			}
			else {
				glfwSetInputMode(mWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
				MouseDisabled = true;
			}
		}
		KeysRisingEdgeTrigger = glfwGetKey(mWindow, GLFW_KEY_P);

		if (glfwGetKey(mWindow, GLFW_KEY_W) == GLFW_PRESS) {
			mApp->onKeyDown(CAMERA_MOVE::MOVE_FRONT);
		}

		if (glfwGetKey(mWindow, GLFW_KEY_S) == GLFW_PRESS) {
			mApp->onKeyDown(CAMERA_MOVE::MOVE_BACK);
		}

		if (glfwGetKey(mWindow, GLFW_KEY_A) == GLFW_PRESS) {
			mApp->onKeyDown(CAMERA_MOVE::MOVE_LEFT);
		}

		if (glfwGetKey(mWindow, GLFW_KEY_D) == GLFW_PRESS) {
			mApp->onKeyDown(CAMERA_MOVE::MOVE_RIGHT);
		}
	}
}