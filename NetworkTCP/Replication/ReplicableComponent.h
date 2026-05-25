#pragma once
#include "ByteWriter.h"
#include "ByteReader.h"
#include <cstdint>

class ReplicableComponent {
public:
	virtual ~ReplicableComponent() = default;

	virtual uint16_t GetComponentTypeId() const = 0;

	virtual void Serialize(ByteWriter& writer) = 0;

	virtual void Deserialize(ByteReader& reader) = 0;

	virtual bool IsDirty() const = 0;

	virtual void ClearAllDirty() = 0;

	virtual void ForceAllDirty() = 0;
};