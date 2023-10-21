#include "AuxiliaryVision.h"
#include "../GlobalStructural.h"

namespace VulKan {

	AuxiliaryVision::AuxiliaryVision(VulKan::Device* device, PipelineS* P, const unsigned int number):
		wDevice(device),
		wPipelineS(P),
		Number(number)
	{
		//线段
		AuxiliaryLineS = new Buffer(
			wDevice, sizeof(AuxiliarySpot) * Number * 2,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
		);
		ContinuousAuxiliaryLine = new ContinuousMap<glm::vec2*, AuxiliaryLineData>(Number);
		ContinuousAuxiliaryForce = new ContinuousMap<glm::vec2*, AuxiliaryForceData>(Number);
		StaticContinuousAuxiliaryLine = new ContinuousMap<void*, StaticAuxiliaryData>(Number, ContinuousMap_New);
		AuxiliarySpot* LP = (AuxiliarySpot*)AuxiliaryLineS->getupdateBufferByMap();
		for (size_t i = 0; i < (Number * 2); ++i)
		{
			LP[i].Pos = { i, i, -10000.0f };
			LP[i].Color = { 0, 1.0f, 0, 1.0f };
		}
		AuxiliaryLineS->endupdateBufferByMap();
		//点
		AuxiliarySpotS = new Buffer(
			wDevice, sizeof(AuxiliarySpot) * Number,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
		);
		ContinuousAuxiliarySpot = new ContinuousMap<glm::vec2*, AuxiliarySpotData>(Number);
		StaticContinuousAuxiliarySpot = new ContinuousMap<void*, StaticAuxiliaryData>(Number, ContinuousMap_New);
		LP = (AuxiliarySpot*)AuxiliarySpotS->getupdateBufferByMap();
		for (size_t i = 0; i < Number; ++i)
		{
			LP[i].Pos = { i, i, -10000.0f };
			LP[i].Color = { 0, 0, 1.0f, 1.0f };
		}
		AuxiliarySpotS->endupdateBufferByMap();
	}

	AuxiliaryVision::~AuxiliaryVision()
	{
		//线段
		delete AuxiliaryLineS;
		delete ContinuousAuxiliaryLine;
		delete ContinuousAuxiliaryForce;
		delete StaticContinuousAuxiliaryLine;

		//点
		delete AuxiliarySpotS;
		delete ContinuousAuxiliarySpot;
		delete StaticContinuousAuxiliarySpot;

		//资源
		for (size_t i = 0; i < wSwapChain->getImageCount(); ++i)
		{
			delete mCommandBuffer[i];
		}
		delete mCommandBuffer;
		delete mDescriptorSetLine;
		delete mDescriptorSetSpot;
		delete mDescriptorPool;
		delete mCommandPool;
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
		VkCommandBufferInheritanceInfo InheritanceInfo{};
		InheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
		InheritanceInfo.renderPass = wRenderPass->getRenderPass();
		for (size_t i = 0; i < wSwapChain->getImageCount(); ++i)
		{
			InheritanceInfo.framebuffer = wSwapChain->getFrameBuffer(i);

			mCommandBuffer[i]->begin(VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT, InheritanceInfo);//开始录制二级指令
			//点
			mCommandBuffer[i]->bindGraphicPipeline(wPipelineS->GetPipeline(PipelineMods::SpotMods)->getPipeline());//获得渲染管线
			mCommandBuffer[i]->bindVertexBuffer({ AuxiliarySpotS->getBuffer() });
			mCommandBuffer[i]->bindDescriptorSet(wPipelineS->GetPipeline(PipelineMods::SpotMods)->getLayout(), mDescriptorSetSpot->getDescriptorSet(i));//获得 模型位置数据， 贴图数据，……
			mCommandBuffer[i]->draw(Number);//获取绘画物体的顶点个数
			//线
			mCommandBuffer[i]->bindGraphicPipeline(wPipelineS->GetPipeline(PipelineMods::LineMods)->getPipeline());//获得渲染管线
			mCommandBuffer[i]->bindVertexBuffer({ AuxiliaryLineS->getBuffer() });
			mCommandBuffer[i]->bindDescriptorSet(wPipelineS->GetPipeline(PipelineMods::LineMods)->getLayout(), mDescriptorSetLine->getDescriptorSet(i));//获得 模型位置数据， 贴图数据，……
			mCommandBuffer[i]->draw(Number);//获取绘画物体的顶点个数

			mCommandBuffer[i]->end();
		}
	}

