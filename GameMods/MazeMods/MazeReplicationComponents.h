#pragma once
#include "../../NetworkTCP/ReplicableComponent.h"
#include "../../NetworkTCP/ReplicatedProperty.h"
#include <event2/util.h>

namespace GAME {

class PlayerPositionComponent : public ReplicableComponent {
public:
	static constexpr uint16_t kTypeId = 1;

	ReplicatedProperty<float> x;
	ReplicatedProperty<float> y;
	ReplicatedProperty<float> angle;
	ReplicatedProperty<evutil_socket_t> playerKey;

	uint16_t GetComponentTypeId() const override { return kTypeId; }

	void Serialize(ByteWriter& writer) override {
		uint16_t mask = 0;
		if (x.IsDirty())     mask |= (1 << 0);
		if (y.IsDirty())     mask |= (1 << 1);
		if (angle.IsDirty()) mask |= (1 << 2);
		if (playerKey.IsDirty()) mask |= (1 << 3);

		writer.WriteChangedMask(mask);
		if (mask & (1 << 0)) writer.WriteFloat(x.Get());
		if (mask & (1 << 1)) writer.WriteFloat(y.Get());
		if (mask & (1 << 2)) writer.WriteFloat(angle.Get());
		if (mask & (1 << 3)) writer.WriteSocket(playerKey.Get());
	}

	void Deserialize(ByteReader& reader) override {
		uint16_t mask = reader.ReadChangedMask();
		float tmp;

		if (mask & (1 << 0)) { reader.ReadFloat(tmp); x.Set(tmp); }
		if (mask & (1 << 1)) { reader.ReadFloat(tmp); y.Set(tmp); }
		if (mask & (1 << 2)) { reader.ReadFloat(tmp); angle.Set(tmp); }
		if (mask & (1 << 3)) {
			evutil_socket_t k;
			reader.ReadSocket(k);
			playerKey.Set(k);
		}
	}

	bool IsDirty() const override {
		return x.IsDirty() || y.IsDirty() || angle.IsDirty() || playerKey.IsDirty();
	}

	void ClearAllDirty() override {
		x.ClearDirty(); y.ClearDirty(); angle.ClearDirty(); playerKey.ClearDirty();
	}

	void ForceAllDirty() override {
		x.ForceDirty(); y.ForceDirty(); angle.ForceDirty(); playerKey.ForceDirty();
	}
};

class PlayerBrokenComponent : public ReplicableComponent {
public:
	static constexpr uint16_t kTypeId = 2;

	ReplicatedProperty<bool> brokenChanged;
	bool brokenData[16 * 16]{};

	uint16_t GetComponentTypeId() const override { return kTypeId; }

	void SetBrokenData(const bool* src) {
		std::memcpy(brokenData, src, sizeof(brokenData));
		brokenChanged.Set(true);
	}

	const bool* GetBrokenData() const { return brokenData; }

	void Serialize(ByteWriter& writer) override {
		uint16_t mask = brokenChanged.IsDirty() ? 1 : 0;
		writer.WriteChangedMask(mask);
		if (mask & 1) {
			writer.WriteBytes(reinterpret_cast<const uint8_t*>(brokenData),
							  16 * 16 * sizeof(bool));
		}
	}

	void Deserialize(ByteReader& reader) override {
		uint16_t mask = reader.ReadChangedMask();
		if (mask & 1) {
			uint32_t size;
			reader.ReadU32(size);
			if (size == 16 * 16 * sizeof(bool)) {
				reader.ReadRaw(brokenData, size);
				brokenChanged.Set(true);
			}
		}
	}

	bool IsDirty() const override { return brokenChanged.IsDirty(); }
	void ClearAllDirty() override { brokenChanged.ClearDirty(); }
	void ForceAllDirty() override { brokenChanged.ForceDirty(); }
};

} // namespace GAME