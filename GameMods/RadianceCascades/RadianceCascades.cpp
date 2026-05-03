// ============================================================================
// RadianceCascades.cpp — 2D 级联辐射度全局光照模块（实现）
// ============================================================================
// 基于 Shadertoy "Radiance Cascades" 参考实现。
//
// 本文件包含：
//   1. SPIR-V 加载与 Shader Module 创建
//   2. GPU 缓冲区分配（Params, SDF, Cascade ×2, Output）
//   3. Compute Pipeline 创建（SDF → Cascade → Display 三阶段）
//   4. 描述符集分配与绑定（含乒乓切换）
//   5. ImGui 纹理显示资源
//   6. 每帧 GameLoop：鼠标平滑 + 参数打包 + dispatch
//   7. 清理逻辑（防重复销毁守卫）
// ============================================================================

#include "RadianceCascades.h"
#include <fstream>
#include <vector>
#include <chrono>
#include <cmath>
#include <algorithm>

namespace GAME {

// ---- SPIR-V 文件路径 ----
// 相对于工作目录（从 build/ 执行时 ../shaders/ 指向项目根目录的 shaders/）
static const char* kSDFSpvPath     = "./shaders/RC_SDF.spv";
static const char* kCascadeSpvPath = "./shaders/RC_Cascade.spv";
static const char* kDisplaySpvPath = "./shaders/RC_Display.spv";

// ============================================================================
// readFile — 将二进制文件内容读取到 vector<char>
// ============================================================================
// 用于加载 .spv 文件（SPIR-V 字节码）到内存。
// 参数 path：SPIR-V 文件路径
// 抛出 std::runtime_error 如果文件无法打开
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
// createShaderModule — 从 SPIR-V 文件创建 Vulkan Shader Module
// ============================================================================
// 流程：
//   1. readFile 加载二进制
//   2. vkCreateShaderModule 创建 VkShaderModule
// 参数 device：逻辑设备
// 参数 path：SPIR-V 文件路径
// 返回创建的 Shader Module 句柄
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
    // 初始化所有运行时状态为默认值
    mTime = 0.0f;
    mMouseAX = 0; mMouseAY = 0; mMouseAZ = 0;
    mMouseBX = 0; mMouseBY = 0; mMouseBZ = 0;
    mMouseCX = 0; mMouseCY = 0; mMouseCZ = 0;
}

RadianceCascades::~RadianceCascades() {
    cleanup();  // 确保所有 Vulkan 资源被释放
}

// ============================================================================
// 输入处理
// ============================================================================

// MouseMove — 跟踪原始鼠标位置
void RadianceCascades::MouseMove(double x, double y) {
    mMousePrevX = mMouseX;
    mMousePrevY = mMouseY;
    mMouseX = x;
    mMouseY = y;
}

void RadianceCascades::MouseRoller(int) {}  // 滚轮事件当前不处理

// KeyDown — 按键处理
void RadianceCascades::KeyDown(GameKeyEnum k) {
    if (k == GameKeyEnum::ESC) {
        // ESC：切换控制台/返回界面（标准模式，非直接退出）
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
        // 键 1：切换直线段绘制模式
        mKeyToggled1 = !mKeyToggled1;
        return;
    }
    if (k == GameKeyEnum::SPACE) {
        // 空格键：切换强制不发光模式（画笔变成黑色无光照）
        mKeyToggledSpace = !mKeyToggledSpace;
        return;
    }
}

// ============================================================================
// 级联条目计数
// ============================================================================

// calculateTotalCascadeEntries — 计算所有级联等级的总条目数
// 将所有 5 层级联的条目数相加，用于分配级联缓冲区。
// 级联 0..4 的数据紧密打包在一个大缓冲区中。
int RadianceCascades::calculateTotalCascadeEntries() const {
    int total = 0;
    for (int lv = 0; lv < N_CASCADES; ++lv) {
        total += calculateCascadeLevelEntries(lv);
    }
    return total;
}

// calculateCascadeLevelEntries — 计算指定级联等级的条目数
// 级联等级 n 的条目数 = sx × sy × dn，其中：
//   sx = mCascadeResX >> n      （探针网格宽度，每层减半）
//   sy = mCascadeResY >> n      （探针网格高度，每层减半）
//   dn = 1 (n=0) 或 C_DRES << (2*(n-1))  （每探针存储的方向数子集，每层 ×4）
//
// n=0: 320×180×1     = 57,600
// n=1: 160×90×16     = 230,400
// n=2: 80×45×64      = 230,400
// n=3: 40×22×256     ≈ 225,280  (22 是向上取整)
// n=4: 20×11×1024    ≈ 225,280
// 总计 ≈ 968,960 条目 × vec4 × 16 bytes = ~15.5 MB
int RadianceCascades::calculateCascadeLevelEntries(int lv) const {
    int sx = mCascadeResX >> lv;
    int sy = mCascadeResY >> lv;
    int dn = (lv == 0) ? 1 : (C_DRES << (2 * (lv - 1)));
    return sx * sy * dn;
}

