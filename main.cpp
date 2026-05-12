#include "application.h"
#include "Vulkan/Window.h"
#include "../DebugLog.h"

#if defined(_WIN32)
#include <Windows.h>
#endif

#if defined(_WIN32)
int main(int argc, char** argv) {
    // 设置 Windows 控制台输出编码为 UTF-8
    SetConsoleOutputCP(CP_UTF8);
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


	GAME::Application* app = new GAME::Application();

	try {
#if defined(_WIN32)
		app->run();
#elif defined(__ANDROID__)
		// Android: JNI 层按生命周期调用 initBeforeSurface() → initAfterSurface() → frameStep()
#endif
	}
	catch (const std::exception& e) {
		LOGE("main exception: %s", e.what());
		if (TOOL::Error) TOOL::Error->error(e.what());
	}
	catch (...) {
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