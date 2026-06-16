#include "GamePlayer.h"
#include "../DebugLog.h"
#include "../BlockS/PixelS.h"
#include "../GlobalVariable.h"

namespace GAME {

	void GamePlayerDestroyPixel(int x, int y, bool Bool, PhysicsBlock::PhysicsFormwork* Object, void* mclass) {
		GamePlayer* Class = (GamePlayer*)mclass;
		Class->mPixelQueue->add({x,y, Bool });
		if (Class->wDamagePrompt != nullptr) {
			Vec2_ spd = Object->PFSpeed();
			Class->wDamagePrompt->AddDamagePrompt(atan2f(spd.y, spd.x));
		}
	}

	// 坦克被子弹击中：碰撞进入回调。
	// 接触点（世界坐标）经 DropCollision 转为坦克 16x16 网格坐标，
	// 然后复用现有的 mPixelQueue 伤害链路（与地形破坏子弹一致）。
	static void TankHitByBullet(const PhysicsBlock::PhysicsFormwork* tankObj,
		const PhysicsBlock::PhysicsFormwork* otherObj,
		const PhysicsBlock::BaseArbiter* arbiter,
		GamePlayer* self)
	{
		// 只处理被子弹（粒子）击中的情况
		if (otherObj->type != PhysicsBlock::PhysicsObjectEnum::particle) return;
		if (arbiter->numContacts <= 0) return;

		const PhysicsBlock::Contact& c = arbiter->contacts[0];

		// 用接触点世界坐标算出坦克网格坐标
		PhysicsBlock::CollisionInfoI info =
			const_cast<PhysicsBlock::PhysicsShape*>(
				static_cast<const PhysicsBlock::PhysicsShape*>(tankObj))
			->DropCollision(c.position);
		if (!info.Collision) return;

		int gx = info.pos.x;
		int gy = info.pos.y;
		if (gx < 0 || gx >= 16 || gy < 0 || gy >= 16) return;

		// 复用既有像素伤害队列（State=false 表示破坏该格）
		self->mPixelQueue->add({ gx, gy, false });

		// 受伤方向提示：使用 Contact::normal 的法向量反方向（子弹入射方向）
		if (self->wDamagePrompt != nullptr) {
			self->wDamagePrompt->AddDamagePrompt(atan2f(-c.normal.y, -c.normal.x));
		}

	// === 方案E：子弹击退 ===
	if (self->GetMovement()) {
		Vec2_ bulletSpeed = const_cast<PhysicsBlock::PhysicsFormwork*>(otherObj)->PFSpeed();
		float bulletSpdLen = std::sqrt(bulletSpeed.x * bulletSpeed.x + bulletSpeed.y * bulletSpeed.y);
		if (bulletSpdLen > 1e-3f) {
			Vec2_ dir = { bulletSpeed.x / bulletSpdLen, bulletSpeed.y / bulletSpdLen };
			// 冲量 = 子弹动量 × 击退系数
			const float knockbackScale = 8.0f;
			FLOAT_ bulletMass = const_cast<PhysicsBlock::PhysicsFormwork*>(otherObj)->PFGetMass();
			float impulseMag = bulletSpdLen * bulletMass * knockbackScale;
			// 带受力点冲量：接触点受力产生自旋（使用 Contact::position，已有子像素精度）
			self->GetObjectCollision()->ApplyImpulse(
				Vec2_{ dir.x * impulseMag, dir.y * impulseMag },
				Vec2_{ c.position.x, c.position.y });
			// 切换到被击飞态（期间不响应玩家/AI 输入）
			self->GetMovement()->SetMode(MovementMode::Ragdoll);
		}
	}

		// 通知 Arms 销毁该子弹
		auto* bullet = const_cast<PhysicsBlock::PhysicsParticle*>(
			static_cast<const PhysicsBlock::PhysicsParticle*>(otherObj));
		if (self->BulletDestroyedHandler) {
			self->BulletDestroyedHandler(bullet);
		}
	}

	GamePlayer::GamePlayer(VulKan::Device* device, VulKan::Pipeline* pipeline, VulKan::SwapChain* swapChain, VulKan::RenderPass* renderPass, 
		PhysicsBlock::PhysicsWorld* PhysicsWorld, float X, float Y)
	{
		mPipeline = pipeline;
		mSwapChain = swapChain;
		mRenderPass = renderPass;
		mSquarePhysics = PhysicsWorld;
		mUniform.mModelMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(X, Y, 0.0f));//位移矩阵

		mPixelQueue = new Queue<PixelState>(100);

