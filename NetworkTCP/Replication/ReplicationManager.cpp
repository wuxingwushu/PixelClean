#include "ReplicationManager.h"
#include "../StructTCP.h"

ReplicationManager& ReplicationManager::Get() {
	static ReplicationManager instance;
	return instance;
}

ReplicableObject* ReplicationManager::RegisterLocalObject(evutil_socket_t networkId) {
	auto* obj = new ReplicableObject(networkId);
	obj->SetOwnerId(networkId);
	mObjects[networkId] = obj;
	mLocalObjects.push_back(obj);
	return obj;
}

ReplicableObject* ReplicationManager::RegisterRemoteObject(evutil_socket_t networkId, evutil_socket_t ownerId) {
	auto* obj = new ReplicableObject(networkId);
	obj->SetOwnerId(ownerId);
	mObjects[networkId] = obj;
	mRemoteObjects.push_back(obj);
	return obj;
}

void ReplicationManager::UnregisterObject(evutil_socket_t networkId) {
	auto it = mObjects.find(networkId);
	if (it == mObjects.end()) return;

	ReplicableObject* obj = it->second;

	auto removeFromList = [obj](std::vector<ReplicableObject*>& list) {
		list.erase(std::remove(list.begin(), list.end(), obj), list.end());
	};
	removeFromList(mLocalObjects);
	removeFromList(mRemoteObjects);

	mObjects.erase(it);
	delete obj;
}

ReplicableObject* ReplicationManager::GetObject(evutil_socket_t networkId) {
	auto it = mObjects.find(networkId);
	return (it != mObjects.end()) ? it->second : nullptr;
}

void ReplicationManager::Tick() {
	mStateWriter.Clear();
	uint16_t dirtyCount = 0;
	size_t countPos = mStateWriter.Size();
	mStateWriter.WriteU16(0);

	for (auto* obj : mLocalObjects) {
		if (!obj->IsAnyComponentDirty()) continue;

		mStateWriter.WriteSocket(obj->GetNetworkId());
		uint16_t compCount = 0;
		size_t compCountPos = mStateWriter.Size();
		mStateWriter.WriteU16(0);

		for (auto* comp : obj->GetAllComponents()) {
			if (!comp->IsDirty()) continue;

			mStateWriter.WriteU16(comp->GetComponentTypeId());
			comp->Serialize(mStateWriter);
			compCount++;
		}

		if (compCount > 0) {
			uint8_t* data = mStateWriter.MutableData();
			std::memcpy(data + compCountPos, &compCount, sizeof(uint16_t));
			obj->ClearAllComponentsDirty();
			dirtyCount++;
		}
	}

	if (dirtyCount > 0) {
		uint8_t* data = mStateWriter.MutableData();
		std::memcpy(data + countPos, &dirtyCount, sizeof(uint16_t));

		if (mSendCallback) {
			mSendCallback(-1, mStateWriter.Data(), mStateWriter.Size());
		}
	}

	if (!mPendingEvents.empty()) {
		using IndexList = std::vector<size_t>;
		std::unordered_map<evutil_socket_t, IndexList> targetGroups;
		for (size_t i = 0; i < mPendingEvents.size(); ++i) {
			targetGroups[mPendingEvents[i].targetFd].push_back(i);
		}

		if (mEventSendCallback) {
			for (auto& pair : targetGroups) {
				evutil_socket_t targetFd = pair.first;
				IndexList& indices = pair.second;

				mEventWriter.Clear();
				mEventWriter.WriteU16(static_cast<uint16_t>(indices.size()));

				for (size_t idx : indices) {
					mEventWriter.WriteU16(mPendingEvents[idx].event->GetEventTypeId());
					mPendingEvents[idx].event->Serialize(mEventWriter);
				}

				mEventSendCallback(targetFd, mEventWriter.Data(), mEventWriter.Size());
			}
		}
		mPendingEvents.clear();
	}
}

void ReplicationManager::ApplyReplication(evutil_socket_t senderFd, ByteReader& reader) {
	uint16_t objectCount = 0;
	reader.ReadU16(objectCount);

	for (uint16_t i = 0; i < objectCount; ++i) {
		evutil_socket_t objId;
		reader.ReadSocket(objId);

		mLastSeen[objId] = std::chrono::steady_clock::now();

		ReplicableObject* obj = GetObject(objId);
		if (!obj) {
			obj = RegisterRemoteObject(objId, senderFd);
		}

		uint16_t compCount = 0;
		reader.ReadU16(compCount);

		for (uint16_t j = 0; j < compCount; ++j) {
			uint16_t compTypeId = 0;
			reader.ReadU16(compTypeId);

			ReplicableComponent* comp = obj->GetComponentByTypeId(compTypeId);
			if (!comp) {
				auto it = mComponentFactories.find(compTypeId);
				if (it != mComponentFactories.end()) {
					auto newComp = it->second();
					if (newComp) {
						comp = obj->AddExistingComponent(std::move(newComp));
					}
				}
			}
			if (comp) {
				comp->Deserialize(reader);
			}
		}
	}
}

void ReplicationManager::RegisterEventHandler(uint16_t eventTypeId, EventHandler handler) {
	mEventHandlers[eventTypeId] = std::move(handler);
}

void ReplicationManager::RegisterRemoteComponentType(uint16_t typeId,
	std::function<std::unique_ptr<ReplicableComponent>()> factory) {
	mComponentFactories[typeId] = std::move(factory);
}

void ReplicationManager::SendEvent(evutil_socket_t targetFd, IReplicationEvent* event) {
	mPendingEvents.push_back({targetFd, std::unique_ptr<IReplicationEvent>(event)});
}

void ReplicationManager::DispatchEvent(evutil_socket_t senderFd, uint16_t eventTypeId, ByteReader& reader) {
	auto it = mEventHandlers.find(eventTypeId);
	if (it != mEventHandlers.end()) {
		it->second(senderFd, reader);
	}
}

void ReplicationManager::FullSyncTo(evutil_socket_t targetFd) {
	mStateWriter.Clear();
	SerializeAllObjects(mStateWriter);

	if (mSendCallback && mStateWriter.Size() > 2) {
		mSendCallback(targetFd, mStateWriter.Data(), mStateWriter.Size());
	}
}

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

void ReplicationManager::ForEachRemoteObject(
	std::function<void(ReplicableObject*)> callback) {
	for (auto* obj : mRemoteObjects) {
		callback(obj);
	}
}

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

void ReplicationManager::SerializeAllObjects(ByteWriter& writer) {
	uint16_t count = static_cast<uint16_t>(mLocalObjects.size());
	writer.WriteU16(count);

	for (auto* obj : mLocalObjects) {
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