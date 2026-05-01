#include "RadianceCascades.h"
#include "../../Vulkan/device.h"
#include "../../Vulkan/swapChain.h"
#include "../../Vulkan/window.h"
#include "../../GlobalVariable.h"
#include <fstream>
#include <cmath>
#include <algorithm>
#include <GLFW/glfw3.h>
namespace GAME {

const std::string RadianceCascades::SDF_SHADER_PATH = "shaders/RC_SDF.spv";
const std::string RadianceCascades::CASCADE_SHADER_PATH = "shaders/RC_Cascade.spv";
const std::string RadianceCascades::DISPLAY_SHADER_PATH = "shaders/RC_Display.spv";

RadianceCascades::RadianceCascades(Configuration wConfiguration)
    : Configuration{wConfiguration}
{
    Global::Monitor = true;

    int sw = mSwapChain->getExtent().width;
    int sh = mSwapChain->getExtent().height;
    mCascadeResX = sw / 4;
    mCascadeResY = sh / 4;

    createBuffers();
    createComputePipelines();
    createDescriptorSets();
    createImGuiResources();
}

RadianceCascades::~RadianceCascades() {
    cleanup();
}

int RadianceCascades::calculateTotalCascadeEntries() const {
    int total = 0;
    for (int n = 0; n < N_CASCADES; ++n) {
        int sn_x = mCascadeResX >> n;
        int sn_y = mCascadeResY >> n;
        int dn = (n == 0) ? 1 : (C_DRES << (2 * (n - 1)));
        total += sn_x * sn_y * dn;
    }
    return total;
}

void RadianceCascades::createBuffers() {
    auto device = mDevice;
    int sdfWidth = mSwapChain->getExtent().width;
    int sdfHeight = mSwapChain->getExtent().height;
    int totalCascade = calculateTotalCascadeEntries();
    VkDeviceSize paramSize = 256;
    VkDeviceSize sdfSize = sdfWidth * sdfHeight * sizeof(float) * 4;
    VkDeviceSize cascadeSize = totalCascade * sizeof(float) * 4;
    VkDeviceSize outputSize = sdfWidth * sdfHeight * sizeof(uint32_t);

    mParamBuffer = new VulKan::Buffer(device, paramSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    mSDFBuffer = new VulKan::Buffer(device, sdfSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    {
        float maxF = std::numeric_limits<float>::max();
        void* data = mSDFBuffer->getupdateBufferByMap();
        struct SDFEntry { float dist; float r, g, b; };
        SDFEntry* entries = static_cast<SDFEntry*>(data);
        for (int i = 0; i < sdfWidth * sdfHeight; ++i) {
            entries[i] = { maxF, 0.0f, 0.0f, 0.0f };
        }
        mSDFBuffer->endupdateBufferByMap();
    }

    mCascadeBufferA = new VulKan::Buffer(device, cascadeSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    mCascadeBufferB = new VulKan::Buffer(device, cascadeSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    {
        void* dataA = mCascadeBufferA->getupdateBufferByMap();
        memset(dataA, 0, (size_t)cascadeSize);
        mCascadeBufferA->endupdateBufferByMap();
        void* dataB = mCascadeBufferB->getupdateBufferByMap();
        memset(dataB, 0, (size_t)cascadeSize);
        mCascadeBufferB->endupdateBufferByMap();
    }

    mOutputBuffer = new VulKan::Buffer(device, outputSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    mOutputImage = new VulKan::Image(
        device, sdfWidth, sdfHeight,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_SAMPLE_COUNT_1_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT);

    {
        VkImageSubresourceRange range{};
        range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        range.levelCount = 1; range.layerCount = 1;
        mOutputImage->setImageLayout(
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            range, mCommandPool);
    }

    mComputeCommandPool = new VulKan::CommandPool(device,
        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    mComputeCommandBuffer = new VulKan::CommandBuffer(device, mComputeCommandPool);
}

static std::vector<char> readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open shader file: " + filename);
    }
    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();
    return buffer;
}

static VkShaderModule createShaderModule(VkDevice device, const std::vector<char>& code) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());
    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shader module");
    }
    return shaderModule;
}

static void createComputePipeline(
    VkDevice device, const std::string& shaderPath,
    const std::vector<VkDescriptorSetLayoutBinding>& bindings,
    VkShaderModule& outShaderModule,
    VkDescriptorSetLayout& outDescriptorSetLayout,
    VkPipelineLayout& outPipelineLayout,
    VkPipeline& outPipeline)
{
    auto code = readFile(shaderPath);
    outShaderModule = createShaderModule(device, code);

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();
    vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &outDescriptorSetLayout);

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &outDescriptorSetLayout;
    vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &outPipelineLayout);

