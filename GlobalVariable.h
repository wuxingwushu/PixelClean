#pragma once
#include <string>
#include "GameMods/GameMods.h"

namespace Global {
	/*  测试  */

	extern bool DrawLinesMode;//画线模式
	extern bool MistSwitch;//是否开启迷雾

	extern int GamePlayerX;
	extern int GamePlayerY;

	extern bool GameResourceLoadingBool;//加载游戏资源开关
	extern bool GameResourceUninstallBool;//卸载游戏资源开关

	extern GameModsEnum GameMode;//游戏模式

	/*  测试  */

	extern bool ConsoleBool;
	extern unsigned int CommandBufferSize;
	extern bool* MainCommandBufferS;//需要更新MainCommandBuffer
	void MainCommandBufferUpdateRequest();//全部 MainCommandBuffer 需要更新;

	extern bool MultiplePeopleMode;	//多人模式
	extern bool ServerOrClient;		//服务器还是客户端

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

	//按键
	extern unsigned char KeyW;
	extern unsigned char KeyS;
	extern unsigned char KeyA;
	extern unsigned char KeyD;
}
