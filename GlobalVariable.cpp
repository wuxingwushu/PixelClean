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
	std::atomic<bool>* MainCommandBufferS = nullptr;
void MainCommandBufferUpdateRequest() {
	for (size_t i = 0; i < CommandBufferSize; ++i)
	{
		MainCommandBufferS[i].store(true, std::memory_order_release);
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
	bool AndroidRequestMoveDown = false;
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
			{
				//用 int 中转：inih 对自定义枚举没有特化，直接传枚举会编译失败
				int vulkanDeviceMode = Ini.Get<int>("Set", "VulkanDeviceMode", (int)VulkanDeviceModeEnum::AutoBest);
				if (vulkanDeviceMode < (int)VulkanDeviceModeEnum::AutoBest || vulkanDeviceMode > (int)VulkanDeviceModeEnum::Specific) {
					vulkanDeviceMode = (int)VulkanDeviceModeEnum::AutoBest;
				}
				VulkanDeviceMode = (VulkanDeviceModeEnum)vulkanDeviceMode;
			}
			//指定设备（VulkanDeviceMode == Specific）时用哪一台；老配置文件里没有这个键，取默认空串
			VulkanDeviceName = Ini.Get<std::string>("Set", "VulkanDeviceName", "");
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
			VulkanDeviceMode = VulkanDeviceModeEnum::AutoBest;
			VulkanDeviceName.clear();
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
		Ini.UpdateEntry("Set", "VulkanDeviceMode", (int)VulkanDeviceMode);
		{
			//VulkanDeviceName 是后加的键：老配置文件里没有它，而 UpdateEntry 对不存在的键会抛异常，
			//所以先看键在不在，不在就用 InsertEntry 补上
			const std::set<std::string> setKeys = Ini.Keys("Set");
			if (setKeys.find("VulkanDeviceName") == setKeys.end()) {
				Ini.InsertEntry("Set", "VulkanDeviceName", VulkanDeviceName);
			}
			else {
				Ini.UpdateEntry("Set", "VulkanDeviceName", VulkanDeviceName);
			}
		}
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

	VulkanDeviceModeEnum VulkanDeviceMode = VulkanDeviceModeEnum::AutoBest;
	std::string VulkanDeviceName;
	std::vector<VulkanDeviceInfo> VulkanDetectedDevices;
	bool RunningOnSoftwareRenderer = false;
	std::string RunningDeviceName;
	std::string CpuSoftwareRenderReason = "未检测到显卡 Vulkan 驱动";

	unsigned char KeyW;
	unsigned char KeyS;
	unsigned char KeyA;
	unsigned char KeyD;
}