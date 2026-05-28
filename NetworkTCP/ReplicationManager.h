#pragma once
#include "ReplicableObject.h"
#include "EventTypes.h"
#include "ByteWriter.h"
#include "ByteReader.h"
#include <unordered_map>
#include <vector>
#include <memory>
#include <functional>
#include <chrono>
#include <algorithm>
#include <event2/util.h>

// EventHandler: 事件处理器回调类型
// sender: 发送方的 socket fd
// reader: 包含事件数据的 ByteReader
using EventHandler = std::function<void(evutil_socket_t sender, ByteReader& reader)>;

// NetworkSendCallback: 状态复制数据的发送回调
// targetFd: 目标 socket fd（-1 表示广播）
// data: 序列化后的状态数据
// size: 数据长度
using NetworkSendCallback = std::function<void(
	evutil_socket_t targetFd,
	const uint8_t* data,
	size_t size
)>;

// EventSendCallback: 事件数据的发送回调
// 与 NetworkSendCallback 类型相同但语义上用于事件通道，支持独立的发送策略
using EventSendCallback = std::function<void(
	evutil_socket_t targetFd,
	const uint8_t* data,
	size_t size
)>;

// ReplicationManager: 网络复制管理器（单例）
// 整个复制系统的核心，管理所有可复制对象和事件
//
// 核心工作流程（每帧）：
//   1. NetworkLayer::RunLoop() 接收数据
//      → HandleIncomingData() 按 Key 路由
//      → ApplyReplication() 更新远程对象    （Key=0）
//      → DispatchEvent() 分发事件           （Key=1）
//   2. GameLoop 更新本地对象（设置位置等）
//   3. Tick() 序列化脏的本地对象并发送
//   4. TickTimeouts() 清理超时的远程对象
//
// 三种数据通道：
//   Key=0 (mSendCallback)：状态复制——持续存在的对象属性，增量同步
//   Key=1 (mEventSendCallback)：事件——一次性消息，如迷宫初始数据
class ReplicationManager {
public:
	static ReplicationManager& Get();

	// 注册本地拥有的可复制对象（本端负责发送其状态）
	ReplicableObject* RegisterLocalObject(evutil_socket_t networkId);

	// 注册远程拥有的可复制对象（远端负责发送其状态，本端接收）
	ReplicableObject* RegisterRemoteObject(evutil_socket_t networkId, evutil_socket_t ownerId);

	// 注销对象（从所有列表中移除并释放内存）
	void UnregisterObject(evutil_socket_t networkId);

	// 按 networkId 查找对象
	ReplicableObject* GetObject(evutil_socket_t networkId);

	const std::vector<ReplicableObject*>& GetLocalObjects() const { return mLocalObjects; }

	// 遍历所有远程对象，用于 GameLoop 中将远程位置同步到 Crowd
	void ForEachRemoteObject(std::function<void(ReplicableObject*)> callback);

	// 注册事件处理器（按事件类型 ID 索引）
	void RegisterEventHandler(uint16_t eventTypeId, EventHandler handler);

	// 注册远程组件工厂（按组件类型 ID 索引）
	// 当本端收到未知组件类型的远程数据时，通过工厂动态创建组件实例
	void RegisterRemoteComponentType(uint16_t typeId,
		std::function<std::unique_ptr<ReplicableComponent>()> factory);

	// 发送事件给指定目标（targetFd = -1 表示广播）
	void SendEvent(evutil_socket_t targetFd, IReplicationEvent* event);

	// 分发接收到的事件到注册的处理器
	void DispatchEvent(evutil_socket_t senderFd, uint16_t eventTypeId, ByteReader& reader);

	// 每帧 Tick：序列化并发送脏的本地对象 + 待处理事件
	void Tick();

	// 应用接收到的状态复制数据：反序列化并更新远程对象
	void ApplyReplication(evutil_socket_t senderFd, ByteReader& reader);

	// 检查远程对象的超时：超过 timeoutMs 未收到数据的对象会被注销
	void TickTimeouts(uint32_t timeoutMs);

	// 设置发送回调（由 NetworkLayer 注入）
	void SetSendCallback(NetworkSendCallback callback) { mSendCallback = callback; }
	void SetEventSendCallback(EventSendCallback callback) { mEventSendCallback = callback; }

	// 全量同步：将本地所有对象序列化并发送给目标 peer
	// 用于新客户端连接时的初始状态同步
	void FullSyncTo(evutil_socket_t targetFd);

private:
	ReplicationManager() = default;

	// 序列化所有脏的本地对象（供 Tick 内部使用）
	void SerializeDirtyObjects(ByteWriter& writer, evutil_socket_t excludeFd);

	// 序列化所有本地对象（强制全量，供 FullSyncTo 使用）
	void SerializeAllObjects(ByteWriter& writer);

	// 所有对象映射（networkId → 对象指针）
	std::unordered_map<evutil_socket_t, ReplicableObject*> mObjects;
	// 本地对象列表（本端负责同步出去）
	std::vector<ReplicableObject*> mLocalObjects;
	// 远程对象列表（远端同步过来，本端接收）
	std::vector<ReplicableObject*> mRemoteObjects;

	// 事件处理器注册表（事件类型 ID → 处理器函数）
	std::unordered_map<uint16_t, EventHandler> mEventHandlers;

	// 远程组件工厂注册表（组件类型 ID → 工厂函数）
	// 当收到未知的远程组件数据时，通过工厂动态创建
	std::unordered_map<uint16_t,
		std::function<std::unique_ptr<ReplicableComponent>()>> mComponentFactories;

	// 待发送事件队列（在 Tick() 中批量发送）
	struct QueuedEvent {
		evutil_socket_t targetFd;              // 目标 socket fd
		std::unique_ptr<IReplicationEvent> event; // 事件对象
	};
	std::vector<QueuedEvent> mPendingEvents;

	// 发送回调（注入 NetworkLayer 的发送函数）
	NetworkSendCallback mSendCallback;
	EventSendCallback mEventSendCallback;

	// 序列化缓冲区（复用，减少内存分配）
	ByteWriter mStateWriter;  // 状态数据写入器
	ByteWriter mEventWriter;  // 事件数据写入器

	// 远程对象最后活跃时间（用于超时检测）
	std::unordered_map<evutil_socket_t, std::chrono::steady_clock::time_point> mLastSeen;
};