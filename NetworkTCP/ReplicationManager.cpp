#include "ReplicationManager.h"
#include "StructTCP.h"

// 单例访问
ReplicationManager& ReplicationManager::Get() {
	static ReplicationManager instance;
	return instance;
}

// RegisterLocalObject: 注册本地拥有的可复制对象
// 本地对象是指本端负责序列化和发送其状态的对象
// 如：服务端的服务器玩家（networkId=0），客户端的客户端玩家（networkId=1）
ReplicableObject* ReplicationManager::RegisterLocalObject(evutil_socket_t networkId) {
	auto* obj = new ReplicableObject(networkId);
	obj->SetOwnerId(networkId);
	mObjects[networkId] = obj;
	mLocalObjects.push_back(obj);
	return obj;
}

// RegisterRemoteObject: 注册远程拥有的可复制对象
// 远程对象是指远端负责发送状态、本端仅接收并反序列化的对象
// 如：服务端看到客户端玩家（networkId=1），客户端看到服务器玩家（networkId=0）
ReplicableObject* ReplicationManager::RegisterRemoteObject(evutil_socket_t networkId, evutil_socket_t ownerId) {
	auto* obj = new ReplicableObject(networkId);
	obj->SetOwnerId(ownerId);
	mObjects[networkId] = obj;
	mRemoteObjects.push_back(obj);
	return obj;
}

// UnregisterObject: 注销对象（从所有列表中移除并释放内存）
// 用于对端断开连接或复制超时时清理
void ReplicationManager::UnregisterObject(evutil_socket_t networkId) {
	auto it = mObjects.find(networkId);
	if (it == mObjects.end()) return;

	ReplicableObject* obj = it->second;

	// 从本地列表和远程列表中移除（实际只会在其中之一）
	auto removeFromList = [obj](std::vector<ReplicableObject*>& list) {
		list.erase(std::remove(list.begin(), list.end(), obj), list.end());
	};
	removeFromList(mLocalObjects);
	removeFromList(mRemoteObjects);

	// 从全局映射中移除并释放
	mObjects.erase(it);
	delete obj;
}

// GetObject: 按 networkId 查找对象
ReplicableObject* ReplicationManager::GetObject(evutil_socket_t networkId) {
	auto it = mObjects.find(networkId);
	return (it != mObjects.end()) ? it->second : nullptr;
}

// Tick: 每帧主循环
// 两件事情：
//   1. 序列化所有脏的本地对象 → 通过 mSendCallback 发送（Key=0 通道）
//   2. 序列化所有待发送事件 → 通过 mEventSendCallback 发送（Key=1 通道），按目标 fd 分组批量发送
//
// 数据格式（状态复制）：
//   [u16: 对象数量] [socket: networkId] [u16: 组件数量] [u16: compTypeId] [comp.Serialize()] ...
//
// 性能优化：先用占位符 0 写入数量，序列化完成后回填实际值（避免两次遍历）
void ReplicationManager::Tick() {
	mStateWriter.Clear();
	uint16_t dirtyCount = 0;

	// 预留对象数量的占位符（稍后回填）
	size_t countPos = mStateWriter.Size();
	mStateWriter.WriteU16(0);

	// === 1. 状态复制 ===
	for (auto* obj : mLocalObjects) {
		// 跳过没有脏组件的对象（增量同步：只发送变化的数据）
		if (!obj->IsAnyComponentDirty()) continue;

		// 写入对象 networkId
		mStateWriter.WriteSocket(obj->GetNetworkId());

		// 预留组件数量的占位符（稍后回填）
		uint16_t compCount = 0;
		size_t compCountPos = mStateWriter.Size();
		mStateWriter.WriteU16(0);

		// 遍历所有组件，只序列化脏的
		for (auto* comp : obj->GetAllComponents()) {
			if (!comp->IsDirty()) continue;

			mStateWriter.WriteU16(comp->GetComponentTypeId());
			comp->Serialize(mStateWriter);
			compCount++;
		}

		// 如果有组件被序列化，回填组件数量并清除脏标记
		if (compCount > 0) {
			uint8_t* data = mStateWriter.MutableData();
			std::memcpy(data + compCountPos, &compCount, sizeof(uint16_t));
			obj->ClearAllComponentsDirty();
			dirtyCount++;
		}
	}

	// 如果有任何对象被序列化，回填对象数量并发送
	if (dirtyCount > 0) {
		uint8_t* data = mStateWriter.MutableData();
		std::memcpy(data + countPos, &dirtyCount, sizeof(uint16_t));

		if (mSendCallback) {
			// -1 表示广播（NetworkLayer 会根据角色决定具体行为）
			mSendCallback(-1, mStateWriter.Data(), mStateWriter.Size());
		}
	}

	// === 2. 事件发送 ===
	// 将事件按目标 fd 分组，每组创建一个独立的数据包
	if (!mPendingEvents.empty()) {
		using IndexList = std::vector<size_t>;
		std::unordered_map<evutil_socket_t, IndexList> targetGroups;
		for (size_t i = 0; i < mPendingEvents.size(); ++i) {
			targetGroups[mPendingEvents[i].targetFd].push_back(i);
		}

		// 按目标 fd 分组发送
		if (mEventSendCallback) {
			for (auto& pair : targetGroups) {
				evutil_socket_t targetFd = pair.first;
				IndexList& indices = pair.second;

				mEventWriter.Clear();
				// 写入事件数量
				mEventWriter.WriteU16(static_cast<uint16_t>(indices.size()));

				// 序列化每个事件：typeId + 事件数据
				for (size_t idx : indices) {
					mEventWriter.WriteU16(mPendingEvents[idx].event->GetEventTypeId());
					mPendingEvents[idx].event->Serialize(mEventWriter);
				}

				mEventSendCallback(targetFd, mEventWriter.Data(), mEventWriter.Size());
			}
		}
		// 清空待发送事件队列（已发送完毕）
		mPendingEvents.clear();
	}
}

