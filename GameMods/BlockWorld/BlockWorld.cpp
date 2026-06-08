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

// 确定性哈希函数：将坐标映射到种子
static int deterministicHash(int x, int y, int z) {
    int h = x * 374761393 + y * 668265263 + z * 1274126177;
    h = (h ^ (h >> 13)) * 1274126177;
    return (h ^ (h >> 16)) & 0x7FFFFFFF;
}

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

    // 使用世界坐标计算确定性种子
    int seed = deterministicHash(x, y, z);
    chunk->generateTerrain(
        self->mContinentalNoise,
        self->mHillsNoise,
        self->mDetailNoise,
        self->mCaveNoise,
        self->mErosionNoise,
        seed
    );

    self->mChunkMap[key] = chunk;
    self->mVertexDataDirty = true;

    LOGD("BlockWorld: Chunk generated at (%d, %d, %d), seed=%d", x, y, z, seed);
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
                if (!chunk->isSolid(x, y, z)) continue;

                int wx = baseX + x;
                int wy = baseY + y;
                int wz = baseZ + z;

                // 面剔除：只有相邻不是固体才渲染面
                if (!chunk->isSolid(x, y, z + 1) && GetBlockAtWorld(wx, wy, wz + 1) == (int)BlockType::Air)
                    BuildBlockFaceVertices(wx, wy, wz, 0, blockType, out);
                if (!chunk->isSolid(x, y, z - 1) && GetBlockAtWorld(wx, wy, wz - 1) == (int)BlockType::Air)
                    BuildBlockFaceVertices(wx, wy, wz, 1, blockType, out);
                if (!chunk->isSolid(x - 1, y, z) && GetBlockAtWorld(wx - 1, wy, wz) == (int)BlockType::Air)
                    BuildBlockFaceVertices(wx, wy, wz, 2, blockType, out);
                if (!chunk->isSolid(x + 1, y, z) && GetBlockAtWorld(wx + 1, wy, wz) == (int)BlockType::Air)
                    BuildBlockFaceVertices(wx, wy, wz, 3, blockType, out);
                if (!chunk->isSolid(x, y - 1, z) && GetBlockAtWorld(wx, wy - 1, wz) == (int)BlockType::Air)
                    BuildBlockFaceVertices(wx, wy, wz, 4, blockType, out);
                if (!chunk->isSolid(x, y + 1, z) && GetBlockAtWorld(wx, wy + 1, wz) == (int)BlockType::Air)
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

    mContinentalNoise = new FastNoiseLite();
    mContinentalNoise->SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    mContinentalNoise->SetFractalType(FastNoiseLite::FractalType_FBm);
    mContinentalNoise->SetFractalOctaves(4);
    mContinentalNoise->SetFrequency(0.015f);

    mHillsNoise = new FastNoiseLite();
    mHillsNoise->SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2S);
    mHillsNoise->SetFractalType(FastNoiseLite::FractalType_FBm);
    mHillsNoise->SetFractalOctaves(3);
    mHillsNoise->SetFrequency(0.04f);

    mDetailNoise = new FastNoiseLite();
    mDetailNoise->SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    mDetailNoise->SetFrequency(0.04f);

    mCaveNoise = new FastNoiseLite();
    mCaveNoise->SetNoiseType(FastNoiseLite::NoiseType_Cellular);
    mCaveNoise->SetCellularDistanceFunction(FastNoiseLite::CellularDistanceFunction_EuclideanSq);
    mCaveNoise->SetFractalType(FastNoiseLite::FractalType_FBm);
    mCaveNoise->SetFractalOctaves(2);
    mCaveNoise->SetFrequency(0.06f);

    mErosionNoise = new FastNoiseLite();
    mErosionNoise->SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    mErosionNoise->SetFrequency(0.01f);

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

    delete mErosionNoise; mErosionNoise = nullptr;
    delete mCaveNoise; mCaveNoise = nullptr;
    delete mDetailNoise; mDetailNoise = nullptr;
    delete mHillsNoise; mHillsNoise = nullptr;
    delete mContinentalNoise; mContinentalNoise = nullptr;

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
    LOGD("BlockWorld::BlockWorld constructor (Minecraft-style with FastNoiseLite)");

    mChunkPlate.SetCallback(OnChunkGenerate, this, OnChunkDelete, this);

    // 相机设置：注视地形表面中心
    // 板块中心在 chunk(1, 1, 0)，3x3x3 网格覆盖 world X:0-95, Y:0-95, Z:0-95
    mCameraTarget = glm::vec3(48.0f, 48.0f, 45.0f);
    mCameraYaw = -45.0f;
    mCameraPitch = -30.0f;
    mCameraDistance = 140.0f;

    // 板块位置：用世界坐标设置（内部会除以 CHUNK_EDGE 转换为 chunk 坐标）
    // chunk(1, 1, 0) = 世界(32, 32, 0)
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

    glm::vec3 dir;
    dir.x = cos(pitchRad) * sin(yawRad);
    dir.y = sin(pitchRad);
    dir.z = cos(pitchRad) * cos(yawRad);

    glm::vec3 camPos = mCameraTarget - dir * mCameraDistance;
    mCamera->lookAt(camPos, mCameraTarget - camPos, glm::vec3(0.0f, 1.0f, 0.0f));
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

    mCameraDistance += z * 5.0f;
    mCameraDistance = std::max(5.0f, std::min(mCameraDistance, 300.0f));
    UpdateCamera();
}

