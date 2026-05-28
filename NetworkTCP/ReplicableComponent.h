#pragma once
#include "ByteWriter.h"
#include "ByteReader.h"
#include <cstdint>

// ReplicableComponent: 网络可复制组件基类
// 所有需要通过复制系统同步的组件都必须继承此类
// 每个组件类型有唯一的 GetComponentTypeId() 标识
// 组件负责自身的序列化/反序列化，以及脏标记管理
class ReplicableComponent {
public:
	virtual ~ReplicableComponent() = default;

	// 返回组件类型唯一标识符，用于网络传输时识别组件类型
	virtual uint16_t GetComponentTypeId() const = 0;

	// 将组件状态序列化为二进制数据（写入 ByteWriter）
	virtual void Serialize(ByteWriter& writer) = 0;

	// 从二进制数据反序列化恢复组件状态（从 ByteReader 读取）
	virtual void Deserialize(ByteReader& reader) = 0;

	// 是否有任何属性自上次发送后发生了变化（增量同步核心）
	virtual bool IsDirty() const = 0;

	// 清除所有属性的脏标记，在 Tick() 发送完成后调用
	virtual void ClearAllDirty() = 0;

	// 强制标记所有属性为脏，用于 FullSync 全量同步场景
	virtual void ForceAllDirty() = 0;
};