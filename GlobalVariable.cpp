#include "GlobalVariable.h"
#include "../DebugLog.h"
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
	unsigned int CommandBufferSize = 0;
	bool* MainCommandBufferS = nullptr;
	void MainCommandBufferUpdateRequest() {
		for (size_t i = 0; i < CommandBufferSize; ++i)
		{
			MainCommandBufferS[i] = true;
		}
	}

	bool MultiplePeopleMode = false;
	bool ServerOrClient = true;

	bool ClickWindow = false;

	TouchStateEnum TouchState = TouchStateEnum::None;

	// Android 虚拟按键
	bool AndroidRequestConsole = false;
	bool AndroidRequestESC = false;
	bool AndroidRequestKey1 = false;
	bool AndroidRequestKey2 = false;
	bool AndroidRequestSpace = false;
	bool AndroidRequestToggleMouse = false;
	bool AndroidKey_W = false;
	bool AndroidKey_S = false;
	bool AndroidKey_A = false;
	bool AndroidKey_D = false;

	/***************************	INI	***************************/
	void Read() {
		LOGI("Global::Read() starting");
		try {
			inih::INIReader Ini{ IniPath_ini };
			
			// 提供默认值，防止 Ini 读取失败导致崩溃
			mWidth = Ini.Get<unsigned int>("Window", "Width", 800);
			mHeight = Ini.Get<unsigned int>("Window", "Height", 600);
			ServerPort = Ini.Get<int>("ServerTCP", "Port", 8888);
			ClientPort = Ini.Get<int>("ClientTCP", "Port", 8888);
			ClientIP = Ini.Get<std::string>("ClientTCP", "IP", "127.0.0.1");
			VulKanValidationLayer = Ini.Get<bool>("Set", "VulKanValidationLayer", false);
			Monitor = Ini.Get<bool>("Set", "Monitor", false);
			MonitorCompatibleMode = Ini.Get<bool>("Set", "MonitorCompatibleMode", true);
			FullScreen = Ini.Get<bool>("Set", "FullScreen", false);
			MusicVolume = Ini.Get<float>("Set", "MusicVolume", 0.5f);
			SoundEffectsVolume = Ini.Get<float>("Set", "SoundEffectsVolume", 0.5f);
			FontZoomRatio = Ini.Get<float>("Set", "FontZoomRatio", 1.0f);
			KeyW = Ini.Get<unsigned char>("Key", "KeyW", 'W');
			KeyS = Ini.Get<unsigned char>("Key", "KeyS", 'S');
			KeyA = Ini.Get<unsigned char>("Key", "KeyA", 'A');
			KeyD = Ini.Get<unsigned char>("Key", "KeyD", 'D');
			LOGI("Global::Read() completed successfully");
		} catch (const std::exception& e) {
			LOGE("Global::Read() failed: %s", e.what());
			// 设置安全默认值
			mWidth = 800;
			mHeight = 600;
			ServerPort = 8888;
			ClientPort = 8888;
			ClientIP = "127.0.0.1";
			VulKanValidationLayer = false;
			Monitor = false;
			MonitorCompatibleMode = true;
			FullScreen = false;
			MusicVolume = 0.5f;
			SoundEffectsVolume = 0.5f;
			FontZoomRatio = 1.0f;
			KeyW = 'W';
			KeyS = 'S';
			KeyA = 'A';
			KeyD = 'D';
		}
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