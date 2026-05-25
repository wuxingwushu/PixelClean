#include "NetworkLayer.h"
#include "../DebugLog.h"
#include <iostream>
#include <chrono>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <signal.h>
#endif

NetworkLayer& NetworkLayer::Get() {
	static NetworkLayer instance;
	return instance;
}

NetworkLayer::~NetworkLayer() {
	Shutdown();
}

bool NetworkLayer::Initialize(NetworkRole role,
							   const std::string& serverIP,
							   int serverPort,
							   int clientPort) {
	mRole = role;

#if defined(_WIN32)
	WSADATA wsa;
	WSAStartup(MAKEWORD(2, 2), &wsa);
#elif defined(__ANDROID__)
	if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
		LOGE("[NetworkLayer] signal fail");
	}
#else
	if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
		return false;
#endif

	mBase = event_base_new();

	if (mRole == NetworkRole::Server) {
		sockaddr_in sin;
		memset(&sin, 0, sizeof(sin));
		sin.sin_family = AF_INET;
		sin.sin_port = htons(serverPort);

		mListener = evconnlistener_new_bind(
			mBase,
			OnServerAccept,
			this,
			LEV_OPT_REUSEABLE | LEV_OPT_CLOSE_ON_FREE,
			10,
			(sockaddr*)&sin,
			sizeof(sin)
		);

		if (!mListener) {
			LOGE("[NetworkLayer] Failed to create listener");
			return false;
		}

		std::cout << "[NetworkLayer] Server listening on port " << serverPort << std::endl;
	}
	else {
		mClientBev = bufferevent_socket_new(mBase, -1, BEV_OPT_CLOSE_ON_FREE);

		sockaddr_in sin;
		memset(&sin, 0, sizeof(sin));
		sin.sin_family = AF_INET;
		sin.sin_port = htons(clientPort);
		evutil_inet_pton(AF_INET, serverIP.c_str(), &sin.sin_addr.s_addr);

		bufferevent_setcb(mClientBev, nullptr, nullptr, OnClientEvent, this);
		bufferevent_enable(mClientBev, EV_READ | EV_WRITE);

		int re = bufferevent_socket_connect(mClientBev, (sockaddr*)&sin, sizeof(sin));
		if (re == 0) {
			std::cout << "[NetworkLayer] Client connecting to " << serverIP << ":" << clientPort << std::endl;
		} else {
			LOGE("[NetworkLayer] Client connect failed");
			return false;
		}
	}

	return true;
}

void NetworkLayer::Shutdown() {
	mPeerFds.clear();
	mServerBevs.clear();
	mReadStates.clear();

	if (mListener) {
		evconnlistener_free(mListener);
		mListener = nullptr;
	}

	if (mClientBev) {
		bufferevent_free(mClientBev);
		mClientBev = nullptr;
	}

	if (mBase) {
		event_base_free(mBase);
		mBase = nullptr;
	}

#if defined(_WIN32)
	WSACleanup();
#endif
}

void NetworkLayer::RunLoop() {
	if (!mBase) return;

	if (mRole == NetworkRole::Server) {
		event_base_loop(mBase, EVLOOP_NONBLOCK);
	} else {
		event_base_loop(mBase, EVLOOP_ONCE);
	}
}

void NetworkLayer::SendTo(evutil_socket_t targetFd, const uint8_t* data, size_t size) {
	SendWithHeader(targetFd, 0, data, size);
}

void NetworkLayer::SendWithHeader(evutil_socket_t targetFd, uint32_t key,
								   const uint8_t* data, size_t size) {
	DataHeader header;
	header.Key = key;
	header.Size = static_cast<unsigned int>(size);

	if (targetFd == -1) {
		if (mRole == NetworkRole::Client && mClientBev) {
			bufferevent_write(mClientBev, &header, sizeof(DataHeader));
			if (size > 0) {
				bufferevent_write(mClientBev, data, size);
			}
		} else {
			for (evutil_socket_t fd : mPeerFds) {
				auto it = mServerBevs.find(fd);
				if (it != mServerBevs.end()) {
					bufferevent_write(it->second, &header, sizeof(DataHeader));
					if (size > 0) {
						bufferevent_write(it->second, data, size);
					}
				}
			}
		}
	} else {
		bufferevent* bev = nullptr;
		if (mRole == NetworkRole::Client) {
			bev = mClientBev;
		} else {
			auto it = mServerBevs.find(targetFd);
			if (it != mServerBevs.end()) {
				bev = it->second;
			}
		}

		if (bev) {
			bufferevent_write(bev, &header, sizeof(DataHeader));
			if (size > 0) {
				bufferevent_write(bev, data, size);
			}
		}
	}
}

