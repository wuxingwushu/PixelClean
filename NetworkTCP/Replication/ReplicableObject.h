#pragma once
#include "ReplicableComponent.h"
#include <vector>
#include <unordered_map>
#include <memory>
#include <type_traits>
#include <event2/util.h>

class ReplicableObject {
public:
	explicit ReplicableObject(evutil_socket_t networkId)
		: mNetworkId(networkId) {}

	evutil_socket_t GetNetworkId() const { return mNetworkId; }

	void SetOwnerId(evutil_socket_t ownerId) { mOwnerId = ownerId; }
	evutil_socket_t GetOwnerId() const { return mOwnerId; }

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

	ReplicableComponent* AddExistingComponent(std::unique_ptr<ReplicableComponent> comp) {
		uint16_t typeId = comp->GetComponentTypeId();
		ReplicableComponent* ptr = comp.get();
		mComponents[typeId] = std::move(comp);
		mComponentList.push_back(ptr);
		return ptr;
	}

	template<typename T>
	T* GetComponent() {
		return static_cast<T*>(GetComponentByTypeId(T::kTypeId));
	}

	ReplicableComponent* GetComponentByTypeId(uint16_t typeId) {
		auto it = mComponents.find(typeId);
		if (it != mComponents.end()) return it->second.get();
		return nullptr;
	}

	const std::vector<ReplicableComponent*>& GetAllComponents() const {
		return mComponentList;
	}

	bool IsAnyComponentDirty() const {
		for (auto* comp : mComponentList) {
			if (comp->IsDirty()) return true;
		}
		return false;
	}

	void ClearAllComponentsDirty() {
		for (auto* comp : mComponentList) {
			comp->ClearAllDirty();
		}
	}

	void ForceAllComponentsDirty() {
		for (auto* comp : mComponentList) {
			comp->ForceAllDirty();
		}
	}

private:
	evutil_socket_t mNetworkId;
	evutil_socket_t mOwnerId = -1;
	std::unordered_map<uint16_t, std::unique_ptr<ReplicableComponent>> mComponents;
	std::vector<ReplicableComponent*> mComponentList;
};