#include "PipelineS.h"
#include "CreatePipeline.h"

namespace VulKan {

	PipelineS::PipelineS(Device* Device, RenderPass* RenderPass) {
		mDevice = Device;
		mRenderPass = RenderPass;

		mPipelineS.push_back(MainPipeline(new Pipeline(Device, RenderPass), Device));
		mNewPipelineS.push_back(MainPipeline);
	}

	PipelineS::~PipelineS() {
		for (size_t i = 0; i < mPipelineS.size(); i++)
		{
			delete mPipelineS[i];
		}
	}

	void PipelineS::ReconfigurationPipelineS() {
		for (size_t i = 0; i < mPipelineS.size(); i++)
		{
			mPipelineS[i]->ReconfigurationPipeline();
			mPipelineS[i] = mNewPipelineS[i](mPipelineS[i], mDevice);
		}
	}
}