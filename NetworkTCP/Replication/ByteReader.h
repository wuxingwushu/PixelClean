#pragma once
#include <cstring>
#include <cstdint>
#include <event2/util.h>

class ByteReader {
public:
	ByteReader(const uint8_t* data, size_t size)
		: mData(data), mSize(size), mOffset(0) {}

	bool ReadU8(uint8_t& v)      { return ReadRaw(&v, sizeof(v)); }
	bool ReadU16(uint16_t& v)    { return ReadRaw(&v, sizeof(v)); }
	bool ReadU32(uint32_t& v)    { return ReadRaw(&v, sizeof(v)); }
	bool ReadFloat(float& v)     { return ReadRaw(&v, sizeof(v)); }
	bool ReadI32(int32_t& v)     { return ReadRaw(&v, sizeof(v)); }
	bool ReadBool(bool& v)       { uint8_t b; if (!ReadRaw(&b, 1)) return false; v = (b != 0); return true; }
	bool ReadSocket(evutil_socket_t& v) { return ReadRaw(&v, sizeof(v)); }

	bool ReadRaw(void* dst, size_t size) {
		if (mOffset + size > mSize) return false;
		std::memcpy(dst, mData + mOffset, size);
		mOffset += size;
		return true;
	}

	uint16_t ReadChangedMask() {
		uint16_t mask = 0;
		if (!ReadU16(mask)) return 0;
		return mask;
	}

	bool HasRemaining() const { return mOffset < mSize; }
	size_t Remaining() const { return mSize - mOffset; }

private:
	const uint8_t* mData;
	size_t mSize;
	size_t mOffset;
};