    VkPipelineShaderStageCreateInfo shaderStageInfo{};
    shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    shaderStageInfo.module = outShaderModule;
    shaderStageInfo.pName = "main";

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.stage = shaderStageInfo;
    pipelineInfo.layout = outPipelineLayout;
    vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &outPipeline);
}

void RadianceCascades::createComputePipelines() {
    VkDevice device = mDevice->getDevice();

    std::vector<VkDescriptorSetLayoutBinding> sdfBindings = {
        {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
    };
    createComputePipeline(device, SDF_SHADER_PATH, sdfBindings,
        mSDFShaderModule, mSDFDescriptorSetLayout, mSDFPipelineLayout, mSDFPipeline);

    std::vector<VkDescriptorSetLayoutBinding> cascadeBindings = {
        {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
    };
    createComputePipeline(device, CASCADE_SHADER_PATH, cascadeBindings,
        mCascadeShaderModule, mCascadeDescriptorSetLayout, mCascadePipelineLayout, mCascadePipeline);

    std::vector<VkDescriptorSetLayoutBinding> displayBindings = {
        {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
    };
    createComputePipeline(device, DISPLAY_SHADER_PATH, displayBindings,
        mDisplayShaderModule, mDisplayDescriptorSetLayout, mDisplayPipelineLayout, mDisplayPipeline);
}

void RadianceCascades::createDescriptorSets() {
    VkDevice device = mDevice->getDevice();

    std::vector<VkDescriptorPoolSize> poolSizes = {
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 20}
    };
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets = 5;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    vkCreateDescriptorPool(device, &poolInfo, nullptr, &mDescriptorPool);

    // SDF
    {
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = mDescriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &mSDFDescriptorSetLayout;
        vkAllocateDescriptorSets(device, &allocInfo, &mSDFDescriptorSet);

        VkDescriptorBufferInfo paramInfo = mParamBuffer->getBufferInfo();
        VkDescriptorBufferInfo sdfInfo = mSDFBuffer->getBufferInfo();
        VkWriteDescriptorSet writes[2] = {};
        writes[0] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, mSDFDescriptorSet, 0, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &paramInfo, nullptr};
        writes[1] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, mSDFDescriptorSet, 1, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &sdfInfo, nullptr};
        vkUpdateDescriptorSets(device, 2, writes, 0, nullptr);
    }

    // Cascade (2套乒乓)
    {
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = mDescriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &mCascadeDescriptorSetLayout;

        VkDescriptorBufferInfo paramInfo = mParamBuffer->getBufferInfo();
        VkDescriptorBufferInfo sdfInfo = mSDFBuffer->getBufferInfo();

        vkAllocateDescriptorSets(device, &allocInfo, &mCascadeDescriptorSets[0]);
        VkDescriptorBufferInfo readA = mCascadeBufferA->getBufferInfo();
        VkDescriptorBufferInfo writeB = mCascadeBufferB->getBufferInfo();
        VkWriteDescriptorSet w0[4] = {};
        w0[0] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, mCascadeDescriptorSets[0], 0, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &paramInfo, nullptr};
        w0[1] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, mCascadeDescriptorSets[0], 1, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &sdfInfo, nullptr};
        w0[2] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, mCascadeDescriptorSets[0], 2, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &readA, nullptr};
        w0[3] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, mCascadeDescriptorSets[0], 3, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &writeB, nullptr};
        vkUpdateDescriptorSets(device, 4, w0, 0, nullptr);

        vkAllocateDescriptorSets(device, &allocInfo, &mCascadeDescriptorSets[1]);
        VkDescriptorBufferInfo readB = mCascadeBufferB->getBufferInfo();
        VkDescriptorBufferInfo writeA = mCascadeBufferA->getBufferInfo();
        VkWriteDescriptorSet w1[4] = {};
        w1[0] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, mCascadeDescriptorSets[1], 0, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &paramInfo, nullptr};
        w1[1] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, mCascadeDescriptorSets[1], 1, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &sdfInfo, nullptr};
        w1[2] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, mCascadeDescriptorSets[1], 2, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &readB, nullptr};
        w1[3] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, mCascadeDescriptorSets[1], 3, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &writeA, nullptr};
        vkUpdateDescriptorSets(device, 4, w1, 0, nullptr);
    }

    // Display (2套乒乓)
    {
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = mDescriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &mDisplayDescriptorSetLayout;

        VkDescriptorBufferInfo paramInfo = mParamBuffer->getBufferInfo();
        VkDescriptorBufferInfo sdfInfo = mSDFBuffer->getBufferInfo();
        VkDescriptorBufferInfo outputInfo = mOutputBuffer->getBufferInfo();

        vkAllocateDescriptorSets(device, &allocInfo, &mDisplayDescriptorSets[0]);
        VkDescriptorBufferInfo cascadeB = mCascadeBufferB->getBufferInfo();
        VkWriteDescriptorSet w0[4] = {};
        w0[0] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, mDisplayDescriptorSets[0], 0, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &paramInfo, nullptr};
        w0[1] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, mDisplayDescriptorSets[0], 1, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &sdfInfo, nullptr};
        w0[2] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, mDisplayDescriptorSets[0], 2, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &cascadeB, nullptr};
        w0[3] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, mDisplayDescriptorSets[0], 3, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &outputInfo, nullptr};
        vkUpdateDescriptorSets(device, 4, w0, 0, nullptr);

        vkAllocateDescriptorSets(device, &allocInfo, &mDisplayDescriptorSets[1]);
        VkDescriptorBufferInfo cascadeA = mCascadeBufferA->getBufferInfo();
        VkWriteDescriptorSet w1[4] = {};
        w1[0] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, mDisplayDescriptorSets[1], 0, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &paramInfo, nullptr};
        w1[1] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, mDisplayDescriptorSets[1], 1, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &sdfInfo, nullptr};
        w1[2] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, mDisplayDescriptorSets[1], 2, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &cascadeA, nullptr};
        w1[3] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, mDisplayDescriptorSets[1], 3, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &outputInfo, nullptr};
        vkUpdateDescriptorSets(device, 4, w1, 0, nullptr);
    }
}

