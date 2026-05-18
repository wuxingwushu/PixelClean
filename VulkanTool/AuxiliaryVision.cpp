#include "AuxiliaryVision.h"
#include "../GlobalStructural.h"
#include "../GlobalVariable.h"
#include "../DebugLog.h"

namespace VulKan {

namespace {

	template<typename TVertex>
	bool ProcessStaticVertices(
		TVertex*& ptr, const TVertex* bufferEnd,
		std::vector<TVertex>& staticVertices,
		StaticVertexTracker& tracker,
		unsigned int& deviation,
		unsigned int& number)
	{
		if (!tracker.UpData) {
			number = deviation;
			ptr += deviation;
			return false;
		}
		tracker.UpData = false;
		if (tracker.FirstNewIndex > staticVertices.size()) {
			tracker.FirstNewIndex = 0;
			deviation = 0;
		}
		else {
			ptr += tracker.FirstNewIndex;
			deviation = tracker.FirstNewIndex;
		}
		for (size_t i = tracker.FirstNewIndex; i < staticVertices.size(); ++i) {
			if (ptr >= bufferEnd) break;
			*ptr = staticVertices[i];
			++ptr;
			++deviation;
		}
		tracker.FirstNewIndex = staticVertices.size();
		number = deviation;
		return true;
	}

	template<typename TVertex, typename TStaticData>
	void ProcessStaticCallbacks(
		TVertex*& ptr, const TVertex* bufferEnd,
		ContinuousMap<void*, TStaticData>* callbackMap,
		bool& upData,
		unsigned int& deviation,
		unsigned int& number)
	{
		if (!upData) {
			number += deviation;
			ptr += deviation;
			return;
		}
		upData = false;
		deviation = 0;
		int funcNumber;
		for (auto& i : *callbackMap) {
			if (i.Size != 0) {
				if (ptr >= bufferEnd) break;
				funcNumber = i.Function(ptr, i.Pointer, i.Size);
				deviation += funcNumber;
				ptr += funcNumber;
			}
		}
		number += deviation;
	}

	template<typename TVertex>
	void ProcessOneShotVertices(
		TVertex*& ptr, const TVertex* bufferEnd,
		std::vector<TVertex>& vertices,
		unsigned int& number)
	{
		number += (unsigned int)vertices.size();
		while (vertices.size() != 0) {
			if (ptr >= bufferEnd) break;
			*ptr = vertices.back();
			++ptr;
			vertices.pop_back();
		}
	}

	template<typename TVertex>
	void HideUnusedVertices(
		TVertex* ptr, unsigned int number, unsigned int& max)
	{
		if (max > number) {
			for (size_t i = number; i < max; ++i) {
				ptr->Pos.z = -10000.0;
				++ptr;
			}
		}
		max = number;
	}

}

	AuxiliaryVision::AuxiliaryVision(VulKan::Device* device, PipelineS* P, const unsigned int number):
		wDevice(device),
		wPipelineS(P),
		Number(number)
	{
		LOGD("[AuxiliaryVision] Constructor");
		AuxiliaryLineS = new Buffer(
			wDevice, sizeof(AuxiliaryLineSpot) * Number * 2,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
		);
		ContinuousAuxiliaryLine = new ContinuousMap<glm::dvec2*, AuxiliaryLineData>(Number);
		ContinuousAuxiliaryForce = new ContinuousMap<glm::dvec2*, AuxiliaryForceData>(Number);
		StaticContinuousAuxiliaryLine = new ContinuousMap<void*, StaticAuxiliaryLineData>(Number, ContinuousMap_New);
		mLinePersistentPtr = static_cast<AuxiliaryLineSpot*>(AuxiliaryLineS->getPersistentMappedPtr());
		for (size_t i = 0; i < (Number * 2); ++i)
		{
			mLinePersistentPtr[i].Pos = { i, i, -10000.0 };
			mLinePersistentPtr[i].Color = { 0, 1.0f, 0, 1.0f };
		}
		AuxiliarySpotS = new Buffer(
			wDevice, sizeof(AuxiliarySpot) * Number,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
		);
		ContinuousAuxiliarySpot = new ContinuousMap<glm::dvec2*, AuxiliarySpotData>(Number);
		StaticContinuousAuxiliarySpot = new ContinuousMap<void*, StaticAuxiliarySpotData>(Number, ContinuousMap_New);
		mSpotPersistentPtr = static_cast<AuxiliarySpot*>(AuxiliarySpotS->getPersistentMappedPtr());
		for (size_t i = 0; i < Number; ++i)
		{
			mSpotPersistentPtr[i].Pos = { i, i, -10000.0 };
			mSpotPersistentPtr[i].Size = 0.2;
			mSpotPersistentPtr[i].Color = { 0, 0, 1.0f, 1.0f };
		}
		AuxiliaryCircleS = new Buffer(
			wDevice, sizeof(AuxiliaryCircle) * Number,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
		);
		mCirclePersistentPtr = static_cast<AuxiliaryCircle*>(AuxiliaryCircleS->getPersistentMappedPtr());
		for (size_t i = 0; i < Number; ++i)
		{
			mCirclePersistentPtr[i].Pos = { i, i, -10000.0 };
			mCirclePersistentPtr[i].Radius = 0.5;
			mCirclePersistentPtr[i].Color = { 0, 0, 1.0f, 1.0f };
		}

		mLineRecordedCount = Number;
		mSpotRecordedCount = Number;
		mCircleRecordedCount = Number;
	}

