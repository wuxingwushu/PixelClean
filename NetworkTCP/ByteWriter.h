#pragma once
#include <cstring>
#include <cstdint>
#include <vector>
#include <string>
#include <event2/util.h>

// ByteWriter: 二进制序列化写入器
// 将基础类型数据按二进制格式写入内部缓冲区，用于网络传输前的数据打包
class ByteWriter {
public:
	ByteWriter() { mBuffer.reserve(4096); }

	// 写入各种基础类型，内部通过 WriteRaw 统一追加到缓冲区末尾
	void WriteU8(uint8_t v)      { WriteRaw(&v, sizeof(v)); }
	void WriteU16(uint16_t v)    { WriteRaw(&v, sizeof(v)); }
	void WriteU32(uint32_t v)    { WriteRaw(&v, sizeof(v)); }
	void WriteFloat(float v)     { WriteRaw(&v, sizeof(v)); }
	void WriteI32(int32_t v)     { WriteRaw(&v, sizeof(v)); }
	void WriteBool(bool v)       { uint8_t b = v ? 1 : 0; WriteRaw(&b, 1); }
	void WriteSocket(evutil_socket_t v) { WriteRaw(&v, sizeof(v)); }

	// 核心写入方法：将原始字节追加到缓冲区末尾
	void WriteRaw(const void* data, size_t size) {
		const uint8_t* src = static_cast<const uint8_t*>(data);
		mBuffer.insert(mBuffer.end(), src, src + size);
	}

	// 写入字符串：先写入长度（u32），再写入字符串内容
	void WriteString(const std::string& str) {
		WriteU32(static_cast<uint32_t>(str.size()));
		WriteRaw(str.data(), str.size());
	}

	// 写入字节数组：先写入长度（u32），再写入数据（长度前缀格式）
	void WriteBytes(const uint8_t* data, uint32_t size) {
		WriteU32(size);
		if (size > 0) WriteRaw(data, size);
	}

	// 获取已写入数据的指针和大小，供发送使用
	const uint8_t* Data() const { return mBuffer.data(); }
	size_t Size() const { return mBuffer.size(); }
	// 清空缓冲区，准备下次写入
	void Clear() { mBuffer.clear(); }

	// 写入 16 位变更掩码（ChangedMask），用于增量同步
	void WriteChangedMask(uint16_t mask) { WriteU16(mask); }

	// 获取可修改的数据指针，用于回填已写入区域（如会后写入对象/组件数量）
	uint8_t* MutableData() { return mBuffer.data(); }

private:
	std::vector<uint8_t> mBuffer;  // 内部字节缓冲区
};