void BlockWorld::KeyDown(GameKeyEnum moveDirection) {
    float moveSpeed = 20.0f * TOOL::FPStime;

    // 根据相机朝向（yaw）计算 XY 平面的移动方向
    float yawRad = glm::radians(mCameraYaw);
    glm::vec3 forwardXY(sin(yawRad), cos(yawRad), 0.0f);
    glm::vec3 rightXY(cos(yawRad), -sin(yawRad), 0.0f);

    switch (moveDirection) {
    case GameKeyEnum::MOVE_LEFT:
        mCameraTarget -= rightXY * moveSpeed;
        break;
    case GameKeyEnum::MOVE_RIGHT:
        mCameraTarget += rightXY * moveSpeed;
        break;
    case GameKeyEnum::MOVE_FRONT:
        mCameraTarget += forwardXY * moveSpeed;
        break;
    case GameKeyEnum::MOVE_BACK:
        mCameraTarget -= forwardXY * moveSpeed;
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
    UpdateCamera();
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
    // Z 固定为 0：地形高度由噪声计算，板块不需要在 Z 方向移动
    MovePlate3DInfo plateInfo = mChunkPlate.UpData(
        mCameraTarget.x, mCameraTarget.y, 0);

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
}

void BlockWorld::GameRecordCommandBuffers() {
    RecordBlockWorldCommandBuffers();
}

void BlockWorld::GameStopInterfaceLoop(unsigned int /*mCurrentFrame*/) {
}

void BlockWorld::GameTCPLoop() {
}

void BlockWorld::GameUI() {
    ImGui::Begin(u8"Minecraft 风格区块世界 (FastNoiseLite)");
    ImVec2 window_pos = ImGui::GetWindowPos();
    ImVec2 window_size = ImGui::GetWindowSize();
    if (((window_pos.x < CursorPosX) && ((window_pos.x + window_size.x) > CursorPosX)) &&
        ((window_pos.y < CursorPosY) && ((window_pos.y + window_size.y) > CursorPosY))) {
        Global::ClickWindow = true;
    }

    ImGui::Text(u8"相机目标: (%.1f, %.1f, %.1f)", mCameraTarget.x, mCameraTarget.y, mCameraTarget.z);
    ImGui::Text(u8"相机距离: %.1f", mCameraDistance);
    ImGui::Text(u8"视角: 偏航=%.1f° 俯仰=%.1f°", mCameraYaw, mCameraPitch);
    ImGui::Text(u8"激活区块数: %zu", mChunkMap.size());
    ImGui::Text(u8"顶点数: %u / %u", mVertexCount, mVertexCapacity);
    ImGui::Text(u8"区块网格: %dx%dx%d", CHUNK_COUNT_X, CHUNK_COUNT_Y, CHUNK_COUNT_Z);
    ImGui::Text(u8"区块大小: %dx%dx%d", (int)ChunkData::CHUNK_SIZE,
               (int)ChunkData::CHUNK_SIZE, (int)ChunkData::CHUNK_SIZE);
    ImGui::Text(u8"板块中心 (chunk): (%d, %d, %d)",
               mChunkPlate.GetPosX(), mChunkPlate.GetPosY(), mChunkPlate.GetPosZ());
    ImGui::Text(u8"海平面高度: %d", ChunkData::WATER_LEVEL);
    ImGui::Text(u8"地形高度范围: ~3-75 (跨3个Z层)");

    ImGui::Separator();
    ImGui::Text(u8"噪声层 (FastNoiseLite):");
    ImGui::BulletText("大陆噪声: OpenSimplex2 fBm, 频率 0.015");
    ImGui::BulletText("丘陵噪声: OpenSimplex2S fBm, 频率 0.04");
    ImGui::BulletText("细节噪声: Perlin 3D, 频率 0.04 (打破分层)");
    ImGui::BulletText("洞穴噪声: Cellular 3D, 频率 0.06");
    ImGui::BulletText("侵蚀噪声: OpenSimplex2, 频率 0.01");

    ImGui::Separator();
    ImGui::Text(u8"操作说明:");
    ImGui::Text(u8"  WASD      - 移动相机（流式加载新区块）");
    ImGui::Text(u8"  右键拖拽  - 旋转视角");
    ImGui::Text(u8"  滚轮      - 缩放");
    ImGui::Text(u8"  ESC       - 返回菜单");

    ImGui::End();
}

} // namespace GAME