		mObjectCollision = new PhysicsBlock::PhysicsShape(Vec2_{X, Y}, glm::ivec2{16, 16});
		mBrokenData = new bool[16 * 16];
		for (size_t x = 0; x < 16; ++x)
		{
			for (size_t y = 0; y < 16; ++y)
			{
				mObjectCollision->at({ x,y }).Entity = true;
				mObjectCollision->at({ x,y }).Collision = true;
				mObjectCollision->at({ x,y }).mass = 1.0;
				mObjectCollision->at({ x,y }).FrictionFactor = 0.2f;
				mBrokenData[x * 16 + y] = true;
			}
		}
		mObjectCollision->UpdateAll();//计算外骨架
		// mObjectCollision->SetOrigin(8, 8);//设置原点 (PhysicsBlock uses CentreMass)
		mObjectCollision->pos = Vec2_{ X, Y };//设置位置
	mObjectCollision->friction = 1.0f;//设置摩擦系数
	// 新物理引擎：通过 PhysicsCollision 全局回调系统注册"坦克被击中"事件
	RegisterBulletHitCallback();
	mSquarePhysics->AddObject(mObjectCollision);//添加玩家碰撞

	// 创建混合驱动移动组件（方案E）。坦克体总质量≈256（16x16 每格 mass=1），
	// ComputeMoveForce 会按真实质量反推施力，所以 MaxSpeed 默认 120 像素/秒足够灵敏。
	mMovement = new MovementComponent(mObjectCollision);

		std::vector<float> mPositions = {
			-8.0f, -8.0f, 0.0f,
			8.0f, -8.0f, 0.0f,
			8.0f, 8.0f, 0.0f,
			-8.0f, 8.0f, 0.0f,
		};

		std::vector<float> mUVs = {
			0.01f,0.01f,
			0.01f,0.99f,
			0.99f,0.99f,
			0.99f,0.01f,
			
		};

		std::vector<unsigned int> mIndexDatas = {
			0,1,2,
			2,3,0,
		};

		mPositionBuffer = VulKan::Buffer::createVertexBuffer(device, mPositions.size() * sizeof(float), mPositions.data());

		mUVBuffer = VulKan::Buffer::createVertexBuffer(device, mUVs.size() * sizeof(float), mUVs.data());

		mIndexBuffer = VulKan::Buffer::createIndexBuffer(device, mIndexDatas.size() * sizeof(float), mIndexDatas.data());

		mIndexDatasSize = mIndexDatas.size();
		mPositions.clear();
		mUVs.clear();
		mIndexDatas.clear();


