#pragma once

#include "../Configuration.h"
#include "../GameMods.h"
#include "../../Tool/MovePlate.h"
#include "../../VulkanTool/AuxiliaryVision.h"
#include "ChunkData.h"
#include <glm/glm.hpp>
#include <unordered_map>
#include <tuple>
#include <vector>

namespace GAME {

// 区块网格配置
constexpr unsigned int CHUNK_COUNT_X = 3;
constexpr unsigned int CHUNK_COUNT_Y = 3;
constexpr unsigned int CHUNK_COUNT_Z = 3;
constexpr unsigned int CHUNK_EDGE = ChunkData::CHUNK_SIZE;
constexpr unsigned int CHUNK_ORIGIN_X = 1;
constexpr unsigned int CHUNK_ORIGIN_Y = 1;
constexpr unsigned int CHUNK_ORIGIN_Z = 0;

// 顶点类型别名
using BlockVertex = VulKan::AuxiliaryLineSpot;

// chunk map 的哈希函数（必须在 BlockWorld 类之前完整定义）
struct ChunkKeyHash {
    size_t operator()(const std::tuple<int, int, int>& key) const {
        return std::get<0>(key) * 73856093
             ^ std::get<1>(key) * 19349663
             ^ std::get<2>(key) * 83492791;
    }
};

class BlockWorld : public GameMods, Configuration {
public:
    explicit BlockWorld(Configuration wConfiguration);
    ~BlockWorld();

    // 鼠标移动事件
    virtual void MouseMove(double xpos, double ypos);

    // 鼠标滚轮事件
    virtual void MouseRoller(int z);

    // 鼠标按键事件
    virtual void MouseButton(MouseBtn button, InputState State);

    // 键盘事件
    virtual void KeyDown(GameKeyEnum moveDirection);

    // 获取 CommandBuffer
    virtual void GameCommandBuffers(unsigned int Format_i);

    // 游戏循环
    virtual void GameLoop(unsigned int mCurrentFrame);

    // 录制 CommandBuffer
    virtual void GameRecordCommandBuffers();

    // 游戏停止界面循环
    virtual void GameStopInterfaceLoop(unsigned int mCurrentFrame);

    // 游戏 TCP 事件
    virtual void GameTCPLoop();

    // 游戏界面
    virtual void GameUI();

private:
    // 静态回调
    static void OnChunkGenerate(int mT, int x, int y, int z, void* Data);
    static void OnChunkDelete(int mT, void* Data);

    // 区块管理
    int GetBlockAtWorld(int wx, int wy, int wz);
    void CleanupOutOfRangeChunks();

    // 顶点构建
    void BuildBlockFaceVertices(
        int wx, int wy, int wz, int face, int blockType,
        std::vector<BlockVertex>& out);
    void BuildChunkVertices(ChunkData* chunk, std::vector<BlockVertex>& out);
    void RebuildAllVertexData();

    // 渲染管线
    void InitBlockWorldPipeline();
    void DestroyBlockWorldPipeline();
    void DestroyBlockWorldVulkanResources();
    void RecordBlockWorldCommandBuffers();

    // 相机控制
    void UpdateCamera();

    // === 噪声生成器 ===
    FastNoiseLite* mContinentalNoise = nullptr;
    FastNoiseLite* mHillsNoise = nullptr;
    FastNoiseLite* mDetailNoise = nullptr;
    FastNoiseLite* mCaveNoise = nullptr;
    FastNoiseLite* mErosionNoise = nullptr;

    // === 区块管理 ===
    using ChunkKey = std::tuple<int, int, int>;
    std::unordered_map<ChunkKey, ChunkData*, ChunkKeyHash> mChunkMap;
    MovePlate3D<int> mChunkPlate;
    unsigned int mPlateOriginX;
    unsigned int mPlateOriginY;
    unsigned int mPlateOriginZ;

    // === 相机 ===
    glm::vec3 mCameraTarget{};
    float mCameraYaw = -45.0f;
    float mCameraPitch = -30.0f;
    float mCameraDistance = 120.0f;

    // === 输入状态 ===
    bool mIsDragging = false;
    bool mLeftMouseDown = false;
    bool mRightMouseDown = false;
    double mLastMouseX = 0.0;
    double mLastMouseY = 0.0;
    int mWinWidth = 0;
    int mWinHeight = 0;

    // === Vulkan 渲染资源 ===
    VulKan::Buffer* mVertexBuffer = nullptr;
    VulKan::DescriptorSet* mDescriptorSet = nullptr;
    VulKan::DescriptorPool* mDescriptorPool = nullptr;
    VulKan::CommandPool* mBlockCommandPool = nullptr;
    VulKan::CommandBuffer** mBlockCommandBuffers = nullptr;

    // === 顶点数据 ===
    unsigned int mVertexCount = 0;
    unsigned int mVertexCapacity = 10000000;
    bool mVertexDataDirty = false;
};

} // namespace GAME