	AuxiliaryVision::~AuxiliaryVision()
	{
		delete AuxiliaryLineS;
		delete ContinuousAuxiliaryLine;
		delete ContinuousAuxiliaryForce;
		delete StaticContinuousAuxiliaryLine;

		delete AuxiliarySpotS;
		delete ContinuousAuxiliarySpot;
		delete StaticContinuousAuxiliarySpot;

		delete AuxiliaryCircleS;

		if (wSwapChain != nullptr) {
			for (size_t i = 0; i < wSwapChain->getImageCount(); ++i)
			{
				delete mCommandBuffer[i];
			}
		}
		delete mCommandBuffer;
		delete mDescriptorSetLine;
		delete mDescriptorSetSpot;
		delete mDescriptorSetCircle;
		delete mDescriptorPool;
		delete mCommandPool;
	}

	void AuxiliaryVision::initUniformManager(
		int frameCount,
		std::vector<VulKan::Buffer*> VPMstdBuffer
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
		mDescriptorPool = new VulKan::DescriptorPool(wDevice);
		mDescriptorPool->build(paramsvector, frameCount * 3);

		mDescriptorSetLine = new VulKan::DescriptorSet(wDevice, paramsvector, wPipelineS->GetPipeline(VulKan::PipelineMods::LineMods)->DescriptorSetLayout, mDescriptorPool, frameCount);
		mDescriptorSetSpot = new VulKan::DescriptorSet(wDevice, paramsvector, wPipelineS->GetPipeline(VulKan::PipelineMods::SpotMods)->DescriptorSetLayout, mDescriptorPool, frameCount);
		mDescriptorSetCircle = new VulKan::DescriptorSet(wDevice, paramsvector, wPipelineS->GetPipeline(VulKan::PipelineMods::CircleMods)->DescriptorSetLayout, mDescriptorPool, frameCount);
	}

	void AuxiliaryVision::initCommandBuffer() {
		VkCommandBufferInheritanceInfo InheritanceInfo{};
		InheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
		InheritanceInfo.renderPass = wRenderPass->getRenderPass();
		for (size_t i = 0; i < wSwapChain->getImageCount(); ++i)
		{
			InheritanceInfo.framebuffer = wSwapChain->getFrameBuffer(i);

			mCommandBuffer[i]->begin(VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT, InheritanceInfo);
			mCommandBuffer[i]->bindGraphicPipeline(wPipelineS->GetPipeline(PipelineMods::SpotMods)->getPipeline());
			mCommandBuffer[i]->bindVertexBuffer({ AuxiliarySpotS->getBuffer() });
			mCommandBuffer[i]->bindDescriptorSet(wPipelineS->GetPipeline(PipelineMods::SpotMods)->getLayout(), mDescriptorSetSpot->getDescriptorSet(i));
			mCommandBuffer[i]->draw(mSpotRecordedCount);
			mCommandBuffer[i]->bindGraphicPipeline(wPipelineS->GetPipeline(PipelineMods::LineMods)->getPipeline());
			mCommandBuffer[i]->bindVertexBuffer({ AuxiliaryLineS->getBuffer() });
			mCommandBuffer[i]->bindDescriptorSet(wPipelineS->GetPipeline(PipelineMods::LineMods)->getLayout(), mDescriptorSetLine->getDescriptorSet(i));
			mCommandBuffer[i]->draw(mLineRecordedCount);
			mCommandBuffer[i]->bindGraphicPipeline(wPipelineS->GetPipeline(PipelineMods::CircleMods)->getPipeline());
			mCommandBuffer[i]->bindVertexBuffer({ AuxiliaryCircleS->getBuffer() });
			mCommandBuffer[i]->bindDescriptorSet(wPipelineS->GetPipeline(PipelineMods::CircleMods)->getLayout(), mDescriptorSetCircle->getDescriptorSet(i));
			mCommandBuffer[i]->draw(mCircleRecordedCount);

			mCommandBuffer[i]->end();
		}
	}

