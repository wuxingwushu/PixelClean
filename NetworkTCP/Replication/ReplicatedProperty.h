#pragma once
#include <cstring>
#include <cmath>

// ReplicatedProperty: 网络复制属性模板
// 封装一个需要网络同步的值，自动追踪"脏"标记（只有值变化时才标记为脏），
// 用于增量同步——只有变化的数据才会被序列化和发送。
// 注意：拷贝构造/赋值被删除，只允许移动语义，防止意外的值复制导致脏标记不一致
template<typename T>
class ReplicatedProperty {
public:
	ReplicatedProperty() = default;

	ReplicatedProperty(const ReplicatedProperty&) = delete;
	ReplicatedProperty& operator=(const ReplicatedProperty&) = delete;
	ReplicatedProperty(ReplicatedProperty&&) = default;
	ReplicatedProperty& operator=(ReplicatedProperty&&) = default;

	// 设置新值，仅当值发生变化时才标记为脏
	void Set(const T& value) {
		if (mValue != value) {
			mValue = value;
			mDirty = true;
		}
	}

	// 读取当前值
	const T& Get() const { return mValue; }

	// 强制标记为脏，用于确保下一帧一定发送（如玩家位置即使静止不动也要定期发送）
	void ForceDirty() { mDirty = true; }

	bool IsDirty() const { return mDirty; }
	void ClearDirty() { mDirty = false; }

	// 隐式转换为 const T&，方便直接使用
	operator const T&() const { return mValue; }

private:
	T mValue{};          // 实际存储的值，默认初始化
	bool mDirty = false; // 脏标记：true 表示值已变化需要同步
};

// float 类型的特化：使用 epsilon 容差比较（0.001f），
// 避免浮点精度导致的微小抖动不断触发同步
template<>
inline void ReplicatedProperty<float>::Set(const float& value) {
	if (std::abs(mValue - value) > 0.001f) {
		mValue = value;
		mDirty = true;
	}
};