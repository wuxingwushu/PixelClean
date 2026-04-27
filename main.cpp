#include "application.h"
#include "Vulkan/Window.h"

#ifdef _WIN32
#include <Windows.h>
#endif

int main(int argc, char** argv) {
#ifdef _WIN32
    // 设置 Windows 控制台输出编码为 UTF-8
    SetConsoleOutputCP(CP_UTF8);
#endif
	Global::Read();
	TOOL::InitThreadPool();
	TOOL::InitPerlinNoise();
	TOOL::InitSpdLog();
	TOOL::InitTimer();
	//TOOL::InitLog();


	GAME::Application* app = new GAME::Application();

	try {
		app->run();
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