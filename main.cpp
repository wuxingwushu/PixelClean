#include "application.h"
#include "Vulkan/Window.h"

int main(int argc, char** argv) {
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