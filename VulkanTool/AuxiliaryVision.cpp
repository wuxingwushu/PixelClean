#include "AuxiliaryVision.h"

namespace VulKan {

	AuxiliaryVision::AuxiliaryVision(VulKan::Device* device, PipelineS* P, const unsigned int number):
		wDevice(device),
		wPipelineS(P),
		Number(number)
	{
		//线段
		AuxiliaryLineS = new Buffer(
			wDevice, sizeof(AuxiliaryLine) * Number * 2,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
		);
		ContinuousAuxiliaryLine = new ContinuousMap<glm::vec2*, AuxiliaryLineData>(Number);
		ContinuousAuxiliaryForce = new ContinuousMap<glm::vec2*, AuxiliaryForceData>(Number);
		AuxiliaryLine* LP = (AuxiliaryLine*)AuxiliaryLineS->getupdateBufferByMap();
		for (size_t i = 0; i < (Number * 2); ++i)
		{
			LP[i].Pos = { i, i, -10000.0f };
			LP[i].Color = { 0, 1.0f, 0, 1.0f };
		}
		AuxiliaryLineS->endupdateBufferByMap();
		//点
		AuxiliarySpotS = new Buffer(
			wDevice, sizeof(AuxiliaryLine) * Number,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
		);
		ContinuousAuxiliarySpot = new ContinuousMap<glm::vec2*, AuxiliarySpotData>(Number);
		LP = (AuxiliaryLine*)AuxiliarySpotS->getupdateBufferByMap();
		for (size_t i = 0; i < Number; ++i)
		{
			LP[i].Pos = { i, i, -10000.0f };
			LP[i].Color = { 0, 0, 1.0f, 1.0f };
		}
		AuxiliarySpotS->endupdateBufferByMap();
	}

	AuxiliaryVision::~AuxiliaryVision()
	{
	}

	//初始化描述符
	void AuxiliaryVision::initUniformManager(
		int frameCount, //GPU画布的数量
		std::vector<VulKan::Buffer*> VPMstdBuffer//玩家相机的变化矩阵 
	) {
		mCommandPool = new CommandPool(wDevice);
		mCommandBuffer = new CommandBuffer*[frameCount];
		for (size_t i = 0; i < frameCount; ++i)
		{
			mCommandBuffer[i] = new CommandBuffer(wDevice, mCommandPool, true);
		}

		VulKan::UniformParameter vpParam = {};
		vpParam.mBinding = 0;
		vpParam.mCount = 1;
		vpParam.mDescriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		vpParam.mSize = sizeof(VPMatrices);
		vpParam.mStage = VK_SHADER_STAGE_VERTEX_BIT;
		vpParam.mBuffers = VPMstdBuffer;

		std::vector<UniformParameter*> paramsvector;
		paramsvector.push_back(&vpParam);
		//各种类型申请多少个
		mDescriptorPool = new VulKan::DescriptorPool(wDevice);
		mDescriptorPool->build(paramsvector, frameCount * 2);

		//将申请的各种类型按照Layout绑定起来
		mDescriptorSetLine = new VulKan::DescriptorSet(wDevice, paramsvector, wPipelineS->GetPipeline(VulKan::PipelineMods::LineMods)->DescriptorSetLayout, mDescriptorPool, frameCount);
		mDescriptorSetSpot = new VulKan::DescriptorSet(wDevice, paramsvector, wPipelineS->GetPipeline(VulKan::PipelineMods::SpotMods)->DescriptorSetLayout, mDescriptorPool, frameCount);
	}

	void AuxiliaryVision::initCommandBuffer() {
		for (size_t i = 0; i < wSwapChain->getImageCount(); ++i)
		{
			VkCommandBufferInheritanceInfo InheritanceInfo{};
			InheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
			InheritanceInfo.renderPass = wRenderPass->getRenderPass();
			InheritanceInfo.framebuffer = wSwapChain->getFrameBuffer(i);

			mCommandBuffer[i]->begin(VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT, InheritanceInfo);//开始录制二级指令
			//线
			mCommandBuffer[i]->bindGraphicPipeline(wPipelineS->GetPipeline(PipelineMods::LineMods)->getPipeline());//获得渲染管线
			mCommandBuffer[i]->bindVertexBuffer({ AuxiliaryLineS->getBuffer()});
			mCommandBuffer[i]->bindDescriptorSet(wPipelineS->GetPipeline(PipelineMods::LineMods)->getLayout(), mDescriptorSetLine->getDescriptorSet(i));//获得 模型位置数据， 贴图数据，……
			mCommandBuffer[i]->draw(Number);//获取绘画物体的顶点个数
			//点
			mCommandBuffer[i]->bindGraphicPipeline(wPipelineS->GetPipeline(PipelineMods::SpotMods)->getPipeline());//获得渲染管线
			mCommandBuffer[i]->bindVertexBuffer({ AuxiliarySpotS->getBuffer() });
			mCommandBuffer[i]->bindDescriptorSet(wPipelineS->GetPipeline(PipelineMods::SpotMods)->getLayout(), mDescriptorSetSpot->getDescriptorSet(i));//获得 模型位置数据， 贴图数据，……
			mCommandBuffer[i]->draw(Number);//获取绘画物体的顶点个数

			mCommandBuffer[i]->end();
		}
	}

	void AuxiliaryVision::UpDataLine() {
		//线
		AuxiliaryLine* LP = (AuxiliaryLine*)AuxiliaryLineS->getupdateBufferByMap();
		for (auto& i : *ContinuousAuxiliaryLine)
		{
			LP->Pos = { *i.Head, 0.0f};
			LP->Color = i.Color;
			++LP;
			LP->Pos = { *i.Tail, 0.0f };
			LP->Color = i.Color;
			++LP;
		}
		for (auto& i : *ContinuousAuxiliaryForce)
		{
			LP->Pos = { *i.pos, 0.0f };
			LP->Color = i.Color;
			++LP;
			LP->Pos = { (*i.pos + *i.Force), 0.0f };
			LP->Color = i.Color;
			++LP;
		}
		int shu = LineVertex.size();
		while (LineVertex.size() != 0)
		{
			*LP = LineVertex.back();
			++LP;
			LineVertex.pop_back();
		}
		shu += ((ContinuousAuxiliaryLine->GetNumber() + ContinuousAuxiliaryForce->GetNumber()) * 2);
		if (LineMax > shu) {
			for (size_t i = shu; i <= LineMax; ++i)
			{
				LP->Pos.z = -10000.0f;
				++LP;
			}
		}
		LineMax = shu;
		AuxiliaryLineS->endupdateBufferByMap();
		//点
		LP = (AuxiliaryLine*)AuxiliarySpotS->getupdateBufferByMap();
		shu = ContinuousAuxiliarySpot->GetNumber();
		for (auto& i : *ContinuousAuxiliarySpot)
		{
			LP->Pos = { *i.pos, 0.0f };
			LP->Color = i.Color;
			++LP;
		}
		shu += SpotVertex.size();
		while (SpotVertex.size() != 0)
		{
			*LP = SpotVertex.back();
			++LP;
			SpotVertex.pop_back();
		}
		if (SpotMax > shu) {
			for (size_t i = shu; i <= SpotMax; ++i)
			{
				LP->Pos.z = -10000.0f;
				++LP;
			}
		}
		SpotMax = shu;
		AuxiliarySpotS->endupdateBufferByMap();
	}

}