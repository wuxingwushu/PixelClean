#include "BlockS.h"
#include "../Tool/ThreadPool.h"


namespace GAME {

	BlockS::BlockS
	(
		VulKan::Device* device,
		VulKan::CommandPool* commandPool,
		int frameCount,
		const VkDescriptorSetLayout descriptorSetLayout,
		std::vector<VulKan::Buffer*> VPMBufferS,
		glm::vec3 Camerapos,
		int width,
		int height,
		VulKan::Sampler* sampler,
		VkRenderPass pass,
		VulKan::Pipeline* pipeline,
		VulKan::SwapChain* swapChain
	) {
		mDevice = device;
		mCommandPool = commandPool;
		mFrameCount = frameCount;
		mDescriptorSetLayout = descriptorSetLayout;
		mVPMBufferS = VPMBufferS;
		mSampler = sampler;
		mRenderPass = pass;
		mPipeline = pipeline;
		mSwapChain = swapChain;

		//内存池
		mMemoryPoolBlock = new MemoryPool<Block, 4096000>*[TOOL::mThreadCount];
		for (size_t i = 0; i < TOOL::mThreadCount; i++)
		{
			mMemoryPoolBlock[i] = new MemoryPool<Block, 4096000>();
		}


		//创建 销毁池
		DestroyBlockPool = new Block** [DestroyNumber];
		DestroyBlockNumber = new unsigned int[DestroyNumber];
		for (int i = 0; i < DestroyNumber; i++)
		{
			DestroyBlockPool[i] = new Block* [1000];
			DestroyBlockNumber[i] = 0;
		}


		//创建多线程生成信号量
		mThreadGenerateBoolS = new bool[TOOL::mThreadCount];
		for (int i = 0; i < TOOL::mThreadCount; i++)
		{
			mThreadGenerateBoolS[i] = true;
		}

		//创建 双CommandBuffer 录制的信号量
		ThreadCommandBufferNumber = (TOOL::mThreadCount / frameCount);
		mThreadBool = new bool[ThreadCommandBufferNumber * frameCount];
		for (int i = 0; i < (ThreadCommandBufferNumber * frameCount); i++) {
			mThreadBool[i] = false;
		}

		//创建 BufferCopy 提交 CommandBuffer
		mBufferCopyCommandPool = new VulKan::CommandPool(mDevice);
		mBufferCopyCommandBuffer = new VulKan::CommandBuffer(mDevice, mBufferCopyCommandPool);
		mThreadCommandPoolS = new VulKan::CommandPool* [ThreadCommandBufferNumber * mFrameCount * 2];
		mThreadCommandBufferS = new VulKan::CommandBuffer* [ThreadCommandBufferNumber * mFrameCount * 2];
		for (int i = 0; i < (ThreadCommandBufferNumber * mFrameCount * 2); i++) {
			mThreadCommandPoolS[i] = new VulKan::CommandPool(mDevice);
			mThreadCommandBufferS[i] = new VulKan::CommandBuffer(mDevice, mThreadCommandPoolS[i], true);
		}

		numberX = int(Camerapos.z * 0.1f) + 1;//在当前相机高度计算出横排多少个
		numberY = int(numberX * (float(height) / float(width))) + 2;//在当前相机高度计算出纵排多少个
		mCameraBlockX = Camerapos.x / mPixel;
		mCameraBlockY = Camerapos.y / mPixel;

		int posx = mCameraBlockX - (numberX / 2);//最左边
		int posy = mCameraBlockY - (numberY / 2);//最上边


		//初始化 mBlock 数组（动态）
		mBlock = new Block** [numberX];
		for (int i = 0; i < numberX; i++) {
			mBlock[i] = new Block* [numberY];
		}

		mPixelTextureS = new PixelTextureS(device, commandPool, sampler);


		//多线程生成Block
		for (int i = 0; i < numberX; i++) {
			for (int j = 0; j < numberY; j++) {
				BlockToUpdateX[BlockCount] = i;
				BlockToUpdateY[BlockCount] = j;
				BlockS_X[BlockCount] = (posx + i) * mPixel;
				BlockS_Y[BlockCount] = (posy + j) * mPixel;
				BlockCount++;
			}
		}
		std::vector<std::future<void>> poold;
		int UpdateNumber = BlockCount / TOOL::mThreadCount;
		int UpdateNumber_yu = BlockCount % TOOL::mThreadCount;
		for (unsigned int i = 0; i < UpdateNumber_yu; i++) {
			poold.push_back(TOOL::mThreadPool->enqueue(&BlockS::ThreadToUpdateBlock, this, ((UpdateNumber * i) + i), (UpdateNumber + 1), i));
		}
		for (unsigned int i = UpdateNumber_yu; i < TOOL::mThreadCount; i++) {
			poold.push_back(TOOL::mThreadPool->enqueue(&BlockS::ThreadToUpdateBlock, this, ((UpdateNumber * i) + UpdateNumber_yu), UpdateNumber, i));
		}
		for (int i = 0; i < TOOL::mThreadCount; i++) {
			poold[i].wait();
		}

		//上传指令
		mBufferCopyCommandBuffer->begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
		for (unsigned int i = 0; i < BlockCount; i++) {
			mBlock[BlockToUpdateX[i]][BlockToUpdateY[i]]->getBlockCommandBuffer(mBufferCopyCommandBuffer);
		}
		BlockCount = 0;
		mBufferCopyCommandBuffer->end();
		mBufferCopyCommandBuffer->submitSync(mDevice->getGraphicQueue(), VK_NULL_HANDLE);

		//多线程录制指令
		ThreadUpdateCommandBuffer();
	}

