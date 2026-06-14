#pragma once

#include "../Configuration.h"
#include "../GameMods.h"
#include "../../Tool/MovePlate.h"
#include "../../VulkanTool/AuxiliaryVision.h"

#include "BlockVertex.h"
#include "ChunkData.h"
#include "TerrainGenPipeline.h"
#include <glm/glm.hpp>
#include <unordered_map>
#include <tuple>
#include <vector>

namespace GAME {

// 区块网格配置
constexpr unsigned int CHUNK_COUNT_X = 5;
constexpr unsigned int CHUNK_COUNT_Y = 5;
constexpr unsigned int CHUNK_COUNT_Z = 5;
constexpr unsigned int CHUNK_EDGE = ChunkData::CHUNK_SIZE;
constexpr unsigned int CHUNK_ORIGIN_X = 2;
constexpr unsigned int CHUNK_ORIGIN_Y = 2;
constexpr unsigned int CHUNK_ORIGIN_Z = 2;

// O6: ChunkKey = int64 打包三个坐标（每个坐标 21-bit，足够覆盖 ±1e6 chunk ≈ ±3e7 方块）
// 21 × 3 = 63 < 64 bits，刚好放下
inline constexpr int64_t ChunkKeyPack(int x, int y, int z) {
    // 将 x/y/z 符号扩展到 21 位并打包
    // shift to non-negative: add 1<<20, 范围 [0, 2^21-1]
    uint64_t ux = (uint64_t)(x + (1 << 20));
    uint64_t uy = (uint64_t)(y + (1 << 20));
    uint64_t uz = (uint64_t)(z + (1 << 20));
    return (int64_t)((ux << 42) | (uy << 21) | uz);
}

inline constexpr void ChunkKeyUnpack(int64_t key, int& x, int& y, int& z) {
    uint64_t u = (uint64_t)key;
    uint64_t ux = (u >> 42) & ((1ULL << 21) - 1);
    uint64_t uy = (u >> 21) & ((1ULL << 21) - 1);
    uint64_t uz = u & ((1ULL << 21) - 1);
    x = (int)ux - (1 << 20);
    y = (int)uy - (1 << 20);
    z = (int)uz - (1 << 20);
}

struct ChunkKeyHash {
    size_t operator()(int64_t key) const {
        // 直接使用打包好的 int64 作为哈希值，已经混合了三个坐标
        return (size_t)key;
    }
};

// 顶点构建期间的快速邻居查询表 (5x5x5)
struct ChunkFlatLookup {
    static constexpr int CX = (int)CHUNK_COUNT_X;  // 5
    static constexpr int CY = (int)CHUNK_COUNT_Y;  // 5
    static constexpr int CZ = (int)CHUNK_COUNT_Z;  // 5
    static constexpr int CS = (int)ChunkData::CHUNK_SIZE;  // 32

    ChunkData* grid[CX][CY][CZ]{};
    int originWorldX = 0;
    int originWorldY = 0;
    int originWorldZ = 0;

    void build(
        const std::unordered_map<int64_t, ChunkData*, ChunkKeyHash>& chunkMap,
        int platePosX, int platePosY, int platePosZ,
        unsigned int plateOriginX, unsigned int plateOriginY, unsigned int plateOriginZ);

    inline int getBlockAt(int wx, int wy, int wz) const {
        int cx = wx / CS;
        int cy = wy / CS;
        int cz = wz / CS;

        if (wx < 0) cx = (wx - CS + 1) / CS;
        if (wy < 0) cy = (wy - CS + 1) / CS;
        if (wz < 0) cz = (wz - CS + 1) / CS;

        int ix = cx - originWorldX;
        int iy = cy - originWorldY;
        int iz = cz - originWorldZ;

        if (ix < 0 || ix >= CX || iy < 0 || iy >= CY || iz < 0 || iz >= CZ)
            return (int)BlockType::Air;

        ChunkData* chunk = grid[ix][iy][iz];
        if (!chunk) return (int)BlockType::Air;

        int lx = wx - cx * CS;
        int ly = wy - cy * CS;
        int lz = wz - cz * CS;
        return chunk->get(lx, ly, lz);
    }
};

class BlockWorld : public GameMods, Configuration {
    friend struct ThreadNoiseSet;  // 允许 TerrainGenPipeline 克隆噪声实例
public:
    explicit BlockWorld(Configuration wConfiguration);
    ~BlockWorld();

    // 地形生成参数（定义在 ChunkData.h，可调参数集中管理）
    TerrainGenParams mTerrainParams;