bufferevent* NetworkLayer::CreateFilteredBev(event_base* base, evutil_socket_t fd,
											  bufferevent_data_cb readCb,
											  bufferevent_data_cb writeCb,
											  bufferevent_event_cb eventCb,
											  void* cbArg) {
	bufferevent* bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE);

	Zip* zip = new Zip();

	bufferevent* bev_filter = bufferevent_filter_new(bev,
		FilterIn,
		FilterOut,
		BEV_OPT_CLOSE_ON_FREE,
		FreeZipContext,
		zip
	);

	timeval t1 = { 3, 0 };
	bufferevent_set_timeouts(bev_filter, &t1, nullptr);

	bufferevent_setcb(bev_filter, readCb, writeCb, eventCb, cbArg);
	bufferevent_enable(bev_filter, EV_READ | EV_WRITE);

	return bev_filter;
}

void NetworkLayer::OnServerAccept(evconnlistener* listener, evutil_socket_t fd,
								   sockaddr* addr, int len, void* arg) {
	NetworkLayer* self = static_cast<NetworkLayer*>(arg);
	std::cout << "[NetworkLayer] New connection: " << fd << std::endl;

	self->mPeerFds.push_back(fd);

	bufferevent* filteredBev = CreateFilteredBev(self->mBase, fd,
												  OnPeerRead, nullptr, OnPeerEvent, self);
	self->mServerBevs[fd] = filteredBev;

	self->mReadStates[filteredBev] = ReadState{};

	auto& repMgr = ReplicationManager::Get();
	evutil_socket_t remoteId = static_cast<evutil_socket_t>(1);
	repMgr.RegisterRemoteObject(remoteId, remoteId);
	repMgr.FullSyncTo(fd);
}

void NetworkLayer::OnPeerRead(bufferevent* be, void* arg) {
	NetworkLayer* self = static_cast<NetworkLayer*>(arg);
	self->HandleIncomingData(be);
}

void NetworkLayer::OnPeerEvent(bufferevent* be, short events, void* arg) {
	NetworkLayer* self = static_cast<NetworkLayer*>(arg);
	std::cout << "[NetworkLayer Event]" << std::flush;

	if (events & BEV_EVENT_TIMEOUT && events & BEV_EVENT_READING) {
		std::cout << " TIMEOUT_READING" << std::endl;
		evutil_socket_t fd = bufferevent_getfd(be);
		ReplicationManager::Get().UnregisterObject(fd);
		self->mServerBevs.erase(fd);
		self->mReadStates.erase(be);
		auto& fds = self->mPeerFds;
		fds.erase(std::remove(fds.begin(), fds.end(), fd), fds.end());
		bufferevent_free(be);
		return;
	}

	if (events & BEV_EVENT_ERROR) {
		std::cout << " ERROR" << std::endl;
		evutil_socket_t fd = bufferevent_getfd(be);
		ReplicationManager::Get().UnregisterObject(fd);
		self->mServerBevs.erase(fd);
		self->mReadStates.erase(be);
		auto& fds = self->mPeerFds;
		fds.erase(std::remove(fds.begin(), fds.end(), fd), fds.end());
		bufferevent_free(be);
		return;
	}

	if (events & BEV_EVENT_EOF) {
		std::cout << " EOF" << std::endl;
		evutil_socket_t fd = bufferevent_getfd(be);
		ReplicationManager::Get().UnregisterObject(fd);
		self->mServerBevs.erase(fd);
		self->mReadStates.erase(be);
		auto& fds = self->mPeerFds;
		fds.erase(std::remove(fds.begin(), fds.end(), fd), fds.end());
		bufferevent_free(be);
		return;
	}
}

void NetworkLayer::OnClientEvent(bufferevent* be, short events, void* arg) {
	NetworkLayer* self = static_cast<NetworkLayer*>(arg);
	std::cout << "[NetworkLayer ClientEvent]" << std::flush;

	if (events & BEV_EVENT_CONNECTED) {
		std::cout << " CONNECTED" << std::endl;

		Zip* zip = new Zip();
		bufferevent* filteredBev = bufferevent_filter_new(be,
			FilterIn, FilterOut,
			BEV_OPT_CLOSE_ON_FREE,
			FreeZipContext, zip);

		bufferevent_setcb(filteredBev, OnPeerRead, OnClientWrite, OnClientEvent, self);
		bufferevent_enable(filteredBev, EV_READ | EV_WRITE);

		self->mClientBev = filteredBev;
		self->mReadStates[filteredBev] = ReadState{};
		return;
	}

	if (events & (BEV_EVENT_ERROR | BEV_EVENT_EOF)) {
		std::cout << " Error/EOF" << std::endl;
		if (self->mClientBev) {
			self->mReadStates.erase(self->mClientBev);
			bufferevent_free(self->mClientBev);
			self->mClientBev = nullptr;
		}
	}
}

