#include "ParticlePool.h"
#include "../DebugLog.h"

namespace GAME {

ParticlePool::ParticlePool(uint32_t capacity) : mCapacity(capacity) {
	// 构造时 mFreeList = [capacity-1, capacity-2, ..., 1, 0]
	// 这样 allocate() pop_back 得到 0, 1, 2, ...（顺序分配，缓存友好）
	mFreeList.reserve(capacity);
	for (uint32_t i = capacity; i > 0; --i) {
		mFreeList.push_back(i - 1);
	}
}

int32_t ParticlePool::allocate() {
	if (mFreeList.empty()) {
		return -1;  // 池空
	}
	uint32_t index = mFreeList.back();
	mFreeList.pop_back();
	return static_cast<int32_t>(index);
}

void ParticlePool::free(uint32_t index) {
	if (index >= mCapacity) {
		return;
	}
	mFreeList.push_back(index);
}

} // namespace GAME
