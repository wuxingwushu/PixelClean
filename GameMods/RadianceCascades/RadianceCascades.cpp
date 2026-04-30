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
    createBuffers();
    createComputePipelines();
    createDescriptorSets();
    createImGuiResources();
}

RadianceCascades::~RadianceCascades() {
    cleanup();
}

// ============================================================================
// 计算级联缓冲区总条目数
// 每个级联n的条目数 = (c_sResX >> n) * (c_sResY >> n) * cn_dRes
// 其中 cn_dRes = (n==0) ? 1 : (c_dRes << 2*(n-1))
// ============================================================================
int RadianceCascades::calculateTotalCascadeEntries() const {
    int total = 0;
    for (int n = 0; n < N_CASCADES; ++n) {
        int sn_x = C_SRES_X >> n;
        int sn_y = C_SRES_Y >> n;
        int dn = (n == 0) ? 1 : (C_DRES << (2 * (n - 1)));
        total += sn_x * sn_y * dn;
    }
    return total;
}

// ============================================================================
// 创建所有GPU缓冲区和输出纹理
// ============================================================================
void RadianceCascades::createBuffers() {
    auto device = mDevice;
    int sdfWidth = mSwapChain->getExtent().width;
    int sdfHeight = mSwapChain->getExtent().height;
    int totalCascade = calculateTotalCascadeEntries();
    VkDeviceSize paramSize = 256;
    VkDeviceSize sdfSize = sdfWidth * sdfHeight * sizeof(float) * 4;
    VkDeviceSize cascadeSize = totalCascade * sizeof(float) * 4;
    VkDeviceSize outputSize = sdfWidth * sdfHeight * sizeof(uint32_t);

    // 参数缓冲区：CPU可见，每帧更新
    mParamBuffer = new VulKan::Buffer(device, paramSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    // SDF缓冲区：CPU可见，持久化存储
    // 数据格式：vec4(有符号距离, 自发光R, 自发光G, 自发光B)
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

    // 级联缓冲区A和B：乒乓切换
    // 每帧交替读写，避免数据竞争
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

    // 输出缓冲区：存储打包RGBA8像素，需要TRANSFER_SRC用于拷贝到纹理
    mOutputBuffer = new VulKan::Buffer(device, outputSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    // 输出纹理：使用UNORM格式（不做自动gamma转换），支持TRANSFER_DST和采样
    // 注意：不使用SRGB格式，因为shader已经做了gamma校正
    mOutputImage = new VulKan::Image(
        device,
        sdfWidth, sdfHeight,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_TYPE_2D,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_SAMPLE_COUNT_1_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT
    );

    // 初始化纹理布局：UNDEFINED → SHADER_READ_ONLY_OPTIMAL
    // 这样第一帧的buffer-to-image copy前可以正确转换到TRANSFER_DST
    {
        VkImageSubresourceRange range{};
        range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        range.baseMipLevel = 0;
        range.levelCount = 1;
        range.baseArrayLayer = 0;
        range.layerCount = 1;
        mOutputImage->setImageLayout(
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            range,
            mCommandPool);
    }

    mComputeCommandPool = new VulKan::CommandPool(device,
        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    mComputeCommandBuffer = new VulKan::CommandBuffer(device, mComputeCommandPool);
}

// ============================================================================
// 辅助函数：读取SPIR-V着色器文件
// ============================================================================
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

// ============================================================================
// 辅助函数：创建Vulkan着色器模块
// ============================================================================
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

// ============================================================================
// 辅助函数：创建完整的计算管线
// 流程：着色器模块 → 描述符集布局 → 管线布局 → 计算管线
// ============================================================================
static void createComputePipeline(
    VkDevice device,
    const std::string& shaderPath,
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

// ============================================================================
// 创建3条计算管线
// SDF管线：2个绑定（参数+SDF）
// Cascade管线：4个绑定（参数+SDF+级联读+级联写）
// Display管线：4个绑定（参数+SDF+级联+输出）
// ============================================================================
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

// ============================================================================
// 创建描述符集
// SDF×1 + Cascade×2(乒乓) + Display×2(乒乓) = 5个描述符集
// ============================================================================
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

    // ---- SDF描述符集 ----
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

    // ---- Cascade描述符集（2个，乒乓切换） ----
    {
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = mDescriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &mCascadeDescriptorSetLayout;

        VkDescriptorBufferInfo paramInfo = mParamBuffer->getBufferInfo();
        VkDescriptorBufferInfo sdfInfo = mSDFBuffer->getBufferInfo();

        // Set 0: read=A, write=B
        vkAllocateDescriptorSets(device, &allocInfo, &mCascadeDescriptorSets[0]);
        VkDescriptorBufferInfo readAInfo = mCascadeBufferA->getBufferInfo();
        VkDescriptorBufferInfo writeBInfo = mCascadeBufferB->getBufferInfo();
        VkWriteDescriptorSet writes0[4] = {};
        writes0[0] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, mCascadeDescriptorSets[0], 0, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &paramInfo, nullptr};
        writes0[1] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, mCascadeDescriptorSets[0], 1, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &sdfInfo, nullptr};
        writes0[2] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, mCascadeDescriptorSets[0], 2, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &readAInfo, nullptr};
        writes0[3] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, mCascadeDescriptorSets[0], 3, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &writeBInfo, nullptr};
        vkUpdateDescriptorSets(device, 4, writes0, 0, nullptr);

        // Set 1: read=B, write=A
        vkAllocateDescriptorSets(device, &allocInfo, &mCascadeDescriptorSets[1]);
        VkDescriptorBufferInfo readBInfo = mCascadeBufferB->getBufferInfo();
        VkDescriptorBufferInfo writeAInfo = mCascadeBufferA->getBufferInfo();
        VkWriteDescriptorSet writes1[4] = {};
        writes1[0] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, mCascadeDescriptorSets[1], 0, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &paramInfo, nullptr};
        writes1[1] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, mCascadeDescriptorSets[1], 1, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &sdfInfo, nullptr};
        writes1[2] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, mCascadeDescriptorSets[1], 2, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &readBInfo, nullptr};
        writes1[3] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, mCascadeDescriptorSets[1], 3, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &writeAInfo, nullptr};
        vkUpdateDescriptorSets(device, 4, writes1, 0, nullptr);
    }

    // ---- Display描述符集（2个，对应乒乓缓冲区） ----
    {
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = mDescriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &mDisplayDescriptorSetLayout;

        VkDescriptorBufferInfo paramInfo = mParamBuffer->getBufferInfo();
        VkDescriptorBufferInfo sdfInfo = mSDFBuffer->getBufferInfo();
        VkDescriptorBufferInfo outputInfo = mOutputBuffer->getBufferInfo();

        // Set 0: cascade=B（cascade set 0写入了B）
        vkAllocateDescriptorSets(device, &allocInfo, &mDisplayDescriptorSets[0]);
        VkDescriptorBufferInfo cascadeBInfo = mCascadeBufferB->getBufferInfo();
        VkWriteDescriptorSet writes0[4] = {};
        writes0[0] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, mDisplayDescriptorSets[0], 0, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &paramInfo, nullptr};
        writes0[1] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, mDisplayDescriptorSets[0], 1, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &sdfInfo, nullptr};
        writes0[2] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, mDisplayDescriptorSets[0], 2, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &cascadeBInfo, nullptr};
        writes0[3] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, mDisplayDescriptorSets[0], 3, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &outputInfo, nullptr};
        vkUpdateDescriptorSets(device, 4, writes0, 0, nullptr);

        // Set 1: cascade=A（cascade set 1写入了A）
        vkAllocateDescriptorSets(device, &allocInfo, &mDisplayDescriptorSets[1]);
        VkDescriptorBufferInfo cascadeAInfo = mCascadeBufferA->getBufferInfo();
        VkWriteDescriptorSet writes1[4] = {};
        writes1[0] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, mDisplayDescriptorSets[1], 0, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &paramInfo, nullptr};
        writes1[1] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, mDisplayDescriptorSets[1], 1, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &sdfInfo, nullptr};
        writes1[2] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, mDisplayDescriptorSets[1], 2, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &cascadeAInfo, nullptr};
        writes1[3] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, mDisplayDescriptorSets[1], 3, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &outputInfo, nullptr};
        vkUpdateDescriptorSets(device, 4, writes1, 0, nullptr);
    }
}