	void AuxiliaryVision::Begin() {
		LinePointerHOST = mLinePersistentPtr;
		AuxiliaryLineSpot* const LineBufferEnd = LinePointerHOST + (Number * 2);

		bool lineVertexUpdated = ProcessStaticVertices(
			LinePointerHOST, LineBufferEnd,
			StaticLineVertex, mLineStaticVertex,
			StaticLineVertexDeviation, LineNumber);
		if (lineVertexUpdated) {
			mStaticLineCallbackUpData = true;
		}

		ProcessStaticCallbacks(
			LinePointerHOST, LineBufferEnd,
			StaticContinuousAuxiliaryLine,
			mStaticLineCallbackUpData,
			StaticLineDeviation, LineNumber);

		for (auto& i : *ContinuousAuxiliaryLine)
		{
			if (LinePointerHOST + 2 > LineBufferEnd) break;
			LinePointerHOST->Pos = { *i.Head, 0.0};
			LinePointerHOST->Color = i.Color;
			++LinePointerHOST;
			LinePointerHOST->Pos = { *i.Tail, 0.0 };
			LinePointerHOST->Color = i.Color;
			++LinePointerHOST;
		}
		for (auto& i : *ContinuousAuxiliaryForce)
		{
			if (LinePointerHOST + 2 > LineBufferEnd) break;
			LinePointerHOST->Pos = { *i.pos, 0.0f };
			LinePointerHOST->Color = i.Color;
			++LinePointerHOST;
			LinePointerHOST->Pos = { (*i.pos + *i.Force), 0.0f };
			LinePointerHOST->Color = i.Color;
			++LinePointerHOST;
		}
		LineNumber += ((ContinuousAuxiliaryLine->GetNumber() + ContinuousAuxiliaryForce->GetNumber()) * 2);

		ProcessOneShotVertices(LinePointerHOST, LineBufferEnd, LineVertex, LineNumber);

		SpotPointerHOST = mSpotPersistentPtr;
		AuxiliarySpot* const SpotBufferEnd = SpotPointerHOST + Number;

		bool spotVertexUpdated = ProcessStaticVertices(
			SpotPointerHOST, SpotBufferEnd,
			StaticSpotVertex, mSpotStaticVertex,
			StaticSpotVertexDeviation, SpotNumber);
		if (spotVertexUpdated) {
			mStaticSpotCallbackUpData = true;
		}

		ProcessStaticCallbacks(
			SpotPointerHOST, SpotBufferEnd,
			StaticContinuousAuxiliarySpot,
			mStaticSpotCallbackUpData,
			StaticSpotDeviation, SpotNumber);

		SpotNumber += ContinuousAuxiliarySpot->GetNumber();
		for (auto& i : *ContinuousAuxiliarySpot)
		{
			if (SpotPointerHOST >= SpotBufferEnd) break;
			SpotPointerHOST->Pos = { *i.pos, 0.0f };
			SpotPointerHOST->Color = i.Color;
			++SpotPointerHOST;
		}

		ProcessOneShotVertices(SpotPointerHOST, SpotBufferEnd, SpotVertex, SpotNumber);

		CirclePointerHOST = mCirclePersistentPtr;
		AuxiliaryCircle* const CircleBufferEnd = CirclePointerHOST + Number;

		ProcessStaticVertices(
			CirclePointerHOST, CircleBufferEnd,
			StaticCircleVertex, mCircleStaticVertex,
			StaticCircleVertexDeviation, CircleNumber);

		ProcessOneShotVertices(CirclePointerHOST, CircleBufferEnd, CircleVertex, CircleNumber);
	}

	bool AuxiliaryVision::End() {
		HideUnusedVertices(LinePointerHOST, LineNumber, LineMax);
		LinePointerHOST = nullptr;

		HideUnusedVertices(SpotPointerHOST, SpotNumber, SpotMax);
		SpotPointerHOST = nullptr;

		HideUnusedVertices(CirclePointerHOST, CircleNumber, CircleMax);
		CirclePointerHOST = nullptr;

		bool needReRecord = false;

		if (LineMax > mLineRecordedCount) {
			mLineRecordedCount = LineMax + kVertexCountBuffer;
			if (mLineRecordedCount > Number) { mLineRecordedCount = Number; }
			needReRecord = true;
		} else if (LineMax + kVertexCountBuffer < mLineRecordedCount) {
			mLineRecordedCount = LineMax;
			if (mLineRecordedCount > Number) { mLineRecordedCount = Number; }
			needReRecord = true;
		}

		if (SpotMax > mSpotRecordedCount) {
			mSpotRecordedCount = SpotMax + kVertexCountBuffer;
			if (mSpotRecordedCount > Number) { mSpotRecordedCount = Number; }
			needReRecord = true;
		} else if (SpotMax + kVertexCountBuffer < mSpotRecordedCount) {
			mSpotRecordedCount = SpotMax;
			if (mSpotRecordedCount > Number) { mSpotRecordedCount = Number; }
			needReRecord = true;
		}

		if (CircleMax > mCircleRecordedCount) {
			mCircleRecordedCount = CircleMax + kVertexCountBuffer;
			if (mCircleRecordedCount > Number) { mCircleRecordedCount = Number; }
			needReRecord = true;
		} else if (CircleMax + kVertexCountBuffer < mCircleRecordedCount) {
			mCircleRecordedCount = CircleMax;
			if (mCircleRecordedCount > Number) { mCircleRecordedCount = Number; }
			needReRecord = true;
		}

		return needReRecord;
	}
	
}