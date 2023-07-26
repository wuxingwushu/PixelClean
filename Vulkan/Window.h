#pragma once

#include "../base.h"

namespace GAME {
	class Application;
}

namespace GAME::VulKan {

	class Window {
	public:


		//构建窗口
		Window(const int& width, const int& height, bool MouseBool, bool FullScreen);

		//解析释放
		~Window();

		void WindowClose();

		//判断窗口是否被关闭
		bool shouldClose();

		//窗口获取事件
		void pollEvents();

		[[nodiscard]] GLFWwindow* getWindow() const noexcept { return mWindow; }

		void setApp(Application* app) { mApp = app; }

		//界面键盘事件
		void ImGuiKeyBoardEvent();
		//游戏中键盘事件
		void processEvent();

	public:
		bool mWindowResized{ false };//窗口大小是否发生改变
		Application* mApp;

	private:
		bool MouseDisabled = false;//是否显示鼠标光标
		int mWidth{ 0 };//储存窗口宽度
		int mHeight{ 0 };//储存窗口高度
		GLFWwindow* mWindow{ NULL };//储存窗口指针

	private:
		//按键上升沿触发（储存上一时刻的值）
		bool KeysRisingEdgeTrigger = false;

	};
}