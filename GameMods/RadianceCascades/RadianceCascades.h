#pragma once

// ============================================================================
// RadianceCascades.h — 2D 级联辐射度全局光照模块（头文件）
// ============================================================================
// 基于 Shadertoy "Radiance Cascades" 参考实现的 2D 光线追踪 GI。
//
// 架构概述：
//   ┌─────────────┐
//   │  SDF Shader │ ← 鼠标笔画 → 2D Signed Distance Field
//   └──────┬──────┘
//          ▼
//   ┌──────────────────┐
//   │ Cascade Shader ×5│ ← 5 层级联光线追踪（自上而下 n=4→0）
//   │ (乒乓缓冲 A↔B)   │     每层探头在 16 方向 × 4^n 采样 radiance
//   └──────┬───────────┘
//          ▼
//   ┌──────────────────┐
//   │  Display Shader  │ ← 级联 fluence + SDF emissivity → 色调映射 → RGBA8
//   └──────┬───────────┘
//          ▼
//   ┌──────────────────┐
//   │  ImGui::Image    │ ← 显示级联光照结果
//   └──────────────────┘
//
// 关键设计决策：
//   - SDF 默认值 = 50.0（非 MAX_FLOAT）：避免双线性插值污染
//   - 天空辐射禁用：暗背景，发光物体自行照亮场景
//   - N_CASCADES = 5, C_DRES = 16
//   - 级联间隔 c_intervalLength = screenLen*4/(4^5-1) ≈ 8.6 像素
// ============================================================================

#include "../Configuration.h"
#include "../GameMods.h"
#include "../../Vulkan/buffer.h"
#include "../../Vulkan/commandPool.h"
#include "../../Vulkan/commandBuffer.h"
#include "../../Vulkan/device.h"
#include "../../Vulkan/image.h"
#include "../../Vulkan/shader.h"
#include "../../InterfaceUI/GUI.h"

namespace GAME {

// ============================================================================
// GPUParams — CPU↔GPU 统一参数块
// ============================================================================
// std430 布局，与三个 compute shader 中的 Params 缓冲区完全对应。
// 通过 mParamBuffer 每帧 map 更新，GPU 侧只读。
// 字段顺序必须与 shader 中声明顺序严格一致，确保字节对齐。
struct GPUParams {
    float time;
    float timeDelta;
    int sdfWidth;
    int sdfHeight;
    float mouseX;
    float mouseY;
    float mousePrevX;
    float mousePrevY;
    int mouseLeftDown;
    int mouseRightDown;
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
    int cascadeLevel;
    float mouseSmoothAX;
    float mouseSmoothAY;
    float mouseSmoothBX;
    float mouseSmoothBY;
    int mouseSmoothADown;
    int mouseSmoothBDown;
    int mouseMagic;
    int keyToggledSpace;
    int keyToggled1;
    float mouseClickStartX;
    float mouseClickStartY;
    float mouseRawX;
    float mouseRawY;
    int clearScreen;
    float smoothRadius;
    float smoothFriction;
    float toneMappingExponent;
    int rayMarchMaxSteps;
    float cascadeIntervalScale;
    float emissivityHueSpeed;
    float emissivitySaturation;
    float emissivityValue;
    float displayBlendRange;
};

// ============================================================================
// RadianceCascades — 主类
// ============================================================================
// 继承 GameMods（游戏模块接口）和 Configuration（窗口配置）。
// 封装所有 Vulkan 资源和 compute shader dispatch 逻辑。
class RadianceCascades : public GameMods, Configuration {
public:
    // ---- 构造/析构 ----
    RadianceCascades(Configuration wConfiguration);
    ~RadianceCascades();

    // ---- 输入事件（GameMods 接口） ----
    void MouseMove(double xpos, double ypos) override;   // 跟踪鼠标位置
    void MouseRoller(int z) override;                     // 滚轮（当前不处理）
    void KeyDown(GameKeyEnum moveDirection) override;     // 按键处理（ESC/1/SPACE）

    // ---- 帧循环（GameMods 接口） ----
    void GameCommandBuffers(unsigned int Format_i) override;  // 图形管线录制（本项目未使用）
    void GameLoop(unsigned int mCurrentFrame) override;       // 每帧逻辑 + GPU 参数更新 + dispatch
    void GameRecordCommandBuffers() override;                 // 初始化：创建所有 GPU 资源
    void GameStopInterfaceLoop(unsigned int mCurrentFrame) override; // 停止循环（空闲）
    void GameTCPLoop() override;                            // 网络循环（未使用）

    // ---- UI ----
    void GameUI() override;  // ImGui 渲染：全屏显示级联光照纹理

private:
    // ---- 算法参数（运行时可调） ----
    static constexpr float MAGIC = 1e25f;

    int mCDres = 16;
    int mNCascades = 5;
    float mBrushRadius = 0.01f;
    float mSmoothRadius = 0.015f;
    float mSmoothFriction = 0.05f;
    float mToneMappingExponent = 2.5f;
    int mRayMarchMaxSteps = 100;
    float mCascadeIntervalScale = 1.0f;
    float mEmissivityHueSpeed = 0.2f;
    float mEmissivitySaturation = 1.0f;
    float mEmissivityValue = 0.8f;
    float mDisplayBlendRange = 3.0f;

    int mPendingCDres = 16;
    int mPendingNCascades = 5;
    bool mNeedsCascadeRebuild = false;

    ImVec2 mImageViewScreenPos = ImVec2(0, 0);
    ImVec2 mImageViewSize = ImVec2(0, 0);
    float mImageViewDisplayScale = 1.0f;
    float mImageViewDataW = 0.0f;
    float mImageViewDataH = 0.0f;

