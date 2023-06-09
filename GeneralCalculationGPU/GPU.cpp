#include "GPU.h"
#include "lodepng.h"

struct Pixel {
    float r, g, b, a;
};

uint32_t* readFile(uint32_t& length, const char* filename) {

    FILE* fp = fopen(filename, "rb");
    if (fp == NULL) {
        printf("Could not find or open file: %s\n", filename);
    }

    // get file size.
    fseek(fp, 0, SEEK_END);
    long filesize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    long filesizepadded = long(ceil(filesize / 4.0)) * 4;

    // read file contents.
    char* str = new char[filesizepadded];
    fread(str, filesize, sizeof(char), fp);
    fclose(fp);

    // data padding. 
    for (int i = filesize; i < filesizepadded; i++) {
        str[i] = 0;
    }

    length = filesizepadded;
    return (uint32_t*)str;
}



void jishuan() {
	unsigned int Size = 128;
	auto y = std::vector<float>(Size, 1.0f);
	auto ydd = std::vector<float>(Size, 15.0f);
	auto x = std::vector<float>(Size, 20.0f);

	for (size_t i = 0; i < y.size(); i++)
	{
		std::cout << y[i] << " ";
	}
	std::cout << std::endl;

	auto instance = vuh::Instance();
	auto device = instance.devices().at(0);    // just get the first available device

	auto d_y = vuh::Array<float>(device, y);   // create device arrays and copy data
	auto d_x = vuh::Array<float>(device, x);

	d_y.fromHost(ydd.begin(), ydd.end());

	using Specs = vuh::typelist<uint32_t>;     // shader specialization constants interface
	struct Params { uint32_t size; float a; };    // shader push-constants interface
	auto program = vuh::Program<Specs, Params>(device, "saxpy.spv"); // load shader
	program.grid(Size / 64).spec(64)({ Size, 0.1 }, d_y, d_x); // run once, wait for completion

	d_y.toHost(begin(y));                      // copy data back to host

	for (size_t i = 0; i < y.size(); i++)
	{
		std::cout << y[i] << " ";
	}
	std::cout << std::endl;
}


void GetWarfareMist(int* y, unsigned int Size_x, unsigned int Size_y, int wanjia_x, int wanjia_y) {
	auto instance = vuh::Instance();
	auto device = instance.devices().at(0);
	std::vector<int> lx, ly, dddd;
	unsigned int suzhi = 900;
	for (size_t i = 0; i < suzhi; i++)
	{
		lx.push_back(int(cos(double(i * 0.1f) / 180.0f * 3.14f) * 250));
		ly.push_back(int(sin(double(i * 0.1f) / 180.0f * 3.14f) * 250));
	}
	for (size_t i = 0; i < (Size_x * Size_y); i++)
	{
		dddd.push_back(y[i]);
	}
	std::cout << y[0] << std::endl;

	auto wj_x = vuh::Array<int>(device, lx);
	auto wj_y = vuh::Array<int>(device, ly);
	auto d_y = vuh::Array<int>(device, dddd);
	using Specs = vuh::typelist<uint32_t>;     // shader specialization constants interface
	struct Params { uint32_t size; int x; int y; unsigned int y_size; };
	auto program = vuh::Program<Specs, Params>(device, "WarfareMist.spv");
	program.grid(suzhi / 64).spec(64)({ suzhi, wanjia_x,wanjia_y,Size_y }, wj_x, wj_y, d_y);

	d_y.toHost(y);
	//VkBuffer bbb = d_y.buffer();
}


