#pragma once
#include "../Vulkan/pipeline.h"


namespace VulKan {

	typedef Pipeline* (*NewPipeline)(Pipeline* P, Device* D);

	enum PipelineMods//要根据 PipelineS 构建 EstablishPipeline 顺序来；
	{
		MainMods = 0,
		LineMods,
		SpotMods,
		GifMods,
		DamagePrompt,
		UVDynamicDiagram
	};

	class PipelineS
	{
	public:
		PipelineS(Device* Device, RenderPass* RenderPass);
		~PipelineS();

		void EstablishPipeline(NewPipeline F) {
			mPipelineS.push_back(F(new Pipeline(mDevice, mRenderPass), mDevice));
			mNewPipelineS.push_back(F);
		};

		void ReconfigurationPipelineS();

		Pipeline* GetPipeline(PipelineMods I) {
			return mPipelineS[I];
		}

	private:
		Device* mDevice;
		RenderPass* mRenderPass;
		std::vector<Pipeline*> mPipelineS;
		std::vector<NewPipeline> mNewPipelineS;
	};

}