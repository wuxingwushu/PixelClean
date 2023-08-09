#pragma once
#include "../Vulkan/pipeline.h"


namespace VulKan {

	typedef Pipeline* (*NewPipeline)(Pipeline* P, Device* D);

	class PipelineS
	{
	public:
		PipelineS(Device* Device, RenderPass* RenderPass);
		~PipelineS();

		void ReconfigurationPipelineS();

		Pipeline* GetPipeline(unsigned int I) {
			return mPipelineS[I];
		}

	private:
		Device* mDevice;
		RenderPass* mRenderPass;
		std::vector<Pipeline*> mPipelineS;
		std::vector<NewPipeline> mNewPipelineS;
	};

}