void RadianceCascades::createImGuiResources() {
    VkDevice device = mDevice->getDevice();

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;
    vkCreateSampler(device, &samplerInfo, nullptr, &mImGuiSampler);

    VkDescriptorSetLayoutBinding binding{};
    binding.binding = 0;
    binding.descriptorCount = 1;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &binding;
    vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &mImGuiDescriptorSetLayout);

    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = 1;
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets = 1;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    vkCreateDescriptorPool(device, &poolInfo, nullptr, &mImGuiDescriptorPool);

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = mImGuiDescriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &mImGuiDescriptorSetLayout;
    vkAllocateDescriptorSets(device, &allocInfo, &mImGuiDescriptorSet);

    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = mOutputImage->getImageView();
    imageInfo.sampler = mImGuiSampler;

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = mImGuiDescriptorSet;
    write.dstBinding = 0;
    write.dstArrayElement = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo = &imageInfo;
    vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
}

void RadianceCascades::dispatchCompute() {
    int sdfWidth = mSwapChain->getExtent().width;
    int sdfHeight = mSwapChain->getExtent().height;
    int totalCascade = calculateTotalCascadeEntries();
    int toggleIdx = mCascadeToggle ? 1 : 0;

    struct Params {
        float time; float timeDelta;
        int sdfWidth; int sdfHeight;
        float mouseX; float mouseY;
        float mousePrevX; float mousePrevY;
        int mouseLeftDown;
        int frameCount;
        int emissiveMode;
        float brushRadius;
        int c_sResX; int c_sResY;
        int c_dRes; int nCascades;
        float c_intervalLength; float c_smoothDistScale;
        int totalCascadeEntries; int cascadeLevel;
    };
    Params p{};
    p.time = mTime;
    p.timeDelta = 1.0f / 60.0f;
    p.sdfWidth = sdfWidth;
    p.sdfHeight = sdfHeight;
    p.mouseX = static_cast<float>(mMouseX);
    p.mouseY = static_cast<float>(mMouseY);
    p.mousePrevX = static_cast<float>(mMousePrevX);
    p.mousePrevY = static_cast<float>(mMousePrevY);
    p.mouseLeftDown = (mMouseLeftDown || mMouseRightDown) ? 1 : 0;
    p.frameCount = mFrameCount;
    p.emissiveMode = (mMouseRightDown && !mMouseLeftDown) ? 1 : 0;
    p.brushRadius = BRUSH_RADIUS;
    p.c_sResX = mCascadeResX;
    p.c_sResY = mCascadeResY;
    p.c_dRes = C_DRES;
    p.nCascades = N_CASCADES;
    p.c_intervalLength = C_INTERVAL_LENGTH * float(sdfHeight) / 720.0f;
    p.c_smoothDistScale = C_SMOOTH_DIST_SCALE;
    p.totalCascadeEntries = totalCascade;
    p.cascadeLevel = 0;
    mParamBuffer->updateBufferByMap(&p, sizeof(Params));

    mComputeCommandBuffer->begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    VkCommandBuffer cmd = mComputeCommandBuffer->getCommandBuffer();

    auto storageBarrier = [](VkBuffer b, VkDeviceSize sz) {
        VkBufferMemoryBarrier bar{};
        bar.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        bar.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        bar.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        bar.buffer = b; bar.offset = 0; bar.size = sz;
        return bar;
    };

    uint32_t sdfGroups = (sdfWidth * sdfHeight + 63) / 64;

    // Pass 1: SDF
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, mSDFPipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
        mSDFPipelineLayout, 0, 1, &mSDFDescriptorSet, 0, nullptr);
    vkCmdDispatch(cmd, sdfGroups, 1, 1);

    VkBufferMemoryBarrier b1 = storageBarrier(
        mSDFBuffer->getBuffer(), mSDFBuffer->getBufferInfo().range);
    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0, 0, nullptr, 1, &b1, 0, nullptr);

    // Pass 2: Cascade（一次dispatch处理所有级联）
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, mCascadePipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
        mCascadePipelineLayout, 0, 1, &mCascadeDescriptorSets[toggleIdx], 0, nullptr);
    uint32_t cascadeGroups = (totalCascade + 63) / 64;
    vkCmdDispatch(cmd, cascadeGroups, 1, 1);

    VkBuffer barBuf = mCascadeToggle ?
        mCascadeBufferA->getBuffer() : mCascadeBufferB->getBuffer();
    VkBufferMemoryBarrier b2 = storageBarrier(barBuf, mCascadeBufferA->getBufferInfo().range);
    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0, 0, nullptr, 1, &b2, 0, nullptr);

    // Pass 3: Display
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, mDisplayPipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
        mDisplayPipelineLayout, 0, 1, &mDisplayDescriptorSets[toggleIdx], 0, nullptr);
    vkCmdDispatch(cmd, sdfGroups, 1, 1);

    VkBufferMemoryBarrier outBar{};
    outBar.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    outBar.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    outBar.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    outBar.buffer = mOutputBuffer->getBuffer();
    outBar.offset = 0;
    outBar.size = mOutputBuffer->getBufferInfo().range;
    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, nullptr, 1, &outBar, 0, nullptr);

    VkImageSubresourceRange range{};
    range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    range.levelCount = 1; range.layerCount = 1;

    VkImageMemoryBarrier toDst{};
    toDst.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    toDst.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    toDst.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    toDst.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    toDst.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toDst.image = mOutputImage->getImage();
    toDst.subresourceRange = range;
    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &toDst);

    mComputeCommandBuffer->copyBufferToImage(
        mOutputBuffer->getBuffer(),
        mOutputImage->getImage(),
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        sdfWidth, sdfHeight);

    VkImageMemoryBarrier toRead{};
    toRead.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    toRead.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    toRead.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    toRead.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toRead.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    toRead.image = mOutputImage->getImage();
    toRead.subresourceRange = range;
    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &toRead);

    mComputeCommandBuffer->end();
    mComputeCommandBuffer->submitSync(mDevice->getGraphicQueue(), VK_NULL_HANDLE);
    mCascadeToggle = !mCascadeToggle;
}

