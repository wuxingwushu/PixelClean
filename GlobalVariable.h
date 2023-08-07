#pragma once
#include <string>

namespace Global {
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
}
