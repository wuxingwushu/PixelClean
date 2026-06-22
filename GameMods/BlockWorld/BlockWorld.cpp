#include "BlockWorld.h"
#include "ChunkData.h"
#include "../../DebugLog.h"
#include "../../GlobalVariable.h"
#include "../../Vulkan/descriptorSet.h"
#include "../../Vulkan/descriptorPool.h"
#include "../../Vulkan/commandBuffer.h"
#include <cmath>
#include <algorithm>
#include <chrono>

namespace GAME {

// ============================================================================
// 静态回调
// ============================================================================

void BlockWorld::OnChunkGenerate(int /*mT*/, int x, int y, int z, void* Data) {
    BlockWorld* self = (BlockWorld*)Data;
    if (!self) return;

    // 检查是否已存在（快速路径，避免无谓的队列操作）
    auto key = ChunkKeyPack(x, y, z);
    if (self->mChunkMap.find(key) != self->mChunkMap.end()) {
        return;
    }

    // 异步生成：请求入队后立即返回，不阻塞主线程
    if (self->mTerrainPipeline) {
        self->mTerrainPipeline->requestChunk(x, y, z);
    } else {
        // 回退：如果 Pipeline 未初始化，使用旧的同步方式
        ChunkData* chunk = new ChunkData(x, y, z);
        chunk->generateTerrain(
            self->mTerrainParams,
            self->mContinentalNoise, self->mErosionNoise, self->mDetailNoise,
            self->mDensityNoise1, self->mDensityNoise2, self->mDensityNoise3, self->mRidgeNoise,
            self->mTemperatureNoise, self->mHumidityNoise, self->mWeirdnessNoise,
            self->mCheeseCaveNoise, self->mSpaghettiCaveNoise, self->mNoodleCaveNoise,
            self->mCaveWarpX, self->mCaveWarpY, self->mCaveWarpZ, self->mRavineNoise,
            self->mAquiferNoise, self->mAquiferBarrierNoise,
            self->mRiverNoise);
        self->mChunkMap[key] = chunk;
        self->MarkChunkDirty(x, y, z);
        LOGD("BlockWorld: Chunk generated (sync fallback) at (%d, %d, %d)", x, y, z);
    }
}

void BlockWorld::OnChunkDelete(int /*mT*/, void* Data) {
    (void)Data;
    // MovePlate3D 回调不提供坐标，清理在 CleanupOutOfRangeChunks 中处理
}

// ============================================================================
// 辅助函数
// ============================================================================

int BlockWorld::GetBlockAtWorld(int wx, int wy, int wz) {
    int chunkX = wx / (int)ChunkData::CHUNK_SIZE;
    int chunkY = wy / (int)ChunkData::CHUNK_SIZE;
    int chunkZ = wz / (int)ChunkData::CHUNK_SIZE;

    if (wx < 0) {
        chunkX = (wx - ChunkData::CHUNK_SIZE + 1) / (int)ChunkData::CHUNK_SIZE;
    }
    if (wy < 0) {
        chunkY = (wy - ChunkData::CHUNK_SIZE + 1) / (int)ChunkData::CHUNK_SIZE;
    }
    if (wz < 0) {
        chunkZ = (wz - ChunkData::CHUNK_SIZE + 1) / (int)ChunkData::CHUNK_SIZE;
    }

    auto it = mChunkMap.find(ChunkKeyPack(chunkX, chunkY, chunkZ));
    if (it == mChunkMap.end()) {
        return 0;
    }

    int localX = wx - chunkX * ChunkData::CHUNK_SIZE;
    int localY = wy - chunkY * ChunkData::CHUNK_SIZE;
    int localZ = wz - chunkZ * ChunkData::CHUNK_SIZE;

    return it->second->get(localX, localY, localZ);
}

// ============================================================================
// ChunkFlatLookup 实现 (O3: 快速邻居查询)
// ============================================================================

void ChunkFlatLookup::build(
    const std::unordered_map<int64_t, ChunkData*, ChunkKeyHash>& chunkMap,
    int platePosX, int platePosY, int platePosZ,
    unsigned int plateOriginX, unsigned int plateOriginY, unsigned int plateOriginZ)
{
    std::memset(grid, 0, sizeof(grid));

    originWorldX = platePosX - (int)plateOriginX;
    originWorldY = platePosY - (int)plateOriginY;
    originWorldZ = platePosZ - (int)plateOriginZ;

    for (auto& pair : chunkMap) {
        int cx, cy, cz;
        ChunkKeyUnpack(pair.first, cx, cy, cz);

        int ix = cx - originWorldX;
        int iy = cy - originWorldY;
        int iz = cz - originWorldZ;

        if (ix >= 0 && ix < CX && iy >= 0 && iy < CY && iz >= 0 && iz < CZ) {
            grid[ix][iy][iz] = pair.second;
        }
    }
}

// ============================================================================
// 区块脏标记（增量顶点更新 — 方案 B + C）
// ============================================================================

void BlockWorld::MarkChunkDirty(ChunkData* chunk) {
    if (!chunk) return;
    MarkChunkDirty(chunk->worldX, chunk->worldY, chunk->worldZ);
}

void BlockWorld::MarkChunkDirty(int cx, int cy, int cz) {
    static const int neighbors[7][3] = {
        { 0,  0,  0},   // 自身
        {+1,  0,  0}, {-1,  0,  0},  // ±X
        { 0, +1,  0}, { 0, -1,  0},  // ±Y
        { 0,  0, +1}, { 0,  0, -1},  // ±Z
    };

    for (int i = 0; i < 7; i++) {
        int nx = cx + neighbors[i][0];
        int ny = cy + neighbors[i][1];
        int nz = cz + neighbors[i][2];
        auto it = mChunkMap.find(ChunkKeyPack(nx, ny, nz));
        if (it != mChunkMap.end()) {
            it->second->vertexDirty = true;
        }
    }
    mVertexDataDirty = true;
}

void BlockWorld::InvalidateAllChunkVertices() {
    for (auto& pair : mChunkMap) {
        pair.second->vertexDirty = true;
    }
    mVertexDataDirty = true;
}

void BlockWorld::CleanupOutOfRangeChunks() {
    std::vector<ChunkKey> toRemove;
    int platePosX = mChunkPlate.GetPosX();
    int platePosY = mChunkPlate.GetPosY();
    int platePosZ = mChunkPlate.GetPosZ();

    for (auto& pair : mChunkMap) {
        int cx, cy, cz;
        ChunkKeyUnpack(pair.first, cx, cy, cz);

        int relX = cx - platePosX + (int)mPlateOriginX;
        int relY = cy - platePosY + (int)mPlateOriginY;
        int relZ = cz - platePosZ + (int)mPlateOriginZ;

        if (relX < 0 || relX >= (int)CHUNK_COUNT_X ||
            relY < 0 || relY >= (int)CHUNK_COUNT_Y ||
            relZ < 0 || relZ >= (int)CHUNK_COUNT_Z) {
            toRemove.push_back(pair.first);
        }
    }

    if (!toRemove.empty()) {
        // 先标记脏：被删区块的邻居需要重建边界顶点
        for (auto& key : toRemove) {
            int kx, ky, kz;
            ChunkKeyUnpack(key, kx, ky, kz);
            MarkChunkDirty(kx, ky, kz);
        }
        // 再删除
        for (auto& key : toRemove) {
            delete mChunkMap[key];
            mChunkMap.erase(key);
            int kx, ky, kz;
            ChunkKeyUnpack(key, kx, ky, kz);
            LOGD("BlockWorld: Chunk removed at (%d, %d, %d)", kx, ky, kz);
        }
        // mVertexDataDirty 已在 MarkChunkDirty 中设置
    }
}

// 根据方块类型获取颜色
static glm::vec4 getBlockColor(int blockType) {
    switch ((BlockType)blockType) {
    case BlockType::Grass:
        return {0.2f, 0.6f, 0.1f, 1.0f};
    case BlockType::Dirt:
        return {0.5f, 0.35f, 0.1f, 1.0f};
    case BlockType::Stone:
        return {0.5f, 0.5f, 0.5f, 1.0f};
    case BlockType::DeepStone:
        return {0.25f, 0.25f, 0.3f, 1.0f};
    case BlockType::Bedrock:
        return {0.1f, 0.1f, 0.1f, 1.0f};
    case BlockType::Sand:
        return {0.8f, 0.8f, 0.5f, 1.0f};
    case BlockType::Water:
        return {0.2f, 0.3f, 0.8f, 0.5f};
    case BlockType::Snow:
        return {0.9f, 0.95f, 1.0f, 1.0f};
    case BlockType::Log:
        return {0.4f, 0.25f, 0.1f, 1.0f};   // 棕色树干
    case BlockType::Leaves:
        return {0.15f, 0.5f, 0.1f, 1.0f};    // 绿色树叶
    case BlockType::Cactus:
        return {0.2f, 0.55f, 0.15f, 1.0f};   // 深绿色仙人掌
    case BlockType::Air:
    default:
        return {0, 0, 0, 0};
    }
}

// 构建单个方块面的顶点（6个顶点 = 两个三角形）
void BlockWorld::BuildBlockFaceVertices(
    int wx, int wy, int wz, int face, int blockType,
    std::vector<BlockVertex>& out)
{
    glm::vec4 color = getBlockColor(blockType);

    float x = (float)wx;
    float y = (float)wy;
    float z = (float)wz;

    glm::vec3 v0, v1, v2, v3;
    glm::vec3 normal;

    switch (face) {
        case 0: // 前面 (Z+) - 从外向里看顺时针 (CW)
            v0 = {x, y, z + 1}; v1 = {x, y + 1, z + 1};
            v2 = {x + 1, y + 1, z + 1}; v3 = {x + 1, y, z + 1};
            normal = {0.0f, 0.0f, 1.0f};
            break;
        case 1: // 后面 (Z-) - 从外向里看顺时针 (CW)
            v0 = {x, y, z}; v1 = {x + 1, y, z};
            v2 = {x + 1, y + 1, z}; v3 = {x, y + 1, z};
            normal = {0.0f, 0.0f, -1.0f};
            break;
        case 2: // 左面 (X-) - 从外向里看顺时针 (CW)
            v0 = {x, y, z}; v1 = {x, y + 1, z};
            v2 = {x, y + 1, z + 1}; v3 = {x, y, z + 1};
            normal = {-1.0f, 0.0f, 0.0f};
            break;
        case 3: // 右面 (X+) - 从外向里看顺时针 (CW)
            v0 = {x + 1, y, z + 1}; v1 = {x + 1, y + 1, z + 1};
            v2 = {x + 1, y + 1, z}; v3 = {x + 1, y, z};
            normal = {1.0f, 0.0f, 0.0f};
            break;
        case 4: // 下面 (Y-) - 从外向里看顺时针 (CW)
            v0 = {x, y, z}; v1 = {x, y, z + 1};
            v2 = {x + 1, y, z + 1}; v3 = {x + 1, y, z};
            normal = {0.0f, -1.0f, 0.0f};
            break;
        case 5: // 上面 (Y+) - 从外向里看顺时针 (CW)
            v0 = {x + 1, y + 1, z + 1}; v1 = {x, y + 1, z + 1};
            v2 = {x, y + 1, z}; v3 = {x + 1, y + 1, z};
            normal = {0.0f, 1.0f, 0.0f};
            break;
        default:
            return;
    }

    out.push_back({v0, color, normal});
    out.push_back({v1, color, normal});
    out.push_back({v2, color, normal});
    out.push_back({v0, color, normal});
    out.push_back({v2, color, normal});
    out.push_back({v3, color, normal});
}

void BlockWorld::BuildChunkVertices(ChunkData* chunk, std::vector<BlockVertex>& out) {
    if (!chunk || !chunk->generated) return;

    const int CS = ChunkData::CHUNK_SIZE;
    int baseX = chunk->worldX * CS;
    int baseY = chunk->worldY * CS;
    int baseZ = chunk->worldZ * CS;

    for (int x = 0; x < CS; x++) {
        for (int y = 0; y < CS; y++) {
            for (int z = 0; z < CS; z++) {
                int blockType = chunk->get(x, y, z);
                if (blockType == (int)BlockType::Air) continue;

                int wx = baseX + x;
                int wy = baseY + y;
                int wz = baseZ + z;

                bool isWater = (blockType == (int)BlockType::Water);

                // 水方块：只在相邻是 Air 时渲染面
                // 固体方块：相邻非固体且世界邻居为 Air 时渲染
                auto shouldRender = [&](int lx, int ly, int lz, int wnx, int wny, int wnz) -> bool {
                    if (isWater) {
                        return GetBlockAtWorld(wnx, wny, wnz) == (int)BlockType::Air;
                    } else {
                        return !chunk->isSolid(lx, ly, lz) &&
                               GetBlockAtWorld(wnx, wny, wnz) == (int)BlockType::Air;
                    }
                };

                if (shouldRender(x, y, z + 1, wx, wy, wz + 1))
                    BuildBlockFaceVertices(wx, wy, wz, 0, blockType, out);
                if (shouldRender(x, y, z - 1, wx, wy, wz - 1))
                    BuildBlockFaceVertices(wx, wy, wz, 1, blockType, out);
                if (shouldRender(x - 1, y, z, wx - 1, wy, wz))
                    BuildBlockFaceVertices(wx, wy, wz, 2, blockType, out);
                if (shouldRender(x + 1, y, z, wx + 1, wy, wz))
                    BuildBlockFaceVertices(wx, wy, wz, 3, blockType, out);
                if (shouldRender(x, y - 1, z, wx, wy - 1, wz))
                    BuildBlockFaceVertices(wx, wy, wz, 4, blockType, out);
                if (shouldRender(x, y + 1, z, wx, wy + 1, wz))
                    BuildBlockFaceVertices(wx, wy, wz, 5, blockType, out);
            }
        }
    }
}

// O3: 快速版 BuildChunkVertices — 使用 ChunkFlatLookup 替代 GetBlockAtWorld
void BlockWorld::BuildChunkVertices(ChunkData* chunk, std::vector<BlockVertex>& out,
                                    const ChunkFlatLookup& lookup) {
    if (!chunk || !chunk->generated) return;

    const int CS = ChunkData::CHUNK_SIZE;
    int baseX = chunk->worldX * CS;
    int baseY = chunk->worldY * CS;
    int baseZ = chunk->worldZ * CS;

    for (int x = 0; x < CS; x++) {
        for (int y = 0; y < CS; y++) {
            for (int z = 0; z < CS; z++) {
                int blockType = chunk->get(x, y, z);
                if (blockType == (int)BlockType::Air) continue;

                int wx = baseX + x;
                int wy = baseY + y;
                int wz = baseZ + z;

                bool isWater = (blockType == (int)BlockType::Water);

                // 使用 ChunkFlatLookup 替代 GetBlockAtWorld
                auto shouldRender = [&](int lx, int ly, int lz, int wnx, int wny, int wnz) -> bool {
                    if (isWater) {
                        return lookup.getBlockAt(wnx, wny, wnz) == (int)BlockType::Air;
                    } else {
                        return !chunk->isSolid(lx, ly, lz) &&
                               lookup.getBlockAt(wnx, wny, wnz) == (int)BlockType::Air;
                    }
                };

                if (shouldRender(x, y, z + 1, wx, wy, wz + 1))
                    BuildBlockFaceVertices(wx, wy, wz, 0, blockType, out);
                if (shouldRender(x, y, z - 1, wx, wy, wz - 1))
                    BuildBlockFaceVertices(wx, wy, wz, 1, blockType, out);
                if (shouldRender(x - 1, y, z, wx - 1, wy, wz))
                    BuildBlockFaceVertices(wx, wy, wz, 2, blockType, out);
                if (shouldRender(x + 1, y, z, wx + 1, wy, wz))
                    BuildBlockFaceVertices(wx, wy, wz, 3, blockType, out);
                if (shouldRender(x, y - 1, z, wx, wy - 1, wz))
                    BuildBlockFaceVertices(wx, wy, wz, 4, blockType, out);
                if (shouldRender(x, y + 1, z, wx, wy + 1, wz))
                    BuildBlockFaceVertices(wx, wy, wz, 5, blockType, out);
            }
        }
    }
}

void BlockWorld::RebuildAllVertexData() {
    LOGD("BlockWorld: RebuildAllVertexData (incremental), chunks=%zu", mChunkMap.size());

    auto start = std::chrono::high_resolution_clock::now();

    // ====== 阶段 0: 构建快速查找表（O(N), N=125） ======
    ChunkFlatLookup chunkLookup;
    chunkLookup.build(mChunkMap,
                      mChunkPlate.GetPosX(), mChunkPlate.GetPosY(), mChunkPlate.GetPosZ(),
                      mPlateOriginX, mPlateOriginY, mPlateOriginZ);

    // ====== 阶段 1: 增量重建脏区块 ======
    unsigned int dirtyCount = 0;
    unsigned int totalVertexCount = 0;

    for (auto& pair : mChunkMap) {
        ChunkData* chunk = pair.second;

        if (chunk->vertexDirty) {
            dirtyCount++;
            // 重建顶点到该区块的独立缓存（使用快速 lookup）
            chunk->cachedVertices.clear();
            BuildChunkVertices(chunk, chunk->cachedVertices, chunkLookup);
            chunk->vertexDirty = false;
        }

        // 累加总顶点数（用于容量检查和偏移计算）
        totalVertexCount += (unsigned int)chunk->cachedVertices.size();
    }

    LOGD("BlockWorld: %u/%zu chunks rebuilt (incremental)", dirtyCount, mChunkMap.size());

    // ====== 阶段 2: 容量检查与扩容 ======
    if (totalVertexCount > mVertexCapacity) {
        LOGW("BlockWorld: vertex count exceeds capacity (%u > %u), expanding",
             totalVertexCount, mVertexCapacity);
        mVertexCapacity = totalVertexCount * 2 + 1000000;
        DestroyBlockWorldVulkanResources();

        // 手动重新分配资源
        mVertexBuffer = new VulKan::Buffer(
            mDevice,
            mVertexCapacity * sizeof(BlockVertex),
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        );

        VulKan::UniformParameter vpParam = {};
        vpParam.mBinding = 0;
        vpParam.mCount = 1;
        vpParam.mDescriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        vpParam.mSize = sizeof(VPMatrices);
        vpParam.mStage = VK_SHADER_STAGE_VERTEX_BIT;
        vpParam.mBuffers = mCameraVPMatricesBuffer;
        std::vector<VulKan::UniformParameter*> params;
        params.push_back(&vpParam);

        mDescriptorPool = new VulKan::DescriptorPool(mDevice);
        mDescriptorPool->build(params, (int)mSwapChain->getImageCount(), 1);

        VulKan::Pipeline* blockPipeline = mPipelineS->GetPipeline(VulKan::PipelineMods::BlockWorldMods);
        mDescriptorSet = new VulKan::DescriptorSet(
            mDevice, params, blockPipeline->DescriptorSetLayout,
            mDescriptorPool, (int)mSwapChain->getImageCount());

        mBlockCommandPool = new VulKan::CommandPool(mDevice);
        mBlockCommandBuffers = new VulKan::CommandBuffer*[mSwapChain->getImageCount()];
        for (size_t i = 0; i < mSwapChain->getImageCount(); ++i) {
            mBlockCommandBuffers[i] = new VulKan::CommandBuffer(mDevice, mBlockCommandPool, true);
        }
    }

    // ====== 阶段 3: 拼接所有区块缓存为单一顶点数组 ======
    if (totalVertexCount == 0) {
        mVertexCount = 0;
        mVertexDataDirty = false;
        return;
    }

    std::vector<BlockVertex> merged;
    merged.reserve(totalVertexCount);

    for (auto& pair : mChunkMap) {
        ChunkData* chunk = pair.second;
        if (!chunk->cachedVertices.empty()) {
            chunk->cachedVertexOffset = (unsigned int)merged.size();
            merged.insert(merged.end(),
                          chunk->cachedVertices.begin(),
                          chunk->cachedVertices.end());
        } else {
            chunk->cachedVertexOffset = 0;
        }
    }

    mVertexCount = (unsigned int)merged.size();

    if (mVertexBuffer) {
        mVertexBuffer->updateBufferByStage(merged.data(),
            merged.size() * sizeof(BlockVertex));
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    LOGD("BlockWorld: Incremental rebuild done in %lld ms (dirty=%u, vertices=%u)",
         (long long)duration.count(), dirtyCount, mVertexCount);

    mVertexDataDirty = false;
}

// ============================================================================
// 噪声参数配置
// ============================================================================

void BlockWorld::SetupAllNoiseFromParams() {
    const auto& p = mTerrainParams;

    // === 基础噪声 ===
    mContinentalNoise->SetSeed(p.seed);
    mContinentalNoise->SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    mContinentalNoise->SetFractalType(FastNoiseLite::FractalType_FBm);
    mContinentalNoise->SetFractalOctaves(p.continentalOctaves);
    mContinentalNoise->SetFrequency(p.continentalFrequency);

    mErosionNoise->SetSeed(p.seed + 1);
    mErosionNoise->SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    mErosionNoise->SetFrequency(p.erosionFrequency);

    mDetailNoise->SetSeed(p.seed + 2);
    mDetailNoise->SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    mDetailNoise->SetFrequency(p.detailFrequency);

    // === 密度场噪声 (3D) ===
    mDensityNoise1->SetSeed(p.seed + 10);
    mDensityNoise1->SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    mDensityNoise1->SetFractalType(FastNoiseLite::FractalType_FBm);
    mDensityNoise1->SetFractalOctaves(p.density1Octaves);
    mDensityNoise1->SetFrequency(p.density1Frequency);

    mDensityNoise2->SetSeed(p.seed + 11);
    mDensityNoise2->SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    mDensityNoise2->SetFractalType(FastNoiseLite::FractalType_FBm);
    mDensityNoise2->SetFractalOctaves(p.density2Octaves);
    mDensityNoise2->SetFrequency(p.density2Frequency);

    mDensityNoise3->SetSeed(p.seed + 12);
    mDensityNoise3->SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    mDensityNoise3->SetFrequency(p.density3Frequency);

    mRidgeNoise->SetSeed(p.seed + 13);
    mRidgeNoise->SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    mRidgeNoise->SetFractalType(FastNoiseLite::FractalType_Ridged);
    mRidgeNoise->SetFractalOctaves(p.ridgeOctaves);
    mRidgeNoise->SetFrequency(p.ridgeFrequency);

    // === 群系噪声 (2D) ===
    mTemperatureNoise->SetSeed(p.seed + 20);
    mTemperatureNoise->SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    mTemperatureNoise->SetFractalType(FastNoiseLite::FractalType_FBm);
    mTemperatureNoise->SetFractalOctaves(p.temperatureOctaves);
    mTemperatureNoise->SetFrequency(p.temperatureFrequency);

    mHumidityNoise->SetSeed(p.seed + 21);
    mHumidityNoise->SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    mHumidityNoise->SetFractalType(FastNoiseLite::FractalType_FBm);
    mHumidityNoise->SetFractalOctaves(p.humidityOctaves);
    mHumidityNoise->SetFrequency(p.humidityFrequency);

    mWeirdnessNoise->SetSeed(p.seed + 22);
    mWeirdnessNoise->SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    mWeirdnessNoise->SetFrequency(p.weirdnessFrequency);

    // === 洞穴噪声 ===
    mCheeseCaveNoise->SetSeed(p.seed + 30);
    mCheeseCaveNoise->SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    mCheeseCaveNoise->SetFractalType(FastNoiseLite::FractalType_FBm);
    mCheeseCaveNoise->SetFractalOctaves(p.cheeseCaveOctaves);
    mCheeseCaveNoise->SetFrequency(p.cheeseCaveFrequency);

    mSpaghettiCaveNoise->SetSeed(p.seed + 31);
    mSpaghettiCaveNoise->SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    mSpaghettiCaveNoise->SetFractalType(FastNoiseLite::FractalType_FBm);
    mSpaghettiCaveNoise->SetFractalOctaves(p.spaghettiCaveOctaves);
    mSpaghettiCaveNoise->SetFrequency(p.spaghettiCaveFrequency);

    mNoodleCaveNoise->SetSeed(p.seed + 32);
    mNoodleCaveNoise->SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    mNoodleCaveNoise->SetFrequency(p.noodleCaveFrequency);

    mCaveWarpX->SetSeed(p.seed + 33);
    mCaveWarpX->SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    mCaveWarpX->SetFrequency(p.caveWarpFrequency);

    mCaveWarpY->SetSeed(p.seed + 34);
    mCaveWarpY->SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    mCaveWarpY->SetFrequency(p.caveWarpFrequency);

    mCaveWarpZ->SetSeed(p.seed + 35);
    mCaveWarpZ->SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    mCaveWarpZ->SetFrequency(p.caveWarpFrequency);

    mRavineNoise->SetSeed(p.seed + 36);
    mRavineNoise->SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    mRavineNoise->SetFrequency(p.ravineFrequency);

    // === 含水层噪声 ===
    mAquiferNoise->SetSeed(p.seed + 40);
    mAquiferNoise->SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    mAquiferNoise->SetFractalType(FastNoiseLite::FractalType_FBm);
    mAquiferNoise->SetFractalOctaves(p.aquiferOctaves);
    mAquiferNoise->SetFrequency(p.aquiferFrequency);

    mAquiferBarrierNoise->SetSeed(p.seed + 41);
    mAquiferBarrierNoise->SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    mAquiferBarrierNoise->SetFrequency(p.aquiferBarrierFrequency);

    // === 河流噪声 ===
    mRiverNoise->SetSeed(p.seed + 50);
    mRiverNoise->SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    mRiverNoise->SetFractalType(FastNoiseLite::FractalType_FBm);
    mRiverNoise->SetFractalOctaves(p.riverOctaves);
    mRiverNoise->SetFrequency(p.riverFrequency);
}

void BlockWorld::RegenerateAllChunks() {
    LOGD("BlockWorld: RegenerateAllChunks");

    // 1. 等待 GPU 完成所有操作
    vkDeviceWaitIdle(mDevice->getDevice());

    // 2. 关闭旧流水线（等待所有 worker 线程退出）
    if (mTerrainPipeline) {
        mTerrainPipeline->shutdown();
        mTerrainPipeline.reset();
    }

    // 3. 清空所有区块数据
    for (auto& pair : mChunkMap) {
        delete pair.second;
    }
    mChunkMap.clear();
    mVertexCount = 0;
    mVertexDataDirty = true;

    // 4. 重新配置噪声实例
    SetupAllNoiseFromParams();

    // 5. 创建新流水线并初始化
    mTerrainPipeline = std::make_unique<TerrainGenPipeline>(0);
    mTerrainPipeline->initialize(*this);

    // 6. 重新请求所有区块
    int platePosX = mChunkPlate.GetPosX();
    int platePosY = mChunkPlate.GetPosY();
    int platePosZ = mChunkPlate.GetPosZ();
    for (unsigned int x = 0; x < CHUNK_COUNT_X; x++) {
        for (unsigned int y = 0; y < CHUNK_COUNT_Y; y++) {
            for (unsigned int z = 0; z < CHUNK_COUNT_Z; z++) {
                int wx = (int)x + platePosX - (int)mPlateOriginX;
                int wy = (int)y + platePosY - (int)mPlateOriginY;
                int wz = (int)z + platePosZ - (int)mPlateOriginZ;
                mTerrainPipeline->requestChunk(wx, wy, wz);
            }
        }
    }

    // 7. 重新录制指令缓冲
    RecordBlockWorldCommandBuffers();

    LOGD("BlockWorld: RegenerateAllChunks done, %zu chunks requested", 
         CHUNK_COUNT_X * CHUNK_COUNT_Y * CHUNK_COUNT_Z);
}

// ============================================================================
// 渲染管线初始化
// ============================================================================

void BlockWorld::InitBlockWorldPipeline() {
    LOGD("BlockWorld: InitBlockWorldPipeline, capacity=%u", mVertexCapacity);

    // === 创建所有噪声实例并配置 ===
    mContinentalNoise = new FastNoiseLite();
    mErosionNoise = new FastNoiseLite();
    mDetailNoise = new FastNoiseLite();
    mDensityNoise1 = new FastNoiseLite();
    mDensityNoise2 = new FastNoiseLite();
    mDensityNoise3 = new FastNoiseLite();
    mRidgeNoise = new FastNoiseLite();
    mTemperatureNoise = new FastNoiseLite();
    mHumidityNoise = new FastNoiseLite();
    mWeirdnessNoise = new FastNoiseLite();
    mCheeseCaveNoise = new FastNoiseLite();
    mSpaghettiCaveNoise = new FastNoiseLite();
    mNoodleCaveNoise = new FastNoiseLite();
    mCaveWarpX = new FastNoiseLite();
    mCaveWarpY = new FastNoiseLite();
    mCaveWarpZ = new FastNoiseLite();
    mRavineNoise = new FastNoiseLite();
    mAquiferNoise = new FastNoiseLite();
    mAquiferBarrierNoise = new FastNoiseLite();
    mRiverNoise = new FastNoiseLite();

    SetupAllNoiseFromParams();

    // === Vulkan 资源 ===
    mVertexBuffer = new VulKan::Buffer(
        mDevice,
        mVertexCapacity * sizeof(BlockVertex),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );

    VulKan::UniformParameter vpParam = {};
    vpParam.mBinding = 0;
    vpParam.mCount = 1;
    vpParam.mDescriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    vpParam.mSize = sizeof(VPMatrices);
    vpParam.mStage = VK_SHADER_STAGE_VERTEX_BIT;
    vpParam.mBuffers = mCameraVPMatricesBuffer;
    std::vector<VulKan::UniformParameter*> params;
    params.push_back(&vpParam);

    mDescriptorPool = new VulKan::DescriptorPool(mDevice);
    mDescriptorPool->build(params, (int)mSwapChain->getImageCount(), 1);

    VulKan::Pipeline* blockPipeline = mPipelineS->GetPipeline(VulKan::PipelineMods::BlockWorldMods);
    mDescriptorSet = new VulKan::DescriptorSet(
        mDevice, params, blockPipeline->DescriptorSetLayout,
        mDescriptorPool, (int)mSwapChain->getImageCount());

    mBlockCommandPool = new VulKan::CommandPool(mDevice);
    mBlockCommandBuffers = new VulKan::CommandBuffer*[mSwapChain->getImageCount()];
    for (size_t i = 0; i < mSwapChain->getImageCount(); ++i) {
        mBlockCommandBuffers[i] = new VulKan::CommandBuffer(mDevice, mBlockCommandPool, true);
    }
}

void BlockWorld::DestroyBlockWorldPipeline() {
    LOGD("BlockWorld: DestroyBlockWorldPipeline");

    // 基础噪声
    delete mRiverNoise;          mRiverNoise = nullptr;

    // 含水层
    delete mAquiferBarrierNoise; mAquiferBarrierNoise = nullptr;
    delete mAquiferNoise;        mAquiferNoise = nullptr;

    // 洞穴
    delete mRavineNoise;         mRavineNoise = nullptr;
    delete mCaveWarpZ;           mCaveWarpZ = nullptr;
    delete mCaveWarpY;           mCaveWarpY = nullptr;
    delete mCaveWarpX;           mCaveWarpX = nullptr;
    delete mNoodleCaveNoise;     mNoodleCaveNoise = nullptr;
    delete mSpaghettiCaveNoise;  mSpaghettiCaveNoise = nullptr;
    delete mCheeseCaveNoise;     mCheeseCaveNoise = nullptr;

    // 群系
    delete mWeirdnessNoise;      mWeirdnessNoise = nullptr;
    delete mHumidityNoise;       mHumidityNoise = nullptr;
    delete mTemperatureNoise;    mTemperatureNoise = nullptr;

    // 密度场
    delete mRidgeNoise;          mRidgeNoise = nullptr;
    delete mDensityNoise3;       mDensityNoise3 = nullptr;
    delete mDensityNoise2;       mDensityNoise2 = nullptr;
    delete mDensityNoise1;       mDensityNoise1 = nullptr;

    // 基础
    delete mDetailNoise;         mDetailNoise = nullptr;
    delete mErosionNoise;        mErosionNoise = nullptr;
    delete mContinentalNoise;    mContinentalNoise = nullptr;

    DestroyBlockWorldVulkanResources();
}

void BlockWorld::DestroyBlockWorldVulkanResources() {
    // 必须等待 GPU 完成所有操作，否则 Validation Layer 后台线程
    // 在异步处理已提交的 command buffer 时会访问已释放的资源，
    // 导致 SubStateManager::SubState() 中 std::map 查找时空指针崩溃。
    vkDeviceWaitIdle(mDevice->getDevice());

    if (mBlockCommandBuffers) {
        for (size_t i = 0; i < mSwapChain->getImageCount(); ++i) {
            delete mBlockCommandBuffers[i];
        }
        delete[] mBlockCommandBuffers;
        mBlockCommandBuffers = nullptr;
    }

    if (mBlockCommandPool) {
        delete mBlockCommandPool;
        mBlockCommandPool = nullptr;
    }

    if (mDescriptorSet) {
        delete mDescriptorSet;
        mDescriptorSet = nullptr;
    }

    if (mDescriptorPool) {
        delete mDescriptorPool;
        mDescriptorPool = nullptr;
    }

    if (mVertexBuffer) {
        delete mVertexBuffer;
        mVertexBuffer = nullptr;
    }
}

void BlockWorld::RecordBlockWorldCommandBuffers() {
    VkCommandBufferInheritanceInfo inheritanceInfo{};
    inheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
    inheritanceInfo.renderPass = mRenderPass->getRenderPass();

    for (size_t i = 0; i < mSwapChain->getImageCount(); ++i) {
        inheritanceInfo.framebuffer = mSwapChain->getFrameBuffer(i);

        mBlockCommandBuffers[i]->begin(
            VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT,
            inheritanceInfo);

        VulKan::Pipeline* blockPipeline = mPipelineS->GetPipeline(VulKan::PipelineMods::BlockWorldMods);
        mBlockCommandBuffers[i]->bindGraphicPipeline(blockPipeline->getPipeline());
        mBlockCommandBuffers[i]->bindVertexBuffer({mVertexBuffer->getBuffer()});
        mBlockCommandBuffers[i]->bindDescriptorSet(
            blockPipeline->getLayout(),
            mDescriptorSet->getDescriptorSet((int)i));

        if (mVertexCount > 0) {
            mBlockCommandBuffers[i]->draw(mVertexCount);
        }

        mBlockCommandBuffers[i]->end();
    }
}

// ============================================================================
// 构造/析构
// ============================================================================

BlockWorld::BlockWorld(Configuration wConfiguration)
    : Configuration{wConfiguration}
    , mChunkPlate(CHUNK_COUNT_X, CHUNK_COUNT_Y, CHUNK_COUNT_Z, CHUNK_EDGE,
                  CHUNK_ORIGIN_X, CHUNK_ORIGIN_Y, CHUNK_ORIGIN_Z)
    , mPlateOriginX(CHUNK_ORIGIN_X)
    , mPlateOriginY(CHUNK_ORIGIN_Y)
    , mPlateOriginZ(CHUNK_ORIGIN_Z)
{
    LOGD("BlockWorld::BlockWorld constructor (Advanced Terrain v2)");

    mChunkPlate.SetCallback(OnChunkGenerate, this, OnChunkDelete, this);

    // 自由飞行相机：设置初始位置（地形中心偏上方，俯瞰地形）
    // 5x5x5 网格，CHUNK_SIZE=32 → 世界坐标范围 0~160
    mCameraYaw = -45.0f;
    mCameraPitch = -30.0f;
    mCameraSpeed = 20.0f;

    // 板块位置：GameLoop 中 UpData 的 Z 固定为 0，所以这里 SetPos Z 也设为 0
    // 避免首帧产生 Z 偏移导致所有区块被移动删除
    mChunkPlate.SetPos(
        (float)(mPlateOriginX * CHUNK_EDGE),
        (float)(mPlateOriginY * CHUNK_EDGE),
        0.0f
    );

    // 初始化渲染管线和噪声源
    InitBlockWorldPipeline();

    // ====== 初始化异步地形生成流水线 ======
    mTerrainPipeline = std::make_unique<TerrainGenPipeline>(0); // 0 = 自动检测核心数
    mTerrainPipeline->initialize(*this);

    // ====== 初始生成所有区块 (异步请求，非阻塞) ======
    int platePosX = mChunkPlate.GetPosX();
    int platePosY = mChunkPlate.GetPosY();
    int platePosZ = mChunkPlate.GetPosZ();
    for (unsigned int x = 0; x < CHUNK_COUNT_X; x++) {
        for (unsigned int y = 0; y < CHUNK_COUNT_Y; y++) {
            for (unsigned int z = 0; z < CHUNK_COUNT_Z; z++) {
                int wx = (int)x + platePosX - (int)mPlateOriginX;
                int wy = (int)y + platePosY - (int)mPlateOriginY;
                int wz = (int)z + platePosZ - (int)mPlateOriginZ;
                mTerrainPipeline->requestChunk(wx, wy, wz);
            }
        }
    }

    // 注意：不再在此处同步调用 RebuildAllVertexData()
    // Chunk生成完成后在 GameLoop 中异步消费 + 触发顶点重建

    // 设置自由飞行相机的初始位置（地形中心偏上方）
    mCamera->setCameraPos(glm::vec3(80.0f, 120.0f, 80.0f));
    UpdateCamera();

    // 关键：必须在构造函数中录制二级指令缓冲，否则第一帧画面为空
    RecordBlockWorldCommandBuffers();

    LOGD("BlockWorld: constructor done, %zu chunks, %u vertices", mChunkMap.size(), mVertexCount);
}

BlockWorld::~BlockWorld() {
    LOGD("BlockWorld::~BlockWorld destructor");

    // 1. 先关闭异步流水线（等待所有Worker退出，避免访问已销毁的噪声实例）
    if (mTerrainPipeline) {
        mTerrainPipeline->shutdown();
        mTerrainPipeline.reset();
    }

    // 2. 销毁渲染管线（清理噪声实例）
    DestroyBlockWorldPipeline();

    // 3. 清理Chunk
    for (auto& pair : mChunkMap) {
        delete pair.second;
    }
    mChunkMap.clear();
}

// ============================================================================
// 相机控制
// ============================================================================

void BlockWorld::UpdateCamera() {
    float pitchRad = glm::radians(mCameraPitch);
    float yawRad = glm::radians(mCameraYaw);

    // 自由飞行相机：视线方向 = (cos(pitch)*sin(yaw), sin(pitch), cos(pitch)*cos(yaw))
    glm::vec3 front;
    front.x = cos(pitchRad) * sin(yawRad);
    front.y = sin(pitchRad);
    front.z = cos(pitchRad) * cos(yawRad);

    // 相机位置 + 视线方向 → 设置朝向
    glm::vec3 camPos = mCamera->getCameraPos();
    mCamera->lookAt(camPos, front, glm::vec3(0.0f, 1.0f, 0.0f));
    mCamera->update();
}

// ============================================================================
// 输入事件
// ============================================================================

void BlockWorld::MouseMove(double xpos, double ypos) {
    CursorPosX = xpos;
    CursorPosY = ypos;
#if defined(__ANDROID__)
    mWinWidth = mWindow->getWidth();
    mWinHeight = mWindow->getHeight();
#else
    glfwGetWindowSize(mWindow->getWindow(), &mWinWidth, &mWinHeight);
#endif

    if (mIsDragging && !Global::ClickWindow) {
        double dx = xpos - mLastMouseX;
        double dy = ypos - mLastMouseY;

        mCameraYaw -= (float)dx * 0.3f;
        mCameraPitch -= (float)dy * 0.3f;
        mCameraPitch = std::max(-89.0f, std::min(mCameraPitch, 89.0f));

        UpdateCamera();
    }

    mLastMouseX = xpos;
    mLastMouseY = ypos;
}

void BlockWorld::MouseButton(MouseBtn button, InputState State) {
    switch (button) {
    case MouseBtn::Left:
        mLeftMouseDown = (State == InputState::Down || State == InputState::Hold);
        break;
    case MouseBtn::Right:
        if (State == InputState::Down) {
            mIsDragging = true;
        } else if (State == InputState::Up) {
            mIsDragging = false;
        }
        mRightMouseDown = (State == InputState::Down || State == InputState::Hold);
        break;
    default:
        break;
    }
}

void BlockWorld::MouseRoller(int z) {
    if (Global::ClickWindow) return;

    mCameraSpeed += z * 5.0f;
    mCameraSpeed = std::max(1.0f, std::min(mCameraSpeed, 200.0f));
}

void BlockWorld::KeyDown(GameKeyEnum moveDirection) {
    float moveSpeed = mCameraSpeed * TOOL::FPStime;

    // ========================================================================
    // 自由飞行相机方向计算
    // ========================================================================
    float pitchRad = glm::radians(mCameraPitch);
    float yawRad = glm::radians(mCameraYaw);

    // 视线方向（3D 单位向量）：视线指哪，W/S 就往哪飞
    glm::vec3 forward(
        cos(pitchRad) * sin(yawRad),
        sin(pitchRad),
        cos(pitchRad) * cos(yawRad)
    );

    // 右方向 = cross(世界上方向, 视线方向)，保证始终水平
    glm::vec3 worldUp(0.0f, 1.0f, 0.0f);
    glm::vec3 right = glm::normalize(glm::cross(worldUp, forward));

    glm::vec3 camPos = mCamera->getCameraPos();

    // ========================================================================
    // 按键移动：直接在3D空间中平移相机位置
    // ========================================================================
    switch (moveDirection) {
    case GameKeyEnum::MOVE_LEFT:
        camPos += right * moveSpeed;       // A键：向右平移
        break;
    case GameKeyEnum::MOVE_RIGHT:
        camPos -= right * moveSpeed;       // D键：向左平移
        break;
    case GameKeyEnum::MOVE_FRONT:
        camPos += forward * moveSpeed;     // W键：沿视线前进
        break;
    case GameKeyEnum::MOVE_BACK:
        camPos -= forward * moveSpeed;     // S键：沿视线后退
        break;
    case GameKeyEnum::MOVE_UP:
    case GameKeyEnum::SPACE:
        camPos.y += moveSpeed;             // 空格：Y轴上升
        break;
    case GameKeyEnum::MOVE_DOWN:
        camPos.y -= moveSpeed;             // Shift：Y轴下降
        break;
    case GameKeyEnum::ESC:
        if (Global::ConsoleBool) {
            Global::ConsoleBool = false;
            InterFace->ConsoleFocusHere = true;
        } else {
            InterFace->SetInterFaceBool();
        }
        break;
    default:
        break;
    }

    mCamera->setCameraPos(camPos);
    // setCameraPos 内部已调用 update()，无需再调 UpdateCamera
}

// ============================================================================
// 游戏循环
// ============================================================================

void BlockWorld::GameCommandBuffers(unsigned int Format_i) {
    // 越界保护：Android 端 imageCount 可能与 MAX_FRAMES_IN_FLIGHT 不同
    if (Format_i >= mSwapChain->getImageCount()) return;

    if (mBlockCommandBuffers) {
        wThreadCommandBufferS->push_back(
            mBlockCommandBuffers[Format_i]->getCommandBuffer());
    }
}

void BlockWorld::GameLoop(unsigned int /*mCurrentFrame*/) {
    // ====== 阶段 1: 板块追踪 ======
    glm::vec3 camPos = mCamera->getCameraPos();
    MovePlate3DInfo plateInfo = mChunkPlate.UpData(camPos.x, camPos.y, camPos.z);

    // ====== 阶段 2: 消费已完成的异步Chunk ======
    if (mTerrainPipeline) {
        auto readyChunks = mTerrainPipeline->pollResults();
        if (!readyChunks.empty()) {
            for (auto& chunk : readyChunks) {
                auto key = ChunkKeyPack(chunk->worldX, chunk->worldY, chunk->worldZ);
                // 安全检查：不覆盖已存在的Chunk
                if (mChunkMap.find(key) == mChunkMap.end()) {
                    ChunkData* raw = chunk.release();
                    mChunkMap[key] = raw;
                    MarkChunkDirty(raw);
                    LOGD("BlockWorld: async chunk ready at (%d,%d,%d)",
                         raw->worldX, raw->worldY, raw->worldZ);
                }
            }
        }
    }

    // ====== 阶段 3: 清理超出范围的Chunk ======
    if (plateInfo.UpData) {
        CleanupOutOfRangeChunks();
    }

    // ====== 阶段 4: 脏顶点重建 ======
    if (mVertexDataDirty) {
        RebuildAllVertexData();
        // 顶点数据变化后必须重新录制二级指令缓冲
        RecordBlockWorldCommandBuffers();
        // 请求重录主指令缓冲：二级 CB 已更新，缓存的主 CB 必须重新录制
        // 才能正确引用新的二级 CB 内容（否则安卓端会因主 CB 缓存导致地形闪烁）
        Global::MainCommandBufferUpdateRequest();
    }

    mCamera->update();
}

void BlockWorld::GameRecordCommandBuffers() {
    RecordBlockWorldCommandBuffers();
}

void BlockWorld::GameStopInterfaceLoop(unsigned int /*mCurrentFrame*/) {
}

void BlockWorld::GameTCPLoop() {
}

unsigned int BlockWorld::GetPendingChunks() const {
    return mTerrainPipeline ? mTerrainPipeline->pendingCount() : 0;
}

unsigned int BlockWorld::GetActiveWorkers() const {
    return mTerrainPipeline ? mTerrainPipeline->activeCount() : 0;
}

void BlockWorld::GameUI() {
    ImGui::Begin(u8"BlockWorld 地形生成控制");
    ImVec2 window_pos = ImGui::GetWindowPos();
    ImVec2 window_size = ImGui::GetWindowSize();
    if (((window_pos.x < CursorPosX) && ((window_pos.x + window_size.x) > CursorPosX)) &&
        ((window_pos.y < CursorPosY) && ((window_pos.y + window_size.y) > CursorPosY))) {
        Global::ClickWindow = true;
    }

    auto& p = mTerrainParams;

    // ========== 状态栏 ==========
    glm::vec3 camPos = mCamera->getCameraPos();
    ImGui::Text(u8"相机: (%.0f, %.0f, %.0f)  速度: %.0f  区块: %zu  顶点: %u/%u  待生成: %u",
               camPos.x, camPos.y, camPos.z, mCameraSpeed,
               mChunkMap.size(), mVertexCount, mVertexCapacity, GetPendingChunks());

    // ========== 重新生成按钮 ==========
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.8f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.5f, 0.1f, 1.0f));
    if (ImGui::Button(u8"应用参数并重新生成地图", ImVec2(-1, 40))) {
        RegenerateAllChunks();
    }
    ImGui::PopStyleColor(3);
    ImGui::Spacing();
    ImGui::Separator();

