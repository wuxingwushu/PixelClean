#pragma once
#include "../Vulkan/pipeline.h"

namespace VulKan {

	Pipeline* MainPipeline(Pipeline* Pipeline, Device* Device);

	//画线
	Pipeline* LinePipeline(Pipeline* Pipeline, Device* Device);

	//画点
	Pipeline* SpotPipeline(Pipeline* Pipeline, Device* Device);

	//GIF
	Pipeline* GIFPipeline(Pipeline* Pipeline, Device* Device);
}