	void AuxiliaryVision::Begin() {
		//线
		LinePointerHOST = (AuxiliarySpot*)AuxiliaryLineS->getupdateBufferByMap();
		//静态
		if (StaticLineVertexUpData) {
			StaticLineVertexUpData = false;
			StaticLineUpData = true;
			StaticLineVertexDeviation = 0;
			for (auto L : StaticLineVertex)
			{
				*LinePointerHOST = L;
				++LinePointerHOST;
				++StaticLineVertexDeviation;
			}
			LineNumber = StaticLineVertexDeviation;
		}
		else {
			LineNumber = StaticLineVertexDeviation;
			LinePointerHOST += StaticLineVertexDeviation;
		}
		if (StaticLineUpData) {
			StaticLineUpData = false;
			int Number;
			StaticLineDeviation = 0;
			for (auto& i : *StaticContinuousAuxiliaryLine) {
				if (i.Size != 0) {
					Number = i.Function(LinePointerHOST, i.Pointer, i.Size);
					StaticLineDeviation += Number;
					LinePointerHOST += Number;
				}
			}
			LineNumber += StaticLineDeviation;
		}
		else {
			LineNumber += StaticLineDeviation;
			LinePointerHOST += StaticLineDeviation;
		}
		//动态
		for (auto& i : *ContinuousAuxiliaryLine)
		{
			LinePointerHOST->Pos = { *i.Head, 0.0f};
			LinePointerHOST->Color = i.Color;
			++LinePointerHOST;
			LinePointerHOST->Pos = { *i.Tail, 0.0f };
			LinePointerHOST->Color = i.Color;
			++LinePointerHOST;
		}
		for (auto& i : *ContinuousAuxiliaryForce)
		{
			LinePointerHOST->Pos = { *i.pos, 0.0f };
			LinePointerHOST->Color = i.Color;
			++LinePointerHOST;
			LinePointerHOST->Pos = { (*i.pos + *i.Force), 0.0f };
			LinePointerHOST->Color = i.Color;
			++LinePointerHOST;
		}
		LineNumber += ((ContinuousAuxiliaryLine->GetNumber() + ContinuousAuxiliaryForce->GetNumber()) * 2);
		//一次性
		LineNumber += LineVertex.size();
		while (LineVertex.size() != 0)
		{
			*LinePointerHOST = LineVertex.back();
			++LinePointerHOST;
			LineVertex.pop_back();
		}
		
		//点
		SpotPointerHOST = (AuxiliarySpot*)AuxiliarySpotS->getupdateBufferByMap();
		//静态
		if (StaticSpotVertexUpData) {
			StaticSpotVertexUpData = false;
			StaticSpotUpData = true;
			StaticSpotVertexDeviation = 0;
			for (auto S : StaticSpotVertex)
			{
				*SpotPointerHOST = S;
				++SpotPointerHOST;
				++StaticSpotVertexDeviation;
			}
			SpotNumber = StaticSpotVertexDeviation;
		}
		else {
			SpotNumber = StaticSpotVertexDeviation;
			SpotPointerHOST += StaticSpotVertexDeviation;
		}
		if (StaticSpotUpData) {
			StaticSpotUpData = false;
			StaticSpotDeviation = 0;
			int Number;
			for (auto& i : *StaticContinuousAuxiliarySpot) {
				if (i.Size != 0) {
					Number = i.Function(SpotPointerHOST, i.Pointer, i.Size);
					StaticSpotDeviation += Number;
					SpotPointerHOST += Number;
				}
			}
			SpotNumber += StaticSpotDeviation;
		}
		else {
			SpotNumber += StaticSpotDeviation;
			SpotPointerHOST += SpotNumber;
		}
		//动态
		SpotNumber += ContinuousAuxiliarySpot->GetNumber();
		for (auto& i : *ContinuousAuxiliarySpot)
		{
			SpotPointerHOST->Pos = { *i.pos, 0.0f };
			SpotPointerHOST->Color = i.Color;
			++SpotPointerHOST;
		}
		//一次性
		SpotNumber += SpotVertex.size();
		while (SpotVertex.size() != 0)
		{
			*SpotPointerHOST = SpotVertex.back();
			++SpotPointerHOST;
			SpotVertex.pop_back();
		}
	}

	void AuxiliaryVision::End() {
		//线段多余
		if (LineMax > LineNumber) {
			for (size_t i = LineNumber; i <= LineMax; ++i)
			{
				LinePointerHOST->Pos.z = -10000.0f;
				++LinePointerHOST;
			}
		}
		LineMax = LineNumber;
		AuxiliaryLineS->endupdateBufferByMap();
		LinePointerHOST = nullptr;

		//点多余
		if (SpotMax > SpotNumber) {
			for (size_t i = SpotNumber; i <= SpotMax; ++i)
			{
				SpotPointerHOST->Pos.z = -10000.0f;
				++SpotPointerHOST;
			}
		}
		SpotMax = SpotNumber;
		AuxiliarySpotS->endupdateBufferByMap();
		SpotPointerHOST = nullptr;
	}
}