	BlockS::~BlockS() {
		delete mPixelTextureS;
		delete mBufferCopyCommandBuffer;//先 销毁 CommandBuffer 再 销毁 CommandPool
		delete mBufferCopyCommandPool;

		for (int i = 0; i < (ThreadCommandBufferNumber * mFrameCount * 2); i++) {
			delete mThreadCommandBufferS[i];//先 销毁 CommandBuffer 再 销毁 CommandPool
			delete mThreadCommandPoolS[i];
		}

		for (int i = 0; i < numberX; i++) {
			for (int j = 0; j < numberY; j++) {
				//delete mBlock[i][j];
				mMemoryPoolBlock[mBlock[i][j]->MemoryPool_i]->deleteElement(mBlock[i][j]);
			}
		}

		for (int i = 0; i < 4; i++)
		{
			for (int ixx = 0; ixx < DestroyBlockNumber[i]; ixx++)
			{
				//delete DestroyBlockPool[i][ixx];
				mMemoryPoolBlock[DestroyBlockPool[i][ixx]->MemoryPool_i]->deleteElement(DestroyBlockPool[i][ixx]);
			}
			DestroyBlockNumber[i] = 0;
		}
		

		//销毁 申请的内存空间
		for (int i = 0; i < numberX; i++) {
			delete mBlock[i];
		}
		delete mBlock;

		for (int i = 0; i < DestroyNumber; i++)
		{
			delete DestroyBlockPool[i];
		}
		delete DestroyBlockPool;

		//delete BlockThread;
		delete mThreadCommandPoolS;
		delete mThreadGenerateBoolS;
		delete DestroyBlockNumber;
		delete mThreadCommandBufferS;
		delete mThreadBool;
	}

