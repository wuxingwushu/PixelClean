#pragma once
#include "ByteWriter.h"
#include "ByteReader.h"

// IReplicationEvent: 复制事件基类
// 事件是一次的、独立于对象状态的网络消息（如迷宫初始化数据、伤害通知等）
// 与 ReplicableComponent 的区别：
//   - Component：持续存在的状态，通过脏标记增量同步（如玩家位置）
//   - Event：一次性的消息，发送后即消费（如"加载迷宫"、"玩家进入"）
// 两者通过 DataHeader::Key 区分路由（Key=0 走 ApplyReplication，Key=1 走 DispatchEvent）
class IReplicationEvent {
public:
	virtual ~IReplicationEvent() = default;

	// 返回事件类型唯一标识符，用于反序列化时查找对应的处理器
	virtual uint16_t GetEventTypeId() const = 0;

	// 序列化事件数据到 ByteWriter（发送前）
	virtual void Serialize(ByteWriter& writer) = 0;

	// 从 ByteReader 反序列化恢复事件数据（接收后）
	virtual void Deserialize(ByteReader& reader) = 0;
};