    // 配置噪声实例从参数
    void SetupAllNoiseFromParams();
    // 重新生成整个地图（修改参数后调用）
    void RegenerateAllChunks();

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

    // 异步流水线状态（供 UI 展示）
    unsigned int GetPendingChunks() const;
    unsigned int GetActiveWorkers() const;

private:
    // 静态回调
    static void OnChunkGenerate(int mT, int x, int y, int z, void* Data);
    static void OnChunkDelete(int mT, void* Data);

    // 区块管理
    int GetBlockAtWorld(int wx, int wy, int wz);
    void CleanupOutOfRangeChunks();

    // 区块管理（增量顶点更新）
    void MarkChunkDirty(ChunkData* chunk);
    void MarkChunkDirty(int cx, int cy, int cz);
    void InvalidateAllChunkVertices();

    // 顶点构建
    void BuildBlockFaceVertices(
        int wx, int wy, int wz, int face, int blockType,
        std::vector<BlockVertex>& out);
    void BuildChunkVertices(ChunkData* chunk, std::vector<BlockVertex>& out);
    void BuildChunkVertices(ChunkData* chunk, std::vector<BlockVertex>& out,
                            const ChunkFlatLookup& lookup);  // O3: 快速版
    void RebuildAllVertexData();

    // 渲染管线
    void InitBlockWorldPipeline();
    void DestroyBlockWorldPipeline();
    void DestroyBlockWorldVulkanResources();
    void RecordBlockWorldCommandBuffers();

    // 相机控制
    void UpdateCamera();

    // === 基础噪声 (2D) ===
    FastNoiseLite* mContinentalNoise = nullptr;   // 大陆度
    FastNoiseLite* mErosionNoise = nullptr;       // 侵蚀度
    FastNoiseLite* mDetailNoise = nullptr;        // 细节/扰动 (3D)

    // === 密度场噪声 (3D) ===
    FastNoiseLite* mDensityNoise1 = nullptr;
    FastNoiseLite* mDensityNoise2 = nullptr;
    FastNoiseLite* mDensityNoise3 = nullptr;
    FastNoiseLite* mRidgeNoise = nullptr;          // 山脊 (2D)

    // === 群系噪声 (2D) ===
    FastNoiseLite* mTemperatureNoise = nullptr;
    FastNoiseLite* mHumidityNoise = nullptr;
    FastNoiseLite* mWeirdnessNoise = nullptr;

    // === 洞穴噪声 ===
    FastNoiseLite* mCheeseCaveNoise = nullptr;     // 奶酪洞穴 (3D)
    FastNoiseLite* mSpaghettiCaveNoise = nullptr;  // 面条洞穴 (3D)
    FastNoiseLite* mNoodleCaveNoise = nullptr;     // 细面条洞穴 (3D)
    FastNoiseLite* mCaveWarpX = nullptr;           // 域扭曲 X (3D)
    FastNoiseLite* mCaveWarpY = nullptr;           // 域扭曲 Y (3D)
    FastNoiseLite* mCaveWarpZ = nullptr;           // 域扭曲 Z (3D)
    FastNoiseLite* mRavineNoise = nullptr;         // 裂谷 (2D)

    // === 含水层噪声 ===
    FastNoiseLite* mAquiferNoise = nullptr;        // 含水层 (3D)
    FastNoiseLite* mAquiferBarrierNoise = nullptr; // 隔水层 (3D)

    // === 河流噪声 ===
    FastNoiseLite* mRiverNoise = nullptr;          // 河流 (2D)

    // === 区块管理 ===
    using ChunkKey = int64_t;  // O6: 打包坐标 int64 替代 std::tuple
    std::unordered_map<ChunkKey, ChunkData*, ChunkKeyHash> mChunkMap;
    MovePlate3D<int> mChunkPlate;
    unsigned int mPlateOriginX;
    unsigned int mPlateOriginY;
    unsigned int mPlateOriginZ;

    // === 异步地形生成流水线（方案A+C融合） ===
    std::unique_ptr<TerrainGenPipeline> mTerrainPipeline;

    // === 相机（自由飞行模式）===
    float mCameraYaw = -45.0f;
    float mCameraPitch = -30.0f;
    float mCameraSpeed = 20.0f;       // 移动速度（滚轮调节）

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
    unsigned int mVertexCapacity = 30000000;  // 预分配足够容量以减少初始加载时的重新分配
    bool mVertexDataDirty = false;
};

} // namespace GAME