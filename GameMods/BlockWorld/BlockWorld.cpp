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

    auto key = std::make_tuple(x, y, z);
    if (self->mChunkMap.find(key) != self->mChunkMap.end()) {
        return;
    }

    ChunkData* chunk = new ChunkData(x, y, z);

    chunk->generateTerrain(
        // 基础噪声
        self->mContinentalNoise,
        self->mErosionNoise,
        self->mDetailNoise,

        // 密度场噪声 (3D)
        self->mDensityNoise1,
        self->mDensityNoise2,
        self->mDensityNoise3,
        self->mRidgeNoise,

        // 群系噪声 (2D)
        self->mTemperatureNoise,
        self->mHumidityNoise,
        self->mWeirdnessNoise,

        // 洞穴噪声
        self->mCheeseCaveNoise,
        self->mSpaghettiCaveNoise,
        self->mNoodleCaveNoise,
        self->mCaveWarpX,
        self->mCaveWarpY,
        self->mCaveWarpZ,
        self->mRavineNoise,

        // 含水层噪声
        self->mAquiferNoise,
        self->mAquiferBarrierNoise,

        // 河流噪声
        self->mRiverNoise,

        0
    );

    self->mChunkMap[key] = chunk;
    self->mVertexDataDirty = true;

    LOGD("BlockWorld: Chunk generated at (%d, %d, %d)", x, y, z);
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

    auto it = mChunkMap.find(std::make_tuple(chunkX, chunkY, chunkZ));
    if (it == mChunkMap.end()) {
        return 0;
    }

    int localX = wx - chunkX * ChunkData::CHUNK_SIZE;
    int localY = wy - chunkY * ChunkData::CHUNK_SIZE;
    int localZ = wz - chunkZ * ChunkData::CHUNK_SIZE;

    return it->second->get(localX, localY, localZ);
}

