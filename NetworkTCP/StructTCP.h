#pragma once
#include "zlib/zlib.h"

// Zip: zlib 压缩/解压流封装
// 持有独立的 deflate（压缩）和 inflate（解压）流
// 用于 NetworkLayer 的 bufferevent 过滤器，对网络数据进行透明压缩
// 数据流方向：FilterOut（压缩发送）→ deflate，FilterIn（解压接收）→ inflate
struct Zip {
	z_stream* y;  // deflate 压缩流（压缩发送方向的数据）
	z_stream* j;  // inflate 解压流（解压接收方向的数据）

	Zip() {
		y = new z_stream();
		deflateInit(y, Z_DEFAULT_COMPRESSION);
		j = new z_stream();
		inflateInit(j);
	}

	~Zip() {
		delete y;
		delete j;
	}
};

// DataHeader: 网络数据包头
// 每个网络数据包前面都有此头部，标识数据用途和长度
// Key 字段的路由规则：
//   Key = 0 → 状态复制数据（ReplicationManager::ApplyReplication）
//   Key = 1 → 事件数据（ReplicationManager::DispatchEvent）
struct DataHeader
{
	unsigned int Key;   // 路由键：0=状态复制，1=事件
	unsigned int Size;  // 载荷数据长度（字节）
};