#pragma once
#include "application.h"
#include "Vulkan/Window.h"


int main(int argc, char** argv) {
	/*int koa[10] = { 0,1,2,3,4,5,6,7,8,9 };
	std::cout << koa << " - " << koa[0] << std::endl;
	std::cout << &koa[1] << " - " << koa[1] << std::endl;
	int* P = koa;
	std::cout << ++P << " - " << *P << std::endl;
	std::cout << ++P << " - " << *P << std::endl;
	std::cout << ++P << " - " << *P << std::endl;
	return 0;*/
	Global::Read();
	TOOL::InitThreadPool();
	TOOL::InitPerlinNoise();
	TOOL::InitSpdLog();
	TOOL::InitTimer();
	//TOOL::InitLog();


	GAME::Application* app = new GAME::Application();

	VulKan::Window* mWin = new VulKan::Window(Global::mWidth, Global::mHeight, false, Global::FullScreen);

	mWin->setApp(app);

	try {
		app->run(mWin);
	}
	catch (const std::exception& e) {
		//std::cout << "main: " << e.what() << std::endl;
		//log_error(e.what());
		TOOL::Error->error(e.what());
	}

	delete mWin;
	delete app;

	TOOL::DeleteThreadPool();
	TOOL::DeletePerlinNoise();
	TOOL::DeleteSpdLog();
	//TOOL::DeleteLog();

	return 0;
}