// ============================================================================
// createBuffers — 创建所有 GPU 缓冲区
// ============================================================================
// 分配以下缓冲区：
//   mParamBuffer    : CPU 可见（HOST_VISIBLE + HOST_COHERENT），每帧 map 更新
//   mSDFBuffer      : GPU 本地（DEVICE_LOCAL），SDF shader 写入，级联 shader 读取
//   mCascadeBufferA/B: GPU 本地乒乓缓冲，交替读写
//   mOutputBuffer   : GPU 本地 + TRANSFER_SRC，compute→transfer→image
//   mOutputImage    : R8G8B8A8_UNORM 纹理，TRANSFER_DST + SAMPLED（ImGui 采样）
void RadianceCascades::createBuffers() {
    VkExtent2D ext = mSwapChain->getExtent();
    int sdfW = (int)ext.width;
    int sdfH = (int)ext.height;
    // 级联分辨率 = SDF 分辨率的 1/4
    mCascadeResX = sdfW / 4;
    mCascadeResY = sdfH / 4;
    int cascadeTotal = calculateTotalCascadeEntries();

    auto dev = mDevice;

    // 统一参数缓冲区（CPU 每帧 map 写入，GPU 读取）
    mParamBuffer = new VulKan::Buffer(
        dev, sizeof(GPUParams),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    // SDF 缓冲区（每像素 1 个 vec4：sd + emissive_RGB）
    mSDFBuffer = new VulKan::Buffer(
        dev, (VkDeviceSize)sdfW * sdfH * sizeof(float) * 4,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    // 级联乒乓缓冲区（各存储完整级联数据）
    VkDeviceSize csize = (VkDeviceSize)cascadeTotal * sizeof(float) * 4;
    mCascadeBufferA = new VulKan::Buffer(
        dev, csize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    mCascadeBufferB = new VulKan::Buffer(
        dev, csize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    // 输出缓冲区（uint32 RGBA8 打包 + 可被 transfer 源读取）
    mOutputBuffer = new VulKan::Buffer(
        dev, (VkDeviceSize)sdfW * sdfH * sizeof(uint32_t),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    // 输出纹理（ImGui 显示的目标图像）
    mOutputImage = new VulKan::Image(
        dev,
        sdfW, sdfH,
        VK_FORMAT_R8G8B8A8_UNORM,           // 标准 RGBA8 格式
        VK_IMAGE_TYPE_2D,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, // transfer 写入 + shader 采样
        VK_SAMPLE_COUNT_1_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT);
}

// ============================================================================
// createComputePipelines — 创建三个 compute pipeline
// ============================================================================
// 流程：
//   1. 从 .spv 文件创建 3 个 Shader Module
//   2. 创建 3 个 Descriptor Set Layout：
//      - SDF layout: 2 个 storage buffer binding (Params + SDF)
//      - Cascade layout: 4 个 storage buffer binding (Params + SDF + Read + Write)
//      - Display layout: 4 个 storage buffer binding (Params + SDF + Cascade + Output)
//   3. 创建 Pipeline Layout：
//      - SDF: 无 push constant
//      - Cascade: 1 个 int32 push constant（cascadeLevel）
//      - Display: 无 push constant
//   4. 创建 3 个 Compute Pipeline
void RadianceCascades::createComputePipelines() {
    VkDevice dev = mDevice->getDevice();

    // ---- 加载 Shader Modules ----
    mSDFShaderModule     = createShaderModule(dev, kSDFSpvPath);
    mCascadeShaderModule = createShaderModule(dev, kCascadeSpvPath);
    mDisplayShaderModule = createShaderModule(dev, kDisplaySpvPath);

    // ---- 辅助：创建 Descriptor Set Layout ----
    // 所有 binding 都是 STORAGE_BUFFER 类型，stage 都是 COMPUTE
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

    mSDFDescriptorSetLayout     = makeLayout(2);  // binding 0=Params, 1=SDF
    mCascadeDescriptorSetLayout = makeLayout(4);  // 0=Params, 1=SDF, 2=CascadeRead, 3=CascadeWrite
    mDisplayDescriptorSetLayout = makeLayout(4);  // 0=Params, 1=SDF, 2=CascadeData, 3=Output

    // ---- SDF Pipeline Layout（无 push constant） ----
    {
        VkPipelineLayoutCreateInfo ci{};
        ci.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        ci.setLayoutCount = 1;
        ci.pSetLayouts    = &mSDFDescriptorSetLayout;
        vkCreatePipelineLayout(dev, &ci, nullptr, &mSDFPipelineLayout);
    }

    // ---- Cascade Pipeline Layout（含 push constant: cascadeLevel） ----
    {
        VkPushConstantRange pr{};
        pr.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        pr.offset = 0;
        pr.size   = sizeof(int32_t);  // 单个 int32 传递级联等级

        VkPipelineLayoutCreateInfo ci{};
        ci.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        ci.setLayoutCount         = 1;
        ci.pSetLayouts            = &mCascadeDescriptorSetLayout;
        ci.pushConstantRangeCount = 1;
        ci.pPushConstantRanges    = &pr;
        vkCreatePipelineLayout(dev, &ci, nullptr, &mCascadePipelineLayout);
    }

    // ---- Display Pipeline Layout（无 push constant） ----
    {
        VkPipelineLayoutCreateInfo ci{};
        ci.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        ci.setLayoutCount = 1;
        ci.pSetLayouts    = &mDisplayDescriptorSetLayout;
        vkCreatePipelineLayout(dev, &ci, nullptr, &mDisplayPipelineLayout);
    }

    // ---- 辅助：创建 Compute Pipeline ----
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
// createDescriptorSets — 分配并绑定描述符集
// ============================================================================
// 创建以下描述符集：
//   mSDFDescriptorSet (1个)                    : Params + SDF
//   mCascadeDescriptorSets[2] (乒乓)           : Params + SDF + Read±Write
//   mDisplayDescriptorSets[2] (乒乓)           : Params + SDF + CascadeData + Output
//
// 级联描述符集索引 0 和 1 交替指 A→B 或 B→A。
// 每次 dispatch 根据当前级联等级选择对应的描述符集。
void RadianceCascades::createDescriptorSets() {
    VkDevice dev = mDevice->getDevice();

    // ---- 描述符池 ----
    VkDescriptorPoolSize poolSize{};
    poolSize.type            = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSize.descriptorCount = 20;
    VkDescriptorPoolCreateInfo poolCI{};
    poolCI.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolCI.maxSets       = 10;
    poolCI.poolSizeCount = 1;
    poolCI.pPoolSizes    = &poolSize;
    vkCreateDescriptorPool(dev, &poolCI, nullptr, &mDescriptorPool);

    // ---- 描述符写入辅助 ----
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

    // 描述符集分配辅助
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

    // 计算各缓冲区大小
    VkExtent2D ext = mSwapChain->getExtent();
    VkDeviceSize sdfSz  = (VkDeviceSize)ext.width * ext.height * sizeof(float) * 4;
    VkDeviceSize cascSz = (VkDeviceSize)calculateTotalCascadeEntries() * sizeof(float) * 4;
    VkDeviceSize outSz  = (VkDeviceSize)ext.width * ext.height * sizeof(uint32_t);

    // ---- SDF 描述符集 ----
    mSDFDescriptorSet = allocSet(mSDFDescriptorSetLayout);
    writeBuf(mSDFDescriptorSet, 0, mParamBuffer->getBuffer(), sizeof(GPUParams));
    writeBuf(mSDFDescriptorSet, 1, mSDFBuffer->getBuffer(),    sdfSz);

    // ---- 级联描述符集（乒乓） ----
    VkBuffer bufA = mCascadeBufferA->getBuffer();
    VkBuffer bufB = mCascadeBufferB->getBuffer();
    VkBuffer sdfB = mSDFBuffer->getBuffer();

    // set 0: read=A, write=B（写目标为 B）
    mCascadeDescriptorSets[0] = allocSet(mCascadeDescriptorSetLayout);
    writeBuf(mCascadeDescriptorSets[0], 0, mParamBuffer->getBuffer(), sizeof(GPUParams));
    writeBuf(mCascadeDescriptorSets[0], 1, sdfB,  sdfSz);
    writeBuf(mCascadeDescriptorSets[0], 2, bufA, cascSz);  // 从 A 读取上级级联
    writeBuf(mCascadeDescriptorSets[0], 3, bufB, cascSz);  // 写入 B

    // set 1: read=B, write=A（写目标为 A）
    mCascadeDescriptorSets[1] = allocSet(mCascadeDescriptorSetLayout);
    writeBuf(mCascadeDescriptorSets[1], 0, mParamBuffer->getBuffer(), sizeof(GPUParams));
    writeBuf(mCascadeDescriptorSets[1], 1, sdfB,  sdfSz);
    writeBuf(mCascadeDescriptorSets[1], 2, bufB, cascSz);  // 从 B 读取上级级联
    writeBuf(mCascadeDescriptorSets[1], 3, bufA, cascSz);  // 写入 A

    // ---- Display 描述符集（乒乓） ----
    // set 0: 从 B 读取级联数据
    mDisplayDescriptorSets[0] = allocSet(mDisplayDescriptorSetLayout);
    writeBuf(mDisplayDescriptorSets[0], 0, mParamBuffer->getBuffer(), sizeof(GPUParams));
    writeBuf(mDisplayDescriptorSets[0], 1, sdfB,  sdfSz);
    writeBuf(mDisplayDescriptorSets[0], 2, bufB, cascSz);  // 读取 cascade B
    writeBuf(mDisplayDescriptorSets[0], 3, mOutputBuffer->getBuffer(), outSz);

    // set 1: 从 A 读取级联数据
    mDisplayDescriptorSets[1] = allocSet(mDisplayDescriptorSetLayout);
    writeBuf(mDisplayDescriptorSets[1], 0, mParamBuffer->getBuffer(), sizeof(GPUParams));
    writeBuf(mDisplayDescriptorSets[1], 1, sdfB,  sdfSz);
    writeBuf(mDisplayDescriptorSets[1], 2, bufA, cascSz);  // 读取 cascade A
    writeBuf(mDisplayDescriptorSets[1], 3, mOutputBuffer->getBuffer(), outSz);
}

// ============================================================================
// createImGuiResources — ImGui 显示输出纹理所需的 Sampler + DescriptorSet
// ============================================================================
// 创建：
//   mImGuiSampler              : 线性过滤，边缘 clamp
//   mImGuiDescriptorSetLayout  : combined image sampler（fragment shader）
//   mImGuiDescriptorPool       : 用于分配 imgui descriptor set
//   mImGuiDescriptorSet        : 绑定 mOutputImage + mImGuiSampler
//
// 同时初始化输出图像的布局为 SHADER_READ_ONLY_OPTIMAL。
void RadianceCascades::createImGuiResources() {
    VkDevice dev = mDevice->getDevice();

    // ---- Sampler：线性过滤 + clamp 到边缘 ----
    VkSamplerCreateInfo sci{};
    sci.sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sci.magFilter    = VK_FILTER_LINEAR;
    sci.minFilter    = VK_FILTER_LINEAR;
    sci.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sci.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    vkCreateSampler(dev, &sci, nullptr, &mImGuiSampler);

    // ---- ImGui Descriptor Set Layout ----
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

    // ---- ImGui Descriptor Pool ----
    VkDescriptorPoolSize psz{};
    psz.type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    psz.descriptorCount = 1;
    VkDescriptorPoolCreateInfo pci{};
    pci.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pci.maxSets       = 1;
    pci.poolSizeCount = 1;
    pci.pPoolSizes    = &psz;
    vkCreateDescriptorPool(dev, &pci, nullptr, &mImGuiDescriptorPool);

    // ---- 分配 + 更新 ImGui Descriptor Set ----
    VkDescriptorSetAllocateInfo ai{};
    ai.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    ai.descriptorPool     = mImGuiDescriptorPool;
    ai.descriptorSetCount = 1;
    ai.pSetLayouts        = &mImGuiDescriptorSetLayout;
    vkAllocateDescriptorSets(dev, &ai, &mImGuiDescriptorSet);

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

    // ---- 初始化输出图像布局 ----
    // 从 UNDEFINED 转到 SHADER_READ_ONLY_OPTIMAL，后续每帧在
    // TRANSFER_DST 和 SHADER_READ_ONLY 之间切换
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
// GameRecordCommandBuffers — 初始化 GPU 资源
// ============================================================================
// 在首帧调用，按顺序创建所有 Vulkan 资源。
void RadianceCascades::GameRecordCommandBuffers() {
    createBuffers();             // 1. 分配缓冲区
    createComputePipelines();    // 2. 创建 Pipeline
    createDescriptorSets();      // 3. 绑定描述符
    createImGuiResources();      // 4. ImGui 纹理 + sampler
}

// ============================================================================
// GameLoop — 每帧逻辑（核心）
// ============================================================================
// 每帧执行流程：
//   1. 计算 delta time，累加 mTime
//   2. 首帧初始化 GPU 资源；否则递增帧计数
//   3. 读取鼠标按键状态（左/右键 + C 键清屏上升沿检测）
//   4. 鼠标平滑处理（Shadertoy Buffer A 的平滑算法）：
//      - 维持 A→B→C 三级鼠标历史
//      - 使用摩擦系数 SMOOTH_FRICTION 平滑鼠标快速移动
//   5. 跟踪直线段起点（用于键 1 模式）
//   6. 打包 GPUParams，通过 map 写入 GPU 缓冲区
//   7. 调用 dispatchCompute() 执行 GPU 管线
//   8. 重置 mNeedClear
void RadianceCascades::GameLoop(unsigned int /*currentFrame*/) {
    // ---- 时间管理 ----
    static auto t0 = std::chrono::high_resolution_clock::now();
    auto t1 = std::chrono::high_resolution_clock::now();
    float dt = std::chrono::duration<float>(t1 - t0).count();
    t0 = t1;
    mTime += dt;

    VkExtent2D ext = mSwapChain->getExtent();

    // ---- 初始化（仅首帧） ----
    if (!mInitialized) {
        GameRecordCommandBuffers();
        mFrameCount = 0;
        mInitialized = true;
    } else {
        ++mFrameCount;
    }

    // ---- 鼠标状态读取 ----
    int leftState  = glfwGetMouseButton(mWindow->getWindow(), GLFW_MOUSE_BUTTON_LEFT);
    int rightState = glfwGetMouseButton(mWindow->getWindow(), GLFW_MOUSE_BUTTON_RIGHT);
    bool leftDown  = (leftState == GLFW_PRESS);
    bool rightDown = (rightState == GLFW_PRESS);
    bool realMouseDown = leftDown || rightDown;  // 任一按钮按下都算"绘制中"

    // ---- 鼠标平滑算法（Shadertoy Buffer A 精确对应） ----
    // 使用摩擦 + 半径限制实现平滑的鼠标跟随，产生自然的画笔路径
    // SMOOTH_RADIUS：鼠标 B 周围的"死区"，新位置在半径内不响应
    // SMOOTH_FRICTION：平滑摩擦系数，值越小平滑越慢
    const float SMOOTH_RADIUS = float(ext.height) * 0.015f;
    const float SMOOTH_FRICTION = 0.05f;

    float rawX = (float)mMouseX;
    float rawY = (float)mMouseY;
    float rawZ = realMouseDown ? 1.0f : 0.0f;

    if (mFrameCount == 0) {
        // 首帧：初始化所有鼠标状态
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
            // B 按下且距离 > 0 → 平滑处理
            float ndx = (rawX - mMouseBX) / dist;  // 从 B 到 raw 的单位方向
            float ndy = (rawY - mMouseBY) / dist;
            float len = std::max(dist - SMOOTH_RADIUS, 0.0f);  // 超出死区的距离
            float ease = 1.0f - std::pow(SMOOTH_FRICTION, dt * 10.0f);  // 平滑缓动
            mMouseCX = mMouseBX + ndx * len * ease;  // 向鼠标位置插值
            mMouseCY = mMouseBY + ndy * len * ease;
            mMouseCZ = rawZ;
        } else {
            // B 未按下或距离为 0 → 直接使用原始位置
            mMouseCX = rawX;
            mMouseCY = rawY;
            mMouseCZ = rawZ;
        }

        // 历史轮换：A ← B ← C
        mMouseAX = mMouseBX;
        mMouseAY = mMouseBY;
        mMouseAZ = mMouseBZ;
        mMouseBX = mMouseCX;
        mMouseBY = mMouseCY;
        mMouseBZ = mMouseCZ;
    }

    // ---- 直线段起点跟踪 ----
    // 当鼠标左键从未按下变为按下时，记录当前位置为线段起点
    if (!mMouseLeftDown && leftDown) {
        mMouseClickStartX = rawX;
        mMouseClickStartY = rawY;
    }

    mMouseLeftDown  = leftDown;
    mMouseRightDown = rightDown;

    // ---- C 键清屏（上升沿检测） ----
    // 仅在从未按变为按下时触发一次清屏
    // mNeedClear 在 dispatch 后重置
    if (glfwGetKey(mWindow->getWindow(), GLFW_KEY_C) == GLFW_PRESS && mPrevCKey == 0) {
        mNeedClear = true;
    }
    mPrevCKey = glfwGetKey(mWindow->getWindow(), GLFW_KEY_C);

    // ---- 打包 GPUParams ----
    GPUParams p{};
    p.time              = mTime;
    p.timeDelta         = dt;
    p.sdfWidth          = (int)ext.width;
    p.sdfHeight         = (int)ext.height;
    p.mouseX            = mMouseCX;
    p.mouseY            = mMouseCY;
    p.mousePrevX        = (float)mMousePrevX;
    p.mousePrevY        = (float)mMousePrevY;
    p.mouseLeftDown     = (mMouseCZ > 0.0f) ? 1 : 0;  // 平滑后的左键状态
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

    // c_intervalLength = screenLen * 4 / (4^N_CASCADES - 1)
    // 对于 N_CASCADES=5: 4^5=1024 → 1023 等分
    // 例如 1920×1080 screenLen≈2203 → c_intervalLength≈8.6 像素
    float screenLen = std::sqrt((float)(ext.width * ext.width + ext.height * ext.height));
    p.c_intervalLength  = screenLen * 4.0f / float((1 << (2 * N_CASCADES)) - 1);

    p.c_smoothDistScale = 0.0f;  // 已弃用，设为 0
    p.totalCascadeEntries = calculateTotalCascadeEntries();
    p.cascadeLevel      = 0;  // SDF shader 不关心此值

    // 鼠标平滑历史
    p.mouseSmoothAX     = mMouseAX;
    p.mouseSmoothAY     = mMouseAY;
    p.mouseSmoothBX     = mMouseBX;
    p.mouseSmoothBY     = mMouseBY;
    p.mouseSmoothADown  = (mMouseAZ > 0.0f) ? 1 : 0;
    p.mouseSmoothBDown  = (mMouseBZ > 0.0f) ? 1 : 0;
    p.mouseMagic        = (mMouseCZ == MAGIC) ? 1 : 0;

    // 按键切换
    p.keyToggledSpace   = mKeyToggledSpace ? 1 : 0;
    p.keyToggled1       = mKeyToggled1 ? 1 : 0;
    p.mouseClickStartX  = mMouseClickStartX;
    p.mouseClickStartY  = mMouseClickStartY;

    // 清屏标志
    p.clearScreen       = mNeedClear ? 1 : 0;

    // ---- 写入 GPU 缓冲区 ----
    mParamBuffer->updateBufferByMap(&p, sizeof(p));

    // ---- 执行 GPU 计算 ----
    dispatchCompute();

    // ---- 重置一次性标志 ----
    mNeedClear = false;
}

// ============================================================================
// dispatchCompute — 录制并提交全部 compute dispatch 序列
// ============================================================================
// 单次 GPU 提交包含以下 dispatch：
//   1. SDF dispatch    : 更新 SDF 缓冲区（鼠标笔画 → 距离场）
//      → 内存屏障：SHADER_WRITE → SHADER_READ
//   2. Cascade dispatch : N_CASCADES 次，自上而下 (n=4→0)
//      → 每次 dispatch 之间插入内存屏障
//      → push constant 设置当前 cascadeLevel
//      → 乒乓描述符集切换
//   3. Display dispatch : 级联 fluence + SDF emissivity → 色调映射 → 输出
//
//   4. Buffer→Image 复制：输出缓冲区 → mOutputImage 纹理
//      → 两个图像布局转换（SHADER_READ_ONLY → TRANSFER_DST → SHADER_READ_ONLY）
//      → 内存屏障保证同步
void RadianceCascades::dispatchCompute() {
    VkExtent2D ext = mSwapChain->getExtent();
    int sdfW = (int)ext.width;
    int sdfH = (int)ext.height;
    // 每个工作组 64 线程，计算所需工作组数量
    uint32_t sdfGroups = (uint32_t)((sdfW * sdfH + 63) / 64);

    VkCommandBuffer cmd = mComputeCommandBuffer->getCommandBuffer();
    mComputeCommandBuffer->begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    // ================================================================
    // === 阶段 1：SDF Shader ===
    // ================================================================
    // 绑定 SDF pipeline + 描述符集，dispatch sdfGroups 个工作组
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, mSDFPipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, mSDFPipelineLayout,
        0, 1, &mSDFDescriptorSet, 0, nullptr);
    vkCmdDispatch(cmd, sdfGroups, 1, 1);

    // 内存屏障：确保 SDF 写入对后续级联 shader 可见
    VkMemoryBarrier memBar{};
    memBar.sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    memBar.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    memBar.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0, 1, &memBar, 0, nullptr, 0, nullptr);

    // ================================================================
    // === 阶段 2：Cascade Shader（5 层级联，自上而下 n=4→0） ===
    // ================================================================
    // 处理顺序：
    //   n=4 (setIdx=0): read=A, write=B → B 有 level4 数据
    //   n=3 (setIdx=1): read=B, write=A → A 有 level3+4 数据（3 合并 4）
    //   n=2 (setIdx=0): read=A, write=B → B 有 level2+3+4 数据
    //   n=1 (setIdx=1): read=B, write=A → A 有 level1+2+3+4 数据
    //   n=0 (setIdx=0): read=A, write=B → B 有全部 5 层数据
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, mCascadePipeline);
    for (int n = N_CASCADES - 1; n >= 0; --n) {
        int setIdx = (N_CASCADES - 1 - n) % 2;
        int entries = calculateCascadeLevelEntries(n);
        uint32_t groups = (uint32_t)((entries + 63) / 64);

        // 设置当前级联等级（push constant）
        vkCmdPushConstants(cmd, mCascadePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT,
            0, sizeof(int32_t), &n);
        // 绑定乒乓描述符集
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, mCascadePipelineLayout,
            0, 1, &mCascadeDescriptorSets[setIdx], 0, nullptr);
        vkCmdDispatch(cmd, groups, 1, 1);

        // 内存屏障：确保本级联写入对下级联可见
        vkCmdPipelineBarrier(cmd,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            0, 1, &memBar, 0, nullptr, 0, nullptr);
    }

    // ================================================================
    // === 阶段 3：Display Shader ===
    // ================================================================
    // 最终级联结果在哪个缓冲区取决于 N_CASCADES-1 的奇偶性：
    //   N_CASCADES=5，n=4 时 setIdx=0 将 level4 写入 B
    //   n=0 时 setIdx=0 将 level0 写入 B
    //   所以最终结果在 B（mDisplayDescriptorSets[0] 从 B 读取）
    int dispSet = (N_CASCADES - 1) % 2;  // = 0
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, mDisplayPipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, mDisplayPipelineLayout,
        0, 1, &mDisplayDescriptorSets[dispSet], 0, nullptr);
    vkCmdDispatch(cmd, sdfGroups, 1, 1);

    // ================================================================
    // === 阶段 4：Buffer → Image 复制 ===
    // ================================================================
    // 内存屏障：output buffer SHADER_WRITE → TRANSFER_READ
    VkMemoryBarrier outBar{};
    outBar.sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    outBar.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    outBar.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 1, &outBar, 0, nullptr, 0, nullptr);

    // 图像布局转换：SHADER_READ_ONLY → TRANSFER_DST（准备接收数据）
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

    // 执行 buffer → image 复制
    mComputeCommandBuffer->copyBufferToImage(
        mOutputBuffer->getBuffer(),
        mOutputImage->getImage(),
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        (uint32_t)mOutputImage->getWidth(),
        (uint32_t)mOutputImage->getHeight());

    // 图像布局转换：TRANSFER_DST → SHADER_READ_ONLY（ImGui 采样可用）
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

    // 结束命令录制 + 提交到图形队列（同步等待完成）
    mComputeCommandBuffer->end();
    mComputeCommandBuffer->submitSync(mDevice->getGraphicQueue());
}