// ApplyReplication: 应用接收到的状态复制数据
// 反序列化远端对象的状态并更新本地副本
// 数据格式与 Tick() 中序列化的一致：
//   [u16: 对象数量] [socket: networkId] [u16: 组件数量] [u16: compTypeId] [comp.Deserialize()] ...
//
// 如果对象或组件在本地不存在，会自动创建：
//   对象不存在 → RegisterRemoteObject（新建远程对象）
//   组件不存在 → 通过 mComponentFactories 创建（组件工厂模式）
void ReplicationManager::ApplyReplication(evutil_socket_t senderFd, ByteReader& reader) {
	uint16_t objectCount = 0;
	reader.ReadU16(objectCount);

	for (uint16_t i = 0; i < objectCount; ++i) {
		evutil_socket_t objId;
		reader.ReadSocket(objId);

		// 更新该对象的最后活跃时间（用于超时检测）
		mLastSeen[objId] = std::chrono::steady_clock::now();

		// 查找或创建对象
		ReplicableObject* obj = GetObject(objId);
		if (!obj) {
			// 首次收到该远程对象的数据 → 自动注册
			obj = RegisterRemoteObject(objId, senderFd);
		}

		uint16_t compCount = 0;
		reader.ReadU16(compCount);

		for (uint16_t j = 0; j < compCount; ++j) {
			uint16_t compTypeId = 0;
			reader.ReadU16(compTypeId);

			// 查找或通过工厂创建组件
			ReplicableComponent* comp = obj->GetComponentByTypeId(compTypeId);
			if (!comp) {
				// 组件不存在 → 尝试通过注册的工厂创建
				auto it = mComponentFactories.find(compTypeId);
				if (it != mComponentFactories.end()) {
					auto newComp = it->second();
					if (newComp) {
						comp = obj->AddExistingComponent(std::move(newComp));
					}
				}
			}
			// 反序列化组件状态
			if (comp) {
				comp->Deserialize(reader);
			}
		}
	}
}

// RegisterEventHandler: 注册事件处理器
void ReplicationManager::RegisterEventHandler(uint16_t eventTypeId, EventHandler handler) {
	mEventHandlers[eventTypeId] = std::move(handler);
}

// RegisterRemoteComponentType: 注册远程组件工厂
// 当收到未知组件类型的远程数据时，通过此工厂创建组件实例
void ReplicationManager::RegisterRemoteComponentType(uint16_t typeId,
	std::function<std::unique_ptr<ReplicableComponent>()> factory) {
	mComponentFactories[typeId] = std::move(factory);
}