    bool mUIShowBrush = true;
    bool mUIShowColor = true;
    bool mUIShowCascade = true;
    bool mUIShowDisplay = true;
    bool mUIShowRayMarch = true;
    bool mUIShowInfo = true;

    // ---- 级联尺寸 ----
    int mCascadeResX = 320;
    int mCascadeResY = 180;

    // ---- GPU 缓冲区 ----
    VulKan::Buffer* mParamBuffer = nullptr;       // 统一参数缓冲区（CPU 可见，每帧 map 更新）
    VulKan::Buffer* mSDFBuffer = nullptr;         // SDF 缓冲区（sdfW×sdfH×vec4，GPU 本地）
    VulKan::Buffer* mCascadeBufferA = nullptr;    // 级联乒乓缓冲 A（计算目标）
    VulKan::Buffer* mCascadeBufferB = nullptr;    // 级联乒乓缓冲 B
    VulKan::Buffer* mOutputBuffer = nullptr;      // 输出缓冲区（sdfW×sdfH×uint32 RGBA8）
    VulKan::Image* mOutputImage = nullptr;        // 输出纹理（R8G8B8A8_UNORM，ImGui 采样）

    // ---- Compute 命令相关 ----
    VulKan::CommandPool* mComputeCommandPool = nullptr;
    VulKan::CommandBuffer* mComputeCommandBuffer = nullptr;

    // ---- SDF Pipeline 资源 ----
    VkShaderModule mSDFShaderModule = VK_NULL_HANDLE;
    VkDescriptorSetLayout mSDFDescriptorSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout mSDFPipelineLayout = VK_NULL_HANDLE;
    VkPipeline mSDFPipeline = VK_NULL_HANDLE;
    VkDescriptorSet mSDFDescriptorSet = VK_NULL_HANDLE;

    // ---- Cascade Pipeline 资源 ----
    VkShaderModule mCascadeShaderModule = VK_NULL_HANDLE;
    VkDescriptorSetLayout mCascadeDescriptorSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout mCascadePipelineLayout = VK_NULL_HANDLE;
    VkPipeline mCascadePipeline = VK_NULL_HANDLE;
    VkDescriptorSet mCascadeDescriptorSets[2] = {VK_NULL_HANDLE, VK_NULL_HANDLE}; // 乒乓描述符集

    // ---- Display Pipeline 资源 ----
    VkShaderModule mDisplayShaderModule = VK_NULL_HANDLE;
    VkDescriptorSetLayout mDisplayDescriptorSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout mDisplayPipelineLayout = VK_NULL_HANDLE;
    VkPipeline mDisplayPipeline = VK_NULL_HANDLE;
    VkDescriptorSet mDisplayDescriptorSets[2] = {VK_NULL_HANDLE, VK_NULL_HANDLE}; // 乒乓描述符集

    // ---- 描述符池 ----
    VkDescriptorPool mDescriptorPool = VK_NULL_HANDLE;

    // ---- ImGui 显示资源 ----
    VkDescriptorSet mImGuiDescriptorSet = VK_NULL_HANDLE;  // 由 ImGui_ImplVulkan_AddTexture() 创建，绑定 mOutputImage

    // ---- 运行时状态 ----
    int mFrameCount = 0;            // 帧计数（自初始化起）
    double mMouseX = 0, mMouseY = 0;          // 当前原始鼠标位置（从 GLFW）
    double mMousePrevX = 0, mMousePrevY = 0;  // 上一帧原始鼠标位置
    bool mMouseLeftDown = false;    // 当前帧左键状态
    bool mMouseRightDown = false;   // 当前帧右键状态
    float mTime = 0.0f;            // 累计运行时间（秒）
    float mMouseAX = 0.0f, mMouseAY = 0.0f, mMouseAZ = 0.0f; // 平滑历史 A
    float mMouseBX = 0.0f, mMouseBY = 0.0f, mMouseBZ = 0.0f; // 平滑历史 B
    float mMouseCX = 0.0f, mMouseCY = 0.0f, mMouseCZ = 0.0f; // 平滑历史 C（当前）
    bool mInitialized = false;      // 资源是否已初始化
    bool mKeyToggledSpace = false;  // 空格键切换状态
    bool mKeyToggled1 = false;      // 键 1 切换状态
    bool mNeedClear = false;        // 是否需要清屏（在 dispatch 后重置）
    bool mCleanedUp = false;        // 防止重复 cleanup 的守卫
    int mPrevCKey = 0;             // 上一帧 C 键状态（用于上升沿检测）
    float mMouseClickStartX = 0.0f; // 点击起始 X（直线段模式）
    float mMouseClickStartY = 0.0f; // 点击起始 Y

    // ---- 私有方法 ----
    int calculateTotalCascadeEntries() const;    // 计算所有级联的条目总数
    int calculateCascadeLevelEntries(int level) const; // 计算指定级联的条目数
    void createBuffers();                        // 创建所有 GPU 缓冲区
    void createComputePipelines();               // 创建 compute pipeline + 描述符布局
    void createDescriptorSets();                 // 分配并绑定描述符集
    void createImGuiResources();                 // 创建 ImGui 显示所需资源
    void dispatchCompute();                      // 录制+提交完整 compute dispatch 序列
    void rebuildCascadeResources();              // 级联参数变更后重建缓冲区和描述符集
    bool hasCascadeParamsChanged() const;         // 检测级联参数是否与当前不同
    void cleanup();                              // 释放所有 Vulkan 资源（带防护卫）
};

} // namespace GAME