	void BlockS::CameraMoveEvent(float CameraX, float CameraY)
	{
		if (mGenerate) {//等待生成结束才可以再一次判断
			int cameraPosx = mCameraBlockX - int(CameraX / mPixel);
			if ((cameraPosx != 0)) {
				TransverseToUpdate(cameraPosx);
				mGenerate = false;
				for (int i = 0; i < (mFrameCount * ThreadCommandBufferNumber); i++)
				{
					mThreadBool[i] = false;
				}
				ThreadDistribution();
				return;
			}
			int cameraPosy = mCameraBlockY - int(CameraY / mPixel);
			if ((cameraPosy != 0)) {
				LongitudinalToUpdate(cameraPosy);
				mGenerate = false;
				for (int i = 0; i < (mFrameCount * ThreadCommandBufferNumber); i++)
				{
					mThreadBool[i] = false;
				}
				ThreadDistribution();
				return;
			}
		}
		else {
			//向 GPU 提交生成信息，并且重新录制 mThreadCommandBufferS
			if (mBufferCopyBool) {
				mBufferCopyBool = false;
				mThreadGenerateBool = false;

				//提交 BufferCopy CommandBuffer
				mBufferCopyCommandBuffer->begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
				for (unsigned int i = 0; i < BlockCount; i++) {
					mBlock[BlockToUpdateX[i]][BlockToUpdateY[i]]->getBlockCommandBuffer(mBufferCopyCommandBuffer);
				}
				BlockCount = 0;
				mBufferCopyCommandBuffer->end();
				mBufferCopyCommandBuffer->submitSync(mDevice->getGraphicQueue(), VK_NULL_HANDLE);

				//通知 主mThreadCommandBufferS 可以更新了
				CommandBufferToUpdate();

				//销毁池
				//std::cout << DestroyBlockNumber[DestroyBlock_i] << std::endl;
				for (int ixx = 0; ixx < DestroyBlockNumber[DestroyBlock_i]; ixx++)
				{
					//delete DestroyBlockPool[DestroyBlock_i][ixx];
					mMemoryPoolBlock[DestroyBlockPool[DestroyBlock_i][ixx]->MemoryPool_i]->deleteElement(DestroyBlockPool[DestroyBlock_i][ixx]);
				}
				DestroyBlockNumber[DestroyBlock_i] = 0;
			}

			//判断 多线程生成是否完毕
			if (mThreadGenerateBool) {
				mBufferCopyBool = true;
				for (unsigned int i = 0; i < TOOL::mThreadCount; i++)
				{
					if (!mThreadGenerateBoolS[i]) {
						mBufferCopyBool = false;
						break;
					}
				}
			}

			//判断 mThreadCommandBufferS 是否重新录入
			for (int i = 0; i < (mFrameCount * ThreadCommandBufferNumber); i++)
			{
				if (mThreadBool[i]) {
					mGenerate = true;
				}
				else {
					mGenerate = false;
					break;
				}
			}

			if (mGenerate) {
				mDoubleCommandBufferBool = !mDoubleCommandBufferBool;
			}
		}
	}

	void BlockS::ThreadDistribution() {
		if (BlockCount <= 0) { return; }

		mThreadGenerateBool = true;//开启 多线程生成结束 的检测

		//将要生成的类较为平均的分配给各个线程
		int UpdateNumber = BlockCount / TOOL::mThreadCount;
		int UpdateNumber_yu = BlockCount % TOOL::mThreadCount;
		for (unsigned int i = 0; i < UpdateNumber_yu; i++) {
			mThreadGenerateBoolS[i] = false;
			TOOL::mThreadPool->enqueue(&BlockS::ThreadToUpdateBlock, this, ((UpdateNumber * i) + i), (UpdateNumber + 1), i);
		}
		for (unsigned int i = UpdateNumber_yu; i < TOOL::mThreadCount; i++) {
			mThreadGenerateBoolS[i] = false;
			TOOL::mThreadPool->enqueue(&BlockS::ThreadToUpdateBlock, this, ((UpdateNumber * i) + UpdateNumber_yu), UpdateNumber, i);
		}
	}

	//多线程生成
	void BlockS::ThreadToUpdateBlock(unsigned int AddresShead, unsigned int Count, unsigned int CommandBufferCount) {
		srand((int)time(NULL));
		for (int i = AddresShead; i < AddresShead + Count; i++) {
			int tushui = int(TOOL::mPerlinNoise->noise((double(BlockS_X[i]) / 1000), (double(BlockS_Y[i]) / 1000), 0.0f) * TextureNumber);
			bool q = tushui > (TextureNumber / 2) ? 1 : 0;
			mBlock[BlockToUpdateX[i]][BlockToUpdateY[i]] = mMemoryPoolBlock[CommandBufferCount]->newElement(mDevice, BlockS_X[i], BlockS_Y[i], CommandBufferCount, q);/*new Block(mDevice, BlockS_X[i], BlockS_Y[i], CommandBufferCount);*/
			//mBlock[BlockToUpdateX[i]][BlockToUpdateY[i]] = new Block(mDevice, BlockS_X[i], BlockS_Y[i], CommandBufferCount);
			mBlock[BlockToUpdateX[i]][BlockToUpdateY[i]]->initUniformManager(mDevice, mCommandPool, mFrameCount, mPixelTextureS->getPixelTexture(tushui), mDescriptorSetLayout, mVPMBufferS, mSampler);
		}

		mThreadGenerateBoolS[CommandBufferCount] = true;//通知这个线程生成完毕
	}

	

