#include "GlobalVariable.h"
#include "ini.h"
#include "FilePath.h"

namespace Global {
	/*  测试  */

	bool DrawLinesMode = false;
	bool MistSwitch = true;

	float GamePlayerX;   
	float GamePlayerY;

	bool GameResourceLoadingBool = false;
	bool GameResourceUninstallBool = false;

	GameModsEnum GameMode;

	/*  测试  */

	bool ConsoleBool = false;
	extern unsigned int CommandBufferSize = 0;
	bool* MainCommandBufferS = nullptr;
	void MainCommandBufferUpdateRequest() {
		for (size_t i = 0; i < CommandBufferSize; ++i)
		{
			MainCommandBufferS[i] = true;
		}
	}

	bool MultiplePeopleMode = false;
	bool ServerOrClient = true;

	/***************************	INI	***************************/
	void Read() {
		inih::INIReader Ini{ IniPath_ini };
		mWidth = Ini.Get<unsigned int>("Window", "Width");
		mHeight = Ini.Get<unsigned int>("Window", "Height");
		ServerPort = Ini.Get<int>("ServerTCP", "Port");
		ClientPort = Ini.Get<int>("ClientTCP", "Port");
		ClientIP = Ini.Get<std::string>("ClientTCP", "IP");
		VulKanValidationLayer = Ini.Get<bool>("Set", "VulKanValidationLayer");
		Monitor = Ini.Get<bool>("Set", "Monitor");
		MonitorCompatibleMode = Ini.Get<bool>("Set", "MonitorCompatibleMode");
		FullScreen = Ini.Get<bool>("Set", "FullScreen");
		MusicVolume = Ini.Get<float>("Set", "MusicVolume");
		SoundEffectsVolume = Ini.Get<float>("Set", "SoundEffectsVolume");
		FontZoomRatio = Ini.Get<float>("Set", "FontZoomRatio");
		KeyW = Ini.Get<unsigned char>("Key", "KeyW");
		KeyS = Ini.Get<unsigned char>("Key", "KeyS");
		KeyA = Ini.Get<unsigned char>("Key", "KeyA");
		KeyD = Ini.Get<unsigned char>("Key", "KeyD");
	}

	void Storage() {
		inih::INIReader Ini{ IniPath_ini };
		Ini.UpdateEntry("Window", "Width", mWidth);
		Ini.UpdateEntry("Window", "Height", mHeight);
		Ini.UpdateEntry("ServerTCP", "Port", ServerPort);
		Ini.UpdateEntry("ClientTCP", "Port", ClientPort);
		Ini.UpdateEntry("ClientTCP", "IP", ClientIP);
		Ini.UpdateEntry("Set", "VulKanValidationLayer", VulKanValidationLayer);
		Ini.UpdateEntry("Set", "Monitor", Monitor);
		Ini.UpdateEntry("Set", "MonitorCompatibleMode", MonitorCompatibleMode);
		Ini.UpdateEntry("Set", "FullScreen", FullScreen);
		Ini.UpdateEntry("Set", "MusicVolume", MusicVolume);
		Ini.UpdateEntry("Set", "SoundEffectsVolume", SoundEffectsVolume);
		Ini.UpdateEntry("Set", "FontZoomRatio", FontZoomRatio);
		Ini.UpdateEntry("Key", "KeyW", KeyW);
		Ini.UpdateEntry("Key", "KeyS", KeyS);
		Ini.UpdateEntry("Key", "KeyA", KeyA);
		Ini.UpdateEntry("Key", "KeyD", KeyD);
		inih::INIWriter::write_Gai(IniPath_ini, Ini);//保存
	}

	unsigned int mWidth;
	unsigned int mHeight;

	int ServerPort;
	int ClientPort;
	std::string ClientIP;

	bool VulKanValidationLayer;
	bool Monitor;
	bool MonitorCompatibleMode;
	bool FullScreen;
	float MusicVolume;
	float SoundEffectsVolume;
	float FontZoomRatio;

	unsigned char KeyW;
	unsigned char KeyS;
	unsigned char KeyA;
	unsigned char KeyD;
}