// ============================================================================
// 创建ImGui纹理显示资源
// 需要一个独立的描述符集，将输出纹理绑定为combined image sampler
// ============================================================================
void RadianceCascades::createImGuiResources() {
    VkDevice device = mDevice->getDevice();

    // 创建ImGui专用采样器
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

    // 描述符集布局：1个combined image sampler
    VkDescriptorSetLayoutBinding binding{};
    binding.binding = 0;
    binding.descriptorCount = 1;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    binding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &binding;
    vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &mImGuiDescriptorSetLayout);

    // 描述符池
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = 1;
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets = 1;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    vkCreateDescriptorPool(device, &poolInfo, nullptr, &mImGuiDescriptorPool);

    // 分配描述符集
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = mImGuiDescriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &mImGuiDescriptorSetLayout;
    vkAllocateDescriptorSets(device, &allocInfo, &mImGuiDescriptorSet);

    // 绑定输出纹理到描述符集
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

// ============================================================================
// 每帧调度计算着色器
// 流程：更新参数 → SDF更新 → 级联计算 → 显示输出 → 拷贝到纹理
// ============================================================================
void RadianceCascades::dispatchCompute() {
    int sdfWidth = mSwapChain->getExtent().width;
    int sdfHeight = mSwapChain->getExtent().height;
    int totalCascade = calculateTotalCascadeEntries();
    int toggleIdx = mCascadeToggle ? 1 : 0;

    // ---- 更新参数缓冲区 ----
    struct Params {
        float time;
        float timeDelta;
        int sdfWidth;
        int sdfHeight;
        float mouseX;
        float mouseY;
        float mousePrevX;
        float mousePrevY;
        int mouseLeftDown;
        int frameCount;
        int emissiveMode;
        float brushRadius;
        int c_sResX;
        int c_sResY;
        int c_dRes;
        int nCascades;
        float c_intervalLength;
        float c_smoothDistScale;
        int totalCascadeEntries;
        int pad0;
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
    p.mouseLeftDown = mMouseLeftDown ? 1 : 0;
    p.frameCount = mFrameCount;
    p.emissiveMode = mEmissiveMode ? 1 : 0;
    p.brushRadius = BRUSH_RADIUS;
    p.c_sResX = C_SRES_X;
    p.c_sResY = C_SRES_Y;
    p.c_dRes = C_DRES;
    p.nCascades = N_CASCADES;
    p.c_intervalLength = C_INTERVAL_LENGTH;
    p.c_smoothDistScale = C_SMOOTH_DIST_SCALE;
    p.totalCascadeEntries = totalCascade;
    mParamBuffer->updateBufferByMap(&p, sizeof(Params));

    // ---- 录制命令缓冲区 ----
    mComputeCommandBuffer->begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    VkCommandBuffer cmdBuf = mComputeCommandBuffer->getCommandBuffer();

    // 存储缓冲区屏障（用于dispatch之间同步读写）
    auto storageBarrier = [](VkBuffer buffer, VkDeviceSize size) {
        VkBufferMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.buffer = buffer;
        barrier.offset = 0;
        barrier.size = size;
        return barrier;
    };

    // ---- Pass 1: SDF更新 ----
    vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, mSDFPipeline);
    vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE,
        mSDFPipelineLayout, 0, 1, &mSDFDescriptorSet, 0, nullptr);
    uint32_t sdfGroups = (sdfWidth * sdfHeight + 63) / 64;
    vkCmdDispatch(cmdBuf, sdfGroups, 1, 1);

    // SDF写入 → Cascade读取同步
    VkBufferMemoryBarrier sdfBarrier = storageBarrier(
        mSDFBuffer->getBuffer(), mSDFBuffer->getBufferInfo().range);
    vkCmdPipelineBarrier(cmdBuf,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0, 0, nullptr, 1, &sdfBarrier, 0, nullptr);

    // ---- Pass 2: 级联计算 ----
    vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, mCascadePipeline);
    vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE,
        mCascadePipelineLayout, 0, 1, &mCascadeDescriptorSets[toggleIdx], 0, nullptr);
    uint32_t cascadeGroups = (totalCascade + 63) / 64;
    vkCmdDispatch(cmdBuf, cascadeGroups, 1, 1);

    // Cascade写入 → Display读取同步
    VkBufferMemoryBarrier cascadeBarrier = storageBarrier(
        mCascadeToggle ? mCascadeBufferA->getBuffer() : mCascadeBufferB->getBuffer(),
        mCascadeBufferA->getBufferInfo().range);
    vkCmdPipelineBarrier(cmdBuf,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0, 0, nullptr, 1, &cascadeBarrier, 0, nullptr);

    // ---- Pass 3: 显示输出 ----
    vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, mDisplayPipeline);
    vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE,
        mDisplayPipelineLayout, 0, 1, &mDisplayDescriptorSets[toggleIdx], 0, nullptr);
    vkCmdDispatch(cmdBuf, sdfGroups, 1, 1);

    // Output写入 → Transfer读取同步
    VkBufferMemoryBarrier outputBarrier{};
    outputBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    outputBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    outputBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    outputBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    outputBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    outputBarrier.buffer = mOutputBuffer->getBuffer();
    outputBarrier.offset = 0;
    outputBarrier.size = mOutputBuffer->getBufferInfo().range;
    vkCmdPipelineBarrier(cmdBuf,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, nullptr, 1, &outputBarrier, 0, nullptr);

    // ---- 拷贝输出缓冲区到纹理 ----
    // Step 1: 转换图像布局 SHADER_READ_ONLY → TRANSFER_DST
    VkImageSubresourceRange range{};
    range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    range.baseMipLevel = 0;
    range.levelCount = 1;
    range.baseArrayLayer = 0;
    range.layerCount = 1;

    VkImageMemoryBarrier toTransferDst{};
    toTransferDst.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    toTransferDst.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    toTransferDst.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    toTransferDst.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    toTransferDst.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toTransferDst.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransferDst.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransferDst.image = mOutputImage->getImage();
    toTransferDst.subresourceRange = range;
    vkCmdPipelineBarrier(cmdBuf,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &toTransferDst);

    // Step 2: 执行buffer → image拷贝
    mComputeCommandBuffer->copyBufferToImage(
        mOutputBuffer->getBuffer(),
        mOutputImage->getImage(),
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        sdfWidth, sdfHeight);

    // Step 3: 转换图像布局 TRANSFER_DST → SHADER_READ_ONLY
    VkImageMemoryBarrier toShaderReadOnly{};
    toShaderReadOnly.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    toShaderReadOnly.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    toShaderReadOnly.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    toShaderReadOnly.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toShaderReadOnly.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    toShaderReadOnly.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toShaderReadOnly.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toShaderReadOnly.image = mOutputImage->getImage();
    toShaderReadOnly.subresourceRange = range;
    vkCmdPipelineBarrier(cmdBuf,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &toShaderReadOnly);

    mComputeCommandBuffer->end();
    mComputeCommandBuffer->submitSync(mDevice->getGraphicQueue(), VK_NULL_HANDLE);

    // 更新Image对象的布局记录
    // 注意：Image::mLayout是私有成员，无法直接设置
    // 但由于我们手动管理了布局转换，下一帧的屏障会使用正确的oldLayout

    mCascadeToggle = !mCascadeToggle;
}