void RadianceCascades::cleanup() {
    VkDevice device = mDevice->getDevice();
    vkDeviceWaitIdle(device);

    if (mImGuiDescriptorSetLayout) vkDestroyDescriptorSetLayout(device, mImGuiDescriptorSetLayout, nullptr);
    if (mImGuiDescriptorPool) vkDestroyDescriptorPool(device, mImGuiDescriptorPool, nullptr);
    if (mImGuiSampler) vkDestroySampler(device, mImGuiSampler, nullptr);
    if (mDescriptorPool) vkDestroyDescriptorPool(device, mDescriptorPool, nullptr);

    if (mSDFPipeline) vkDestroyPipeline(device, mSDFPipeline, nullptr);
    if (mSDFPipelineLayout) vkDestroyPipelineLayout(device, mSDFPipelineLayout, nullptr);
    if (mSDFDescriptorSetLayout) vkDestroyDescriptorSetLayout(device, mSDFDescriptorSetLayout, nullptr);
    if (mSDFShaderModule) vkDestroyShaderModule(device, mSDFShaderModule, nullptr);

    if (mCascadePipeline) vkDestroyPipeline(device, mCascadePipeline, nullptr);
    if (mCascadePipelineLayout) vkDestroyPipelineLayout(device, mCascadePipelineLayout, nullptr);
    if (mCascadeDescriptorSetLayout) vkDestroyDescriptorSetLayout(device, mCascadeDescriptorSetLayout, nullptr);
    if (mCascadeShaderModule) vkDestroyShaderModule(device, mCascadeShaderModule, nullptr);

    if (mDisplayPipeline) vkDestroyPipeline(device, mDisplayPipeline, nullptr);
    if (mDisplayPipelineLayout) vkDestroyPipelineLayout(device, mDisplayPipelineLayout, nullptr);
    if (mDisplayDescriptorSetLayout) vkDestroyDescriptorSetLayout(device, mDisplayDescriptorSetLayout, nullptr);
    if (mDisplayShaderModule) vkDestroyShaderModule(device, mDisplayShaderModule, nullptr);

    delete mOutputImage;
    delete mOutputBuffer;
    delete mCascadeBufferA;
    delete mCascadeBufferB;
    delete mSDFBuffer;
    delete mParamBuffer;

    delete mComputeCommandBuffer;
    delete mComputeCommandPool;
}

