#pragma once
#include "../base.h"
#include <iostream>
#include "event2/event.h"
#include "event2/listener.h"
#include "event2/bufferevent.h"
//#ifdef _WIN32
//#undef socklen_t
//#define _WINSOCKAPI_
//#include <winsock2.h> // 注意要包含Winsock2头文件
//#pragma comment(lib, "ws2_32.lib") // 链接Winsock库
//#else
//#include <signal.h>
//#endif


#include "../Tool/ContinuousMap.h"

#include "../Camera.h"

struct ServerPos {
	float X;
	float Y;
	float ang;
	bool Fire = false;
	evutil_socket_t Key;
};

extern event_base* server_base;
extern evconnlistener* server_ev;


extern ContinuousMap<evutil_socket_t, ServerPos> server;

//错误，超时 （连接断开会进入）
void server_event_cb(bufferevent* be, short events, void* arg);

//读事件
void server_read_cb(bufferevent* be, void* arg);

//写事件
void server_write_cb(bufferevent* be, void* arg);

//链接事件
void server_listen_cb(evconnlistener* ev, evutil_socket_t s, sockaddr* sin, int slen, void* arg);


void server_init(unsigned int Duan);

void server_delete();
