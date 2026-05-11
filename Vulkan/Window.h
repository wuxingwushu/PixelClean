#pragma once
#if defined(_WIN32)
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#elif defined(__ANDROID__)
#include <android/native_window.h>
#endif

class GameMods;

namespace VulKan {

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

		void SetWindow(bool FullScreen);

#if defined(_WIN32)
		[[nodiscard]] inline GLFWwindow* getWindow() const noexcept { return mWindow; }
#elif defined(__ANDROID__)
		[[nodiscard]] inline ANativeWindow* getWindow() const noexcept { return mWindow; }
#endif

		[[nodiscard]] inline int getWidth() const noexcept { return mWidth; }
		[[nodiscard]] inline int getHeight() const noexcept { return mHeight; }

		// 注册鼠标事件
		void setApp(GameMods* app);

		// 卸载鼠标事件
		void ReleaseApp();

		//界面键盘事件
		void ImGuiKeyBoardEvent();
		//游戏中键盘事件
		void processEvent();

		// Android: 设置系统窗口引用
#if defined(__ANDROID__)
		void setAndroidWindow(ANativeWindow* nativeWindow);
#endif

	public:
		bool mWindowResized{ false };//窗口大小是否发生改变
		float MouseScroll; // 鼠标滚轮值
		GameMods* mApp{ nullptr };

	private:
		bool MouseDisabled = false;//是否显示鼠标光标
		int mWidth{ 0 };//储存窗口宽度
		int mHeight{ 0 };//储存窗口高度
#if defined(_WIN32)
		GLFWwindow* mWindow{ NULL };//储存窗口指针
#elif defined(__ANDROID__)
		ANativeWindow* mWindow{ nullptr };
#endif

	private:
		//按键上升沿触发（储存上一时刻的值）
		bool KeysRisingEdgeTrigger_Esc = false;
		bool KeysRisingEdgeTrigger = false;

		bool KeysRisingEdgeTrigger_Console = false;
		bool KeysRisingEdgeTrigger_0 = false;
		bool KeysRisingEdgeTrigger_1 = false;
		bool KeysRisingEdgeTrigger_Space = false;
	};
}