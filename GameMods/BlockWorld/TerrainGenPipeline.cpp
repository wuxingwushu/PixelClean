#include "TerrainGenPipeline.h"
#include "BlockWorld.h"
#include "../../DebugLog.h"
#include <cstring>

namespace GAME {

// ============================================================================
// 辅助：深拷贝一个 FastNoiseLite（POD-like，直接 memcpy 安全）
// ============================================================================
static std::unique_ptr<FastNoiseLite> cloneNoise(const FastNoiseLite* src) {
    if (!src) return nullptr;
    auto dst = std::make_unique<FastNoiseLite>();
    std::memcpy(dst.get(), src, sizeof(FastNoiseLite));
    return dst;
}

// ============================================================================
// ThreadNoiseSet — 噪声实例克隆
// ============================================================================
ThreadNoiseSet ThreadNoiseSet::cloneFrom(const BlockWorld& bw) {
    ThreadNoiseSet ns;

    // === 基础噪声 ===
    ns.continentalNoise    = cloneNoise(bw.mContinentalNoise);
    ns.erosionNoise        = cloneNoise(bw.mErosionNoise);
    ns.detailNoise         = cloneNoise(bw.mDetailNoise);

    // === 密度场噪声 (3D) ===
    ns.densityNoise1       = cloneNoise(bw.mDensityNoise1);
    ns.densityNoise2       = cloneNoise(bw.mDensityNoise2);
    ns.densityNoise3       = cloneNoise(bw.mDensityNoise3);
    ns.ridgeNoise          = cloneNoise(bw.mRidgeNoise);

    // === 群系噪声 (2D) ===
    ns.temperatureNoise    = cloneNoise(bw.mTemperatureNoise);
    ns.humidityNoise       = cloneNoise(bw.mHumidityNoise);
    ns.weirdnessNoise      = cloneNoise(bw.mWeirdnessNoise);

    // === 洞穴噪声 ===
    ns.cheeseCaveNoise     = cloneNoise(bw.mCheeseCaveNoise);
    ns.spaghettiCaveNoise  = cloneNoise(bw.mSpaghettiCaveNoise);
    ns.noodleCaveNoise     = cloneNoise(bw.mNoodleCaveNoise);
    ns.caveWarpX           = cloneNoise(bw.mCaveWarpX);
    ns.caveWarpY           = cloneNoise(bw.mCaveWarpY);
    ns.caveWarpZ           = cloneNoise(bw.mCaveWarpZ);
    ns.ravineNoise         = cloneNoise(bw.mRavineNoise);

    // === 含水层噪声 ===
    ns.aquiferNoise        = cloneNoise(bw.mAquiferNoise);
    ns.aquiferBarrierNoise = cloneNoise(bw.mAquiferBarrierNoise);

    // === 河流噪声 ===
    ns.riverNoise          = cloneNoise(bw.mRiverNoise);

    return ns;
}

// ============================================================================
// JobQueue 实现
// ============================================================================
void JobQueue::push(TerrainJob job) {
    std::lock_guard<std::mutex> lock(mMutex);
    if (mQueue.size() >= MAX_QUEUE_SIZE) {
        LOGW("TerrainGenPipeline: request queue full, dropping job (%d,%d,%d)",
             job.chunkX, job.chunkY, job.chunkZ);
        return;
    }
    mQueue.push(std::move(job));
    mCV.notify_one();
}

bool JobQueue::tryPop(TerrainJob& out) {
    std::lock_guard<std::mutex> lock(mMutex);
    if (mQueue.empty()) return false;
    out = std::move(mQueue.front());
    mQueue.pop();
    return true;
}

TerrainJob JobQueue::pop() {
    std::unique_lock<std::mutex> lock(mMutex);
    mCV.wait(lock, [this] { return !mQueue.empty() || mShutdown; });
    if (mShutdown && mQueue.empty()) {
        return TerrainJob{};
    }
    TerrainJob job = std::move(mQueue.front());
    mQueue.pop();
    return job;
}

void JobQueue::shutdown() {
    std::lock_guard<std::mutex> lock(mMutex);
    mShutdown = true;
    mCV.notify_all();
}

size_t JobQueue::size() const {
    std::lock_guard<std::mutex> lock(mMutex);
    return mQueue.size();
}

// ============================================================================
// TerrainGenPipeline 实现
// ============================================================================
TerrainGenPipeline::TerrainGenPipeline(unsigned int numWorkers) {
    if (numWorkers == 0) {
        unsigned int hw = std::thread::hardware_concurrency();
        mNumWorkers = (hw > 1) ? (hw - 1) : 1;
    } else {
        mNumWorkers = numWorkers;
    }
    LOGD("TerrainGenPipeline: created with %u workers", mNumWorkers);
}

TerrainGenPipeline::~TerrainGenPipeline() {
    shutdown();
}

void TerrainGenPipeline::initialize(const BlockWorld& bw) {
    if (mInitialized) return;

    // 复制地形生成参数
    mParams = bw.mTerrainParams;

    mThreadNoises.reserve(mNumWorkers);
    for (unsigned int i = 0; i < mNumWorkers; i++) {
        mThreadNoises.push_back(ThreadNoiseSet::cloneFrom(bw));
    }

    mWorkers.reserve(mNumWorkers);
    for (unsigned int i = 0; i < mNumWorkers; i++) {
        mWorkers.emplace_back(&TerrainGenPipeline::workerLoop, this, i);
    }

    mInitialized = true;
    LOGD("TerrainGenPipeline: initialized, %u workers running", mNumWorkers);
}

void TerrainGenPipeline::requestChunk(int chunkX, int chunkY, int chunkZ) {
    if (!mInitialized) return;

    if (isChunkAlreadyHandled(chunkX, chunkY, chunkZ)) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(mInflightMutex);
        mInflightChunks.insert(ChunkKeyPack(chunkX, chunkY, chunkZ));
    }

