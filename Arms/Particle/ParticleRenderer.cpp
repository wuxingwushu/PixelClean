#include "ParticleRenderer.h"
#include "../Vulkan/buffer.h"
#include "../Vulkan/device.h"
#include "../Vulkan/commandPool.h"
#include "../Vulkan/commandBuffer.h"
#include "../Vulkan/pipeline.h"
#include "../Vulkan/renderPass.h"
#include "../Vulkan/swapChain.h"
#include "../Vulkan/descriptorSet.h"
#include "../GlobalVariable.h"
#include "../DebugLog.h"
#include <glm/gtc/matrix_transform.hpp>
#include <cstring>

namespace GAME {

ParticleRenderer::ParticleRenderer(VulKan::Device* device,
                                   VulKan::CommandPool* commandPool,
                                   uint32_t frameCount,
                                   uint32_t maxParticles)
	: mDevice(device), mFrameCount(frameCount), mMaxParticles(maxParticles) {
	LOGD("ParticleRenderer::ParticleRenderer(frameCount=%u, maxParticles=%u)", frameCount, maxParticles);

	// 为每帧创建一个 SSBO（持久映射）
	// SSBO 大小 = maxParticles * sizeof(ParticleInstanceData)
	VkDeviceSize ssboSize = static_cast<VkDeviceSize>(maxParticles) * sizeof(ParticleInstanceData);
	mInstanceBuffers.resize(frameCount);
	for (uint32_t i = 0; i < frameCount; ++i) {
		mInstanceBuffers[i] = VulKan::Buffer::createStorageBuffer(device, ssboSize, nullptr, true);
		LOGD("ParticleRenderer: created instance SSBO[%u], size=%llu bytes", i, ssboSize);
	}

	// 为每帧创建一个指令池 + 二级指令缓冲
	mCommandPools.resize(frameCount);
	mCommandBuffers.resize(frameCount);
	for (uint32_t i = 0; i < frameCount; ++i) {
		mCommandPools[i] = new VulKan::CommandPool(device);
		mCommandBuffers[i] = new VulKan::CommandBuffer(device, mCommandPools[i], true); // true = secondary
	}
}

ParticleRenderer::~ParticleRenderer() {
	for (uint32_t i = 0; i < mFrameCount; ++i) {
		delete mCommandBuffers[i];
		delete mCommandPools[i];
		delete mInstanceBuffers[i];
	}
}

void ParticleRenderer::setGeometry(VulKan::Buffer* posBuf, VulKan::Buffer* uvBuf,
                                   VulKan::Buffer* idxBuf, uint32_t indexCount) {
	mPositionBuffer = posBuf;
	mUVBuffer = uvBuf;
	mIndexBuffer = idxBuf;
	mIndexCount = indexCount;
}

void ParticleRenderer::uploadInstanceData(const std::vector<ParticleCPUData>& cpuData,
                                          uint32_t activeCount,
                                          uint32_t frameIndex) {
	if (frameIndex >= mFrameCount) return;

	VulKan::Buffer* ssbo = mInstanceBuffers[frameIndex];
	void* mapped = ssbo->getPersistentMappedPtr();
	if (mapped == nullptr) {
		LOGE("ParticleRenderer::uploadInstanceData: SSBO[%u] mapped ptr is null", frameIndex);
		return;
	}

	// 遍历活跃粒子，把 ParticleCPUData 转成 ParticleInstanceData
	// modelMatrix = translate * rotate * scale
	auto* instances = static_cast<ParticleInstanceData*>(mapped);
	uint32_t written = 0;
	for (uint32_t i = 0; i < cpuData.size() && written < activeCount; ++i) {
		const ParticleCPUData& d = cpuData[i];
		if (!d.alive) continue;

		// DEBUG: 打印前 3 个粒子的 zoom 值
		if (written < 3) {
			LOGI("[ParticleRenderer] upload[%u]: idx=%u zoom=%.2f x=%.1f y=%.1f angle=%.2f color=(%.2f,%.2f,%.2f,%.2f)",
				written, i, d.zoom, d.x, d.y, d.angle,
				d.color.r, d.color.g, d.color.b, d.color.a);
		}

		// 构建模型矩阵：translate(x,y,0) * rotate(angle) * scale(zoom)
		glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(d.x, d.y, 0.0f));
		model = glm::rotate(model, d.angle, glm::vec3(0.0f, 0.0f, 1.0f));
		model = glm::scale(model, glm::vec3(d.zoom, d.zoom, 1.0f));

		instances[written].modelMatrix = model;
		instances[written].color = d.color;
		++written;
	}

	LOGI("[ParticleRenderer] uploadInstanceData: activeCount=%u written=%u recorded=%u",
	     activeCount, written, mRecordedInstanceCount);

	// 隐藏 [written, mRecordedInstanceCount) 区间的槽位（对应 AuxiliaryVision::HideUnusedVertices）
	// 用 z=-10000 的模型矩阵覆盖，被近裁剪面剔除，避免残留渲染
	if (written < mRecordedInstanceCount) {
		glm::mat4 hidden = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -10000.0f));
		for (uint32_t i = written; i < mRecordedInstanceCount; ++i) {
			instances[i].modelMatrix = hidden;
			instances[i].color = glm::vec4(0.0f);
		}
	}
}

