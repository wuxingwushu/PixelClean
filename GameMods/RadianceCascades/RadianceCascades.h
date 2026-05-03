#pragma once

#include "../Configuration.h"
#include "../GameMods.h"
#include "../../Vulkan/buffer.h"
#include "../../Vulkan/commandPool.h"
#include "../../Vulkan/commandBuffer.h"
#include "../../Vulkan/device.h"
#include "../../Vulkan/image.h"
#include "../../Vulkan/shader.h"
#include "../../Imgui/imgui.h"

namespace GAME {

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
};

class RadianceCascades : public GameMods, Configuration {
public:
    RadianceCascades(Configuration wConfiguration);
    ~RadianceCascades();

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
    static constexpr int C_DRES = 16;
    static constexpr int N_CASCADES = 5;
    static constexpr float BRUSH_RADIUS = 0.01f;
    static constexpr float MAGIC = 1e25f;

    int mCascadeResX = 320;
    int mCascadeResY = 180;

    VulKan::Buffer* mParamBuffer = nullptr;
    VulKan::Buffer* mSDFBuffer = nullptr;
    VulKan::Buffer* mCascadeBufferA = nullptr;
    VulKan::Buffer* mCascadeBufferB = nullptr;
    VulKan::Buffer* mOutputBuffer = nullptr;
    VulKan::Image* mOutputImage = nullptr;

    VulKan::CommandPool* mComputeCommandPool = nullptr;
    VulKan::CommandBuffer* mComputeCommandBuffer = nullptr;

    VkShaderModule mSDFShaderModule = VK_NULL_HANDLE;
    VkDescriptorSetLayout mSDFDescriptorSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout mSDFPipelineLayout = VK_NULL_HANDLE;
    VkPipeline mSDFPipeline = VK_NULL_HANDLE;
    VkDescriptorSet mSDFDescriptorSet = VK_NULL_HANDLE;

    VkShaderModule mCascadeShaderModule = VK_NULL_HANDLE;
    VkDescriptorSetLayout mCascadeDescriptorSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout mCascadePipelineLayout = VK_NULL_HANDLE;
    VkPipeline mCascadePipeline = VK_NULL_HANDLE;
    VkDescriptorSet mCascadeDescriptorSets[2] = {VK_NULL_HANDLE, VK_NULL_HANDLE};

    VkShaderModule mDisplayShaderModule = VK_NULL_HANDLE;
    VkDescriptorSetLayout mDisplayDescriptorSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout mDisplayPipelineLayout = VK_NULL_HANDLE;
    VkPipeline mDisplayPipeline = VK_NULL_HANDLE;
    VkDescriptorSet mDisplayDescriptorSets[2] = {VK_NULL_HANDLE, VK_NULL_HANDLE};

    VkDescriptorPool mDescriptorPool = VK_NULL_HANDLE;

    VkDescriptorSetLayout mImGuiDescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool mImGuiDescriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet mImGuiDescriptorSet = VK_NULL_HANDLE;
    VkSampler mImGuiSampler = VK_NULL_HANDLE;

    int mFrameCount = 0;
    double mMouseX = 0, mMouseY = 0;
    double mMousePrevX = 0, mMousePrevY = 0;
    bool mMouseLeftDown = false;
    bool mMouseRightDown = false;
    float mTime = 0.0f;
    float mMouseAX = 0.0f, mMouseAY = 0.0f, mMouseAZ = 0.0f;
    float mMouseBX = 0.0f, mMouseBY = 0.0f, mMouseBZ = 0.0f;
    float mMouseCX = 0.0f, mMouseCY = 0.0f, mMouseCZ = 0.0f;
    bool mInitialized = false;
    bool mKeyToggledSpace = false;
    bool mKeyToggled1 = false;
    float mMouseClickStartX = 0.0f;
    float mMouseClickStartY = 0.0f;

    int calculateTotalCascadeEntries() const;
    int calculateCascadeLevelEntries(int level) const;
    void createBuffers();
    void createComputePipelines();
    void createDescriptorSets();
    void createImGuiResources();
    void dispatchCompute();
    void cleanup();
};

} // namespace GAME