// SendEvent: 发送事件
// 事件不会立即发送，而是加入待发送队列，在下一帧 Tick() 中批量发送
// 注意：传入的 event 指针所有权转移给队列（使用 unique_ptr）
void ReplicationManager::SendEvent(evutil_socket_t targetFd, IReplicationEvent* event) {
	mPendingEvents.push_back({targetFd, std::unique_ptr<IReplicationEvent>(event)});
}

// DispatchEvent: 分发接收到的事件
// 按 eventTypeId 查找注册的处理器并调用
void ReplicationManager::DispatchEvent(evutil_socket_t senderFd, uint16_t eventTypeId, ByteReader& reader) {
	auto it = mEventHandlers.find(eventTypeId);
	if (it != mEventHandlers.end()) {
		it->second(senderFd, reader);
	}
}

// FullSyncTo: 全量同步
// 将本地所有对象的状态序列化并发送给指定 peer
// 用于新客户端连接时的初始状态同步
// 与增量 Tick() 的区别：
//   - FullSyncTo：强制所有组件标记脏 → 发送全部数据 → 单播给特定 peer
//   - Tick()：仅发送脏组件 → 广播给所有 peer
void ReplicationManager::FullSyncTo(evutil_socket_t targetFd) {
	mStateWriter.Clear();
	SerializeAllObjects(mStateWriter);

	if (mSendCallback && mStateWriter.Size() > 2) {
		mSendCallback(targetFd, mStateWriter.Data(), mStateWriter.Size());
	}
}

// TickTimeouts: 超时检测
// 遍历所有远程对象的最后活跃时间，超过 timeoutMs 毫秒没有收到数据的对象会被注销
// 用于清理断线玩家的远程对象
void ReplicationManager::TickTimeouts(uint32_t timeoutMs) {
	auto now = std::chrono::steady_clock::now();
	for (auto it = mLastSeen.begin(); it != mLastSeen.end();) {
		auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
			now - it->second).count();
		if (elapsed > static_cast<long long>(timeoutMs)) {
			UnregisterObject(it->first);
			it = mLastSeen.erase(it);
		} else {
			++it;
		}
	}
}

// ForEachRemoteObject: 遍历所有远程对象
// 用于 GameLoop 中将远程玩家的位置数据同步到游戏世界（Crowd）
void ReplicationManager::ForEachRemoteObject(
	std::function<void(ReplicableObject*)> callback) {
	for (auto* obj : mRemoteObjects) {
		callback(obj);
	}
}

// SerializeDirtyObjects: 序列化所有脏的本地对象
// 当前未被 Tick() 使用，保留用于未来可能的定向增量同步
void ReplicationManager::SerializeDirtyObjects(ByteWriter& writer, evutil_socket_t excludeFd) {
	for (auto* obj : mLocalObjects) {
		if (!obj->IsAnyComponentDirty()) continue;
		if (obj->GetNetworkId() == excludeFd) continue;

		writer.WriteSocket(obj->GetNetworkId());
		uint16_t compCount = 0;
		size_t compCountPos = writer.Size();
		writer.WriteU16(0);

		for (auto* comp : obj->GetAllComponents()) {
			if (!comp->IsDirty()) continue;
			writer.WriteU16(comp->GetComponentTypeId());
			comp->Serialize(writer);
			compCount++;
		}

		if (compCount > 0) {
			uint8_t* data = writer.MutableData();
			std::memcpy(data + compCountPos, &compCount, sizeof(uint16_t));
		}
	}
}

// SerializeAllObjects: 序列化所有本地对象（全量）
// 强制所有组件标记脏，序列化后清除脏标记
void ReplicationManager::SerializeAllObjects(ByteWriter& writer) {
	uint16_t count = static_cast<uint16_t>(mLocalObjects.size());
	writer.WriteU16(count);

	for (auto* obj : mLocalObjects) {
		// 强制所有组件标记为脏（确保全量发送）
		obj->ForceAllComponentsDirty();
		writer.WriteSocket(obj->GetNetworkId());

		uint16_t compCount = static_cast<uint16_t>(obj->GetAllComponents().size());
		writer.WriteU16(compCount);

		for (auto* comp : obj->GetAllComponents()) {
			writer.WriteU16(comp->GetComponentTypeId());
			comp->Serialize(writer);
			comp->ClearAllDirty();
		}
	}
}