void NetworkLayer::OnClientWrite(bufferevent* be, void* arg) {}

void NetworkLayer::HandleIncomingData(bufferevent* be) {
	ReadState& state = mReadStates[be];
	struct evbuffer* input = bufferevent_get_input(be);

	while (evbuffer_get_length(input) > 0) {
		if (state.header.Size == 0) {
			if (evbuffer_get_length(input) < sizeof(DataHeader)) {
				return;
			}
			bufferevent_read(be, &state.header, sizeof(DataHeader));
			state.payload.resize(state.header.Size);
			state.payloadRead = 0;
		}

		int remaining = static_cast<int>(state.header.Size - state.payloadRead);
		int len = bufferevent_read(be, state.payload.data() + state.payloadRead, remaining);
		if (len <= 0) return;

		state.payloadRead += len;

		if (state.payloadRead >= state.header.Size) {
			uint32_t key = state.header.Key;
			ByteReader reader(state.payload.data(), state.payload.size());

			if (key == 0) {
				ReplicationManager::Get().ApplyReplication(
					bufferevent_getfd(be), reader);
			} else if (key == 1) {
				uint16_t eventCount;
				reader.ReadU16(eventCount);
				for (uint16_t i = 0; i < eventCount; ++i) {
					uint16_t eventTypeId;
					reader.ReadU16(eventTypeId);
					ReplicationManager::Get().DispatchEvent(
						bufferevent_getfd(be), eventTypeId, reader);
				}
			}

			state.header.Size = 0;
			state.payload.clear();
			state.payloadRead = 0;
		}
	}
}

bufferevent_filter_result NetworkLayer::FilterIn(
	evbuffer* src, evbuffer* dst,
	ev_ssize_t limit, bufferevent_flush_mode mode, void* arg) {
	Zip* z = (Zip*)arg;
	z_stream* p = z->j;

	if (!p) return BEV_ERROR;

	evbuffer_iovec v_in[1];
	int n = evbuffer_peek(src, -1, NULL, v_in, 1);
	if (n <= 0) return BEV_NEED_MORE;

	p->avail_in = static_cast<uInt>(v_in[0].iov_len);
	p->next_in = (Byte*)v_in[0].iov_base;

	evbuffer_iovec v_out[1];
	evbuffer_reserve_space(dst, 4096, v_out, 1);

	p->avail_out = static_cast<uInt>(v_out[0].iov_len);
	p->next_out = (Byte*)v_out[0].iov_base;

	int re = inflate(p, Z_SYNC_FLUSH);
	if (re != Z_OK) {
		std::cerr << "[NetworkLayer] inflate failed!" << std::endl;
	}

	int nread = static_cast<int>(v_in[0].iov_len - p->avail_in);
	int nwrite = static_cast<int>(v_out[0].iov_len - p->avail_out);

	evbuffer_drain(src, nread);
	v_out[0].iov_len = nwrite;
	evbuffer_commit_space(dst, v_out, 1);

	return BEV_OK;
}

void NetworkLayer::FreeZipContext(void* ctx) {
	delete static_cast<Zip*>(ctx);
}

bufferevent_filter_result NetworkLayer::FilterOut(
	evbuffer* src, evbuffer* dst,
	ev_ssize_t limit, bufferevent_flush_mode mode, void* arg) {
	Zip* z = (Zip*)arg;
	z_stream* p = z->y;

	if (!p) return BEV_ERROR;

	evbuffer_iovec v_in[1];
	int n = evbuffer_peek(src, -1, 0, v_in, 1);
	if (n <= 0) return BEV_NEED_MORE;

	p->avail_in = static_cast<uInt>(v_in[0].iov_len);
	p->next_in = (Byte*)v_in[0].iov_base;

	evbuffer_iovec v_out[1];
	evbuffer_reserve_space(dst, 4096, v_out, 1);

	p->avail_out = static_cast<uInt>(v_out[0].iov_len);
	p->next_out = (Byte*)v_out[0].iov_base;

	int re = deflate(p, Z_SYNC_FLUSH);
	if (re != Z_OK) {
		LOGE("[NetworkLayer] deflate failed");
	}

	int nread = static_cast<int>(v_in[0].iov_len - p->avail_in);
	int nwrite = static_cast<int>(v_out[0].iov_len - p->avail_out);

	evbuffer_drain(src, nread);
	v_out[0].iov_len = nwrite;
	evbuffer_commit_space(dst, v_out, 1);

	return BEV_OK;
}