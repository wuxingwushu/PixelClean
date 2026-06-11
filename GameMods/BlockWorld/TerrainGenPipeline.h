#pragma once

#include "../../VulkanTool/AuxiliaryVision.h"

// 顶点类型别名（必须在 ChunkData.h 之前定义，防止与 BlockWorld.h 重复定义冲突）
#ifndef BLOCK_VERTEX_ALIAS_DEFINED
#define BLOCK_VERTEX_ALIAS_DEFINED
using BlockVertex = VulKan::AuxiliaryLineSpot;
#endif

#include "ChunkData.h"
#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_set>
#include <vector>

namespace GAME {

// 前向声明
class BlockWorld;

// ============================================================================
// 每个Worker线程持有的独立噪声实例集合（17个FastNoiseLite副本）
// ============================================================================
struct ThreadNoiseSet {
    std::unique_ptr<FastNoiseLite> continentalNoise;
    std::unique_ptr<FastNoiseLite> erosionNoise;
    std::unique_ptr<FastNoiseLite> detailNoise;
    std::unique_ptr<FastNoiseLite> densityNoise1;
    std::unique_ptr<FastNoiseLite> densityNoise2;
    std::unique_ptr<FastNoiseLite> densityNoise3;
    std::unique_ptr<FastNoiseLite> ridgeNoise;
    std::unique_ptr<FastNoiseLite> temperatureNoise;
    std::unique_ptr<FastNoiseLite> humidityNoise;
    std::unique_ptr<FastNoiseLite> weirdnessNoise;
    std::unique_ptr<FastNoiseLite> cheeseCaveNoise;
    std::unique_ptr<FastNoiseLite> spaghettiCaveNoise;
    std::unique_ptr<FastNoiseLite> noodleCaveNoise;
    std::unique_ptr<FastNoiseLite> caveWarpX;
    std::unique_ptr<FastNoiseLite> caveWarpY;
    std::unique_ptr<FastNoiseLite> caveWarpZ;
    std::unique_ptr<FastNoiseLite> ravineNoise;
    std::unique_ptr<FastNoiseLite> aquiferNoise;
    std::unique_ptr<FastNoiseLite> aquiferBarrierNoise;
    std::unique_ptr<FastNoiseLite> riverNoise;

    // 从 BlockWorld 的噪声实例深拷贝一份（实现放在 .cpp 中）
    static ThreadNoiseSet cloneFrom(const BlockWorld& bw);
};

// ============================================================================
// 单个地形生成任务
// ============================================================================
struct TerrainJob {
    int chunkX = 0;
    int chunkY = 0;
    int chunkZ = 0;
    std::unique_ptr<ChunkData> result;
};

// ============================================================================
// 任务队列（线程安全，支持阻塞等待 + 非阻塞 try_pop）
// ============================================================================
class JobQueue {
public:
    void push(TerrainJob job);
    bool tryPop(TerrainJob& out);   // 非阻塞，返回 false 表示队列空
    TerrainJob pop();               // 阻塞等待
    void shutdown();                // 唤醒所有等待线程
    size_t size() const;

private:
    std::queue<TerrainJob> mQueue;
    mutable std::mutex mMutex;
    std::condition_variable mCV;
    bool mShutdown = false;

    static constexpr size_t MAX_QUEUE_SIZE = 256;
};

// ============================================================================
// 核心流水线类：异步线程池地形生成
// ============================================================================
class TerrainGenPipeline {
public:
    explicit TerrainGenPipeline(unsigned int numWorkers = 0);
    ~TerrainGenPipeline();

    TerrainGenPipeline(const TerrainGenPipeline&) = delete;
    TerrainGenPipeline& operator=(const TerrainGenPipeline&) = delete;

    // 初始化：传入 BlockWorld 引用以克隆噪声实例
    void initialize(const BlockWorld& bw);

    // 发起异步Chunk生成请求（非阻塞，立即返回）
    void requestChunk(int chunkX, int chunkY, int chunkZ);

    // 主线程每帧调用：非阻塞获取已完成的Chunk
    std::vector<std::unique_ptr<ChunkData>> pollResults();

    // 关闭流水线（析构时自动调用）
    void shutdown();

    // 状态查询
    unsigned int pendingCount() const;
    unsigned int activeCount() const;
    unsigned int workerCount() const { return mNumWorkers; }

private:
    void workerLoop(unsigned int workerId);
    std::unique_ptr<ChunkData> executeJob(const TerrainJob& job, unsigned int workerId);
    bool isChunkAlreadyHandled(int cx, int cy, int cz);

    unsigned int mNumWorkers = 0;
    bool mInitialized = false;

    JobQueue mRequestQueue;
    JobQueue mResultQueue;

    std::vector<std::thread> mWorkers;
    std::vector<ThreadNoiseSet> mThreadNoises;

    // 地形生成参数
    TerrainGenParams mParams;

    // 已请求但未完成的Chunk坐标集合（用于去重）
    std::unordered_set<int64_t> mInflightChunks;
    mutable std::mutex mInflightMutex;
};

} // namespace GAME