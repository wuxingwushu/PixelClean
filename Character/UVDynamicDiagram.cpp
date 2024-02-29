#include "UVDynamicDiagram.h"
#include "../GlobalStructural.h"


namespace GAME {

	void UVDynamicDiagramDestroyPixel(int x, int y, bool Bool, SquarePhysics::ObjectDecorator* Object, void* mclass) {
		UVDynamicDiagram* LUVDynamicDiagram = (UVDynamicDiagram*)mclass;
		
		std::cout << "破坏: " << x << " - " << y << " : " << LUVDynamicDiagram->GetPixelIndex(x, y) << std::endl;
	}

	UVDynamicDiagram::UVDynamicDiagram(VulKan::Device* device, VulKan::Pipeline* pipeline, VulKan::SwapChain* swapChain, VulKan::RenderPass* renderPass, std::vector<VulKan::Buffer*> VPMstdBuffer, SquarePhysics::SquarePhysics* SquarePhysics)
		:wPipeline(pipeline), wSwapChain(swapChain), wRenderPass(renderPass), wSquarePhysics(SquarePhysics)
	{
		mCommandPool = new VulKan::CommandPool(device);
		mCommandBuffer = new VulKan::CommandBuffer*[wSwapChain->getImageCount()];
		for (int i = 0; i < wSwapChain->getImageCount(); ++i) {
			mCommandBuffer[i] = new VulKan::CommandBuffer(device, mCommandPool, true);
		}

		int UIP[] = {
			0,1,2,3,4,5,6,7,8
		};

		mSequentialIndex = VulKan::Buffer::createVertexBuffer(device, sizeof(int) * 9, UIP);

		float dianpos[] = {
			0.0f, 0.0f,
			1.0f, 0.0f,
			2.0f, 0.0f,
			0.0f, 1.0f,
			1.0f, 1.0f,
			2.0f, 1.0f,
			0.0f, 2.0f,
			1.0f, 2.0f,
			2.0f, 2.0f,
		};

		mPixelPosition = VulKan::Buffer::createVertexBuffer(device, sizeof(float) * 18, dianpos);

		float bangzi[] = {
			1.0f,0,0,1.0f,
			0,1.0f,0,1.0f,
			0,0,1.0f,1.0f
		};

		

		mIndexAnimationGrid = new SquarePhysics::IndexAnimationGrid(3, 3, 1, 8);
		SquarePhysics::PixelAttribute* pixelAttribute = mIndexAnimationGrid->mPixelAttributeS;
		pixelAttribute[0].Collision = true;
		pixelAttribute[1].Collision = true;
		pixelAttribute[2].Collision = true;
		mIndexAnimationGrid->SetAnimationIndex(uvdh);
		mIndexAnimationGrid->SetPos({ 20,20 });
		mIndexAnimationGrid->SetOrigin(1,1);
		mIndexAnimationGrid->SetAngle(0);
		mIndexAnimationGrid->SetCollisionCallback(UVDynamicDiagramDestroyPixel, this);
		mIndexAnimationGrid->OutlineCalculate();
		wSquarePhysics->AddObjectCollision(mIndexAnimationGrid);

		mUVDynamicDiagramStruct.mModelMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(20.0f, 20.0f, 0.0f));
		mUVDynamicDiagramStruct.UnitSize = 9;
		mUVDynamicDiagramStruct.CurrentFrame = 0;

		std::vector<VulKan::UniformParameter*> mUniformParams{};

		VulKan::UniformParameter* vpParam = new VulKan::UniformParameter();
		vpParam->mBinding = 0;
		vpParam->mCount = 1;
		vpParam->mDescriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		vpParam->mSize = sizeof(VPMatrices);
		vpParam->mStage = VK_SHADER_STAGE_VERTEX_BIT;
		vpParam->mBuffers = VPMstdBuffer;
		mUniformParams.push_back(vpParam);

		VulKan::UniformParameter* objectParam = new VulKan::UniformParameter();
		objectParam->mBinding = 1;
		objectParam->mCount = 1;
		objectParam->mDescriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		objectParam->mSize = sizeof(UVDynamicDiagramStruct);
		objectParam->mStage = VK_SHADER_STAGE_VERTEX_BIT;
		mPositionToData = VulKan::Buffer::createUniformBuffer(device, objectParam->mSize, nullptr);
		UVDynamicDiagramStruct* P = (UVDynamicDiagramStruct*)mPositionToData->getupdateBufferByMap();
		*P = mUVDynamicDiagramStruct;
		mPositionToData->endupdateBufferByMap();
		for (int i = 0; i < wSwapChain->getImageCount(); ++i) {
			objectParam->mBuffers.push_back(mPositionToData);
		}
		mUniformParams.push_back(objectParam);