void BlockWorld::CleanupOutOfRangeChunks() {
    std::vector<std::tuple<int, int, int>> toRemove;
    int platePosX = mChunkPlate.GetPosX();
    int platePosY = mChunkPlate.GetPosY();
    int platePosZ = mChunkPlate.GetPosZ();

    for (auto& pair : mChunkMap) {
        int cx = std::get<0>(pair.first);
        int cy = std::get<1>(pair.first);
        int cz = std::get<2>(pair.first);

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
        for (auto& key : toRemove) {
            delete mChunkMap[key];
            mChunkMap.erase(key);
            LOGD("BlockWorld: Chunk removed at (%d, %d, %d)",
                 std::get<0>(key), std::get<1>(key), std::get<2>(key));
        }
        mVertexDataDirty = true;
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

    switch (face) {
        case 0: // 前面 (Z+) - 从外向里看顺时针 (CW)
            v0 = {x, y, z + 1}; v1 = {x, y + 1, z + 1};
            v2 = {x + 1, y + 1, z + 1}; v3 = {x + 1, y, z + 1};
            break;
        case 1: // 后面 (Z-) - 从外向里看顺时针 (CW)
            v0 = {x, y, z}; v1 = {x + 1, y, z};
            v2 = {x + 1, y + 1, z}; v3 = {x, y + 1, z};
            break;
        case 2: // 左面 (X-) - 从外向里看顺时针 (CW)
            v0 = {x, y, z}; v1 = {x, y + 1, z};
            v2 = {x, y + 1, z + 1}; v3 = {x, y, z + 1};
            break;
        case 3: // 右面 (X+) - 从外向里看顺时针 (CW)
            v0 = {x + 1, y, z + 1}; v1 = {x + 1, y + 1, z + 1};
            v2 = {x + 1, y + 1, z}; v3 = {x + 1, y, z};
            break;
        case 4: // 下面 (Y-) - 从外向里看顺时针 (CW)
            v0 = {x, y, z}; v1 = {x, y, z + 1};
            v2 = {x + 1, y, z + 1}; v3 = {x + 1, y, z};
            break;
        case 5: // 上面 (Y+) - 从外向里看顺时针 (CW)
            v0 = {x + 1, y + 1, z + 1}; v1 = {x, y + 1, z + 1};
            v2 = {x, y + 1, z}; v3 = {x + 1, y + 1, z};
            break;
        default:
            return;
    }

    out.push_back({v0, color});
    out.push_back({v1, color});
    out.push_back({v2, color});
    out.push_back({v0, color});
    out.push_back({v2, color});
    out.push_back({v3, color});
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

void BlockWorld::RebuildAllVertexData() {
    LOGD("BlockWorld: RebuildAllVertexData, chunks=%zu", mChunkMap.size());

    std::vector<BlockVertex> vertices;
    vertices.reserve(mVertexCapacity);

    auto start = std::chrono::high_resolution_clock::now();

    for (auto& pair : mChunkMap) {
        BuildChunkVertices(pair.second, vertices);
    }

    mVertexCount = vertices.size();
    LOGD("BlockWorld: vertex count = %u (capacity %u)", mVertexCount, mVertexCapacity);

    if (vertices.empty()) {
        mVertexDataDirty = false;
        return;
    }

    // 处理容量超出
    if (vertices.size() > mVertexCapacity) {
        LOGW("BlockWorld: vertex count exceeds capacity (%zu > %u), expanding",
             vertices.size(), mVertexCapacity);
        mVertexCapacity = (unsigned int)(vertices.size() * 2 + 1000000);
        DestroyBlockWorldVulkanResources();

        // 手动重新分配资源，避免递归调用
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

    if (mVertexBuffer) {
        mVertexBuffer->updateBufferByStage(vertices.data(),
            vertices.size() * sizeof(BlockVertex));
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    LOGD("BlockWorld: Rebuild done in %lld ms", (long long)duration.count());

    mVertexDataDirty = false;
}

// ============================================================================
// 渲染管线初始化
// ============================================================================

void BlockWorld::InitBlockWorldPipeline() {
    LOGD("BlockWorld: InitBlockWorldPipeline, capacity=%u", mVertexCapacity);

    // === 基础噪声 ===
    mContinentalNoise = new FastNoiseLite();
    mContinentalNoise->SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    mContinentalNoise->SetFractalType(FastNoiseLite::FractalType_FBm);
    mContinentalNoise->SetFractalOctaves(4);
    mContinentalNoise->SetFrequency(0.015f);

    mErosionNoise = new FastNoiseLite();
    mErosionNoise->SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    mErosionNoise->SetFrequency(0.01f);

    mDetailNoise = new FastNoiseLite();
    mDetailNoise->SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    mDetailNoise->SetFrequency(0.04f);

    // === 密度场噪声 (3D) ===
    mDensityNoise1 = new FastNoiseLite();
    mDensityNoise1->SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    mDensityNoise1->SetFractalType(FastNoiseLite::FractalType_FBm);
    mDensityNoise1->SetFractalOctaves(6);
    mDensityNoise1->SetFrequency(0.003f);

    mDensityNoise2 = new FastNoiseLite();
    mDensityNoise2->SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    mDensityNoise2->SetFractalType(FastNoiseLite::FractalType_FBm);
    mDensityNoise2->SetFractalOctaves(4);
    mDensityNoise2->SetFrequency(0.01f);

    mDensityNoise3 = new FastNoiseLite();
    mDensityNoise3->SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    mDensityNoise3->SetFrequency(0.04f);

    mRidgeNoise = new FastNoiseLite();
    mRidgeNoise->SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    mRidgeNoise->SetFractalType(FastNoiseLite::FractalType_Ridged);
    mRidgeNoise->SetFractalOctaves(3);
    mRidgeNoise->SetFrequency(0.01f);

    // === 群系噪声 (2D) ===
    mTemperatureNoise = new FastNoiseLite();
    mTemperatureNoise->SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    mTemperatureNoise->SetFractalType(FastNoiseLite::FractalType_FBm);
    mTemperatureNoise->SetFractalOctaves(3);
    mTemperatureNoise->SetFrequency(0.0008f);

    mHumidityNoise = new FastNoiseLite();
    mHumidityNoise->SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    mHumidityNoise->SetFractalType(FastNoiseLite::FractalType_FBm);
    mHumidityNoise->SetFractalOctaves(3);
    mHumidityNoise->SetFrequency(0.0008f);

    mWeirdnessNoise = new FastNoiseLite();
    mWeirdnessNoise->SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    mWeirdnessNoise->SetFrequency(0.001f);

    // === 洞穴噪声 ===
    mCheeseCaveNoise = new FastNoiseLite();
    mCheeseCaveNoise->SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    mCheeseCaveNoise->SetFractalType(FastNoiseLite::FractalType_FBm);
    mCheeseCaveNoise->SetFractalOctaves(4);
    mCheeseCaveNoise->SetFrequency(0.02f);

    mSpaghettiCaveNoise = new FastNoiseLite();
    mSpaghettiCaveNoise->SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    mSpaghettiCaveNoise->SetFractalType(FastNoiseLite::FractalType_FBm);
    mSpaghettiCaveNoise->SetFractalOctaves(3);
    mSpaghettiCaveNoise->SetFrequency(0.05f);

    mNoodleCaveNoise = new FastNoiseLite();
    mNoodleCaveNoise->SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    mNoodleCaveNoise->SetFrequency(0.08f);

    mCaveWarpX = new FastNoiseLite();
    mCaveWarpX->SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    mCaveWarpX->SetFrequency(0.03f);

    mCaveWarpY = new FastNoiseLite();
    mCaveWarpY->SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    mCaveWarpY->SetFrequency(0.03f);

    mCaveWarpZ = new FastNoiseLite();
    mCaveWarpZ->SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    mCaveWarpZ->SetFrequency(0.03f);

    mRavineNoise = new FastNoiseLite();
    mRavineNoise->SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    mRavineNoise->SetFrequency(0.02f);

    // === 含水层噪声 ===
    mAquiferNoise = new FastNoiseLite();
    mAquiferNoise->SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    mAquiferNoise->SetFractalType(FastNoiseLite::FractalType_FBm);
    mAquiferNoise->SetFractalOctaves(3);
    mAquiferNoise->SetFrequency(0.015f);

    mAquiferBarrierNoise = new FastNoiseLite();
    mAquiferBarrierNoise->SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    mAquiferBarrierNoise->SetFrequency(0.03f);

    // === 河流噪声 ===
    mRiverNoise = new FastNoiseLite();
    mRiverNoise->SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    mRiverNoise->SetFractalType(FastNoiseLite::FractalType_FBm);
    mRiverNoise->SetFractalOctaves(4);
    mRiverNoise->SetFrequency(0.003f);

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

    // 初始生成所有区块
    int platePosX = mChunkPlate.GetPosX();
    int platePosY = mChunkPlate.GetPosY();
    int platePosZ = mChunkPlate.GetPosZ();
    for (unsigned int x = 0; x < CHUNK_COUNT_X; x++) {
        for (unsigned int y = 0; y < CHUNK_COUNT_Y; y++) {
            for (unsigned int z = 0; z < CHUNK_COUNT_Z; z++) {
                int wx = (int)x + platePosX - (int)mPlateOriginX;
                int wy = (int)y + platePosY - (int)mPlateOriginY;
                int wz = (int)z + platePosZ - (int)mPlateOriginZ;
                OnChunkGenerate(0, wx, wy, wz, this);
            }
        }
    }

    // 一次性构建所有顶点
    RebuildAllVertexData();

    // 设置自由飞行相机的初始位置（地形中心偏上方）
    mCamera->setCameraPos(glm::vec3(80.0f, 120.0f, 80.0f));
    UpdateCamera();

    // 关键：必须在构造函数中录制二级指令缓冲，否则第一帧画面为空
    RecordBlockWorldCommandBuffers();

    LOGD("BlockWorld: constructor done, %zu chunks, %u vertices", mChunkMap.size(), mVertexCount);
}

BlockWorld::~BlockWorld() {
    LOGD("BlockWorld::~BlockWorld destructor");

    DestroyBlockWorldPipeline();

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
    if (mBlockCommandBuffers && Format_i < mSwapChain->getImageCount()) {
        wThreadCommandBufferS->push_back(
            mBlockCommandBuffers[Format_i]->getCommandBuffer());
    }
}

void BlockWorld::GameLoop(unsigned int mCurrentFrame) {
    // 区块板块追踪相机在 X/Z 水平面的位置 + Y 高度
    glm::vec3 camPos = mCamera->getCameraPos();
    MovePlate3DInfo plateInfo = mChunkPlate.UpData(camPos.x, camPos.y, camPos.z);

    if (plateInfo.UpData) {
        CleanupOutOfRangeChunks();
    }

    if (mVertexDataDirty) {
        RebuildAllVertexData();
        // 顶点数据变化后必须重新录制二级指令缓冲
        RecordBlockWorldCommandBuffers();
    }

    mCamera->update();

    // 更新 Camera 变换矩阵到 GPU
    VPMatrices* mVPMatrices = (VPMatrices*)mCameraVPMatricesBuffer[mCurrentFrame]->getPersistentMappedPtr();
    mVPMatrices->mViewMatrix = mCamera->getViewMatrix();
    mVPMatrices->mProjectionMatrix = mCamera->getProjectMatrix();
}

void BlockWorld::GameRecordCommandBuffers() {
    RecordBlockWorldCommandBuffers();
}

void BlockWorld::GameStopInterfaceLoop(unsigned int /*mCurrentFrame*/) {
}

void BlockWorld::GameTCPLoop() {
}

void BlockWorld::GameUI() {
    ImGui::Begin(u8"BlockWorld 高级地形生成 v2 (FastNoiseLite)");
    ImVec2 window_pos = ImGui::GetWindowPos();
    ImVec2 window_size = ImGui::GetWindowSize();
    if (((window_pos.x < CursorPosX) && ((window_pos.x + window_size.x) > CursorPosX)) &&
        ((window_pos.y < CursorPosY) && ((window_pos.y + window_size.y) > CursorPosY))) {
        Global::ClickWindow = true;
    }

    glm::vec3 camPos = mCamera->getCameraPos();
    ImGui::Text(u8"相机位置: (%.1f, %.1f, %.1f)", camPos.x, camPos.y, camPos.z);
    ImGui::Text(u8"飞行速度: %.1f", mCameraSpeed);
    ImGui::Text(u8"视角: 偏航=%.1f° 俯仰=%.1f°", mCameraYaw, mCameraPitch);
    ImGui::Text(u8"激活区块数: %zu", mChunkMap.size());
    ImGui::Text(u8"顶点数: %u / %u", mVertexCount, mVertexCapacity);
    ImGui::Text(u8"区块网格: %dx%dx%d", CHUNK_COUNT_X, CHUNK_COUNT_Y, CHUNK_COUNT_Z);
    ImGui::Text(u8"区块大小: %dx%dx%d", (int)ChunkData::CHUNK_SIZE,
               (int)ChunkData::CHUNK_SIZE, (int)ChunkData::CHUNK_SIZE);

    ImGui::Separator();
    ImGui::Text(u8"===== 地形生成系统 =====");
    ImGui::Text(u8"密度场: Spline (大陆度→高度) + 山脊 + 3D 噪声");
    ImGui::Text(u8"生物群系: 温度+湿度+大陆度+侵蚀度+奇异性 → 18种群系");
    ImGui::Text(u8"洞穴: Domain Warping + Cheese/Spaghetti/Noodle/Ravine 四层");
    ImGui::Text(u8"含水层: 3D噪声驱动局部水位 + 隔水层");
    ImGui::Text(u8"河流: 2D噪声过零点检测 + 地形侵蚀 + 水填充");
    ImGui::Text(u8"植被: 群系驱动 橡树/云杉/仙人掌");

    ImGui::Separator();
    ImGui::Text(u8"操作说明:");
    ImGui::Text(u8"  WASD      - 自由飞行（视线方向移动）");
    ImGui::Text(u8"  Space     - Z轴上升");
    ImGui::Text(u8"  Shift     - Z轴下降");
    ImGui::Text(u8"  右键拖拽  - 旋转视角");
    ImGui::Text(u8"  滚轮      - 调节飞行速度");
    ImGui::Text(u8"  ESC       - 返回菜单");

    ImGui::End();
}

} // namespace GAME