// ============================================================================
// 其余接口
// ============================================================================

// GameCommandBuffers — 图形管线录制
// 本项目不使用 swap‑chain 图形管线，compute 在 GameLoop 中通过 dispatchCompute 提交
void RadianceCascades::GameCommandBuffers(unsigned int) {
}

// GameStopInterfaceLoop — 停止循环
// 空实现（参考其他 mod 如 MazeMods 的做法）。
// 早期版本在每帧调用 cleanup() 导致 Vulkan 验证层报"double destroy"错误。
void RadianceCascades::GameStopInterfaceLoop(unsigned int) {
}

void RadianceCascades::GameTCPLoop() {}

// ============================================================================
// GameUI — ImGui 渲染：全屏显示级联光照纹理
// ============================================================================
// 创建一个无标题栏、不可移动/调整大小的全屏 ImGui 窗口，
// 将 mOutputImage 纹理按比例缩放填满可用区域。
void RadianceCascades::GameUI() {
    if (!mOutputImage || mImGuiDescriptorSet == VK_NULL_HANDLE) return;

    VkExtent2D ext = mSwapChain->getExtent();

    // 全屏无边框窗口，完全覆盖显示区域
    ImGui::Begin(u8"Radiance Cascades GI", nullptr,
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar);
    ImGui::SetWindowPos(ImVec2(0, 0));
    ImGui::SetWindowSize(ImVec2((float)ext.width, (float)ext.height));

    // 将纹理缩放至可用区域，保持宽高比
    ImVec2 winsz = ImGui::GetContentRegionAvail();
    float imgW = (float)mOutputImage->getWidth();
    float imgH = (float)mOutputImage->getHeight();
    float scale = std::min(winsz.x / imgW, winsz.y / imgH);
    ImVec2 sz(imgW * scale, imgH * scale);
    ImGui::Image((ImTextureID)(intptr_t)mImGuiDescriptorSet, sz);
    ImGui::End();
}

