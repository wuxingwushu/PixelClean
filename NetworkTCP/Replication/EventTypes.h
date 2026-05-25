#pragma once
#include "ByteWriter.h"
#include "ByteReader.h"

class IReplicationEvent {
public:
	virtual ~IReplicationEvent() = default;
	virtual uint16_t GetEventTypeId() const = 0;
	virtual void Serialize(ByteWriter& writer) = 0;
	virtual void Deserialize(ByteReader& reader) = 0;
};