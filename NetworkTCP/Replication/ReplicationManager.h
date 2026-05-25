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

using EventHandler = std::function<void(evutil_socket_t sender, ByteReader& reader)>;

using NetworkSendCallback = std::function<void(
	evutil_socket_t targetFd,
	const uint8_t* data,
	size_t size
)>;

using EventSendCallback = std::function<void(
	evutil_socket_t targetFd,
	const uint8_t* data,
	size_t size
)>;

class ReplicationManager {
public:
	static ReplicationManager& Get();

	ReplicableObject* RegisterLocalObject(evutil_socket_t networkId);

	ReplicableObject* RegisterRemoteObject(evutil_socket_t networkId, evutil_socket_t ownerId);

	void UnregisterObject(evutil_socket_t networkId);

	ReplicableObject* GetObject(evutil_socket_t networkId);

	const std::vector<ReplicableObject*>& GetLocalObjects() const { return mLocalObjects; }

	void ForEachRemoteObject(std::function<void(ReplicableObject*)> callback);

	void RegisterEventHandler(uint16_t eventTypeId, EventHandler handler);

	void RegisterRemoteComponentType(uint16_t typeId,
		std::function<std::unique_ptr<ReplicableComponent>()> factory);

	void SendEvent(evutil_socket_t targetFd, IReplicationEvent* event);

	void DispatchEvent(evutil_socket_t senderFd, uint16_t eventTypeId, ByteReader& reader);

	void Tick();

	void ApplyReplication(evutil_socket_t senderFd, ByteReader& reader);

	void TickTimeouts(uint32_t timeoutMs);

	void SetSendCallback(NetworkSendCallback callback) { mSendCallback = callback; }
	void SetEventSendCallback(EventSendCallback callback) { mEventSendCallback = callback; }

	void FullSyncTo(evutil_socket_t targetFd);

private:
	ReplicationManager() = default;

	void SerializeDirtyObjects(ByteWriter& writer, evutil_socket_t excludeFd);

	void SerializeAllObjects(ByteWriter& writer);

	std::unordered_map<evutil_socket_t, ReplicableObject*> mObjects;
	std::vector<ReplicableObject*> mLocalObjects;
	std::vector<ReplicableObject*> mRemoteObjects;

	std::unordered_map<uint16_t, EventHandler> mEventHandlers;

	std::unordered_map<uint16_t,
		std::function<std::unique_ptr<ReplicableComponent>()>> mComponentFactories;

	struct QueuedEvent {
		evutil_socket_t targetFd;
		std::unique_ptr<IReplicationEvent> event;
	};
	std::vector<QueuedEvent> mPendingEvents;

	NetworkSendCallback mSendCallback;
	EventSendCallback mEventSendCallback;

	ByteWriter mStateWriter;
	ByteWriter mEventWriter;

	std::unordered_map<evutil_socket_t, std::chrono::steady_clock::time_point> mLastSeen;
};