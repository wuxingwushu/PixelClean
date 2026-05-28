#include "NetworkLayer.h"
#include "../DebugLog.h"
#include <iostream>
#include <chrono>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <signal.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif

// 单例访问
NetworkLayer& NetworkLayer::Get() {
	static NetworkLayer instance;
	return instance;
}

NetworkLayer::~NetworkLayer() {
	Shutdown();
}

// Initialize: 初始化网络层
// 服务端：创建监听 socket，等待客户端连接
// 客户端：创建 socket 并连接到服务端，初始时不设读/写回调（CONNECTED 后再通过过滤器包装）
bool NetworkLayer::Initialize(NetworkRole role,
							   const std::string& serverIP,
							   int serverPort,
							   int clientPort) {
	mRole = role;

#if defined(_WIN32)
	// Windows 需要初始化 Winsock2
	WSADATA wsa;
	WSAStartup(MAKEWORD(2, 2), &wsa);
#elif defined(__ANDROID__)
	// Android 忽略 SIGPIPE 信号（防止写关闭的 socket 导致进程崩溃）
	if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
		LOGE("[NetworkLayer] signal fail");
	}
#else
	if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
		return false;
#endif

	// 创建 libevent 事件循环基础对象
	mBase = event_base_new();

	if (mRole == NetworkRole::Server) {
		// === 服务端模式 ===
		// 创建监听 socket，绑定到指定端口
		sockaddr_in sin;
		memset(&sin, 0, sizeof(sin));
		sin.sin_family = AF_INET;
		sin.sin_port = htons(serverPort);

		// evconnlistener_new_bind：创建并绑定监听器
		// OnServerAccept：新连接回调
		// LEV_OPT_REUSEABLE：端口复用
		// LEV_OPT_CLOSE_ON_FREE：释放时自动关闭
		// 10：backlog（等待队列长度）
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
		// === 客户端模式 ===
		// 创建客户端 bufferevent（初始不绑定读写回调，等连接成功后再设置）
		mClientBev = bufferevent_socket_new(mBase, -1, BEV_OPT_CLOSE_ON_FREE);

		sockaddr_in sin;
		memset(&sin, 0, sizeof(sin));
		sin.sin_family = AF_INET;
		sin.sin_port = htons(clientPort);
		evutil_inet_pton(AF_INET, serverIP.c_str(), &sin.sin_addr.s_addr);

		// 初始只设置事件回调（用于监听 CONNECTED 事件），读/写回调为空
		// 连接成功后 OnClientEvent 会创建压缩过滤器并替换 mClientBev
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

// Shutdown: 清理所有网络资源
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

// RunLoop: 每帧运行网络事件循环
// 服务端：非阻塞模式（EVLOOP_NONBLOCK），处理当前所有就绪事件后立即返回
// 客户端：单次模式（EVLOOP_ONCE），处理一个事件后返回
void NetworkLayer::RunLoop() {
	if (!mBase) return;

	if (mRole == NetworkRole::Server) {
		event_base_loop(mBase, EVLOOP_NONBLOCK);
	} else {
		event_base_loop(mBase, EVLOOP_ONCE);
	}
}

// SendTo: 发送状态复制数据（Key=0）
void NetworkLayer::SendTo(evutil_socket_t targetFd, const uint8_t* data, size_t size) {
	SendWithHeader(targetFd, 0, data, size);
}

// SendWithHeader: 发送带 DataHeader 的数据
// targetFd = -1 时的广播逻辑：
//   客户端：发送到唯一的服务端连接（mClientBev）
//   服务端：遍历所有 peer fd 发送到每个已连接的客户端
// targetFd != -1 时的单播：
//   客户端：发送到 mClientBev
//   服务端：按 fd 查找对应的 mServerBevs 条目并发送
void NetworkLayer::SendWithHeader(evutil_socket_t targetFd, uint32_t key,
								   const uint8_t* data, size_t size) {
	// 构造包头
	DataHeader header;
	header.Key = key;
	header.Size = static_cast<unsigned int>(size);

	if (targetFd == -1) {
		// === 广播模式 ===
		if (mRole == NetworkRole::Client && mClientBev) {
			// 客户端广播 = 发送给服务端
			bufferevent_write(mClientBev, &header, sizeof(DataHeader));
			if (size > 0) {
				bufferevent_write(mClientBev, data, size);
			}
		} else {
			// 服务端广播 = 发送给所有已连接的客户端
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
		// === 单播模式 ===
		bufferevent* bev = nullptr;
		if (mRole == NetworkRole::Client) {
			// 客户端单播：只能发给服务端
			bev = mClientBev;
		} else {
			// 服务端单播：发给指定 fd 的客户端
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

// CreateFilteredBev: 创建带 zlib 压缩过滤器的 bufferevent
// 数据流：原始 socket ←→ 压缩过滤器 ←→ 应用程序
//   发送方向：应用写入 → FilterOut 压缩 → socket 发出
//   接收方向：socket 收到 → FilterIn 解压 → 应用读取
// 同时设置 3 秒读超时：如果 3 秒内没有收到数据，触发 TIMEOUT_READING 事件
bufferevent* NetworkLayer::CreateFilteredBev(event_base* base, evutil_socket_t fd,
											  bufferevent_data_cb readCb,
											  bufferevent_data_cb writeCb,
											  bufferevent_event_cb eventCb,
											  void* cbArg) {
	// 1. 创建原始 socket bufferevent
	bufferevent* bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE);

	// 2. 创建 Zip 压缩上下文（deflate + inflate 流）
	Zip* zip = new Zip();

	// 3. 在原始 bufferevent 上添加压缩过滤器层
	//    FilterIn：解压接收到的数据
	//    FilterOut：压缩要发送的数据
	//    BEV_OPT_CLOSE_ON_FREE：释放时关闭底层 bufferevent
	//    FreeZipContext：释放时的清理回调
	bufferevent* bev_filter = bufferevent_filter_new(bev,
		FilterIn,
		FilterOut,
		BEV_OPT_CLOSE_ON_FREE,
		FreeZipContext,
		zip
	);

	// 4. 设置 3 秒读超时（服务端会检测客户端是否断线）
	timeval t1 = { 3, 0 };
	bufferevent_set_timeouts(bev_filter, &t1, nullptr);

	// 5. 设置应用层的读写和事件回调
	bufferevent_setcb(bev_filter, readCb, writeCb, eventCb, cbArg);
	bufferevent_enable(bev_filter, EV_READ | EV_WRITE);

	return bev_filter;
}

// OnServerAccept: 服务端接受新客户端连接
// 1. 记录新 peer 的 fd
// 2. 创建带压缩过滤器的 bufferevent
// 3. 初始化读取状态
// 4. 为客户端预注册远程对象（networkId=1）
// 5. 向新客户端发送全量同步（FullSync）
// 注意：networkId 硬编码为 1，当前仅支持单客户端
void NetworkLayer::OnServerAccept(evconnlistener* listener, evutil_socket_t fd,
								   sockaddr* addr, int len, void* arg) {
	NetworkLayer* self = static_cast<NetworkLayer*>(arg);
	std::cout << "[NetworkLayer] New connection: " << fd << std::endl;

	// 记录 peer
	self->mPeerFds.push_back(fd);

	// 创建带压缩过滤器的 bufferevent，设置读写和事件回调
	bufferevent* filteredBev = CreateFilteredBev(self->mBase, fd,
												  OnPeerRead, nullptr, OnPeerEvent, self);
	self->mServerBevs[fd] = filteredBev;

	// 初始化该连接的读取状态（用于 TCP 粘包处理）
	self->mReadStates[filteredBev] = ReadState{};

	// 预注册客户端远程对象，并发送全量同步
	auto& repMgr = ReplicationManager::Get();
	evutil_socket_t remoteId = static_cast<evutil_socket_t>(1);
	repMgr.RegisterRemoteObject(remoteId, remoteId);  // 客户端对象 networkId=1
	repMgr.FullSyncTo(fd);  // 发送服务端所有本地对象状态给新客户端
}

// OnPeerRead: peer 数据可读回调，转到 HandleIncomingData 处理
void NetworkLayer::OnPeerRead(bufferevent* be, void* arg) {
	NetworkLayer* self = static_cast<NetworkLayer*>(arg);
	self->HandleIncomingData(be);
}

// OnPeerEvent: 服务端 peer 事件回调
// 处理三种事件：读超时（TIMEOUT_READING）、错误（ERROR）、连接关闭（EOF）
// 每种事件都会清理该 peer 的所有资源和注册的对象
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

// OnClientEvent: 客户端事件回调
// CONNECTED：连接成功 → 创建压缩过滤器层，替换 mClientBev
// ERROR/EOF：连接断开 → 释放 bufferevent
// 注意：更新 mClientBev 为压缩过滤后的版本，这样后续的 SendWithHeader 通过压缩层发送
// 注意：客户端未设置读超时（与服务器不同）
void NetworkLayer::OnClientEvent(bufferevent* be, short events, void* arg) {
	NetworkLayer* self = static_cast<NetworkLayer*>(arg);
	std::cout << "[NetworkLayer ClientEvent]" << std::flush;

	if (events & BEV_EVENT_CONNECTED) {
		std::cout << " CONNECTED" << std::endl;

		// 创建压缩上下文并包装当前连接
		Zip* zip = new Zip();
		bufferevent* filteredBev = bufferevent_filter_new(be,
			FilterIn, FilterOut,
			BEV_OPT_CLOSE_ON_FREE,
			FreeZipContext, zip);

		// 设置压缩层为新的读写回调目标
		bufferevent_setcb(filteredBev, OnPeerRead, OnClientWrite, OnClientEvent, self);
		bufferevent_enable(filteredBev, EV_READ | EV_WRITE);

		// 关键：替换 mClientBev 为压缩过滤后的 bufferevent
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

// OnClientWrite: 客户端写完成回调（当前空实现，保留用于未来扩展）
void NetworkLayer::OnClientWrite(bufferevent* be, void* arg) {}

// HandleIncomingData: 处理接收到的数据
// TCP 粘包处理的有限状态机：
//   状态 1 (header.Size == 0)：等待读取 DataHeader
//   状态 2：读取载荷数据（payload）
//   状态 3：载荷完整 → 按 Key 路由到复制管理器 → 回到状态 1
//
// 路由规则：
//   Key=0 → ApplyReplication：状态复制（对象属性更新）
//   Key=1 → DispatchEvent：事件分发（一次性消息）
void NetworkLayer::HandleIncomingData(bufferevent* be) {
	ReadState& state = mReadStates[be];
	struct evbuffer* input = bufferevent_get_input(be);

	while (evbuffer_get_length(input) > 0) {
		// 状态 1：读取 DataHeader
		if (state.header.Size == 0) {
			// 数据不够完整的包头，等待更多数据
			if (evbuffer_get_length(input) < sizeof(DataHeader)) {
				return;
			}
			bufferevent_read(be, &state.header, sizeof(DataHeader));
			state.payload.resize(state.header.Size);
			state.payloadRead = 0;
		}

		// 状态 2：读取载荷数据
		int remaining = static_cast<int>(state.header.Size - state.payloadRead);
		int len = bufferevent_read(be, state.payload.data() + state.payloadRead, remaining);
		if (len <= 0) return;

		state.payloadRead += len;

		// 状态 3：载荷读取完整 → 路由分发
		if (state.payloadRead >= state.header.Size) {
			uint32_t key = state.header.Key;
			ByteReader reader(state.payload.data(), state.payload.size());

			if (key == 0) {
				// 状态复制数据 → 更新远程对象
				ReplicationManager::Get().ApplyReplication(
					bufferevent_getfd(be), reader);
			} else if (key == 1) {
				// 事件数据 → 事件分发
				uint16_t eventCount;
				reader.ReadU16(eventCount);
				for (uint16_t i = 0; i < eventCount; ++i) {
					uint16_t eventTypeId;
					reader.ReadU16(eventTypeId);
					ReplicationManager::Get().DispatchEvent(
						bufferevent_getfd(be), eventTypeId, reader);
				}
			}

			// 重置状态，准备读取下一个数据包
			state.header.Size = 0;
			state.payload.clear();
			state.payloadRead = 0;
		}
	}
}

// FilterIn: zlib 解压过滤器（接收方向）
// 从 src（socket 收到的压缩数据）解压后写入 dst（应用层接收缓冲区）
bufferevent_filter_result NetworkLayer::FilterIn(
	evbuffer* src, evbuffer* dst,
	ev_ssize_t limit, bufferevent_flush_mode mode, void* arg) {
	Zip* z = (Zip*)arg;
	z_stream* p = z->j;  // 使用 inflate 流（解压）

	if (!p) return BEV_ERROR;

	// 查看 src 中的数据（不消耗）
	evbuffer_iovec v_in[1];
	int n = evbuffer_peek(src, -1, NULL, v_in, 1);
	if (n <= 0) return BEV_NEED_MORE;

	// 设置 zlib 输入缓冲区
	p->avail_in = static_cast<uInt>(v_in[0].iov_len);
	p->next_in = (Byte*)v_in[0].iov_base;

	// 在 dst 中预留输出空间
	evbuffer_iovec v_out[1];
	evbuffer_reserve_space(dst, 4096, v_out, 1);

	p->avail_out = static_cast<uInt>(v_out[0].iov_len);
	p->next_out = (Byte*)v_out[0].iov_base;

	// 执行 zlib 解压
	int re = inflate(p, Z_SYNC_FLUSH);
	if (re != Z_OK) {
		std::cerr << "[NetworkLayer] inflate failed!" << std::endl;
	}

	// 计算实际读取和写入的字节数
	int nread = static_cast<int>(v_in[0].iov_len - p->avail_in);
	int nwrite = static_cast<int>(v_out[0].iov_len - p->avail_out);

	// 消耗已处理的 src 数据，提交已写入的 dst 数据
	evbuffer_drain(src, nread);
	v_out[0].iov_len = nwrite;
	evbuffer_commit_space(dst, v_out, 1);

	return BEV_OK;
}

// FreeZipContext: 释放压缩上下文（作为 bufferevent 的释放回调）
void NetworkLayer::FreeZipContext(void* ctx) {
	delete static_cast<Zip*>(ctx);
}

// FilterOut: zlib 压缩过滤器（发送方向）
// 从 src（应用层要发送的数据）压缩后写入 dst（socket 发送缓冲区）
bufferevent_filter_result NetworkLayer::FilterOut(
	evbuffer* src, evbuffer* dst,
	ev_ssize_t limit, bufferevent_flush_mode mode, void* arg) {
	Zip* z = (Zip*)arg;
	z_stream* p = z->y;  // 使用 deflate 流（压缩）

	if (!p) return BEV_ERROR;

	evbuffer_iovec v_in[1];
	int n = evbuffer_peek(src, -1, 0, v_in, 1);
	if (n <= 0) return BEV_NEED_MORE;

	// 设置 zlib 输入缓冲区
	p->avail_in = static_cast<uInt>(v_in[0].iov_len);
	p->next_in = (Byte*)v_in[0].iov_base;

	// 在 dst 中预留输出空间
	evbuffer_iovec v_out[1];
	evbuffer_reserve_space(dst, 4096, v_out, 1);

	p->avail_out = static_cast<uInt>(v_out[0].iov_len);
	p->next_out = (Byte*)v_out[0].iov_base;

	// 执行 zlib 压缩
	int re = deflate(p, Z_SYNC_FLUSH);
	if (re != Z_OK) {
		LOGE("[NetworkLayer] deflate failed");
	}

	// 计算实际读取和写入的字节数
	int nread = static_cast<int>(v_in[0].iov_len - p->avail_in);
	int nwrite = static_cast<int>(v_out[0].iov_len - p->avail_out);

	// 消耗已处理的 src 数据，提交已写入的 dst 数据
	evbuffer_drain(src, nread);
	v_out[0].iov_len = nwrite;
	evbuffer_commit_space(dst, v_out, 1);

	return BEV_OK;
}