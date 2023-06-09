//客户端
#pragma once
#include "../base.h"
#include <iostream>
#include "event2/event.h"
#include "event2/listener.h"
#include "event2/bufferevent.h"
//#ifdef _WIN32
//#define _WINSOCKAPI_
//#include <winsock2.h> // 注意要包含Winsock2头文件
//#pragma comment(lib, "ws2_32.lib") // 链接Winsock库
//#else
//#include <signal.h>
//#endif


#include "../Tool/ContinuousMap.h"

#include "../Camera.h"

struct ClientPos {
	float X;
	float Y;
	float ang;
	bool Fire = false;
	evutil_socket_t Key;
};

extern bool client_Fire;

extern event_base* client_base;
extern bufferevent* client_bev;

extern ContinuousMap<evutil_socket_t, ClientPos> client;

//错误，超时 （连接断开会进入）
void client_event_cb(bufferevent* be, short events, void* arg);

void client_write_cb(bufferevent* be, void* arg);

void client_read_cb(bufferevent* be, void* arg);

void client_init(std::string IPV, unsigned int Duan);

void client_delete();

//进入事件帧循环
//event_base_loop(client_base, EVLOOP_ONCE);