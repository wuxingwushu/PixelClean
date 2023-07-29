#pragma once
#include <string>

namespace Global {
	extern bool MultiplePeopleMode;//多人模式
	extern bool ServerOrClient;//服务器还是客户端

	/***************************	INI	***************************/
	void Read();//读取
	void Storage();//储存

	extern unsigned int mWidth;
	extern unsigned int mHeight;
	extern int ServerPort;
	extern int ClientPort;
	extern std::string ClientIP;
}