		mBufferCopyCommandPool = new VulKan::CommandPool(device);
		mBufferCopyCommandBuffer = new VulKan::CommandBuffer*[mSwapChain->getImageCount()];
		for (size_t i = 0; i < mSwapChain->getImageCount(); ++i)
		{
			mBufferCopyCommandBuffer[i] = new VulKan::CommandBuffer(device, mBufferCopyCommandPool, true);
		}
	}

	GamePlayer::~GamePlayer()
	{
		// 先销毁移动组件（它持有物理体裸指针，但不拥有它）
		if (mMovement != nullptr) {
			delete mMovement;
			mMovement = nullptr;
		}

		if (mSquarePhysics != nullptr) {
			mSquarePhysics->RemoveObject(mObjectCollision);//移除玩家碰撞
		}

		if (mPositionBuffer != nullptr) {
			delete mPositionBuffer;
		}
		if (mUVBuffer != nullptr) {
			delete mUVBuffer;
		}
		if (mIndexBuffer != nullptr) {
			delete mIndexBuffer;
		}
		if (mBrokenData != nullptr) {
			delete[] mBrokenData;
		}

		for (VulKan::UniformParameter* UPData : mUniformParams) {
			if (UPData->mBinding == 0) { delete UPData; continue; }
			for (VulKan::Buffer* Data : UPData->mBuffers) {
				if (Data != nullptr) {
					delete Data;
				}
			}
			delete UPData;
		}

		if (mDescriptorSet != nullptr) {
			delete mDescriptorSet;
		}

		if (mDescriptorPool != nullptr) {
			delete mDescriptorPool;
		}
		for (size_t i = 0; i < mSwapChain->getImageCount(); ++i)
		{
			if (mBufferCopyCommandBuffer[i] != nullptr) {
				delete mBufferCopyCommandBuffer[i];
			}
		}
		if (mBufferCopyCommandPool != nullptr) {
			delete mBufferCopyCommandPool;
		}
		
		mObjectCollision = nullptr;
	}

	void GamePlayer::setGamePlayerMatrix(float time, const int& frameCount, bool mode)
	{
		if (mUniform.StrikeState > 0) {
			mUniform.StrikeState -= 0.5f * time;
			if (mUniform.StrikeState < 0) {
				mUniform.StrikeState = 0;
			}
		}
		mUniform.mModelMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(mObjectCollision->pos.x, mObjectCollision->pos.y, 0.0f));//位移矩阵
		mUniform.mModelMatrix = glm::rotate(mUniform.mModelMatrix, (float)glm::radians(mObjectCollision->angle * 180.0f / 3.14f), glm::vec3(0.0f, 0.0f, 1.0f));
		if (mode) {
			for (int i = 0; i < frameCount; ++i) {
				mUniformParams[1]->mBuffers[i]->updateBufferByMap((void*)(&mUniform), sizeof(ObjectUniform));
			}
		}
		else {
			mUniformParams[1]->mBuffers[frameCount]->updateBufferByMap((void*)(&mUniform), sizeof(ObjectUniform));
		}
	}

	void GamePlayer::UpDataBroken(bool* Broken) {
		GetPixelSPointer();
		for (size_t x = 0; x < 16; ++x)
		{
			for (size_t y = 0; y < 16; ++y)
			{
				SetPixelS(x, y, Broken[x * 16 + y]);
			}
		}
		EndPixelSPointer();
	}

	bool GamePlayer::GetCrucial(int x, int y) {
		if (x >= 6 && x <= 9 && y >= 6 && y <= 9) {
			return true;
		}
		return false;
	}

	void GamePlayer::initUniformManager(
		VulKan::Device* device,
		const VulKan::CommandPool* commandPool,
		int frameCount, 
		int textureID,
		const VkDescriptorSetLayout mDescriptorSetLayout,
		std::vector<VulKan::Buffer*> VPMstdBuffer,
		VulKan::Sampler* sampler
	)
	{
		LOGI("GamePlayer::initUniformManager() called");
		mPixelTexture = new VulKan::PixelTexture(device, commandPool, pixelS[textureID],16,16,4, sampler);
		unsigned char* LTexturePointer = (unsigned char*)mPixelTexture->getHOSTImagePointer();
		unsigned char Lcolor[4] = { 255,0,0,255 };
		memcpy(&LTexturePointer[(16 * 4) * 14 + 8 * 4], Lcolor, 4);
		memcpy(&LTexturePointer[(16 * 4) * 14 + 7 * 4], Lcolor, 4);
		memcpy(&LTexturePointer[(16 * 4) * 15 + 8 * 4], Lcolor, 4);
		memcpy(&LTexturePointer[(16 * 4) * 15 + 7 * 4], Lcolor, 4);
		mPixelTexture->endHOSTImagePointer();
		mPixelTexture->UpDataImage();

		VulKan::UniformParameter* vpParam = new VulKan::UniformParameter();
		vpParam->mBinding = 0;
		vpParam->mCount = 1;
		vpParam->mDescriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		vpParam->mSize = sizeof(VPMatrices);
		vpParam->mStage = VK_SHADER_STAGE_VERTEX_BIT;
		vpParam->mBuffers = VPMstdBuffer;

		/*for (int i = 0; i < frameCount; ++i) {
			VulKan::Buffer* buffer = VulKan::Buffer::createUniformBuffer(device, vpParam->mSize, nullptr);
			vpParam->mBuffers.push_back(buffer);
		}*/

		mUniformParams.push_back(vpParam);

		VulKan::UniformParameter* objectParam = new VulKan::UniformParameter();
		objectParam->mBinding = 1;
		objectParam->mCount = 1;
		objectParam->mDescriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		objectParam->mSize = sizeof(ObjectUniform);
		objectParam->mStage = VK_SHADER_STAGE_VERTEX_BIT;

		for (int i = 0; i < frameCount; ++i) {
			VulKan::Buffer* buffer = VulKan::Buffer::createUniformBuffer(device, objectParam->mSize, nullptr);
			objectParam->mBuffers.push_back(buffer);
		}

		mUniformParams.push_back(objectParam);

		VulKan::UniformParameter* textureParam = new VulKan::UniformParameter();
		textureParam->mBinding = 2;
		textureParam->mCount = 1;
		textureParam->mDescriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		textureParam->mStage = VK_SHADER_STAGE_FRAGMENT_BIT;
		//textureParam->mTexture = new Texture(device, commandPool, texturepath);
		textureParam->mPixelTexture = mPixelTexture;

		mUniformParams.push_back(textureParam);


		//各种类型申请多少个
		mDescriptorPool = new VulKan::DescriptorPool(device);
		mDescriptorPool->build(mUniformParams, frameCount);

		//将申请的各种类型按照Layout绑定起来
		mDescriptorSet = new VulKan::DescriptorSet(device, mUniformParams, mDescriptorSetLayout, mDescriptorPool, frameCount);

		setGamePlayerMatrix(frameCount, true);
	}

	void GamePlayer::InitCommandBuffer() {
		VkCommandBufferInheritanceInfo InheritanceInfo{};
		InheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
		InheritanceInfo.renderPass = mRenderPass->getRenderPass();

		for (size_t i = 0; i < mSwapChain->getImageCount(); ++i)
		{
			InheritanceInfo.framebuffer = mSwapChain->getFrameBuffer(i);

			mBufferCopyCommandBuffer[i]->begin(VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT, InheritanceInfo);//开始录制二级指令
			mBufferCopyCommandBuffer[i]->bindGraphicPipeline(mPipeline->getPipeline());//获得渲染管线

			mBufferCopyCommandBuffer[i]->bindDescriptorSet(mPipeline->getLayout(), getDescriptorSet(i));//获得 模型位置数据， 贴图数据，……
			mBufferCopyCommandBuffer[i]->bindVertexBuffer(getVertexBuffers());//获取顶点数据，UV值
			mBufferCopyCommandBuffer[i]->bindIndexBuffer(getIndexBuffer()->getBuffer());//获得顶点索引
			mBufferCopyCommandBuffer[i]->drawIndex(getIndexCount());//获取绘画物体的顶点个数

			mBufferCopyCommandBuffer[i]->end();
		}
	}

	void GamePlayer::GetPixelSPointer() {
		TexturePointer = (unsigned char*)mPixelTexture->getHOSTImagePointer();
	}
	void GamePlayer::SetPixelS(unsigned int x, unsigned int y, bool Switch) {
		if (mBrokenData[x * 16 + y] == Switch) {
			return;
		}
		mBrokenData[x * 16 + y] = Switch;
		if (Switch) {
			memset(&TexturePointer[(x * 16 + y) * 4 + 3], 255, 1);
		}
		else {
			memset(&TexturePointer[(x * 16 + y) * 4 + 3], 0, 1);
			mUniform.StrikeState = 0.5f;
			if (GetCrucial(x, y)) {
				DeathInBattle = true;
			}
		}
	}
	void GamePlayer::EndPixelSPointer() {
		mPixelTexture->endHOSTImagePointer();
		mPixelTexture->UpDataImage();
	}
	void GamePlayer::UpData() {
		if (mPixelQueue->GetNumber() != 0) {
			PixelState* LPixel;
			GetPixelSPointer();
			while (mPixelQueue->GetNumber() != 0)
			{
				LPixel = mPixelQueue->pop();
				if (LPixel != nullptr) {
					if ((LPixel->X >= 0) && (LPixel->Y >= 0) && (LPixel->X < 16) && (LPixel->Y < 16)) {
						SetPixelS(LPixel->X, LPixel->Y, LPixel->State);
					}
				}
			}
			EndPixelSPointer();
		}
	}

	void GamePlayer::RegisterBulletHitCallback() {
		// 通过全局 PhysicsCollision 回调系统注册"坦克被击中"事件
		// 当粒子（子弹）进入坦克形状时触发 TankHitByBullet
		mSquarePhysics->mCollision.AddCollisionEnterListener(
			mObjectCollision,
			[this](const PhysicsBlock::PhysicsFormwork* a,
				const PhysicsBlock::PhysicsFormwork* b,
				const PhysicsBlock::BaseArbiter* arbiter) {
				// a 是本坦克，b 是对方物体
				TankHitByBullet(a, b, arbiter, this);
			}
		);
	}

	void GamePlayer::ResetTank(float X, float Y) {
		// 复位死亡标记
		DeathInBattle = false;
		// 清空待处理伤害
		while (mPixelQueue->GetNumber() != 0) { mPixelQueue->pop(); }
		// 恢复所有像素
		for (size_t x = 0; x < 16; ++x) {
			for (size_t y = 0; y < 16; ++y) {
				mObjectCollision->at({ (int)x, (int)y }).Entity = true;
				mObjectCollision->at({ (int)x, (int)y }).Collision = true;
				mBrokenData[x * 16 + y] = true;
			}
		}
		mObjectCollision->UpdateAll();
		mObjectCollision->pos = Vec2_{ X, Y };
		mObjectCollision->speed = Vec2_{ 0, 0 };
		mObjectCollision->angle = 0;
		mUniform.StrikeState = 0;
		// 刷新贴图
		GetPixelSPointer();
		for (size_t x = 0; x < 16; ++x) {
			for (size_t y = 0; y < 16; ++y) {
				memset(&TexturePointer[(x * 16 + y) * 4 + 3], 255, 1);
			}
		}
		EndPixelSPointer();
	}
}
