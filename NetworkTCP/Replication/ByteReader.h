#pragma once
#include <cstring>
#include <cstdint>
#include <event2/util.h>

// ByteReader: 二进制反序列化读取器
// 从原始字节缓冲区中按顺序读取基础类型数据，通过游标管理读取位置
class ByteReader {
public:
	// 绑定要读取的原始数据和长度
	ByteReader(const uint8_t* data, size_t size)
		: mData(data), mSize(size), mOffset(0) {}

	// 读取各种基础类型，内部通过 ReadRaw 统一处理
	bool ReadU8(uint8_t& v)      { return ReadRaw(&v, sizeof(v)); }
	bool ReadU16(uint16_t& v)    { return ReadRaw(&v, sizeof(v)); }
	bool ReadU32(uint32_t& v)    { return ReadRaw(&v, sizeof(v)); }
	bool ReadFloat(float& v)     { return ReadRaw(&v, sizeof(v)); }
	bool ReadI32(int32_t& v)     { return ReadRaw(&v, sizeof(v)); }
	bool ReadBool(bool& v)       { uint8_t b; if (!ReadRaw(&b, 1)) return false; v = (b != 0); return true; }
	bool ReadSocket(evutil_socket_t& v) { return ReadRaw(&v, sizeof(v)); }

	// 核心读取方法：检查边界后 memcpy 数据，推进游标
	bool ReadRaw(void* dst, size_t size) {
		if (mOffset + size > mSize) return false;
		std::memcpy(dst, mData + mOffset, size);
		mOffset += size;
		return true;
	}

	// 读取 16 位的变更掩码（ChangedMask），用于增量同步标记哪些属性发生变化
	uint16_t ReadChangedMask() {
		uint16_t mask = 0;
		if (!ReadU16(mask)) return 0;
		return mask;
	}

	// 检查是否还有剩余数据未读取
	bool HasRemaining() const { return mOffset < mSize; }
	// 返回剩余可读字节数
	size_t Remaining() const { return mSize - mOffset; }

private:
	const uint8_t* mData;   // 原始数据指针（只读）
	size_t mSize;           // 数据总长度
	size_t mOffset;         // 当前读取游标位置
};