	void BlockS::LongitudinalToUpdate(int Y)
	{
		if (Y > 0) {
			mCameraBlockY--;
			for (int i = 0; i < numberX; i++) {
				DestroyBlockPool[DestroyBlock_i][DestroyBlockNumber[DestroyBlock_i]] = mBlock[i][numberY - 1];
				DestroyBlockNumber[DestroyBlock_i]++;
				for (int k = numberY - 1; k > 0; k--) {
					mBlock[i][k] = mBlock[i][k - 1];
				}
				BlockToUpdateX[BlockCount] = i;
				BlockToUpdateY[BlockCount] = 0;
				BlockS_X[BlockCount] = mBlock[i][1]->mX;
				BlockS_Y[BlockCount] = mBlock[i][1]->mY - mPixel;
				BlockCount++;
			}
		}
		else {
			mCameraBlockY++;
			for (int i = 0; i < numberX; i++) {
				DestroyBlockPool[DestroyBlock_i][DestroyBlockNumber[DestroyBlock_i]] = mBlock[i][0];
				DestroyBlockNumber[DestroyBlock_i]++;
				//memcpy(&mBlock[i][0], &mBlock[i][1], ((numberY - 1) * sizeof(mBlock[i][0])));
				for (int k = 1; k < numberY; k++) {
					mBlock[i][k - 1] = mBlock[i][k];
				}
				BlockToUpdateX[BlockCount] = i;
				BlockToUpdateY[BlockCount] = numberY - 1;
				BlockS_X[BlockCount] = mBlock[i][numberY - 2]->mX;
				BlockS_Y[BlockCount] = mBlock[i][numberY - 2]->mY + mPixel;
				BlockCount++;
			}
		}
		DestroyBlock_i++;
		DestroyBlock_i %= DestroyNumber;
	}

	void BlockS::TransverseToUpdate(int X)
	{
		if (X > 0) {
			mCameraBlockX--;
			for (int i = 0; i < numberY; i++) {
				DestroyBlockPool[DestroyBlock_i][DestroyBlockNumber[DestroyBlock_i]] = mBlock[numberX - 1][i];
				DestroyBlockNumber[DestroyBlock_i]++;
				/*for (int k = numberX - 1; k > 0; k--) {
					mBlock[k][i] = mBlock[k - 1][i];
				}*/
				BlockToUpdateX[BlockCount] = 0;
				BlockToUpdateY[BlockCount] = i;
				BlockS_X[BlockCount] = mBlock[0][i]->mX - mPixel;
				BlockS_Y[BlockCount] = mBlock[0][i]->mY;
				/*BlockS_X[BlockCount] = mBlock[1][i]->mX - mPixel;
				BlockS_Y[BlockCount] = mBlock[1][i]->mY;*/
				BlockCount++;
			}
			Block** blocksdd = mBlock[numberX - 1];
			for (int k = numberX - 1; k > 0; k--) {
				mBlock[k] = mBlock[k - 1];
			}
			mBlock[0] = blocksdd;
			/*Block** blocksdd = mBlock[numberX - 1];
			TOOL::memcpyf(&mBlock[(numberX - 1)], &mBlock[(numberX - 2)], (numberX - 1),sizeof(mBlock[numberX - 1]));
			mBlock[0] = blocksdd;*/
		}
		else {
			mCameraBlockX++;
			for (int i = 0; i < numberY; i++) {
				DestroyBlockPool[DestroyBlock_i][DestroyBlockNumber[DestroyBlock_i]] = mBlock[0][i];
				DestroyBlockNumber[DestroyBlock_i]++;
				/*for (int k = 1; k < numberX; k++) {
					mBlock[k - 1][i] = mBlock[k][i];
				}*/
				BlockToUpdateX[BlockCount] = numberX - 1;
				BlockToUpdateY[BlockCount] = i;
				BlockS_X[BlockCount] = mBlock[numberX - 1][i]->mX + mPixel;
				BlockS_Y[BlockCount] = mBlock[numberX - 1][i]->mY;
				/*BlockS_X[BlockCount] = mBlock[numberX - 2][i]->mX + mPixel;
				BlockS_Y[BlockCount] = mBlock[numberX - 2][i]->mY;*/
				BlockCount++;
			}
			Block** blocksd = mBlock[0];
			memcpy(&mBlock[0], &mBlock[1], ((numberX - 1) * sizeof(mBlock[0])));
			mBlock[numberX - 1] = blocksd;
		}
		DestroyBlock_i++;
		DestroyBlock_i %= DestroyNumber;
	}

