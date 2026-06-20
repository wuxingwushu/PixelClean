#pragma once
// 粒子系统重构 - 方案 D
// 实例化渲染器：SSBO + Instanced Rendering，1 次 bind + 1 次 draw
#include "ParticleCommon.h"
#include "ParticlePool.h"
#include <vulkan/vulkan.h>
#include <vector>

namespace VulKan { class Buffer; class DescriptorSet; class Pipeline; class Device; class CommandPool; class CommandBuffer; class RenderPass; class SwapChain; }

namespace GAME {

class ParticleRenderer {
public:
	ParticleRenderer(VulKan::Device* device,
	                 VulKan::CommandPool* commandPool,
	                 uint32_t frameCount,
	                 uint32_t maxParticles);
	~ParticleRenderer();

	// 每帧录制前：把 CPU 数据写入 SSBO（持久映射内存）
	// 遍历活跃粒子，把 ParticleCPUData 转成 ParticleInstanceData（modelMatrix = translate * rotate * scale）
	// 隐藏区间为 [written, mRecordedInstanceCount)（对应 AuxiliaryVision::HideUnusedVertices）
	void uploadInstanceData(const std::vector<ParticleCPUData>& cpuData,
	                        uint32_t activeCount,
	                        uint32_t frameIndex);

	// 录制单帧的二级指令缓冲（1 次 bind + 1 次 draw）
	void recordFrame(uint32_t frameIndex,
	                 VulKan::RenderPass* rp,
	                 VulKan::SwapChain* sc,
	                 VulKan::Pipeline* pipeline,
	                 VulKan::DescriptorSet* descriptorSet,
	                 uint32_t instanceCount);

	// 初始录制：在 recordCommandBuffer 时调用一次，录制 kInstanceCountBuffer 个实例
	void initialRecord(VulKan::RenderPass* rp,
	                   VulKan::SwapChain* sc,
	                   VulKan::Pipeline* pipeline,
	                   VulKan::DescriptorSet* descriptorSet);

	// 阈值检查：需要重录时重录全部帧的二级 CB 并请求主 CB 更新（参考 AuxiliaryVision::End）
	// 返回是否触发了重录
	bool checkAndReRecord(uint32_t activeCount,
	                      VulKan::RenderPass* rp,
	                      VulKan::SwapChain* sc,
	                      VulKan::Pipeline* pipeline,
	                      VulKan::DescriptorSet* descriptorSet);

	// 获取指定帧的指令缓冲（推入 out 数组）
	// 始终推送句柄（已录 instanceCount 由 mRecordedInstanceCount 决定，活跃数为 0 时靠隐藏槽位）
	void getCommandBuffer(std::vector<VkCommandBuffer>* out, uint32_t frameIndex);

	// 获取已录实例数
	uint32_t getRecordedInstanceCount() const noexcept { return mRecordedInstanceCount; }

	// 重置所有指令池（窗口重建时调用）
	void resetCommandPools();

	// 资源访问（供 ParticleWorld 组装 DescriptorSet）
	VulKan::Buffer* getInstanceBuffer(uint32_t frameIndex) const { return mInstanceBuffers[frameIndex]; }
	uint32_t        getMaxParticles() const noexcept { return mMaxParticles; }

	// 设置共享几何体（由 ParticleWorld 传入）
	void setGeometry(VulKan::Buffer* posBuf, VulKan::Buffer* uvBuf,
	                 VulKan::Buffer* idxBuf, uint32_t indexCount);

private:
	VulKan::Device*       mDevice{ nullptr };
	uint32_t              mFrameCount{ 0 };
	uint32_t              mMaxParticles{ 0 };

	// 滞后余量：避免活跃数微小波动导致频繁重录（参考 AuxiliaryVision::kVertexCountBuffer）
	static constexpr uint32_t kInstanceCountBuffer = 32;

	// 已录入命令缓冲区的实例数（参考 AuxiliaryVision::mLineRecordedCount）
	uint32_t mRecordedInstanceCount{ 0 };

	// 每帧一个 SSBO（双/多缓冲，避免帧内写冲突）
	std::vector<VulKan::Buffer*> mInstanceBuffers;

	// 每帧一个指令池 + 二级指令缓冲
	std::vector<VulKan::CommandPool*>   mCommandPools;
	std::vector<VulKan::CommandBuffer*> mCommandBuffers;

	// 共享几何体（由 World 持有所有权，Renderer 仅引用）
	VulKan::Buffer* mPositionBuffer{ nullptr };
	VulKan::Buffer* mUVBuffer{ nullptr };
	VulKan::Buffer* mIndexBuffer{ nullptr };
	uint32_t        mIndexCount{ 0 };
};

} // namespace GAME