void Mandelbrot(GAME::VulKan::Device* device) {
    //GAME::VulKan::Instance* mInstance = new GAME::VulKan::Instance(true);
    //unsigned int w = 100, h = 100;
    //GAME::VulKan::Window* mWin = new GAME::VulKan::Window(w, h, 0, 0);
    //GAME::VulKan::WindowSurface* mWindowSurface = new GAME::VulKan::WindowSurface(mInstance, mWin);
    //GAME::VulKan::Device* device = new GAME::VulKan::Device(mInstance, mWindowSurface);

	VkDevice mDevice = device->getDevice();

	VkDescriptorSetLayout descriptorSetLayout;
	VkDescriptorPool descriptorPool;
	VkDescriptorSet descriptorSet;

    VkPipeline pipeline;
    VkPipelineLayout pipelineLayout;
    VkShaderModule computeShaderModule;

	const int WIDTH = 1000; // Size of rendered mandelbrot set.
	const int HEIGHT = 1000; // Size of renderered mandelbrot set.
	const int WORKGROUP_SIZE = 32; // Workgroup size in compute shader.

    unsigned Siezd = WIDTH * HEIGHT * WORKGROUP_SIZE;

	auto buffer = new GAME::VulKan::Buffer(device, Siezd, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, VK_SHARING_MODE_EXCLUSIVE);

    auto buffer2 = new GAME::VulKan::Buffer(device, sizeof(float), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, VK_SHARING_MODE_EXCLUSIVE);
    float dadada = 0.5f;
    buffer2->updateBufferByMap(&dadada, sizeof(float));

	
	VkDescriptorSetLayoutBinding descriptorSetLayoutBinding[2] = {};
	descriptorSetLayoutBinding[0].binding = 0; // binding = 0
	descriptorSetLayoutBinding[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	descriptorSetLayoutBinding[0].descriptorCount = 1;
	descriptorSetLayoutBinding[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    descriptorSetLayoutBinding[1].binding = 1; // binding = 0
    descriptorSetLayoutBinding[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorSetLayoutBinding[1].descriptorCount = 1;
    descriptorSetLayoutBinding[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {};
	descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	descriptorSetLayoutCreateInfo.bindingCount = 2; // only a single binding in this descriptor set layout. //*******************************************
	descriptorSetLayoutCreateInfo.pBindings = descriptorSetLayoutBinding;                                   //*******************************************

	// Create the descriptor set layout. 
	vkCreateDescriptorSetLayout(mDevice, &descriptorSetLayoutCreateInfo, NULL, &descriptorSetLayout);

	VkDescriptorPoolSize descriptorPoolSize[2] = {};                                                        //*******************************************
    descriptorPoolSize[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorPoolSize[0].descriptorCount = 1;

    descriptorPoolSize[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;                                         //*******************************************
    descriptorPoolSize[1].descriptorCount = 1;                                                              //*******************************************


    VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {};
    descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorPoolCreateInfo.maxSets = 1; // we only need to allocate one descriptor set from the pool.
    descriptorPoolCreateInfo.poolSizeCount = 2;                                                             //*******************************************
    descriptorPoolCreateInfo.pPoolSizes = descriptorPoolSize;

    // create descriptor pool.
    vkCreateDescriptorPool(mDevice, &descriptorPoolCreateInfo, NULL, &descriptorPool);

    /*
    With the pool allocated, we can now allocate the descriptor set. 
    */
    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {};
    descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptorSetAllocateInfo.descriptorPool = descriptorPool; // pool to allocate from.
    descriptorSetAllocateInfo.descriptorSetCount = 1; // allocate a single descriptor set.
    descriptorSetAllocateInfo.pSetLayouts = &descriptorSetLayout;

    // allocate descriptor set.
    vkAllocateDescriptorSets(mDevice, &descriptorSetAllocateInfo, &descriptorSet);

    /*
    Next, we need to connect our actual storage buffer with the descrptor. 
    We use vkUpdateDescriptorSets() to update the descriptor set.
    */

    // Specify the buffer to bind to the descriptor.

    VkWriteDescriptorSet writeDescriptorSet[2] = {};                                                //*******************************************
    writeDescriptorSet[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDescriptorSet[0].dstSet = descriptorSet; // write to this descriptor set.
    writeDescriptorSet[0].dstBinding = 0; // write to the first, and only binding.
    writeDescriptorSet[0].descriptorCount = 1; // update a single descriptor.
    writeDescriptorSet[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; // storage buffer.
    writeDescriptorSet[0].pBufferInfo = &buffer->getBufferInfo();

    writeDescriptorSet[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDescriptorSet[1].dstSet = descriptorSet; // write to this descriptor set.
    writeDescriptorSet[1].dstBinding = 1; // write to the first, and only binding.
    writeDescriptorSet[1].descriptorCount = 1; // update a single descriptor.
    writeDescriptorSet[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; // storage buffer.
    writeDescriptorSet[1].pBufferInfo = &buffer2->getBufferInfo();


    // perform the update of the descriptor set.
    vkUpdateDescriptorSets(mDevice, 2, writeDescriptorSet, 0, NULL);                                //*******************************************

    uint32_t filelength;
    // the code in comp.spv was created by running the command:
    // glslangValidator.exe -V shader.comp
    uint32_t* code = readFile(filelength, "comp.spv");
    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.pCode = code;
    createInfo.codeSize = filelength;

    vkCreateShaderModule(mDevice, &createInfo, NULL, &computeShaderModule);
    delete[] code;

    /*
    Now let us actually create the compute pipeline.
    A compute pipeline is very simple compared to a graphics pipeline.
    It only consists of a single stage with a compute shader.

    So first we specify the compute shader stage, and it's entry point(main).
    */
    VkPipelineShaderStageCreateInfo shaderStageCreateInfo = {};
    shaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStageCreateInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    shaderStageCreateInfo.module = computeShaderModule;
    shaderStageCreateInfo.pName = "main";

    /*
    The pipeline layout allows the pipeline to access descriptor sets.
    So we just specify the descriptor set layout we created earlier.
    */
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
    pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCreateInfo.setLayoutCount = 1;
    pipelineLayoutCreateInfo.pSetLayouts = &descriptorSetLayout;
    vkCreatePipelineLayout(mDevice, &pipelineLayoutCreateInfo, NULL, &pipelineLayout);

    VkComputePipelineCreateInfo pipelineCreateInfo = {};
    pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineCreateInfo.stage = shaderStageCreateInfo;
    pipelineCreateInfo.layout = pipelineLayout;

    /*
    Now, we finally create the compute pipeline.
    */
    vkCreateComputePipelines(mDevice, VK_NULL_HANDLE,1, &pipelineCreateInfo,NULL, &pipeline);

    GAME::VulKan::CommandPool* mCommandPool = new GAME::VulKan::CommandPool(device);
    GAME::VulKan::CommandBuffer* mCommandBuffer = new GAME::VulKan::CommandBuffer(device, mCommandPool);

    mCommandBuffer->begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    mCommandBuffer->bindGraphicPipeline(pipeline, VK_PIPELINE_BIND_POINT_COMPUTE);
    mCommandBuffer->bindDescriptorSet(pipelineLayout, descriptorSet, VK_PIPELINE_BIND_POINT_COMPUTE);
    vkCmdDispatch(mCommandBuffer->getCommandBuffer(), (uint32_t)ceil(WIDTH / float(WORKGROUP_SIZE)), (uint32_t)ceil(HEIGHT / float(WORKGROUP_SIZE)), 1);
    mCommandBuffer->end();
    mCommandBuffer->submitSync(device->getGraphicQueue(), VK_NULL_HANDLE);


    void* mappedMemory = buffer->getupdateBufferByMap();
    // Map the buffer memory, so that we can read from it on the CPU.
    Pixel* pmappedMemory = (Pixel*)mappedMemory;

    // Get the color data from the buffer, and cast it to bytes.
    // We save the data to a vector.
    std::vector<unsigned char> image;
    image.reserve(WIDTH* HEIGHT * 4);
    for (int i = 0; i < WIDTH * HEIGHT; i += 1) {
        image.push_back((unsigned char)(255.0f * (pmappedMemory[i].r)));
        image.push_back((unsigned char)(255.0f * (pmappedMemory[i].g)));
        image.push_back((unsigned char)(255.0f * (pmappedMemory[i].b)));
        image.push_back((unsigned char)(255.0f * (pmappedMemory[i].a)));
    }
    // Done reading, so unmap.
    buffer->endupdateBufferByMap();

    // Now we save the acquired color data to a .png.
    unsigned error = lodepng::encode("mandelbrot.png", image, WIDTH, HEIGHT);
    if (error) printf("encoder error %d: %s", error, lodepng_error_text(error));

    vkDestroyPipelineLayout(mDevice, pipelineLayout, nullptr);
    vkDestroyPipeline(mDevice, pipeline, nullptr);
    vkDestroyShaderModule(mDevice, computeShaderModule, nullptr);
    //vkFreeDescriptorSets(mDevice, descriptorPool, 1, &descriptorSet);
    vkDestroyDescriptorSetLayout(mDevice, descriptorSetLayout, nullptr);
    vkDestroyDescriptorPool(mDevice, descriptorPool, nullptr);
    delete buffer;
    delete mCommandBuffer;
    delete mCommandPool;
}