    // ========== 种子 ==========
    ImGui::InputInt(u8"随机种子", &p.seed, 1, 100);
    if (ImGui::Button(u8"随机种子")) {
        p.seed = (int)std::chrono::steady_clock::now().time_since_epoch().count();
    }
    ImGui::Separator();

    // ========== 基础噪声 ==========
    if (ImGui::CollapsingHeader(u8"基础噪声 (2D)", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderFloat(u8"大陆度频率", &p.continentalFrequency, 0.001f, 0.1f, "%.4f");
        ImGui::SliderInt(u8"大陆度八倍频", &p.continentalOctaves, 1, 8);
        ImGui::SliderFloat(u8"侵蚀度频率", &p.erosionFrequency, 0.001f, 0.1f, "%.4f");
        ImGui::SliderFloat(u8"细节频率", &p.detailFrequency, 0.001f, 0.2f, "%.4f");
    }

    // ========== 密度场噪声 ==========
    if (ImGui::CollapsingHeader(u8"密度场噪声 (3D)", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderFloat(u8"低频密度频率", &p.density1Frequency, 0.0005f, 0.02f, "%.4f");
        ImGui::SliderInt(u8"低频密度八倍频", &p.density1Octaves, 1, 8);
        ImGui::SliderFloat(u8"中频密度频率", &p.density2Frequency, 0.001f, 0.05f, "%.4f");
        ImGui::SliderInt(u8"中频密度八倍频", &p.density2Octaves, 1, 8);
        ImGui::SliderFloat(u8"高频密度频率", &p.density3Frequency, 0.01f, 0.2f, "%.4f");
        ImGui::SliderFloat(u8"山脊噪声频率", &p.ridgeFrequency, 0.001f, 0.05f, "%.4f");
        ImGui::SliderInt(u8"山脊八倍频", &p.ridgeOctaves, 1, 6);
    }

    // ========== 群系噪声 ==========
    if (ImGui::CollapsingHeader(u8"群系噪声 (2D)")) {
        ImGui::SliderFloat(u8"温度频率", &p.temperatureFrequency, 0.0001f, 0.01f, "%.5f");
        ImGui::SliderInt(u8"温度八倍频", &p.temperatureOctaves, 1, 6);
        ImGui::SliderFloat(u8"湿度频率", &p.humidityFrequency, 0.0001f, 0.01f, "%.5f");
        ImGui::SliderInt(u8"湿度八倍频", &p.humidityOctaves, 1, 6);
        ImGui::SliderFloat(u8"奇异性频率", &p.weirdnessFrequency, 0.0001f, 0.01f, "%.5f");
    }

    // ========== 洞穴噪声 ==========
    if (ImGui::CollapsingHeader(u8"洞穴噪声")) {
        ImGui::Checkbox(u8"启用洞穴", &p.cavesEnabled);
        if (p.cavesEnabled) {
            ImGui::SliderFloat(u8"奶酪洞穴频率", &p.cheeseCaveFrequency, 0.005f, 0.1f, "%.4f");
            ImGui::SliderInt(u8"奶酪洞穴八倍频", &p.cheeseCaveOctaves, 1, 6);
            ImGui::SliderFloat(u8"面条洞穴频率", &p.spaghettiCaveFrequency, 0.01f, 0.5f, "%.4f");
            ImGui::SliderInt(u8"面条洞穴八倍频", &p.spaghettiCaveOctaves, 1, 6);
            ImGui::SliderFloat(u8"细面条洞穴频率", &p.noodleCaveFrequency, 0.01f, 0.5f, "%.4f");
            ImGui::SliderFloat(u8"洞穴扭曲频率", &p.caveWarpFrequency, 0.005f, 0.1f, "%.4f");
            ImGui::SliderFloat(u8"洞穴扭曲强度", &p.caveWarpStrength, 0.0f, 2.0f, "%.2f");
            ImGui::SliderFloat(u8"裂谷频率", &p.ravineFrequency, 0.005f, 0.1f, "%.4f");
        }
    }

    // ========== 含水层 ==========
    if (ImGui::CollapsingHeader(u8"含水层")) {
        ImGui::Checkbox(u8"启用含水层", &p.aquifersEnabled);
        if (p.aquifersEnabled) {
            ImGui::SliderFloat(u8"含水层频率", &p.aquiferFrequency, 0.005f, 0.1f, "%.4f");
            ImGui::SliderInt(u8"含水层八倍频", &p.aquiferOctaves, 1, 6);
            ImGui::SliderFloat(u8"隔水层频率", &p.aquiferBarrierFrequency, 0.01f, 0.2f, "%.4f");
        }
    }

    // ========== 河流 ==========
    if (ImGui::CollapsingHeader(u8"河流")) {
        ImGui::Checkbox(u8"启用河流", &p.riversEnabled);
        if (p.riversEnabled) {
            ImGui::SliderFloat(u8"河流频率", &p.riverFrequency, 0.001f, 0.02f, "%.4f");
            ImGui::SliderInt(u8"河流八倍频", &p.riverOctaves, 1, 6);
        }
    }

    // ========== 其他 ==========
    if (ImGui::CollapsingHeader(u8"其他设置")) {
        ImGui::SliderInt(u8"水位高度", &p.waterLevel, 0, 100);
        ImGui::SliderFloat(u8"地形高度缩放", &p.terrainHeightScale, 0.1f, 3.0f, "%.2f");
        ImGui::Checkbox(u8"生成树木", &p.treesEnabled);
    }

    ImGui::End();
}

} // namespace GAME