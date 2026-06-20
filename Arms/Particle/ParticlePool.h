#pragma once
// 粒子系统重构 - 方案 D
// 索引池（LIFO 栈）：替代旧的 PileUp<Particle>，只存索引不存指针
#include <vector>
#include <cstdint>

namespace GAME {

class ParticlePool {
public:
	explicit ParticlePool(uint32_t capacity);

	// 分配一个索引，返回 -1 表示池空
	int32_t allocate();

	// 归还索引
	void free(uint32_t index);

	[[nodiscard]] uint32_t capacity() const noexcept { return mCapacity; }
	[[nodiscard]] uint32_t freeCount() const noexcept { return static_cast<uint32_t>(mFreeList.size()); }
	[[nodiscard]] uint32_t activeCount() const noexcept { return mCapacity - freeCount(); }

private:
	uint32_t              mCapacity;
	std::vector<uint32_t> mFreeList;   // 空闲索引栈（LIFO）
};

} // namespace GAME
