#pragma once
#include "ReplicationManager.h"
#include "StructTCP.h"
#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <event2/listener.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

// NetworkRole: 网络角色枚举
enum class NetworkRole {
	Server,  // 服务端：监听端口，接受多个客户端连接，管理所有 NPC 和游戏逻辑
	Client   // 客户端：连接到服务端，仅在本地渲染，逻辑由服务端决定
};

// NetworkLayer: 网络传输层（单例）
// 基于 libevent 的 TCP 网络通信层，封装了：
//   1. 服务端监听/接受连接   2. 客户端连接
//   3. zlib 压缩/解压过滤器  4. 数据包的发送与接收
//   5. 连接超时/断开处理
// 所有网络数据包前都有一个 DataHeader，其 Key 字段决定路由：
//   Key=0 → ReplicationManager::ApplyReplication（状态复制）
//   Key=1 → ReplicationManager::DispatchEvent（事件分发）
class NetworkLayer {
public:
	static NetworkLayer& Get();

	// 初始化网络层
	// role:      服务端或客户端
	// serverIP:  服务端 IP（客户端连接用）
	// serverPort: 服务端端口（服务端监听用）
	// clientPort: 客户端目标端口（客户端连接用）
	bool Initialize(NetworkRole role,
					const std::string& serverIP,
					int serverPort,
					int clientPort);

	// 关闭网络层，释放所有连接和资源
	void Shutdown();

	// 运行事件循环（每帧调用一次）
	// 服务端：EVLOOP_NONBLOCK（非阻塞，处理一轮事件后返回）
	// 客户端：EVLOOP_ONCE（处理最多一个事件后返回）
	void RunLoop();

	// 发送原始数据（Key=0，即状态复制数据）
	void SendTo(evutil_socket_t targetFd, const uint8_t* data, size_t size);

	// 发送带包头的数据（指定 Key 路由键）
	// targetFd = -1：广播（客户端发给服务端，服务端发给所有 peer）
	// targetFd = 具体值：单播给指定 peer
	void SendWithHeader(evutil_socket_t targetFd, uint32_t key,
						const uint8_t* data, size_t size);

	event_base* GetEventBase() { return mBase; }

	const std::vector<evutil_socket_t>& GetPeerFds() const { return mPeerFds; }

private:
	NetworkLayer() = default;
	~NetworkLayer();

	// === libevent 回调函数（静态，通过 void* arg 传 this） ===

	// 客户端事件回调：连接成功/失败、错误、EOF
	static void OnClientEvent(bufferevent* be, short events, void* arg);
	// 客户端写完成回调（当前空实现）
	static void OnClientWrite(bufferevent* be, void* arg);
	// 服务端接受新连接回调
	static void OnServerAccept(evconnlistener* listener, evutil_socket_t fd,
							   sockaddr* addr, int len, void* arg);
	// peer 数据可读回调（客户端和服务端共用）
	static void OnPeerRead(bufferevent* be, void* arg);
	// peer 事件回调：超时、错误、EOF
	static void OnPeerEvent(bufferevent* be, short events, void* arg);

	// 创建带 zlib 压缩过滤器的 bufferevent
	static bufferevent* CreateFilteredBev(event_base* base, evutil_socket_t fd,
											  bufferevent_data_cb readCb,
											  bufferevent_data_cb writeCb,
											  bufferevent_event_cb eventCb,
											  void* cbArg);

	// 释放压缩上下文（Zip 对象）
	static void FreeZipContext(void* ctx);

	// zlib 过滤器回调：解压接收的数据（FilterIn）
	static bufferevent_filter_result FilterIn(
		evbuffer* src, evbuffer* dst,
		ev_ssize_t limit, bufferevent_flush_mode mode, void* arg);
	// zlib 过滤器回调：压缩发送的数据（FilterOut）
	static bufferevent_filter_result FilterOut(
		evbuffer* src, evbuffer* dst,
		ev_ssize_t limit, bufferevent_flush_mode mode, void* arg);

	// 处理接收到的数据：解析 DataHeader，按 Key 路由到复制管理器
	void HandleIncomingData(bufferevent* be);

	// === 成员变量 ===

	NetworkRole mRole = NetworkRole::Server;  // 当前网络角色
	event_base* mBase = nullptr;              // libevent 事件循环基础对象
	bufferevent* mClientBev = nullptr;        // 客户端与服务端的连接（带 zlib 过滤器）
	evconnlistener* mListener = nullptr;      // 服务端监听器

	std::unordered_map<evutil_socket_t, bufferevent*> mServerBevs; // 服务端 peer 连接映射
	std::vector<evutil_socket_t> mPeerFds;    // 所有 peer 的 socket fd 列表

	std::vector<uint8_t> mSendBuffer;          // 发送缓冲区（备用）

	// 每个 bufferevent 的读取状态
	// TCP 是流式协议，可能需要多次读取才能组装完整的数据包
	struct ReadState {
		DataHeader header = {0, 0};           // 当前正在读取的包头
		std::vector<uint8_t> payload;          // 载荷缓冲区
		size_t payloadRead = 0;               // 已读取的载荷字节数
	};
	std::unordered_map<bufferevent*, ReadState> mReadStates;
};