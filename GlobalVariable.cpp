#include "GlobalVariable.h"
#include "ini.h"
#include "FilePath.h"

namespace Global {
	bool* MainCommandBufferS = nullptr;
	bool MultiplePeopleMode = false;
	bool ServerOrClient;

	/***************************	INI	***************************/
	void Read() {
		inih::INIReader Ini{ IniPath };
		mWidth = Ini.Get<unsigned int>("Window", "Width");
		mHeight = Ini.Get<unsigned int>("Window", "Height");
		ServerPort = Ini.Get<int>("ServerTCP", "Port");
		ClientPort = Ini.Get<int>("ClientTCP", "Port");
		ClientIP = Ini.Get<std::string>("ClientTCP", "IP");
		VulKanValidationLayer = Ini.Get<bool>("Set", "VulKanValidationLayer");
		Monitor = Ini.Get<bool>("Set", "Monitor");
		FullScreen = Ini.Get<bool>("Set", "FullScreen");
	}

	void Storage() {
		inih::INIReader Ini{ IniPath };
		Ini.UpdateEntry("Window", "Width", mWidth);
		Ini.UpdateEntry("Window", "Height", mHeight);
		Ini.UpdateEntry("ServerTCP", "Port", ServerPort);
		Ini.UpdateEntry("ClientTCP", "Port", ClientPort);
		Ini.UpdateEntry("ClientTCP", "IP", ClientIP);
		Ini.UpdateEntry("Set", "VulKanValidationLayer", VulKanValidationLayer);
		Ini.UpdateEntry("Set", "Monitor", Monitor);
		Ini.UpdateEntry("Set", "FullScreen", FullScreen);
		inih::INIWriter::write_Gai(IniPath, Ini);//保存
	}

	unsigned int mWidth;
	unsigned int mHeight;

	int ServerPort;
	int ClientPort;
	std::string ClientIP;

	bool VulKanValidationLayer;
	bool Monitor;
	bool FullScreen;
}