void ParticleRenderer::recordFrame(uint32_t frameIndex,
                                   VulKan::RenderPass* rp,
                                   VulKan::SwapChain* sc,
                                   VulKan::Pipeline* pipeline,
                                   VulKan::DescriptorSet* descriptorSet,
                                   uint32_t instanceCount) {
	if (frameIndex >= mFrameCount || instanceCount == 0) return;

	VulKan::CommandBuffer* cb = mCommandBuffers[frameIndex];

	VkCommandBufferInheritanceInfo inheritanceInfo{};
	inheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
	inheritanceInfo.renderPass = rp->getRenderPass();
	inheritanceInfo.framebuffer = sc->getFrameBuffer(frameIndex);

	cb->begin(VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT, inheritanceInfo);
	cb->bindGraphicPipeline(pipeline->getPipeline());

	// 绑定顶点缓冲（position + UV）
	std::vector<VkBuffer> vertexBuffers = { mPositionBuffer->getBuffer(), mUVBuffer->getBuffer() };
	cb->bindVertexBuffer(vertexBuffers);
	cb->bindIndexBuffer(mIndexBuffer->getBuffer());

	// 绑定描述符集（binding 0: VP UBO, binding 1: Instance SSBO）
	cb->bindDescriptorSet(pipeline->getLayout(), descriptorSet->getDescriptorSet(frameIndex));

	// 1 次 draw：indexCount 个索引，instanceCount 个实例
	// 直接调用 vkCmdDrawIndexed 以支持 instanceCount 参数（CommandBuffer::drawIndex 硬编码 instanceCount=1）
	VkCommandBuffer rawCb = cb->getCommandBuffer();
	vkCmdDrawIndexed(rawCb, mIndexCount, instanceCount, 0, 0, 0);

	cb->end();
}

void ParticleRenderer::getCommandBuffer(std::vector<VkCommandBuffer>* out, uint32_t frameIndex) {
	if (frameIndex >= mFrameCount) return;
	// 始终推送句柄：已录 instanceCount 由 mRecordedInstanceCount 决定，
	// 活跃数为 0 时靠 uploadInstanceData 中的隐藏槽位处理（参考 AuxiliaryVision）
	out->push_back(mCommandBuffers[frameIndex]->getCommandBuffer());
}

void ParticleRenderer::initialRecord(VulKan::RenderPass* rp,
                                     VulKan::SwapChain* sc,
                                     VulKan::Pipeline* pipeline,
                                     VulKan::DescriptorSet* descriptorSet) {
	// 初始录制一个带滞后余量的容量（参考 AuxiliaryVision 初始 mLineRecordedCount）
	mRecordedInstanceCount = kInstanceCountBuffer;
	if (mRecordedInstanceCount > mMaxParticles) mRecordedInstanceCount = mMaxParticles;
	for (uint32_t i = 0; i < mFrameCount; ++i) {
		recordFrame(i, rp, sc, pipeline, descriptorSet, mRecordedInstanceCount);
	}
	LOGI("[ParticleRenderer] initialRecord: recordedInstanceCount=%u", mRecordedInstanceCount);
}

bool ParticleRenderer::checkAndReRecord(uint32_t activeCount,
                                        VulKan::RenderPass* rp,
                                        VulKan::SwapChain* sc,
                                        VulKan::Pipeline* pipeline,
                                        VulKan::DescriptorSet* descriptorSet) {
	bool needReRecord = false;
	const uint32_t maxThreshold = kInstanceCountBuffer * 2;

	if (activeCount > mRecordedInstanceCount) {
		// 活跃数增长超过已录容量 → 扩容
		mRecordedInstanceCount = activeCount + kInstanceCountBuffer;
		if (mRecordedInstanceCount > mMaxParticles) mRecordedInstanceCount = mMaxParticles;
		needReRecord = true;
	} else if (mRecordedInstanceCount > maxThreshold &&
	           activeCount + maxThreshold < mRecordedInstanceCount) {
		// 活跃数显著缩减 → 缩容（避免浪费 GPU 绘制隐藏实例）
		mRecordedInstanceCount = activeCount + kInstanceCountBuffer;
		if (mRecordedInstanceCount > mMaxParticles) mRecordedInstanceCount = mMaxParticles;
		needReRecord = true;
	}

	if (needReRecord) {
		for (uint32_t i = 0; i < mFrameCount; ++i) {
			recordFrame(i, rp, sc, pipeline, descriptorSet, mRecordedInstanceCount);
		}
		Global::MainCommandBufferUpdateRequest();
		LOGI("[ParticleRenderer] checkAndReRecord: re-recorded, activeCount=%u newRecorded=%u",
		     activeCount, mRecordedInstanceCount);
	}
	return needReRecord;
}

void ParticleRenderer::resetCommandPools() {
	for (uint32_t i = 0; i < mFrameCount; ++i) {
		mCommandPools[i]->reset();
	}
}

} // namespace GAME
