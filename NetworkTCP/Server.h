#pragma once
#include "../base.h"
#include <iostream>
#include "event2/event.h"
#include "event2/listener.h"
#include "event2/bufferevent.h"
#include "event2/buffer.h"
#include "zlib/zlib.h"
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

#include "../Character/Crowd.h"
#include "../Arms/Arms.h"

struct ServerPos {
	float X;
	float Y;
	float ang;
	bool Fire = false;
	evutil_socket_t Key;
};


//错误，超时 （连接断开会进入）
void server_event_cb(bufferevent* be, short events, void* arg);

//读事件
void server_read_cb(bufferevent* be, void* arg);

//写事件
void server_write_cb(bufferevent* be, void* arg);

//链接事件
void server_listen_cb(evconnlistener* ev, evutil_socket_t s, sockaddr* sin, int slen, void* arg);

//输入过滤器
bufferevent_filter_result server_filter_in(
	evbuffer* s,
	evbuffer* d,
	ev_ssize_t limit,
	bufferevent_flush_mode mode,
	void* arg
);

//输出过滤器
bufferevent_filter_result server_filter_out(
	evbuffer* s,
	evbuffer* d,
	ev_ssize_t limit,
	bufferevent_flush_mode mode,
	void* arg
);


class server
{
public:
	//单列模式
	static server* GetServer() {
		if (mServer == nullptr) {
			std::cout << "创建server" << std::endl;
			mServer = new server(25565);//端口号
		}
		return mServer;
	}

	~server();

	[[nodiscard]] ContinuousMap<evutil_socket_t, ServerPos>* GetServerData() const noexcept {
		return mServerData;
	}

	[[nodiscard]] event_base* GetEvent_Base() const noexcept {
		return server_base;
	}

	[[nodiscard]] evconnlistener* GetEvconnlistener() const noexcept {
		return server_ev;
	}

	[[nodiscard]] bool GetFire() const noexcept {
		return server_Fire;
	}

	void SetFire(bool Fire) noexcept {
		server_Fire = Fire;
	}

	void SetArms(GAME::Arms* Arms) {
		mArms = Arms;
	}

	void SetCrowd(GAME::Crowd* Crowd) {
		mCrowd = Crowd;
	}

	GAME::Arms* GetArms() {
		return mArms;
	}

	GAME::Crowd* GetCrowd() {
		return mCrowd;
	}

private:
	static server* mServer;
	server(unsigned int Duan);

	bool server_Fire = false;
	event_base* server_base;
	evconnlistener* server_ev;
	ContinuousMap<evutil_socket_t, ServerPos>* mServerData;

	GAME::Arms* mArms = nullptr;
	GAME::Crowd* mCrowd = nullptr;
};