// ============================================================================
// 清理所有GPU资源
// ============================================================================
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

// ============================================================================
// 鼠标移动回调（由Window类调用）
// ============================================================================
void RadianceCascades::MouseMove(double xpos, double ypos) {
    if (mMouseLeftDown) {
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

// ============================================================================
// 键盘回调（由Window类调用，仅处理现有GameKeyEnum值）
// 额外的键位通过GLFW在GameLoop中直接检测
// ============================================================================
void RadianceCascades::KeyDown(GameKeyEnum moveDirection) {
}

// ============================================================================
// 游戏主循环：每帧调用
// 使用GLFW直接检测键盘和鼠标输入
// ============================================================================
void RadianceCascades::GameLoop(unsigned int mCurrentFrame) {
    mTime += 1.0f / 60.0f;

    // ---- 通过GLFW直接检测输入 ----
    GLFWwindow* window = mWindow->getWindow();
    if (window) {
        // 鼠标左键状态
        bool leftDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
        if (leftDown && !mMouseLeftDown) {
            mMousePrevX = mMouseX;
            mMousePrevY = mMouseY;
        }
        mMouseLeftDown = leftDown;

        // 键盘检测（使用按键切换，避免每帧触发）
        static bool spaceWasPressed = false;
        bool spaceIsPressed = glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS;
        if (spaceIsPressed && !spaceWasPressed) {
            mEmissiveMode = !mEmissiveMode;
        }
        spaceWasPressed = spaceIsPressed;

        static bool cWasPressed = false;
        bool cIsPressed = glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS;
        if (cIsPressed && !cWasPressed) {
            mNeedReset = true;
        }
        cWasPressed = cIsPressed;

        static bool rWasPressed = false;
        bool rIsPressed = glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS;
        if (rIsPressed && !rWasPressed) {
            mFrameCount = 0;
            mNeedReset = true;
        }
        rWasPressed = rIsPressed;
    }

    // ---- 清除画布 ----
    if (mNeedReset) {
        float maxF = std::numeric_limits<float>::max();
        void* data = mSDFBuffer->getupdateBufferByMap();
        struct SDFEntry { float dist; float r, g, b; };
        SDFEntry* entries = static_cast<SDFEntry*>(data);
        int sdfWidth = mSwapChain->getExtent().width;
        int sdfHeight = mSwapChain->getExtent().height;
        for (int i = 0; i < sdfWidth * sdfHeight; ++i) {
            entries[i] = { maxF, 0.0f, 0.0f, 0.0f };
        }
        mSDFBuffer->endupdateBufferByMap();
        if (mFrameCount > 0) mFrameCount = 0;
        mNeedReset = false;
    }

    // ---- 调度计算着色器 ----
    dispatchCompute();

    mFrameCount++;
}

// ============================================================================
// ImGui界面：显示输出纹理和操作说明
// ============================================================================
void RadianceCascades::GameUI() {
    int width = mSwapChain->getExtent().width;
    int height = mSwapChain->getExtent().height;

    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(float(width), float(height)), ImGuiCond_FirstUseEver);
    ImGui::Begin("Radiance Cascades 2D GI", nullptr,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    ImVec2 windowSize = ImGui::GetContentRegionAvail();
    float aspect = float(width) / float(height);
    float windowAspect = windowSize.x / windowSize.y;
    ImVec2 imageSize;
    if (windowAspect > aspect) {
        imageSize.y = windowSize.y;
        imageSize.x = windowSize.y * aspect;
    } else {
        imageSize.x = windowSize.x;
        imageSize.y = windowSize.x / aspect;
    }

    ImGui::SetCursorPos(ImVec2(
        (ImGui::GetWindowWidth() - imageSize.x) * 0.5f,
        (ImGui::GetWindowHeight() - imageSize.y) * 0.5f
    ));
    ImGui::Image((ImTextureID)mImGuiDescriptorSet, imageSize,
        ImVec2(0, 0), ImVec2(1, 1));

    ImGui::End();

    ImGui::Begin("RC Controls");
    ImGui::Text("Mouse Drag: Draw");
    ImGui::Text("Space: Toggle Emissive (%s)", mEmissiveMode ? "Wall" : "Light");
    ImGui::Text("C: Clear Canvas");
    ImGui::Text("R: Reset Scene");
    ImGui::Text("Frame: %d", mFrameCount);
    ImGui::Text("Cascade Entries: %d", calculateTotalCascadeEntries());
    ImGui::End();
}

void RadianceCascades::GameCommandBuffers(unsigned int Format_i) {}
void RadianceCascades::GameRecordCommandBuffers() {}
void RadianceCascades::GameStopInterfaceLoop(unsigned int mCurrentFrame) {
    mMouseLeftDown = false;
}
void RadianceCascades::GameTCPLoop() {}

}
