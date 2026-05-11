#include "application.h"
#include "Vulkan/Window.h"

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
	Global::Read();
	TOOL::InitThreadPool();
	TOOL::InitPerlinNoise();
	TOOL::InitSpdLog();
	TOOL::InitTimer();
	//TOOL::InitLog();


	GAME::Application* app = new GAME::Application();

	try {
#if defined(_WIN32)
		app->run();
#elif defined(__ANDROID__)
		// Android: Surface 创建后由 JNI 层调用 app->runWithoutWindow()
#endif
	}
	catch (const std::exception& e) {
		std::cout << "main: " << e.what() << std::endl;
		//log_error(e.what());
		TOOL::Error->error(e.what());
	}

	delete app;

	TOOL::DeleteThreadPool();
	TOOL::DeletePerlinNoise();
	TOOL::DeleteSpdLog();
	//TOOL::DeleteLog();

	return 0;
}