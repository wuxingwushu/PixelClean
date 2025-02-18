//客户端
#pragma once
#include "../base.h"
#include <iostream>
#include "event2/event.h"
#include "event2/listener.h"
#include "event2/bufferevent.h"
#include "event2/buffer.h"
#include "zlib/zlib.h"
//#ifdef _WIN32
//#define _WINSOCKAPI_
//#include <winsock2.h> // 注意要包含Winsock2头文件
//#pragma comment(lib, "ws2_32.lib") // 链接Winsock库
//#else
//#include <signal.h>
//#endif


#include "../Tool/ContinuousMap.h"

#include "../Camera.h"

#include "ClientSynchronizeEvents.h"

#include "StructTCP.h"
#include "Struct.h"

#include "../GlobalVariable.h"


//错误，超时 （连接断开会进入）
void client_event_cb(bufferevent* be, short events, void* arg);

//写事件
void client_write_cb(bufferevent* be, void* arg);

//读事件
void client_read_cb(bufferevent* be, void* arg);

//输入过滤器
bufferevent_filter_result client_filter_in(
	evbuffer* s,
	evbuffer* d,
	ev_ssize_t limit,
	bufferevent_flush_mode mode,
	void* arg
);

//输出过滤器
bufferevent_filter_result client_filter_out(
	evbuffer* s,
	evbuffer* d,
	ev_ssize_t limit,
	bufferevent_flush_mode mode,
	void* arg
);

class client :public SynchronizeClass
{
public:
	static client* GetClient() {
		if (mClient == nullptr) {
			std::cout << "创建Client  IP: " << Global::ClientIP << "   Port: " << Global::ClientPort << std::endl;
			mClient = new client(Global::ClientIP, Global::ClientPort);
		}
		return mClient;
	}
	
	~client();

	[[nodiscard]] ContinuousMap<evutil_socket_t, RoleSynchronizationData>* GetClientData() const noexcept {
		return mClientData;
	}

	[[nodiscard]] event_base* GetEvent_Base() const noexcept {
		return client_base;
	}

	[[nodiscard]] bufferevent* GetBufferEvent() const noexcept {
		return client_bev;
	}

	void InitSynchronizeMap();

	bool InitbuffereventTO = true;//初始化信息

	evutil_socket_t fd;
private:
	static client* mClient;
	client(std::string IPV, unsigned int Duan);

	event_base* client_base;
	bufferevent* client_bev = nullptr;
	ContinuousMap<evutil_socket_t, RoleSynchronizationData>* mClientData;

	
};

//进入事件帧循环
//event_base_loop(client_base, EVLOOP_ONCE);