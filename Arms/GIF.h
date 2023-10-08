#include "../VulKanTool/texture.h"
#include "../Vulkan/pipeline.h"
#include "../Vulkan/description.h"
#include "../Vulkan/descriptorSet.h"
#include "../Vulkan/swapChain.h"

namespace GAME {
	class GifPipeline
	{
	public:
		GifPipeline(unsigned int mHeight, unsigned int mWidth, VulKan::Device* Device, VulKan::RenderPass* RenderPass);
		~GifPipeline();

		[[nodiscard]] constexpr VulKan::Pipeline* GetPipeline() const noexcept {
			return mPipeline;
		}

	private:
		VulKan::Device* mDevice = nullptr;
		VulKan::Pipeline* mPipeline = nullptr;
		VulKan::RenderPass* mRenderPass = nullptr;
	};

	struct ObjectUniformGIF {
		glm::mat4 mModelMatrix;
		float chuang = 0.0f;
		unsigned int zhen = 0;

		ObjectUniformGIF() {
			mModelMatrix = glm::mat4(1.0f);
		}
	};


	class GIF
	{
	public:
		GIF(VulKan::Device* device, GifPipeline* pipeline, VulKan::SwapChain* swapChain, VulKan::RenderPass* RenderPass, unsigned int FrameS);
		~GIF();

		void initUniformManager(
			const char* path, 
			std::vector<VulKan::Buffer*> VPMstdBuffer, 
			VulKan::Sampler* sampler
		);

		//更新描述符，模型位置
		//mode = false 模式：持续更新位置（运动） [ 变换矩阵，那个GPU画布 ];
		//mode = true  模式：一次设置位置（静止） [ 变换矩阵，GPU画布数量，true ];
		void SetGamePlayerMatrix(glm::mat4 Matrix, const int& frameCount, bool mode);

		//GIF更新画面（移动UV）
		void SetFrame(unsigned int frame, const int& frameCount);

		void UpDataCommandBuffer();



		//获取  顶点和UV  VkBuffer数组
		[[nodiscard]] std::vector<VkBuffer> getVertexBuffers() const {
			return { mPositionBuffer->getBuffer(), mUVBuffer->getBuffer() };
		}

		//获得 索引Buffer
		[[nodiscard]] VulKan::Buffer* getIndexBuffer() const noexcept { return mIndexBuffer; }

		//获得 索引数量
		[[nodiscard]] size_t getIndexCount() const noexcept { return mIndexDatasSize; }

		//获得 描述符
		[[nodiscard]] VkDescriptorSet getDescriptorSet(int frameCount) const { return mDescriptorSet->getDescriptorSet(frameCount); }

		[[nodiscard]] VkCommandBuffer getCommandBuffer(int i)  const { return mCommandBuffer[i]->getCommandBuffer(); }

		void SetRecreate(GifPipeline* GifPipeline, VulKan::SwapChain* SwapChain) noexcept {
			mGifPipeline = GifPipeline;
			mSwapChain = SwapChain;
		}


	private://模型变换矩阵
		ObjectUniformGIF mUniform;
		unsigned int GIFFrame;

	private://模型   顶点，UV，顶点索引
		VulKan::Buffer* mPositionBuffer{ nullptr };
		VulKan::Buffer* mUVBuffer{ nullptr };
		VulKan::Buffer* mIndexBuffer{ nullptr };
		size_t mIndexDatasSize;

		VulKan::Texture* mtexture = nullptr; //贴图

	private://描述模型   位置   贴图
		std::vector<VulKan::UniformParameter*> mUniformParams;

		VulKan::DescriptorPool* mDescriptorPool{ nullptr };
		VulKan::DescriptorSet* mDescriptorSet{ nullptr };//指令录制用的数据

	private:
		VulKan::CommandPool** mCommandPool;
		VulKan::CommandBuffer** mCommandBuffer;

		VulKan::Device* mDevice;
		GifPipeline* mGifPipeline;
		VulKan::SwapChain* mSwapChain;
		VulKan::RenderPass* mRenderPass;
		unsigned int mFrameS; //GIF 的 帧数

	};
}