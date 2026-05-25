#pragma once
#include "../../NetworkTCP/Replication/EventTypes.h"
#include "../../GlobalStructural.h"
#include <string>
#include <vector>

namespace GAME {

class BulletShootEvent : public IReplicationEvent {
public:
	static constexpr uint16_t kTypeId = 100;

	float x = 0, y = 0, angle = 0;
	unsigned int bulletType = 0;

	BulletShootEvent() = default;
	BulletShootEvent(float x_, float y_, float angle_, unsigned int type_)
		: x(x_), y(y_), angle(angle_), bulletType(type_) {}

	uint16_t GetEventTypeId() const override { return kTypeId; }

	void Serialize(ByteWriter& writer) override {
		writer.WriteFloat(x);
		writer.WriteFloat(y);
		writer.WriteFloat(angle);
		writer.WriteU32(bulletType);
	}

	void Deserialize(ByteReader& reader) override {
		reader.ReadFloat(x);
		reader.ReadFloat(y);
		reader.ReadFloat(angle);
		reader.ReadU32(bulletType);
	}
};

class PixelDestroyEvent : public IReplicationEvent {
public:
	static constexpr uint16_t kTypeId = 101;

	PixelState pixel{};

	PixelDestroyEvent() = default;
	explicit PixelDestroyEvent(const PixelState& p) : pixel(p) {}

	uint16_t GetEventTypeId() const override { return kTypeId; }

	void Serialize(ByteWriter& writer) override {
		writer.WriteI32(pixel.X);
		writer.WriteI32(pixel.Y);
		writer.WriteBool(pixel.State);
	}

	void Deserialize(ByteReader& reader) override {
		reader.ReadI32(pixel.X);
		reader.ReadI32(pixel.Y);
		reader.ReadBool(pixel.State);
	}
};

class ChatEvent : public IReplicationEvent {
public:
	static constexpr uint16_t kTypeId = 102;

	std::string message;

	ChatEvent() = default;
	explicit ChatEvent(const std::string& msg) : message(msg) {}

	uint16_t GetEventTypeId() const override { return kTypeId; }

	void Serialize(ByteWriter& writer) override {
		writer.WriteString(message);
	}

	void Deserialize(ByteReader& reader) override {
		uint32_t len;
		reader.ReadU32(len);
		if (len > 0) {
			message.resize(len);
			reader.ReadRaw(&message[0], len);
		}
	}
};

class RequestLabyrinthInitEvent : public IReplicationEvent {
public:
	static constexpr uint16_t kTypeId = 200;

	uint16_t GetEventTypeId() const override { return kTypeId; }

	void Serialize(ByteWriter& writer) override {}
	void Deserialize(ByteReader& reader) override {}
};

class LabyrinthInitDataEvent : public IReplicationEvent {
public:
	static constexpr uint16_t kTypeId = 201;

	int numberX = 0;
	int numberY = 0;
	std::vector<unsigned int> blockTypesData;
	std::vector<int> blockPixelsData;

	uint16_t GetEventTypeId() const override { return kTypeId; }

	void Serialize(ByteWriter& writer) override {
		writer.WriteI32(numberX);
		writer.WriteI32(numberY);

		uint32_t typeSize = static_cast<uint32_t>(blockTypesData.size() * sizeof(unsigned int));
		writer.WriteU32(typeSize);
		if (typeSize > 0) writer.WriteRaw(blockTypesData.data(), typeSize);

		uint32_t pixelSize = static_cast<uint32_t>(blockPixelsData.size() * sizeof(int));
		writer.WriteU32(pixelSize);
		if (pixelSize > 0) writer.WriteRaw(blockPixelsData.data(), pixelSize);
	}

	void Deserialize(ByteReader& reader) override {
		reader.ReadI32(numberX);
		reader.ReadI32(numberY);

		uint32_t typeSize;
		reader.ReadU32(typeSize);
		if (typeSize > 0) {
			blockTypesData.resize(typeSize / sizeof(unsigned int));
			reader.ReadRaw(blockTypesData.data(), typeSize);
		}

		uint32_t pixelSize;
		reader.ReadU32(pixelSize);
		if (pixelSize > 0) {
			blockPixelsData.resize(pixelSize / sizeof(int));
			reader.ReadRaw(blockPixelsData.data(), pixelSize);
		}
	}
};

} // namespace GAME