void RadianceCascades::MouseMove(double xpos, double ypos) {
    if (mMouseLeftDown || mMouseRightDown) {
        mMousePrevX = mMouseX;
        mMousePrevY = mMouseY;
    } else {
        mMousePrevX = xpos;
        mMousePrevY = ypos;
    }
    mMouseX = xpos;
    mMouseY = ypos;
}

void RadianceCascades::MouseRoller(int z) {}
void RadianceCascades::KeyDown(GameKeyEnum moveDirection) {}

void RadianceCascades::GameLoop(unsigned int mCurrentFrame) {
    mTime += 1.0f / 60.0f;

    GLFWwindow* window = mWindow->getWindow();
    if (window) {
        bool leftDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
        bool rightDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
        bool anyDown = leftDown || rightDown;
        bool wasAnyDown = mMouseLeftDown || mMouseRightDown;

        if (anyDown && !wasAnyDown) {
            mMousePrevX = mMouseX;
            mMousePrevY = mMouseY;
        } else if (anyDown) {
            mMousePrevX = mMouseX;
            mMousePrevY = mMouseY;
        }

        mMouseLeftDown = leftDown;
        mMouseRightDown = rightDown;

        static bool cWasPressed = false;
        bool cIsPressed = glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS;
        if (cIsPressed && !cWasPressed) {
            float maxF = std::numeric_limits<float>::max();
            void* data = mSDFBuffer->getupdateBufferByMap();
            struct SDFEntry { float dist; float r, g, b; };
            SDFEntry* entries = static_cast<SDFEntry*>(data);
            int w = mSwapChain->getExtent().width;
            int h = mSwapChain->getExtent().height;
            for (int i = 0; i < w * h; ++i) {
                entries[i] = { maxF, 0.0f, 0.0f, 0.0f };
            }
            mSDFBuffer->endupdateBufferByMap();
        }
        cWasPressed = cIsPressed;

        static bool rWasPressed = false;
        if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS && !rWasPressed) {
            mFrameCount = 0;
        }
        rWasPressed = glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS;

        static bool escWasPressed = false;
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS && !escWasPressed) {
            Global::GameResourceUninstallBool = true;
        }
        escWasPressed = glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS;
    }

    dispatchCompute();
    mFrameCount++;
}

void RadianceCascades::GameUI() {
    int width = mSwapChain->getExtent().width;
    int height = mSwapChain->getExtent().height;

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(float(width), float(height)));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("##RCMain", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse);

    ImVec2 sz = ImGui::GetContentRegionAvail();
    ImGui::Image((ImTextureID)mImGuiDescriptorSet, sz, ImVec2(0, 0), ImVec2(1, 1));

    ImGui::End();
    ImGui::PopStyleVar();

    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.5f);
    ImGui::Begin("##RCControls", nullptr,
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoInputs);
    ImGui::Text("Left Click: Draw Emissive");
    ImGui::Text("Right Click: Draw Wall");
    ImGui::Text("C: Clear  R: Reset  Esc: Exit");
    ImGui::Text("Frame: %d", mFrameCount);
    ImGui::End();
}

void RadianceCascades::GameCommandBuffers(unsigned int Format_i) {}
void RadianceCascades::GameRecordCommandBuffers() {}
void RadianceCascades::GameStopInterfaceLoop(unsigned int mCurrentFrame) {
    mMouseLeftDown = false;
    mMouseRightDown = false;
}
void RadianceCascades::GameTCPLoop() {}

}
