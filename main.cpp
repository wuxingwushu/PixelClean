#include "application.h"
#include "Vulkan/Window.h"
#include "../DebugLog.h"

#if defined(_WIN32)
#include <Windows.h>
#include <cstdio>
#include <exception>
#endif

// 直接把致命信息写入文件（不依赖 spdlog，因为 PIXEL_ENABLE_LOG=0 时 LOG 宏是空操作）
static void WriteFatalMsg(const char* msg) {
#if defined(_WIN32)
	FILE* f = fopen("Logs/FatalException.txt", "ab+");
	if (!f) f = fopen("FatalException.txt", "ab+"); // 兜底：CWD 下
	if (f) {
		fprintf(f, "%s\n", msg);
		fflush(f);
		fclose(f);
	}
#endif
}

// 未捕获异常 / 工作线程内异常都会走到 terminate，在这里记录后继续 abort
static void TerminateHandler() {
	WriteFatalMsg("=== std::terminate 被调用（未捕获异常，可能来自工作线程）===");
	if (std::current_exception()) {
		try {
			std::rethrow_exception(std::current_exception());
		}
		catch (const std::exception& e) {
			WriteFatalMsg(e.what());
		}
		catch (...) {
			WriteFatalMsg("未知异常类型");
		}
	}
	else {
		WriteFatalMsg("无当前异常信息（可能直接调用了 abort）");
	}
	std::abort();
}

#if defined(_WIN32)
int main(int argc, char** argv) {
    // 设置 Windows 控制台输出编码为 UTF-8
    SetConsoleOutputCP(CP_UTF8);
	std::set_terminate(TerminateHandler);
#elif defined(__ANDROID__)
extern "C" int pixelclean_main(int argc, char** argv) {
#endif
	LOGI("PixelClean starting...");
	Global::Read();
	LOGD("Global config loaded");
	TOOL::InitThreadPool();
	TOOL::InitPerlinNoise();
	TOOL::InitSpdLog();
	TOOL::InitTimer();
	LOGD("Tools initialized");
	//TOOL::InitLog();


	GAME::Application* app = nullptr;

	try {
		// 构造函数内部有大量 throw，必须放在 try 内，否则异常直接导致 terminate/abort
		app = new GAME::Application();

#if defined(_WIN32)
		app->run();
#elif defined(__ANDROID__)
		// Android: JNI 层按生命周期调用 initBeforeSurface() → initAfterSurface() → frameStep()
#endif
	}
	catch (const std::exception& e) {
		WriteFatalMsg(e.what());
		LOGE("main exception: %s", e.what());
		if (TOOL::Error) TOOL::Error->error(e.what());
	}
	catch (...) {
		WriteFatalMsg("main 未知异常");
		LOGE("main unknown exception");
	}

	LOGD("PixelClean shutting down...");
	delete app;

	TOOL::DeleteThreadPool();
	TOOL::DeletePerlinNoise();
	TOOL::DeleteSpdLog();
	//TOOL::DeleteLog();

	return 0;
}
