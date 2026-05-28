#pragma once
#include "ReplicableComponent.h"
#include <vector>
#include <unordered_map>
#include <memory>
#include <type_traits>
#include <event2/util.h>

// ReplicableObject: 网络可复制对象
// 代表一个可以通过网络同步的游戏实体（如玩家）
// 每个对象有唯一的 networkId，持有多个 ReplicableComponent
// 组件按类型 ID 存储在 map 中（支持 O(1) 按类型查找），
// 同时按插入顺序存储在 list 中（支持高效遍历）
class ReplicableObject {
public:
	explicit ReplicableObject(evutil_socket_t networkId)
		: mNetworkId(networkId) {}

	// 获取此对象在网络中的唯一 ID
	evutil_socket_t GetNetworkId() const { return mNetworkId; }

	// 设置/获取所有者 ID（即拥有此对象的远端 peer 的 socket fd）
	void SetOwnerId(evutil_socket_t ownerId) { mOwnerId = ownerId; }
	evutil_socket_t GetOwnerId() const { return mOwnerId; }

	// 创建并添加一个组件（模板版本），构造参数转发
	template<typename T, typename... Args>
	T* AddComponent(Args&&... args) {
		static_assert(std::is_base_of<ReplicableComponent, T>::value,
					  "T must derive from ReplicableComponent");
		auto comp = std::make_unique<T>(std::forward<Args>(args)...);
		T* ptr = comp.get();
		uint16_t typeId = comp->GetComponentTypeId();
		mComponents[typeId] = std::move(comp);
		mComponentList.push_back(ptr);
		return ptr;
	}

	// 添加一个已构造的组件（通过 unique_ptr 转移所有权）
	// 用于组件工厂模式——远端接收到数据后按 typeId 动态创建组件
	ReplicableComponent* AddExistingComponent(std::unique_ptr<ReplicableComponent> comp) {
		uint16_t typeId = comp->GetComponentTypeId();
		ReplicableComponent* ptr = comp.get();
		mComponents[typeId] = std::move(comp);
		mComponentList.push_back(ptr);
		return ptr;
	}

	// 通过模板类型获取组件指针
	template<typename T>
	T* GetComponent() {
		return static_cast<T*>(GetComponentByTypeId(T::kTypeId));
	}

	// 通过类型 ID 获取组件指针
	ReplicableComponent* GetComponentByTypeId(uint16_t typeId) {
		auto it = mComponents.find(typeId);
		if (it != mComponents.end()) return it->second.get();
		return nullptr;
	}

	// 获取所有组件列表，用于遍历序列化
	const std::vector<ReplicableComponent*>& GetAllComponents() const {
		return mComponentList;
	}

	// 检查是否有任何组件标记为脏（需要增量同步）
	bool IsAnyComponentDirty() const {
		for (auto* comp : mComponentList) {
			if (comp->IsDirty()) return true;
		}
		return false;
	}

	// 清除所有组件的脏标记，Tick() 发送完成后调用
	void ClearAllComponentsDirty() {
		for (auto* comp : mComponentList) {
			comp->ClearAllDirty();
		}
	}

	// 强制所有组件标记为脏，用于 FullSync 全量同步
	void ForceAllComponentsDirty() {
		for (auto* comp : mComponentList) {
			comp->ForceAllDirty();
		}
	}

private:
	evutil_socket_t mNetworkId;    // 网络唯一标识符
	evutil_socket_t mOwnerId = -1; // 所有者 peer 的 socket fd
	std::unordered_map<uint16_t, std::unique_ptr<ReplicableComponent>> mComponents; // 按类型 ID 索引
	std::vector<ReplicableComponent*> mComponentList; // 按插入顺序列表
};