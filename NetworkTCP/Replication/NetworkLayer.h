#pragma once
#include "ReplicationManager.h"
#include "../StructTCP.h"
#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <event2/listener.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

enum class NetworkRole {
	Server,
	Client
};

class NetworkLayer {
public:
	static NetworkLayer& Get();

	bool Initialize(NetworkRole role,
					const std::string& serverIP,
					int serverPort,
					int clientPort);

	void Shutdown();

	void RunLoop();

	void SendTo(evutil_socket_t targetFd, const uint8_t* data, size_t size);

	void SendWithHeader(evutil_socket_t targetFd, uint32_t key,
						const uint8_t* data, size_t size);

	event_base* GetEventBase() { return mBase; }

	const std::vector<evutil_socket_t>& GetPeerFds() const { return mPeerFds; }

private:
	NetworkLayer() = default;
	~NetworkLayer();

	static void OnClientEvent(bufferevent* be, short events, void* arg);
	static void OnClientWrite(bufferevent* be, void* arg);
	static void OnServerAccept(evconnlistener* listener, evutil_socket_t fd,
							   sockaddr* addr, int len, void* arg);
	static void OnPeerRead(bufferevent* be, void* arg);
	static void OnPeerEvent(bufferevent* be, short events, void* arg);
	static bufferevent* CreateFilteredBev(event_base* base, evutil_socket_t fd,
											  bufferevent_data_cb readCb,
											  bufferevent_data_cb writeCb,
											  bufferevent_event_cb eventCb,
											  void* cbArg);

	static void FreeZipContext(void* ctx);

	static bufferevent_filter_result FilterIn(
		evbuffer* src, evbuffer* dst,
		ev_ssize_t limit, bufferevent_flush_mode mode, void* arg);
	static bufferevent_filter_result FilterOut(
		evbuffer* src, evbuffer* dst,
		ev_ssize_t limit, bufferevent_flush_mode mode, void* arg);

	void HandleIncomingData(bufferevent* be);

	NetworkRole mRole = NetworkRole::Server;
	event_base* mBase = nullptr;
	bufferevent* mClientBev = nullptr;
	evconnlistener* mListener = nullptr;

	std::unordered_map<evutil_socket_t, bufferevent*> mServerBevs;
	std::vector<evutil_socket_t> mPeerFds;

	std::vector<uint8_t> mSendBuffer;

	struct ReadState {
		DataHeader header = {0, 0};
		std::vector<uint8_t> payload;
		size_t payloadRead = 0;
	};
	std::unordered_map<bufferevent*, ReadState> mReadStates;
};