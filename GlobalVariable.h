#pragma once
#include <atomic>
#include <string>
#include <vector>
#include "GameMods/GameMods.h"

namespace Global {
	//Vulkan 渲染设备的选择。它同时决定"创建 VkInstance 时用哪个 ICD"和"最终用哪台物理设备"，
	//所以切换后必须重启程序才生效。
	//0/1/3 都要求"只要有硬件设备就绝不用 CPU 软件设备"（否则就会出现"选了显卡却还是 CPU 渲染"）。
	enum class VulkanDeviceModeEnum {
		AutoBest = 0,	//自动选择最高性能：在识别到的设备里挑评分最高的（有显卡时就是它）
		AutoWorst = 1,	//自动选择最低性能：挑评分最低的（省电/兼容性测试用）；但有硬件设备时仍然不会选 CPU 软件设备
		CPU = 2,		//只用 CPU 软件渲染(SwiftShader)
		Specific = 3,	//指定设备：用 VulkanDeviceName 那一台（按名字匹配，见 Vulkan/device.cpp）
	};

	/*  测试  */

	extern bool DrawLinesMode;//画线模式
	extern bool MistSwitch;//是否开启迷雾

	extern float GamePlayerX;
	extern float GamePlayerY;

	extern bool GameResourceLoadingBool;//加载游戏资源开关
	extern bool GameResourceUninstallBool;//卸载游戏资源开关

	extern GameModsEnum GameMode;//游戏模式

	/*  测试  */

	extern bool ConsoleBool;
	extern unsigned int CommandBufferSize;
	extern std::atomic<bool>* MainCommandBufferS;//需要更新MainCommandBuffer（原子操作保证多线程安全）
void MainCommandBufferUpdateRequest();//全部 MainCommandBuffer 需要更新;

	extern bool MultiplePeopleMode;	//多人模式
	extern bool ServerOrClient;		//服务器还是客户端

	extern bool ClickWindow;// 鼠标是否 是点击窗口

	extern TouchStateEnum TouchState;// Android 触摸状态

	// Android 虚拟按键（由 JNI 层设置）
	extern bool AndroidRequestConsole;		// 请求打开控制台 (/)
	extern bool AndroidRequestESC;			// 请求 ESC
	extern bool AndroidRequestKey1;			// 请求按键 1
	extern bool AndroidRequestKey2;			// 请求按键 2
	extern bool AndroidRequestSpace;		// 请求空格
	extern bool AndroidRequestToggleMouse;	// 请求切换鼠标模式 (~)
	extern bool AndroidRequestMoveDown;		// 请求下降 (Shift)
	extern bool AndroidKey_W;				// W 键按下状态
	extern bool AndroidKey_S;				// S 键按下状态
	extern bool AndroidKey_A;				// A 键按下状态
	extern bool AndroidKey_D;				// D 键按下状态

	/***************************	INI	***************************/
	void Read();//读取
	void Storage();//储存

	//窗口大小
	extern unsigned int mWidth;		//宽
	extern unsigned int mHeight;	//高

	//TCP
	extern int ServerPort;			//服务器 端口
	extern int ClientPort;			//客户端链接服务器 端口
	extern std::string ClientIP;	//客户端链接服务器 IP

	//设置
	extern bool VulKanValidationLayer;	//VulKan 验证层 的 开关
	extern bool Monitor;				//监视器
	extern bool MonitorCompatibleMode;	//监视器兼容模式
	extern bool FullScreen;				//全屏
	extern float MusicVolume;			//音乐音量
	extern float SoundEffectsVolume;	//音效音量
	extern float FontZoomRatio;			//字体缩放比
	extern VulkanDeviceModeEnum VulkanDeviceMode;	//渲染设备：自动最高性能/自动最低性能/CPU 软件渲染/指定设备
	extern std::string VulkanDeviceName;			//VulkanDeviceMode == Specific 时要用的设备名

	//机器上被 Vulkan loader 识别到的硬件设备（由 Vulkan/instance.cpp 在创建真实 VkInstance 之前探测并缓存）。
	//设置界面用它在"渲染设备"下拉框里列出"识别到的显卡"；name 与 VkPhysicalDeviceProperties::deviceName
	//完全一致，Vulkan/device.cpp 靠它匹配用户指定的那一台设备。
	struct VulkanDeviceInfo {
		std::string name;		//VkPhysicalDeviceProperties::deviceName
		int deviceType = 0;		//VkPhysicalDeviceType（1=集成显卡 2=独立显卡 3=虚拟显卡 4=CPU 软件设备 0=其它）
		bool usable = true;		//是否满足本程序的最低要求（不满足时界面会标注，选中它不会启动）
	};
	extern std::vector<VulkanDeviceInfo> VulkanDetectedDevices;	//启动时探测到的硬件设备（不含 CPU 软件设备）

	//当前这次运行实际用的是不是 CPU 软件渲染（由 Vulkan/device.cpp 在选中物理设备后填写，
	//用于在界面左上角提醒用户"现在是 CPU 渲染，很慢"）
	extern bool RunningOnSoftwareRenderer;
	extern std::string RunningDeviceName;	//当前实际使用的设备名（诊断/界面提示用）

	//走到 CPU 软件渲染的原因（由 Vulkan/instance.cpp 在自动切换 ICD 时填写）。
	//界面左上角的橙字提醒会原样带上它，让用户一眼区分"根本没显卡驱动"和"有显卡但都不能用"。
	extern std::string CpuSoftwareRenderReason;

	//当前设置是不是"只用 CPU 软件渲染"（避免各处直接比较枚举）
	inline bool IsCpuRenderingMode() noexcept { return VulkanDeviceMode == VulkanDeviceModeEnum::CPU; }

	//当前设置是不是"指定了某一台设备"
	inline bool IsSpecificDeviceMode() noexcept { return VulkanDeviceMode == VulkanDeviceModeEnum::Specific; }

	//按键
	extern unsigned char KeyW;
	extern unsigned char KeyS;
	extern unsigned char KeyA;
	extern unsigned char KeyD;
}
