#pragma once

#include "../Configuration.h"
#include "../GameMods.h"
#include "../../Vulkan/buffer.h"
#include "../../Vulkan/commandPool.h"
#include "../../Vulkan/commandBuffer.h"
#include "../../Vulkan/device.h"
#include "../../Vulkan/image.h"
#include "../../Vulkan/pipeline.h"
#include "../../Vulkan/shader.h"
#include "../../Vulkan/descriptorSetLayout.h"
#include "../../Vulkan/descriptorPool.h"
#include "../../Vulkan/descriptorSet.h"
#include "../../Vulkan/sampler.h"
#include "../../ImGui/imgui.h"

// ============================================================================
// RadianceCascades — 2D全局光照 GameMod
//
// 基于Radiance Cascades算法实现实时2D全局光照效果
// 支持鼠标绘制场景（发光/非发光物体），实时计算多次反弹的间接光照
//
// 操作说明：
//   鼠标左键拖动 - 绘制发光物体
//   鼠标右键拖动 - 绘制墙壁（遮挡光线）
//   C键          - 清空画布（全部变为空地）
//   R键          - 重置为默认场景
//   Esc键        - 退出返回菜单
//
// 算法原理：
//   将2D光照分解为多个级联层级：
//   - 低级联(cascade 0)：高空间分辨率、低方向分辨率、覆盖近距离
//   - 高级联(cascade N)：低空间分辨率、高方向分辨率、覆盖远距离
//   自底向上合并：辐射度 = 本级直接辐射度 + 可见性 × 上级联辐射度
// ============================================================================

namespace GAME {

class RadianceCascades : public GameMods, Configuration {
public:
    RadianceCascades(Configuration wConfiguration);
    ~RadianceCascades();

    // ---- GameMods接口 ----
    void MouseMove(double xpos, double ypos) override;
    void MouseRoller(int z) override;
    void KeyDown(GameKeyEnum moveDirection) override;
    void GameCommandBuffers(unsigned int Format_i) override;
    void GameLoop(unsigned int mCurrentFrame) override;
    void GameRecordCommandBuffers() override;
    void GameStopInterfaceLoop(unsigned int mCurrentFrame) override;
    void GameTCPLoop() override;
    void GameUI() override;

private:
    // ---- 级联参数常量 ----
    static constexpr int C_SRES_X = 320;          // cascade 0 空间分辨率X
    static constexpr int C_SRES_Y = 180;          // cascade 0 空间分辨率Y
    static constexpr int C_DRES = 16;             // 基础方向分辨率
    static constexpr int N_CASCADES = 4;          // 级联总数
    static constexpr float C_INTERVAL_LENGTH = 7.0f; // cascade 0 光线行进长度
    static constexpr float C_SMOOTH_DIST_SCALE = 10.0f; // 平滑距离缩放
    static constexpr float BRUSH_RADIUS = 0.02f;  // 画笔半径（占屏幕高度比例）

    // ---- GPU缓冲区 ----
    VulKan::Buffer* mParamBuffer = nullptr;       // 参数缓冲区
    VulKan::Buffer* mSDFBuffer = nullptr;         // SDF缓冲区（持久化）
    VulKan::Buffer* mCascadeBufferA = nullptr;    // 级联缓冲区A（乒乓）
    VulKan::Buffer* mCascadeBufferB = nullptr;    // 级联缓冲区B（乒乓）
    VulKan::Buffer* mOutputBuffer = nullptr;      // 输出缓冲区

    // ---- 纹理与显示 ----
    VulKan::Image* mOutputImage = nullptr;         // 输出纹理（UNORM格式，GPU端直接拷贝）

    // ---- 计算管线资源 ----
    VulKan::CommandPool* mComputeCommandPool = nullptr;
    VulKan::CommandBuffer* mComputeCommandBuffer = nullptr;

    // SDF管线
    VkShaderModule mSDFShaderModule = VK_NULL_HANDLE;
    VkDescriptorSetLayout mSDFDescriptorSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout mSDFPipelineLayout = VK_NULL_HANDLE;
    VkPipeline mSDFPipeline = VK_NULL_HANDLE;
    VkDescriptorSet mSDFDescriptorSet = VK_NULL_HANDLE;

    // Cascade管线
    VkShaderModule mCascadeShaderModule = VK_NULL_HANDLE;
    VkDescriptorSetLayout mCascadeDescriptorSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout mCascadePipelineLayout = VK_NULL_HANDLE;
    VkPipeline mCascadePipeline = VK_NULL_HANDLE;
    VkDescriptorSet mCascadeDescriptorSets[2] = {VK_NULL_HANDLE, VK_NULL_HANDLE};

    // Display管线
    VkShaderModule mDisplayShaderModule = VK_NULL_HANDLE;
    VkDescriptorSetLayout mDisplayDescriptorSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout mDisplayPipelineLayout = VK_NULL_HANDLE;
    VkPipeline mDisplayPipeline = VK_NULL_HANDLE;
    VkDescriptorSet mDisplayDescriptorSets[2] = {VK_NULL_HANDLE, VK_NULL_HANDLE};

    // 描述符池（所有管线共用）
    VkDescriptorPool mDescriptorPool = VK_NULL_HANDLE;

    // ---- ImGui纹理显示 ----
    VkDescriptorSetLayout mImGuiDescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool mImGuiDescriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet mImGuiDescriptorSet = VK_NULL_HANDLE;
    VkSampler mImGuiSampler = VK_NULL_HANDLE;

    // ---- 状态 ----
    int mFrameCount = 0;
    bool mCascadeToggle = false;
    double mMouseX = 0, mMouseY = 0;
    double mMousePrevX = 0, mMousePrevY = 0;
    bool mMouseLeftDown = false;
    bool mMouseRightDown = false;
    float mTime = 0.0f;

    // ---- 辅助函数 ----
    int calculateTotalCascadeEntries() const;
    void createBuffers();
    void createComputePipelines();
    void createDescriptorSets();
    void createImGuiResources();
    void dispatchCompute();
    void cleanup();

    // 着色器SPIR-V文件路径
    static const std::string SDF_SHADER_PATH;
    static const std::string CASCADE_SHADER_PATH;
    static const std::string DISPLAY_SHADER_PATH;
};

}
