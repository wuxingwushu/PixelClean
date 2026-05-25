#pragma once
#include <cstring>
#include <cstdint>
#include <vector>
#include <string>
#include <event2/util.h>

class ByteWriter {
public:
	ByteWriter() { mBuffer.reserve(4096); }

	void WriteU8(uint8_t v)      { WriteRaw(&v, sizeof(v)); }
	void WriteU16(uint16_t v)    { WriteRaw(&v, sizeof(v)); }
	void WriteU32(uint32_t v)    { WriteRaw(&v, sizeof(v)); }
	void WriteFloat(float v)     { WriteRaw(&v, sizeof(v)); }
	void WriteI32(int32_t v)     { WriteRaw(&v, sizeof(v)); }
	void WriteBool(bool v)       { uint8_t b = v ? 1 : 0; WriteRaw(&b, 1); }
	void WriteSocket(evutil_socket_t v) { WriteRaw(&v, sizeof(v)); }

	void WriteRaw(const void* data, size_t size) {
		const uint8_t* src = static_cast<const uint8_t*>(data);
		mBuffer.insert(mBuffer.end(), src, src + size);
	}

	void WriteString(const std::string& str) {
		WriteU32(static_cast<uint32_t>(str.size()));
		WriteRaw(str.data(), str.size());
	}

	void WriteBytes(const uint8_t* data, uint32_t size) {
		WriteU32(size);
		if (size > 0) WriteRaw(data, size);
	}

	const uint8_t* Data() const { return mBuffer.data(); }
	size_t Size() const { return mBuffer.size(); }
	void Clear() { mBuffer.clear(); }

	void WriteChangedMask(uint16_t mask) { WriteU16(mask); }

	uint8_t* MutableData() { return mBuffer.data(); }

private:
	std::vector<uint8_t> mBuffer;
};