    TerrainJob job;
    job.chunkX = chunkX;
    job.chunkY = chunkY;
    job.chunkZ = chunkZ;
    job.result = nullptr;

    mRequestQueue.push(std::move(job));
}

std::vector<std::unique_ptr<ChunkData>> TerrainGenPipeline::pollResults() {
    std::vector<std::unique_ptr<ChunkData>> results;

    TerrainJob job;
    while (mResultQueue.tryPop(job)) {
        if (job.result) {
            {
                std::lock_guard<std::mutex> lock(mInflightMutex);
                mInflightChunks.erase(ChunkKeyPack(job.chunkX, job.chunkY, job.chunkZ));
            }
            results.push_back(std::move(job.result));
        }
    }

    return results;
}

void TerrainGenPipeline::shutdown() {
    if (!mInitialized) return;

    mRequestQueue.shutdown();

    // 必须在 join 之前设置 mInitialized = false，否则 workerLoop 中
    // 检查 mInitialized 永不为 false，worker 永远不会退出，造成死锁。
    mInitialized = false;

    for (auto& t : mWorkers) {
        if (t.joinable()) {
            t.join();
        }
    }
    mWorkers.clear();

    mThreadNoises.clear();

    LOGD("TerrainGenPipeline: shutdown complete");
}

unsigned int TerrainGenPipeline::pendingCount() const {
    return (unsigned int)mRequestQueue.size();
}

unsigned int TerrainGenPipeline::activeCount() const {
    std::lock_guard<std::mutex> lock(mInflightMutex);
    return (unsigned int)mInflightChunks.size();
}

// ============================================================================
// Worker 主循环
// ============================================================================
void TerrainGenPipeline::workerLoop(unsigned int workerId) {
    LOGD("TerrainGenPipeline: worker %u started", workerId);

    while (true) {
        TerrainJob job = mRequestQueue.pop();

        if (!mInitialized) break;

        job.result = executeJob(job, workerId);

        mResultQueue.push(std::move(job));
    }

    LOGD("TerrainGenPipeline: worker %u exiting", workerId);
}

// ============================================================================
// 执行单个Chunk地形生成（与原始 OnChunkGenerate 逻辑一致）
// ============================================================================
std::unique_ptr<ChunkData> TerrainGenPipeline::executeJob(const TerrainJob& job, unsigned int workerId) {
    auto chunk = std::make_unique<ChunkData>(job.chunkX, job.chunkY, job.chunkZ);

    ThreadNoiseSet& ns = mThreadNoises[workerId];

    chunk->generateTerrain(
        mParams,
        ns.continentalNoise.get(),
        ns.erosionNoise.get(),
        ns.detailNoise.get(),
        ns.densityNoise1.get(),
        ns.densityNoise2.get(),
        ns.densityNoise3.get(),
        ns.ridgeNoise.get(),
        ns.temperatureNoise.get(),
        ns.humidityNoise.get(),
        ns.weirdnessNoise.get(),
        ns.cheeseCaveNoise.get(),
        ns.spaghettiCaveNoise.get(),
        ns.noodleCaveNoise.get(),
        ns.caveWarpX.get(),
        ns.caveWarpY.get(),
        ns.caveWarpZ.get(),
        ns.ravineNoise.get(),
        ns.aquiferNoise.get(),
        ns.aquiferBarrierNoise.get(),
        ns.riverNoise.get()
    );

    return chunk;
}

bool TerrainGenPipeline::isChunkAlreadyHandled(int cx, int cy, int cz) {
    std::lock_guard<std::mutex> lock(mInflightMutex);
    auto key = ChunkKeyPack(cx, cy, cz);
    return mInflightChunks.find(key) != mInflightChunks.end();
}

} // namespace GAME