		VulKan::UniformParameter* basisChart = new VulKan::UniformParameter();
		basisChart->mBinding = 2;
		basisChart->mCount = 1;
		basisChart->mDescriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		basisChart->mSize = sizeof(float) * 12;
		basisChart->mStage = VK_SHADER_STAGE_VERTEX_BIT;
		mBasisChart = new VulKan::Buffer(
			device, basisChart->mSize,
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,//告诉他这是 StorageBuffer 
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT//告诉他这个是CPU端内存可见，更新了立马显现。
		);
		float* FP = (float*)mBasisChart->getupdateBufferByMap();
		for (size_t i = 0; i < 12; ++i)
		{
			*FP = bangzi[i];
			++FP;
		}
		mBasisChart->endupdateBufferByMap();
		for (int i = 0; i < wSwapChain->getImageCount(); ++i) {
			basisChart->mBuffers.push_back(mBasisChart);
		}
		mUniformParams.push_back(basisChart);

		VulKan::UniformParameter* animationUVS = new VulKan::UniformParameter();
		animationUVS->mBinding = 3;
		animationUVS->mCount = 1;
		animationUVS->mDescriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		animationUVS->mSize = sizeof(int) * 72;
		animationUVS->mStage = VK_SHADER_STAGE_VERTEX_BIT;
		mAnimationUVS = new VulKan::Buffer(
			device, animationUVS->mSize,
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,//告诉他这是 StorageBuffer 
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT//告诉他这个是CPU端内存可见，更新了立马显现。
		);
		int* UP = (int*)mAnimationUVS->getupdateBufferByMap();
		for (size_t i = 0; i < 72; ++i)
		{
			*UP = uvdh[i];
			++UP;
		}
		mAnimationUVS->endupdateBufferByMap();
		for (int i = 0; i < wSwapChain->getImageCount(); ++i) {
			animationUVS->mBuffers.push_back(mAnimationUVS);
		}
		mUniformParams.push_back(animationUVS);

		//各种类型申请多少个
		mDescriptorPool = new VulKan::DescriptorPool(device);
		mDescriptorPool->build(mUniformParams, wSwapChain->getImageCount());

		//将申请的各种类型按照Layout绑定起来
		mDescriptorSet = new VulKan::DescriptorSet(device, mUniformParams, wPipeline->DescriptorSetLayout, mDescriptorPool, wSwapChain->getImageCount());

		for (auto i : mUniformParams)
		{
			delete i;
		}
	}

	UVDynamicDiagram::~UVDynamicDiagram()
	{
		delete mPositionToData;
		delete mBasisChart;
		delete mAnimationUVS;
		delete mPixelPosition;
		delete mSequentialIndex;

		
		delete mDescriptorSet;
		delete mDescriptorPool;
		for (size_t i = 0; i < wSwapChain->getImageCount(); ++i)
		{
			delete mCommandBuffer[i];
		}
		delete mCommandBuffer;
		delete mCommandPool;
	}

	void UVDynamicDiagram::InitCommandBuffer()
	{
		VkCommandBufferInheritanceInfo InheritanceInfo{};
		InheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
		InheritanceInfo.renderPass = wRenderPass->getRenderPass();

		for (size_t i = 0; i < wSwapChain->getImageCount(); ++i)
		{
			InheritanceInfo.framebuffer = wSwapChain->getFrameBuffer(i);

			mCommandBuffer[i]->begin(VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT, InheritanceInfo);//开始录制二级指令
			mCommandBuffer[i]->bindGraphicPipeline(wPipeline->getPipeline());//获得渲染管线

			mCommandBuffer[i]->bindVertexBuffer({ mPixelPosition->getBuffer(), mSequentialIndex->getBuffer() });//获取顶点数据，UV值
			mCommandBuffer[i]->bindDescriptorSet(wPipeline->getLayout(), mDescriptorSet->getDescriptorSet(i));//获得 模型位置数据， 贴图数据，……
			mCommandBuffer[i]->draw(Quantity);//获取绘画物体的顶点个数

			mCommandBuffer[i]->end();
		}
	}

}