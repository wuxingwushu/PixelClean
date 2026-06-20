#pragma once
// 粒子系统重构 - 方案 D（Component-Based Pool）
// 公共结构体定义：SSBO 实例数据 + CPU 侧粒子属性
#include <glm/glm.hpp>
#include <cstdint>

namespace GAME {

// SSBO 中单个粒子的数据，std140 对齐
// 总大小: 64 (mat4) + 16 (vec4) = 80 字节，满足 16 字节对齐
struct ParticleInstanceData {
	glm::mat4 modelMatrix;   // 64 字节
	glm::vec4 color;         // 16 字节 (RGBA)
};

// 粒子类型：区分子弹和特效（方案 6A：共用 Pool）
enum class ParticleType : uint8_t {
	Effect = 0,  // 粒子特效（自由运动学）
	Bullet = 1,  // 子弹（跟随物理粒子）
};

// CPU 侧粒子属性（用于 Updater 计算）
struct ParticleCPUData {
	float x{ 0.0f };
	float y{ 0.0f };
	float speed{ 0.0f };
	float angle{ 0.0f };       // 运动方向（弧度）
	float zoom{ 1.0f };        // 缩放
	float zoomDecay{ 0.3f };   // 缩放衰减系数（原硬编码 0.3f）
	glm::vec4 color{ 1.0f };   // RGBA
	bool  alive{ false };      // 是否活跃
	ParticleType type{ ParticleType::Effect };  // 粒子类型

	// 子弹专用：物理粒子指针（Type::Bullet 时由 Updater 回调同步位置）
	// 用 void* 避免引入 PhysicsBlock 头依赖
	void* physicsParticle{ nullptr };
};

// 粒子最大数量上限（SSBO 大小据此分配）
constexpr uint32_t MAX_PARTICLES = 8192;

} // namespace GAME