// ============================================================================
// cleanup — 释放所有 Vulkan 资源
// ============================================================================
// 防护卫 mCleanedUp 确保即使在多次析构或接口调用中也不会重复销毁。
// 销毁顺序：从创建的反序进行（先创建的依赖后创建的）。
//   1. Sampler
//   2. ImGui descriptor set layout + pool
//   3. Compute descriptor pool
//   4. Pipeline
//   5. Pipeline layout
//   6. Descriptor set layout
//   7. Shader module
//   8. Buffer + Image 对象（delete）
//   9. Command buffer + pool
void RadianceCascades::cleanup() {
    if (mCleanedUp) return;  // 防止重复销毁
    mCleanedUp = true;

    VkDevice dev = mDevice ? mDevice->getDevice() : VK_NULL_HANDLE;

    // ---- Sampler ----
    if (mImGuiSampler)               { vkDestroySampler(dev, mImGuiSampler, nullptr); mImGuiSampler = VK_NULL_HANDLE; }

    // ---- ImGui 描述符资源 ----
    if (mImGuiDescriptorSetLayout)   { vkDestroyDescriptorSetLayout(dev, mImGuiDescriptorSetLayout, nullptr); mImGuiDescriptorSetLayout = VK_NULL_HANDLE; }
    if (mImGuiDescriptorPool)        { vkDestroyDescriptorPool(dev, mImGuiDescriptorPool, nullptr); mImGuiDescriptorPool = VK_NULL_HANDLE; }

    // ---- Compute 描述符池 ----
    if (mDescriptorPool)             { vkDestroyDescriptorPool(dev, mDescriptorPool, nullptr); mDescriptorPool = VK_NULL_HANDLE; }

    // ---- Pipeline ----
    if (mSDFPipeline)                { vkDestroyPipeline(dev, mSDFPipeline, nullptr); mSDFPipeline = VK_NULL_HANDLE; }
    if (mCascadePipeline)            { vkDestroyPipeline(dev, mCascadePipeline, nullptr); mCascadePipeline = VK_NULL_HANDLE; }
    if (mDisplayPipeline)            { vkDestroyPipeline(dev, mDisplayPipeline, nullptr); mDisplayPipeline = VK_NULL_HANDLE; }

    // ---- Pipeline Layout ----
    if (mSDFPipelineLayout)          { vkDestroyPipelineLayout(dev, mSDFPipelineLayout, nullptr); mSDFPipelineLayout = VK_NULL_HANDLE; }
    if (mCascadePipelineLayout)      { vkDestroyPipelineLayout(dev, mCascadePipelineLayout, nullptr); mCascadePipelineLayout = VK_NULL_HANDLE; }
    if (mDisplayPipelineLayout)      { vkDestroyPipelineLayout(dev, mDisplayPipelineLayout, nullptr); mDisplayPipelineLayout = VK_NULL_HANDLE; }

    // ---- Descriptor Set Layout ----
    if (mSDFDescriptorSetLayout)     { vkDestroyDescriptorSetLayout(dev, mSDFDescriptorSetLayout, nullptr); mSDFDescriptorSetLayout = VK_NULL_HANDLE; }
    if (mCascadeDescriptorSetLayout) { vkDestroyDescriptorSetLayout(dev, mCascadeDescriptorSetLayout, nullptr); mCascadeDescriptorSetLayout = VK_NULL_HANDLE; }
    if (mDisplayDescriptorSetLayout) { vkDestroyDescriptorSetLayout(dev, mDisplayDescriptorSetLayout, nullptr); mDisplayDescriptorSetLayout = VK_NULL_HANDLE; }

    // ---- Shader Module ----
    if (mSDFShaderModule)            { vkDestroyShaderModule(dev, mSDFShaderModule, nullptr); mSDFShaderModule = VK_NULL_HANDLE; }
    if (mCascadeShaderModule)        { vkDestroyShaderModule(dev, mCascadeShaderModule, nullptr); mCascadeShaderModule = VK_NULL_HANDLE; }
    if (mDisplayShaderModule)        { vkDestroyShaderModule(dev, mDisplayShaderModule, nullptr); mDisplayShaderModule = VK_NULL_HANDLE; }

    // ---- Buffer / Image 对象 ----
    delete mOutputImage;       mOutputImage = nullptr;
    delete mOutputBuffer;      mOutputBuffer = nullptr;
    delete mCascadeBufferA;    mCascadeBufferA = nullptr;
    delete mCascadeBufferB;    mCascadeBufferB = nullptr;
    delete mSDFBuffer;         mSDFBuffer = nullptr;
    delete mParamBuffer;       mParamBuffer = nullptr;

    // ---- Command Buffer / Pool ----
    delete mComputeCommandBuffer; mComputeCommandBuffer = nullptr;
    delete mComputeCommandPool;   mComputeCommandPool = nullptr;
}

} // namespace GAME
