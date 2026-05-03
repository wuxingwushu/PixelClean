#include "RadianceCascades.h"
#include <fstream>
#include <vector>
#include <chrono>
#include <cmath>
#include <algorithm>

namespace GAME {

// ============================================================================
// SPIR‑V 文件路径（相对于工作目录：build/ 执行时 ../shaders/ 就是项目根目录的 shaders/）
// ============================================================================
static const char* kSDFSpvPath     = "./shaders/RC_SDF.spv";
static const char* kCascadeSpvPath = "./shaders/RC_Cascade.spv";
static const char* kDisplaySpvPath = "./shaders/RC_Display.spv";

// ============================================================================
// 工具：读取二进制文件
// ============================================================================
static std::vector<char> readFile(const std::string& path) {
    std::ifstream f(path, std::ios::ate | std::ios::binary);
    if (!f.is_open()) {
        throw std::runtime_error("Cannot open: " + path);
    }
    size_t sz = (size_t)f.tellg();
    std::vector<char> buf(sz);
    f.seekg(0);
    f.read(buf.data(), sz);
    return buf;
}

// ============================================================================
// 工具：从 SPIR‑V 文件创建 VkShaderModule
// ============================================================================
static VkShaderModule createShaderModule(VkDevice device, const std::string& path) {
    auto code = readFile(path);
    VkShaderModuleCreateInfo ci{};
    ci.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    ci.codeSize = code.size();
    ci.pCode    = reinterpret_cast<const uint32_t*>(code.data());
    VkShaderModule mod;
    vkCreateShaderModule(device, &ci, nullptr, &mod);
    return mod;
}

// ============================================================================
// 构造 / 析构
// ============================================================================
RadianceCascades::RadianceCascades(Configuration wCfg)
    : Configuration(wCfg) {
    mTime = 0.0f;
    mMouseAX = 0; mMouseAY = 0; mMouseAZ = 0;
    mMouseBX = 0; mMouseBY = 0; mMouseBZ = 0;
    mMouseCX = 0; mMouseCY = 0; mMouseCZ = 0;
}

RadianceCascades::~RadianceCascades() {
    cleanup();
}

// ============================================================================
// 输入
// ============================================================================
void RadianceCascades::MouseMove(double x, double y) {
    mMousePrevX = mMouseX;
    mMousePrevY = mMouseY;
    mMouseX = x;
    mMouseY = y;
}

void RadianceCascades::MouseRoller(int) {}

void RadianceCascades::KeyDown(GameKeyEnum k) {
    if (k == GameKeyEnum::ESC) {
        if (Global::ConsoleBool)
        {
            Global::ConsoleBool = false;
            InterFace->ConsoleFocusHere = true;
        }
        else
        {
            InterFace->SetInterFaceBool();
        }
    }
    if (k == GameKeyEnum::Key_1) {
        mKeyToggled1 = !mKeyToggled1;
        return;
    }
    if (k == GameKeyEnum::SPACE) {
        mKeyToggledSpace = !mKeyToggledSpace;
        return;
    }
}

// ============================================================================
// 级联条目计数
// ============================================================================
int RadianceCascades::calculateTotalCascadeEntries() const {
    int total = 0;
    for (int lv = 0; lv < N_CASCADES; ++lv) {
        total += calculateCascadeLevelEntries(lv);
    }
    return total;
}

int RadianceCascades::calculateCascadeLevelEntries(int lv) const {
    int sx = mCascadeResX >> lv;
    int sy = mCascadeResY >> lv;
    int dn = (lv == 0) ? 1 : (C_DRES << (2 * (lv - 1)));
    return sx * sy * dn;
}

// ============================================================================
// 创建全部 GPU 缓冲区
// ============================================================================
void RadianceCascades::createBuffers() {
    VkExtent2D ext = mSwapChain->getExtent();
    int sdfW = (int)ext.width;
    int sdfH = (int)ext.height;
    mCascadeResX = sdfW / 4;
    mCascadeResY = sdfH / 4;
    int cascadeTotal = calculateTotalCascadeEntries();

    auto dev = mDevice;

    // Params（CPU 可见，每帧 map 更新）
    mParamBuffer = new VulKan::Buffer(
        dev, sizeof(GPUParams),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    // SDF（GPU only）
    mSDFBuffer = new VulKan::Buffer(
        dev, (VkDeviceSize)sdfW * sdfH * sizeof(float) * 4,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    // Cascade 乒乓
    VkDeviceSize csize = (VkDeviceSize)cascadeTotal * sizeof(float) * 4;
    mCascadeBufferA = new VulKan::Buffer(
        dev, csize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    mCascadeBufferB = new VulKan::Buffer(
        dev, csize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    // Output（uint32 RGBA8，同时需要被 transfer 读出）
    mOutputBuffer = new VulKan::Buffer(
        dev, (VkDeviceSize)sdfW * sdfH * sizeof(uint32_t),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    // Output image（供 ImGui 显示的 R8G8B8A8_UNORM 纹理）
    mOutputImage = new VulKan::Image(
        dev,
        sdfW, sdfH,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_TYPE_2D,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_SAMPLE_COUNT_1_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT);
}

// ============================================================================
// 创建 compute pipelines
// ============================================================================
void RadianceCascades::createComputePipelines() {
    VkDevice dev = mDevice->getDevice();

    // ---- Shader modules ----
    mSDFShaderModule     = createShaderModule(dev, kSDFSpvPath);
    mCascadeShaderModule = createShaderModule(dev, kCascadeSpvPath);
    mDisplayShaderModule = createShaderModule(dev, kDisplaySpvPath);

    // ---- Descriptor set layouts ----
    auto makeLayout = [&](int bindCount) {
        std::vector<VkDescriptorSetLayoutBinding> b(bindCount);
        for (int i = 0; i < bindCount; ++i) {
            b[i].binding         = (uint32_t)i;
            b[i].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            b[i].descriptorCount = 1;
            b[i].stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT;
        }
        VkDescriptorSetLayoutCreateInfo ci{};
        ci.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        ci.bindingCount = (uint32_t)bindCount;
        ci.pBindings    = b.data();
        VkDescriptorSetLayout lay;
        vkCreateDescriptorSetLayout(dev, &ci, nullptr, &lay);
        return lay;
    };

    mSDFDescriptorSetLayout     = makeLayout(2);  // binding 0=Params  1=SDF
    mCascadeDescriptorSetLayout = makeLayout(4);  // 0=Params 1=SDF 2=CascadeRead 3=CascadeWrite
    mDisplayDescriptorSetLayout = makeLayout(4);  // 0=Params 1=SDF 2=CascadeData 3=Output

    // ---- Pipeline layouts ----
    // SDF 无 push constant
    {
        VkPipelineLayoutCreateInfo ci{};
        ci.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        ci.setLayoutCount = 1;
        ci.pSetLayouts    = &mSDFDescriptorSetLayout;
        vkCreatePipelineLayout(dev, &ci, nullptr, &mSDFPipelineLayout);
    }
    // Cascade 有 push constant（cascadeLevel）
    {
        VkPushConstantRange pr{};
        pr.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        pr.offset = 0;
        pr.size   = sizeof(int32_t);

        VkPipelineLayoutCreateInfo ci{};
        ci.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        ci.setLayoutCount         = 1;
        ci.pSetLayouts            = &mCascadeDescriptorSetLayout;
        ci.pushConstantRangeCount = 1;
        ci.pPushConstantRanges    = &pr;
        vkCreatePipelineLayout(dev, &ci, nullptr, &mCascadePipelineLayout);
    }
    // Display 无 push constant
    {
        VkPipelineLayoutCreateInfo ci{};
        ci.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        ci.setLayoutCount = 1;
        ci.pSetLayouts    = &mDisplayDescriptorSetLayout;
        vkCreatePipelineLayout(dev, &ci, nullptr, &mDisplayPipelineLayout);
    }

    // ---- Compute pipelines ----
    auto makeComputePipeline = [&](VkShaderModule mod, VkPipelineLayout lay) {
        VkComputePipelineCreateInfo ci{};
        ci.sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        ci.stage.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        ci.stage.stage  = VK_SHADER_STAGE_COMPUTE_BIT;
        ci.stage.module = mod;
        ci.stage.pName  = "main";
        ci.layout       = lay;
        VkPipeline pipe;
        vkCreateComputePipelines(dev, VK_NULL_HANDLE, 1, &ci, nullptr, &pipe);
        return pipe;
    };

    mSDFPipeline     = makeComputePipeline(mSDFShaderModule,     mSDFPipelineLayout);
    mCascadePipeline = makeComputePipeline(mCascadeShaderModule, mCascadePipelineLayout);
    mDisplayPipeline = makeComputePipeline(mDisplayShaderModule, mDisplayPipelineLayout);
}

// ============================================================================
// 描述符池 + 描述符集
// ============================================================================
void RadianceCascades::createDescriptorSets() {
    VkDevice dev = mDevice->getDevice();

    // ---- 池 ----
    VkDescriptorPoolSize poolSize{};
    poolSize.type            = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSize.descriptorCount = 20;
    VkDescriptorPoolCreateInfo poolCI{};
    poolCI.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolCI.maxSets       = 10;
    poolCI.poolSizeCount = 1;
    poolCI.pPoolSizes    = &poolSize;
    vkCreateDescriptorPool(dev, &poolCI, nullptr, &mDescriptorPool);

    // ---- 更新工具 ----
    auto writeBuf = [&](VkDescriptorSet set, uint32_t bind, VkBuffer buf, VkDeviceSize sz) {
        VkDescriptorBufferInfo bi{};
        bi.buffer = buf;
        bi.offset = 0;
        bi.range  = sz;
        VkWriteDescriptorSet w{};
        w.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w.dstSet          = set;
        w.dstBinding      = bind;
        w.descriptorCount = 1;
        w.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        w.pBufferInfo     = &bi;
        vkUpdateDescriptorSets(dev, 1, &w, 0, nullptr);
    };

    auto allocSet = [&](VkDescriptorSetLayout lay) {
        VkDescriptorSetAllocateInfo ai{};
        ai.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        ai.descriptorPool     = mDescriptorPool;
        ai.descriptorSetCount = 1;
        ai.pSetLayouts        = &lay;
        VkDescriptorSet s;
        vkAllocateDescriptorSets(dev, &ai, &s);
        return s;
    };

    VkExtent2D ext = mSwapChain->getExtent();
    VkDeviceSize sdfSz  = (VkDeviceSize)ext.width * ext.height * sizeof(float) * 4;
    VkDeviceSize cascSz = (VkDeviceSize)calculateTotalCascadeEntries() * sizeof(float) * 4;
    VkDeviceSize outSz  = (VkDeviceSize)ext.width * ext.height * sizeof(uint32_t);

    // ---- SDF descriptor set ----
    mSDFDescriptorSet = allocSet(mSDFDescriptorSetLayout);
    writeBuf(mSDFDescriptorSet, 0, mParamBuffer->getBuffer(), sizeof(GPUParams));
    writeBuf(mSDFDescriptorSet, 1, mSDFBuffer->getBuffer(),    sdfSz);

    // ---- Cascade descriptor sets（乒乓） ----
    VkBuffer bufA = mCascadeBufferA->getBuffer();
    VkBuffer bufB = mCascadeBufferB->getBuffer();
    VkBuffer sdfB = mSDFBuffer->getBuffer();

    // set 0: read=A  write=B
    mCascadeDescriptorSets[0] = allocSet(mCascadeDescriptorSetLayout);
    writeBuf(mCascadeDescriptorSets[0], 0, mParamBuffer->getBuffer(), sizeof(GPUParams));
    writeBuf(mCascadeDescriptorSets[0], 1, sdfB,  sdfSz);
    writeBuf(mCascadeDescriptorSets[0], 2, bufA, cascSz);
    writeBuf(mCascadeDescriptorSets[0], 3, bufB, cascSz);

    // set 1: read=B  write=A
    mCascadeDescriptorSets[1] = allocSet(mCascadeDescriptorSetLayout);
    writeBuf(mCascadeDescriptorSets[1], 0, mParamBuffer->getBuffer(), sizeof(GPUParams));
    writeBuf(mCascadeDescriptorSets[1], 1, sdfB,  sdfSz);
    writeBuf(mCascadeDescriptorSets[1], 2, bufB, cascSz);
    writeBuf(mCascadeDescriptorSets[1], 3, bufA, cascSz);

    // ---- Display descriptor sets ----
    // set 0: reads cascade buffer B
    mDisplayDescriptorSets[0] = allocSet(mDisplayDescriptorSetLayout);
    writeBuf(mDisplayDescriptorSets[0], 0, mParamBuffer->getBuffer(), sizeof(GPUParams));
    writeBuf(mDisplayDescriptorSets[0], 1, sdfB,  sdfSz);
    writeBuf(mDisplayDescriptorSets[0], 2, bufB, cascSz);
    writeBuf(mDisplayDescriptorSets[0], 3, mOutputBuffer->getBuffer(), outSz);

    // set 1: reads cascade buffer A
    mDisplayDescriptorSets[1] = allocSet(mDisplayDescriptorSetLayout);
    writeBuf(mDisplayDescriptorSets[1], 0, mParamBuffer->getBuffer(), sizeof(GPUParams));
    writeBuf(mDisplayDescriptorSets[1], 1, sdfB,  sdfSz);
    writeBuf(mDisplayDescriptorSets[1], 2, bufA, cascSz);
    writeBuf(mDisplayDescriptorSets[1], 3, mOutputBuffer->getBuffer(), outSz);
}

// ============================================================================
// ImGui 显示输出纹理所需资源
// ============================================================================
void RadianceCascades::createImGuiResources() {
    VkDevice dev = mDevice->getDevice();

    // Sampler
    VkSamplerCreateInfo sci{};
    sci.sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sci.magFilter    = VK_FILTER_LINEAR;
    sci.minFilter    = VK_FILTER_LINEAR;
    sci.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sci.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    vkCreateSampler(dev, &sci, nullptr, &mImGuiSampler);

    // Descriptor set layout (combined image sampler)
    VkDescriptorSetLayoutBinding lb{};
    lb.binding         = 0;
    lb.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    lb.descriptorCount = 1;
    lb.stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;
    VkDescriptorSetLayoutCreateInfo lici{};
    lici.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    lici.bindingCount = 1;
    lici.pBindings    = &lb;
    vkCreateDescriptorSetLayout(dev, &lici, nullptr, &mImGuiDescriptorSetLayout);

    // Pool
    VkDescriptorPoolSize psz{};
    psz.type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    psz.descriptorCount = 1;
    VkDescriptorPoolCreateInfo pci{};
    pci.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pci.maxSets       = 1;
    pci.poolSizeCount = 1;
    pci.pPoolSizes    = &psz;
    vkCreateDescriptorPool(dev, &pci, nullptr, &mImGuiDescriptorPool);

    // Set
    VkDescriptorSetAllocateInfo ai{};
    ai.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    ai.descriptorPool     = mImGuiDescriptorPool;
    ai.descriptorSetCount = 1;
    ai.pSetLayouts        = &mImGuiDescriptorSetLayout;
    vkAllocateDescriptorSets(dev, &ai, &mImGuiDescriptorSet);

    // Update
    VkDescriptorImageInfo ii{};
    ii.imageView   = mOutputImage->getImageView();
    ii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    ii.sampler     = mImGuiSampler;
    VkWriteDescriptorSet w{};
    w.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    w.dstSet          = mImGuiDescriptorSet;
    w.dstBinding      = 0;
    w.descriptorCount = 1;
    w.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    w.pImageInfo      = &ii;
    vkUpdateDescriptorSets(dev, 1, &w, 0, nullptr);

    // --- Output image layout 初始化 ---
    {
        mComputeCommandPool = new VulKan::CommandPool(mDevice);
        mComputeCommandBuffer = new VulKan::CommandBuffer(mDevice, mComputeCommandPool);
        mComputeCommandBuffer->begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

        VkImageMemoryBarrier bar{};
        bar.sType         = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        bar.oldLayout     = VK_IMAGE_LAYOUT_UNDEFINED;
        bar.newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        bar.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        bar.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        bar.image         = mOutputImage->getImage();
        bar.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        bar.srcAccessMask = 0;
        bar.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        mComputeCommandBuffer->transferImageLayout(bar,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

        mComputeCommandBuffer->end();
        mComputeCommandBuffer->submitSync(mDevice->getGraphicQueue());
    }
}

// ============================================================================
// 初始化：录制 CommandBuffer
// ============================================================================
void RadianceCascades::GameRecordCommandBuffers() {
    createBuffers();
    createComputePipelines();
    createDescriptorSets();
    createImGuiResources();
}

// ============================================================================
// 每帧逻辑：更新参数 + GPU dispatch
// ============================================================================
void RadianceCascades::GameLoop(unsigned int /*currentFrame*/) {
    static auto t0 = std::chrono::high_resolution_clock::now();
    auto t1 = std::chrono::high_resolution_clock::now();
    float dt = std::chrono::duration<float>(t1 - t0).count();
    t0 = t1;
    mTime += dt;

    VkExtent2D ext = mSwapChain->getExtent();

    if (!mInitialized) {
        GameRecordCommandBuffers();
        mFrameCount = 0;
        mInitialized = true;
    } else {
        ++mFrameCount;
    }

    int leftState  = glfwGetMouseButton(mWindow->getWindow(), GLFW_MOUSE_BUTTON_LEFT);
    int rightState = glfwGetMouseButton(mWindow->getWindow(), GLFW_MOUSE_BUTTON_RIGHT);
    bool leftDown  = (leftState == GLFW_PRESS);
    bool rightDown = (rightState == GLFW_PRESS);
    bool realMouseDown = leftDown || rightDown;

    // ---- Shadertoy Buffer A exact: 鼠标平滑 ----
    const float SMOOTH_RADIUS = float(ext.height) * 0.015f;
    const float SMOOTH_FRICTION = 0.05f;

    float rawX = (float)mMouseX;
    float rawY = (float)mMouseY;
    float rawZ = realMouseDown ? 1.0f : 0.0f;

    if (mFrameCount == 0) {
        mMouseCX = rawX;
        mMouseCY = rawY;
        mMouseCZ = rawZ;
        mMouseAX = 0.0f;
        mMouseAY = 0.0f;
        mMouseAZ = 0.0f;
        mMouseBX = 0.0f;
        mMouseBY = 0.0f;
        mMouseBZ = 0.0f;
    } else {
        float dist = std::sqrt((mMouseBX - rawX) * (mMouseBX - rawX) +
                               (mMouseBY - rawY) * (mMouseBY - rawY));

        if (mMouseBZ > 0.0f && dist > 0.0f) {
            float ndx = (rawX - mMouseBX) / dist;
            float ndy = (rawY - mMouseBY) / dist;
            float len = std::max(dist - SMOOTH_RADIUS, 0.0f);
            float ease = 1.0f - std::pow(SMOOTH_FRICTION, dt * 10.0f);
            mMouseCX = mMouseBX + ndx * len * ease;
            mMouseCY = mMouseBY + ndy * len * ease;
            mMouseCZ = rawZ;
        } else {
            mMouseCX = rawX;
            mMouseCY = rawY;
            mMouseCZ = rawZ;
        }

        mMouseAX = mMouseBX;
        mMouseAY = mMouseBY;
        mMouseAZ = mMouseBZ;
        mMouseBX = mMouseCX;
        mMouseBY = mMouseCY;
        mMouseBZ = mMouseCZ;
    }

    if (!mMouseLeftDown && leftDown) {
        mMouseClickStartX = rawX;
        mMouseClickStartY = rawY;
    }

    mMouseLeftDown  = leftDown;
    mMouseRightDown = rightDown;

    if (glfwGetKey(mWindow->getWindow(), GLFW_KEY_C) == GLFW_PRESS && mPrevCKey == 0) {
        mNeedClear = true;
    }
    mPrevCKey = glfwGetKey(mWindow->getWindow(), GLFW_KEY_C);

    GPUParams p{};
    p.time              = mTime;
    p.timeDelta         = dt;
    p.sdfWidth          = (int)ext.width;
    p.sdfHeight         = (int)ext.height;
    p.mouseX            = mMouseCX;
    p.mouseY            = mMouseCY;
    p.mousePrevX        = (float)mMousePrevX;
    p.mousePrevY        = (float)mMousePrevY;
    p.mouseLeftDown     = (mMouseCZ > 0.0f) ? 1 : 0;
    p.mouseRightDown    = mMouseRightDown ? 1 : 0;
    p.mouseRawX         = rawX;
    p.mouseRawY         = rawY;
    p.frameCount        = mFrameCount;
    p.emissiveMode      = 0;
    p.brushRadius       = BRUSH_RADIUS;
    p.c_sResX           = mCascadeResX;
    p.c_sResY           = mCascadeResY;
    p.c_dRes            = C_DRES;
    p.nCascades         = N_CASCADES;
    float screenLen = std::sqrt((float)(ext.width * ext.width + ext.height * ext.height));
    p.c_intervalLength  = screenLen * 4.0f / float((1 << (2 * N_CASCADES)) - 1);
    p.c_smoothDistScale = 0.0f;
    p.totalCascadeEntries = calculateTotalCascadeEntries();
    p.cascadeLevel      = 0;
    p.mouseSmoothAX     = mMouseAX;
    p.mouseSmoothAY     = mMouseAY;
    p.mouseSmoothBX     = mMouseBX;
    p.mouseSmoothBY     = mMouseBY;
    p.mouseSmoothADown  = (mMouseAZ > 0.0f) ? 1 : 0;
    p.mouseSmoothBDown  = (mMouseBZ > 0.0f) ? 1 : 0;
    p.mouseMagic        = (mMouseCZ == MAGIC) ? 1 : 0;
    p.keyToggledSpace   = mKeyToggledSpace ? 1 : 0;
    p.keyToggled1       = mKeyToggled1 ? 1 : 0;
    p.mouseClickStartX  = mMouseClickStartX;
    p.mouseClickStartY  = mMouseClickStartY;
    p.clearScreen       = mNeedClear ? 1 : 0;

    mParamBuffer->updateBufferByMap(&p, sizeof(p));

    dispatchCompute();
    mNeedClear = false;
}

// ============================================================================
// 录制+提交全部 compute dispatch
// ============================================================================
void RadianceCascades::dispatchCompute() {
    VkExtent2D ext = mSwapChain->getExtent();
    int sdfW = (int)ext.width;
    int sdfH = (int)ext.height;
    uint32_t sdfGroups = (uint32_t)((sdfW * sdfH + 63) / 64);

    VkCommandBuffer cmd = mComputeCommandBuffer->getCommandBuffer();
    mComputeCommandBuffer->begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    // === SDF ===
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, mSDFPipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, mSDFPipelineLayout,
        0, 1, &mSDFDescriptorSet, 0, nullptr);
    vkCmdDispatch(cmd, sdfGroups, 1, 1);

    // 屏障：SDF write → SDF read
    VkMemoryBarrier memBar{};
    memBar.sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    memBar.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    memBar.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0, 1, &memBar, 0, nullptr, 0, nullptr);

    // === Cascade（自上而下） ===
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, mCascadePipeline);
    for (int n = N_CASCADES - 1; n >= 0; --n) {
        int setIdx = (N_CASCADES - 1 - n) % 2; // n=3→0, n=2→1, n=1→0, n=0→1
        int entries = calculateCascadeLevelEntries(n);
        uint32_t groups = (uint32_t)((entries + 63) / 64);

        vkCmdPushConstants(cmd, mCascadePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT,
            0, sizeof(int32_t), &n);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, mCascadePipelineLayout,
            0, 1, &mCascadeDescriptorSets[setIdx], 0, nullptr);
        vkCmdDispatch(cmd, groups, 1, 1);

        // 屏障：cascade write → cascade read（下一级联需要上一级联的结果）
        vkCmdPipelineBarrier(cmd,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            0, 1, &memBar, 0, nullptr, 0, nullptr);
    }

    // === Display ===
    int dispSet = (N_CASCADES - 1) % 2; // cascade 0 final output: n=3→B, n=2→A, n=1→B, n=0→A → setIdx=(3)%2=1 → reads A
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, mDisplayPipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, mDisplayPipelineLayout,
        0, 1, &mDisplayDescriptorSets[dispSet], 0, nullptr);
    vkCmdDispatch(cmd, sdfGroups, 1, 1);

    // === copy output buffer → image ===
    // 屏障：output buffer write → transfer read
    VkMemoryBarrier outBar{};
    outBar.sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    outBar.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    outBar.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 1, &outBar, 0, nullptr, 0, nullptr);

    // 图片布局：SHADER_READ_ONLY → TRANSFER_DST
    {
        VkImageMemoryBarrier imgBar{};
        imgBar.sType         = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        imgBar.oldLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imgBar.newLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        imgBar.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        imgBar.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        imgBar.image         = mOutputImage->getImage();
        imgBar.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        imgBar.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        imgBar.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        mComputeCommandBuffer->transferImageLayout(imgBar,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT);
    }

    mComputeCommandBuffer->copyBufferToImage(
        mOutputBuffer->getBuffer(),
        mOutputImage->getImage(),
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        (uint32_t)mOutputImage->getWidth(),
        (uint32_t)mOutputImage->getHeight());

    // 图片布局：TRANSFER_DST → SHADER_READ_ONLY
    {
        VkImageMemoryBarrier imgBar{};
        imgBar.sType         = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        imgBar.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        imgBar.newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imgBar.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        imgBar.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        imgBar.image         = mOutputImage->getImage();
        imgBar.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        imgBar.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        imgBar.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        mComputeCommandBuffer->transferImageLayout(imgBar,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    }

    mComputeCommandBuffer->end();
    mComputeCommandBuffer->submitSync(mDevice->getGraphicQueue());
}

// ============================================================================
// 其余接口
// ============================================================================
void RadianceCascades::GameCommandBuffers(unsigned int) {
    // 本项目不使用 swap‑chain 图形管线，compute 在 GameLoop 中提交
}

void RadianceCascades::GameStopInterfaceLoop(unsigned int) {
}

void RadianceCascades::GameTCPLoop() {}

void RadianceCascades::GameUI() {
    if (!mOutputImage || mImGuiDescriptorSet == VK_NULL_HANDLE) return;

    VkExtent2D ext = mSwapChain->getExtent();

    ImGui::Begin(u8"Radiance Cascades GI", nullptr,
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar);
    ImGui::SetWindowPos(ImVec2(0, 0));
    ImGui::SetWindowSize(ImVec2((float)ext.width, (float)ext.height));


    ImVec2 winsz = ImGui::GetContentRegionAvail();
    float imgW = (float)mOutputImage->getWidth();
    float imgH = (float)mOutputImage->getHeight();
    float scale = std::min(winsz.x / imgW, winsz.y / imgH);
    ImVec2 sz(imgW * scale, imgH * scale);
    ImGui::Image((ImTextureID)(intptr_t)mImGuiDescriptorSet, sz);
    ImGui::End();
}

// ============================================================================
// 清理
// ============================================================================
void RadianceCascades::cleanup() {
    if (mCleanedUp) return;
    mCleanedUp = true;

    VkDevice dev = mDevice ? mDevice->getDevice() : VK_NULL_HANDLE;

    if (mImGuiSampler)               { vkDestroySampler(dev, mImGuiSampler, nullptr); mImGuiSampler = VK_NULL_HANDLE; }
    if (mImGuiDescriptorSetLayout)   { vkDestroyDescriptorSetLayout(dev, mImGuiDescriptorSetLayout, nullptr); mImGuiDescriptorSetLayout = VK_NULL_HANDLE; }
    if (mImGuiDescriptorPool)        { vkDestroyDescriptorPool(dev, mImGuiDescriptorPool, nullptr); mImGuiDescriptorPool = VK_NULL_HANDLE; }

    if (mDescriptorPool)             { vkDestroyDescriptorPool(dev, mDescriptorPool, nullptr); mDescriptorPool = VK_NULL_HANDLE; }

    if (mSDFPipeline)                { vkDestroyPipeline(dev, mSDFPipeline, nullptr); mSDFPipeline = VK_NULL_HANDLE; }
    if (mCascadePipeline)            { vkDestroyPipeline(dev, mCascadePipeline, nullptr); mCascadePipeline = VK_NULL_HANDLE; }
    if (mDisplayPipeline)            { vkDestroyPipeline(dev, mDisplayPipeline, nullptr); mDisplayPipeline = VK_NULL_HANDLE; }

    if (mSDFPipelineLayout)          { vkDestroyPipelineLayout(dev, mSDFPipelineLayout, nullptr); mSDFPipelineLayout = VK_NULL_HANDLE; }
    if (mCascadePipelineLayout)      { vkDestroyPipelineLayout(dev, mCascadePipelineLayout, nullptr); mCascadePipelineLayout = VK_NULL_HANDLE; }
    if (mDisplayPipelineLayout)      { vkDestroyPipelineLayout(dev, mDisplayPipelineLayout, nullptr); mDisplayPipelineLayout = VK_NULL_HANDLE; }

    if (mSDFDescriptorSetLayout)     { vkDestroyDescriptorSetLayout(dev, mSDFDescriptorSetLayout, nullptr); mSDFDescriptorSetLayout = VK_NULL_HANDLE; }
    if (mCascadeDescriptorSetLayout) { vkDestroyDescriptorSetLayout(dev, mCascadeDescriptorSetLayout, nullptr); mCascadeDescriptorSetLayout = VK_NULL_HANDLE; }
    if (mDisplayDescriptorSetLayout) { vkDestroyDescriptorSetLayout(dev, mDisplayDescriptorSetLayout, nullptr); mDisplayDescriptorSetLayout = VK_NULL_HANDLE; }

    if (mSDFShaderModule)            { vkDestroyShaderModule(dev, mSDFShaderModule, nullptr); mSDFShaderModule = VK_NULL_HANDLE; }
    if (mCascadeShaderModule)        { vkDestroyShaderModule(dev, mCascadeShaderModule, nullptr); mCascadeShaderModule = VK_NULL_HANDLE; }
    if (mDisplayShaderModule)        { vkDestroyShaderModule(dev, mDisplayShaderModule, nullptr); mDisplayShaderModule = VK_NULL_HANDLE; }

    delete mOutputImage;       mOutputImage = nullptr;
    delete mOutputBuffer;      mOutputBuffer = nullptr;
    delete mCascadeBufferA;    mCascadeBufferA = nullptr;
    delete mCascadeBufferB;    mCascadeBufferB = nullptr;
    delete mSDFBuffer;         mSDFBuffer = nullptr;
    delete mParamBuffer;       mParamBuffer = nullptr;

    delete mComputeCommandBuffer; mComputeCommandBuffer = nullptr;
    delete mComputeCommandPool;   mComputeCommandPool = nullptr;
}

} // namespace GAME