	//重新录制frameCount个画布的CommandBuffer
	void BlockS::CommandBufferToUpdate() {
		int UpdateNumber = (numberX * numberY) / ThreadCommandBufferNumber;
		int UpdateNumber_yu = (numberX * numberY) % ThreadCommandBufferNumber;
		for (int i = 0; i < mFrameCount; i++) {
			for (int j = 0; j < UpdateNumber_yu; j++){
				TOOL::mThreadPool->enqueue(&BlockS::ThreadCommandBufferToUpdate, this, i, j, ((UpdateNumber * j) + j), (UpdateNumber + 1));
			}
			for (int j = UpdateNumber_yu; j < ThreadCommandBufferNumber; j++) {
				TOOL::mThreadPool->enqueue(&BlockS::ThreadCommandBufferToUpdate, this, i, j, ((UpdateNumber * j) + UpdateNumber_yu), UpdateNumber);
			}
		}
	}

	void BlockS::ThreadUpdateCommandBuffer() {
		std::vector<std::future<void>> pool;
		int UpdateNumber = (numberX * numberY) / ThreadCommandBufferNumber;
		int UpdateNumber_yu = (numberX * numberY) % ThreadCommandBufferNumber;
		for (int i = 0; i < mFrameCount; i++) {
			for (int j = 0; j < UpdateNumber_yu; j++){
				pool.push_back(TOOL::mThreadPool->enqueue(&BlockS::ThreadCommandBufferToUpdate, this, i, j, ((UpdateNumber * j) + j), (UpdateNumber + 1)));
			}
			for (int j = UpdateNumber_yu; j < ThreadCommandBufferNumber; j++) {
				pool.push_back(TOOL::mThreadPool->enqueue(&BlockS::ThreadCommandBufferToUpdate, this, i, j, ((UpdateNumber * j) + UpdateNumber_yu), UpdateNumber));
			}
		}
		for (int i = 0; i < (mFrameCount * ThreadCommandBufferNumber); i++) {
			pool[i].wait();
		}
		mDoubleCommandBufferBool = !mDoubleCommandBufferBool;
	}

	void BlockS::ThreadCommandBufferToUpdate(unsigned int FrameCount, unsigned int BufferCount, unsigned int AddresShead, unsigned int Count)
	{
		unsigned int mFrameBufferCount = (FrameCount * ThreadCommandBufferNumber) + BufferCount;
		VulKan::CommandBuffer* commandbuffer = mThreadCommandBufferS[mFrameBufferCount + (mDoubleCommandBufferBool ? (mFrameCount * ThreadCommandBufferNumber) : 0)];

		VkCommandBufferInheritanceInfo InheritanceInfo{};
		InheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
		InheritanceInfo.renderPass = mRenderPass;
		InheritanceInfo.framebuffer = mSwapChain->getFrameBuffer(FrameCount);



		commandbuffer->begin(VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT, InheritanceInfo);//开始录制二级指令
		commandbuffer->bindGraphicPipeline(mPipeline->getPipeline());//获得渲染管线
		for (int i = AddresShead; i < AddresShead + Count; i++) {
			int x = i / numberY;
			int y = i % numberY;
			commandbuffer->bindDescriptorSet(mPipeline->getLayout(), mBlock[x][y]->getDescriptorSet(FrameCount));//获得 模型位置数据， 贴图数据，……
			commandbuffer->bindVertexBuffer(mBlock[x][y]->getVertexBuffers());//获取顶点数据，UV值
			commandbuffer->bindIndexBuffer(mBlock[x][y]->getIndexBuffer()->getBuffer());//获得顶点索引
			commandbuffer->drawIndex(mBlock[x][y]->getIndexCount());//获取绘画物体的顶点个数
		}
		commandbuffer->end();


		mThreadBool[mFrameBufferCount] = true;//通知这个线程录制完毕
	}


	//屏幕大小改变，mDescriptorSetLayout 也要更新，因为渲染管线更新了
	void BlockS::WindowResizedToUpdate(
		VulKan::Pipeline* pipeline,
		VulKan::SwapChain* swapChain,
		VkRenderPass Pass
	)
	{
		mDescriptorSetLayout = pipeline->DescriptorSetLayout;
		mPipeline = pipeline;
		mSwapChain = swapChain;
		mRenderPass = Pass;

		ThreadUpdateCommandBuffer();
	}
}