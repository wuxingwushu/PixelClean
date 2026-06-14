#pragma once

#include "BlockVertex.h"
#include "../../Tool/FastNoiseLite.h"
#include <cstring>
#include <cmath>
#include <vector>
#include <algorithm>

namespace GAME {

enum class BlockType : int {
    Air = 0,
    Grass = 1,
    Dirt = 2,
    Stone = 3,
    DeepStone = 4,
    Bedrock = 5,
    Sand = 6,
    Water = 7,
    Snow = 8,
    Log = 9,      // 树干
    Leaves = 10,  // 树叶
    Cactus = 11   // 仙人掌
};

enum class BiomeType : int {
    Ocean = 0, DeepOcean, WarmOcean,
    Plains, Forest, FlowerForest,
    Desert, Savanna, Badlands,
    Taiga, SnowyTundra, SnowyTaiga,
    Mountains, GravellyMountains, SnowyMountains,
    Swamp, Jungle,
    MushroomFields, IceSpikes,
    Count
};

enum class TreeType : int {
    Oak, Birch, Spruce, Jungle, Acacia, SwampOak, CactusTree
};

// ============================================================================
// Spline 辅助
// ============================================================================
struct SplinePoint { float input; float output; };

inline float splineInterpolate(float x, const std::vector<SplinePoint>& points) {
    if (x <= points.front().input) return points.front().output;
    if (x >= points.back().input)  return points.back().output;
    size_t i = 0;
    while (i < points.size() - 1 && points[i + 1].input < x) i++;
    float t = (x - points[i].input) / (points[i + 1].input - points[i].input);
    t = t * t * (3.0f - 2.0f * t); // smoothstep
    return points[i].output + t * (points[i + 1].output - points[i].output);
}

inline float continentalSpline(float c) {
    static const std::vector<SplinePoint> s = {
        {0.0f,-40.0f}, {0.2f,-20.0f}, {0.35f,-5.0f},
        {0.45f,0.0f}, {0.55f,5.0f}, {0.65f,20.0f},
        {0.75f,45.0f}, {0.85f,80.0f}, {1.0f,100.0f}
    };
    return splineInterpolate(c, s);
}

inline float erosionSpline(float e) {
    static const std::vector<SplinePoint> s = {
        {0.0f,1.5f}, {0.3f,1.2f}, {0.5f,1.0f}, {0.7f,0.7f}, {1.0f,0.3f}
    };
    return splineInterpolate(e, s);
}

inline float normalizeNoise(float v) { return (v + 1.0f) * 0.5f; }

// ============================================================================
// 地形生成参数（所有可调参数集中管理）
// ============================================================================
struct TerrainGenParams {
    int seed = 1337;

    // === 基础噪声 (2D) ===
    float continentalFrequency = 0.015f;
    int   continentalOctaves = 4;
    float erosionFrequency = 0.01f;
    float detailFrequency = 0.04f;

    // === 密度场噪声 (3D) ===
    float density1Frequency = 0.003f;
    int   density1Octaves = 6;
    float density2Frequency = 0.01f;
    int   density2Octaves = 4;
    float density3Frequency = 0.04f;
    float ridgeFrequency = 0.01f;
    int   ridgeOctaves = 3;

    // === 群系噪声 (2D) ===
    float temperatureFrequency = 0.0008f;
    int   temperatureOctaves = 3;
    float humidityFrequency = 0.0008f;
    int   humidityOctaves = 3;
    float weirdnessFrequency = 0.001f;

    // === 洞穴噪声 ===
    float cheeseCaveFrequency = 0.02f;
    int   cheeseCaveOctaves = 4;
    float spaghettiCaveFrequency = 0.05f;
    int   spaghettiCaveOctaves = 3;
    float noodleCaveFrequency = 0.08f;
    float caveWarpFrequency = 0.03f;
    float ravineFrequency = 0.02f;

    // === 含水层噪声 ===
    float aquiferFrequency = 0.015f;
    int   aquiferOctaves = 3;
    float aquiferBarrierFrequency = 0.03f;

    // === 河流噪声 ===
    float riverFrequency = 0.003f;
    int   riverOctaves = 4;

    // === 总开关 ===
    bool  cavesEnabled = true;
    bool  aquifersEnabled = true;
    bool  riversEnabled = true;
    bool  treesEnabled = true;

    // === 水位 ===
    int   waterLevel = 32;

    // === 地形高度缩放 ===
    float terrainHeightScale = 1.0f;

    // === 洞穴扭曲强度 ===
    float caveWarpStrength = 0.5f;
};

// ============================================================================
// ChunkData
// ============================================================================
struct ChunkData {
    static constexpr unsigned int CHUNK_SIZE = 32;
    static constexpr int WATER_LEVEL = 32;
    static constexpr int MAX_HEIGHT = CHUNK_SIZE * 5; // 5 层区块

    int worldX, worldY, worldZ;
    bool generated = false;

    int blocks[CHUNK_SIZE][CHUNK_SIZE][CHUNK_SIZE]{};

    // === 顶点增量更新缓存 ===
    bool vertexDirty = true;                     // 顶点需要重建标记
    std::vector<BlockVertex> cachedVertices;     // 缓存的顶点数据
    unsigned int cachedVertexOffset = 0;         // 在大缓冲中的偏移（顶点数，用于拼接）

    ChunkData(int wx, int wy, int wz)
        : worldX(wx), worldY(wy), worldZ(wz) {
        std::memset(blocks, 0, sizeof(blocks));
        cachedVertices.reserve(20000);  // 预分配减少 realloc
    }

    int get(int x, int y, int z) const {
        if (x < 0 || x >= (int)CHUNK_SIZE ||
            y < 0 || y >= (int)CHUNK_SIZE ||
            z < 0 || z >= (int)CHUNK_SIZE) {
            return (int)BlockType::Air;
        }
        return blocks[x][y][z];
    }

    void set(int x, int y, int z, int blockType) {
        if (x >= 0 && x < (int)CHUNK_SIZE &&
            y >= 0 && y < (int)CHUNK_SIZE &&
            z >= 0 && z < (int)CHUNK_SIZE) {
            blocks[x][y][z] = blockType;
        }
    }

    bool isSolid(int x, int y, int z) const {
        if (x < 0 || x >= (int)CHUNK_SIZE ||
            y < 0 || y >= (int)CHUNK_SIZE ||
            z < 0 || z >= (int)CHUNK_SIZE) {
            return false;
        }
        return blocks[x][y][z] != (int)BlockType::Air &&
               blocks[x][y][z] != (int)BlockType::Water;
    }

    // ========================================================================
    // 生物群系判定
    // ========================================================================
    static BiomeType getBiomeAt(float continentalN, float erosionN,
                                float tempN, float humidN, float weirdN);
    static int getSurfaceBlockAt(BiomeType biome, int worldY, int waterLevel);

    // ========================================================================
    // 洞穴生成
    // ========================================================================
    static void warpCoordinate(float& wx, float& wy, float& wz,
                               FastNoiseLite* wxN, FastNoiseLite* wyN, FastNoiseLite* wzN,
                               float strength);
    static bool isCheeseCave(float wx, float wy, float wz,
                             FastNoiseLite* caveNoise,
                             FastNoiseLite* cwX, FastNoiseLite* cwY, FastNoiseLite* cwZ,
                             int maxWorldY);
    static bool isSpaghettiCave(float wx, float wy, float wz,
                                FastNoiseLite* caveNoise,
                                FastNoiseLite* cwX, FastNoiseLite* cwY, FastNoiseLite* cwZ);
    static bool isNoodleCave(float wx, float wy, float wz,
                             FastNoiseLite* caveNoise,
                             FastNoiseLite* cwX, FastNoiseLite* cwY, FastNoiseLite* cwZ);
    static bool isRavine(float wx, float wy, float wz, FastNoiseLite* ravineNoise, int maxWorldY);

    // ========================================================================
    // 含水层
    // ========================================================================
    static float getLocalWaterLevel(float wx, float wy, float wz,
                                    FastNoiseLite* aquiferNoise, FastNoiseLite* continentalNoise);
    static bool isAquiferBarrier(float wx, float wy, float wz, FastNoiseLite* barrierNoise);
    static void fillAquifers(float wx, float wy, float wz,
                             FastNoiseLite* aquiferNoise, FastNoiseLite* barrierNoise,
                             FastNoiseLite* continentalNoise, int& blockType);

    // ========================================================================
    // 河流生成
    // ========================================================================
    static float getRiverValue(float wx, float wz,
                               FastNoiseLite* riverNoise, FastNoiseLite* continentalNoise);
    static float applyRiverErosion(float wx, float wz, float terrainHeight,
                                   FastNoiseLite* riverNoise, FastNoiseLite* continentalNoise);
    static void fillRiverWater(float wx, float wy, float wz, float terrainHeight,
                               FastNoiseLite* riverNoise, FastNoiseLite* continentalNoise,
                               int& blockType);

    // ========================================================================
    // 树木生成
    // ========================================================================
    static TreeType getTreeTypeForBiome(BiomeType biome, float random);
    static bool shouldPlaceTree(float wx, float wz, BiomeType biome,
                                FastNoiseLite* detailNoise, float terrainHeight, int waterLevel);
    static void placeOakTree(int baseX, int baseY, int baseZ,
                             int blocks[CHUNK_SIZE][CHUNK_SIZE][CHUNK_SIZE],
                             FastNoiseLite* detailNoise);
    static void placeSpruceTree(int baseX, int baseY, int baseZ,
                                int blocks[CHUNK_SIZE][CHUNK_SIZE][CHUNK_SIZE],
                                FastNoiseLite* detailNoise);
    static void placeCactusPlant(int baseX, int baseY, int baseZ,
                                 int blocks[CHUNK_SIZE][CHUNK_SIZE][CHUNK_SIZE],
                                 FastNoiseLite* detailNoise);

    // ========================================================================
    // 主地形生成
    // ========================================================================
    void generateTerrain(
        const TerrainGenParams& params,
        // 基础噪声
        FastNoiseLite* continentalNoise, // 大陆度 (2D)
        FastNoiseLite* erosionNoise,     // 侵蚀度 (2D)
        FastNoiseLite* detailNoise,      // 细节/扰动 (3D)

        // 密度场噪声 (3D)
        FastNoiseLite* densityNoise1,
        FastNoiseLite* densityNoise2,
        FastNoiseLite* densityNoise3,
        FastNoiseLite* ridgeNoise,       // 山脊 (2D)

        // 群系噪声 (2D)
        FastNoiseLite* temperatureNoise,
        FastNoiseLite* humidityNoise,
        FastNoiseLite* weirdnessNoise,

        // 洞穴噪声
        FastNoiseLite* cheeseCaveNoise,
        FastNoiseLite* spaghettiCaveNoise,
        FastNoiseLite* noodleCaveNoise,
        FastNoiseLite* caveWarpX,
        FastNoiseLite* caveWarpY,
        FastNoiseLite* caveWarpZ,
        FastNoiseLite* ravineNoise,

        // 含水层噪声
        FastNoiseLite* aquiferNoise,
        FastNoiseLite* aquiferBarrierNoise,

        // 河流噪声
        FastNoiseLite* riverNoise)
    {
        const int CS = (int)CHUNK_SIZE;
        int baseWorldX = worldX * CS;
        int baseWorldY = worldY * CS;
        int baseWorldZ = worldZ * CS;
        const int waterLevel = params.waterLevel;
        const float terrainScale = params.terrainHeightScale;

        // 预存每列 (x,z) 的群系信息和侵蚀后高度，供后续阶段使用
        BiomeType biomeCache[CHUNK_SIZE][CHUNK_SIZE];
        float erodedHeightCache[CHUNK_SIZE][CHUNK_SIZE];

        // ====== Pass 1: 密度场 + 基本方块类型 ======
        for (unsigned int x = 0; x < CHUNK_SIZE; x++) {
            for (unsigned int z = 0; z < CHUNK_SIZE; z++) {
                float wx = (float)(baseWorldX + (int)x);
                float wz = (float)(baseWorldZ + (int)z);

                // --- 1a. 噪声参数 ---
                float continentalN     = continentalNoise->GetNoise(wx, wz);
                float continentalVal   = normalizeNoise(continentalN);
                float erosionN         = erosionNoise->GetNoise(wx, wz);
                float erosionVal       = normalizeNoise(erosionN);
                float temperatureN     = temperatureNoise->GetNoise(wx, wz);
                float temperatureVal   = normalizeNoise(temperatureN);
                float humidityN        = humidityNoise->GetNoise(wx, wz);
                float humidityVal      = normalizeNoise(humidityN);
                float weirdN           = weirdnessNoise->GetNoise(wx, wz);
                float weirdVal         = normalizeNoise(weirdN);
                float ridgeVal         = ridgeNoise->GetNoise(wx, wz);

                // --- 1b. 群系 ---
                BiomeType biome = getBiomeAt(continentalVal, erosionVal,
                                             temperatureVal, humidityVal, weirdVal);
                biomeCache[x][z] = biome;

                // --- 1c. Spline 地形高度 ---
                float baseH  = continentalSpline(continentalVal);
                float erosionF = erosionSpline(erosionVal);
                float ridgeOff = std::abs(ridgeVal) * 30.0f;
                float finalHeight = (baseH + ridgeOff) * erosionF * terrainScale;

                // 群系高度修正
                switch (biome) {
                    case BiomeType::Mountains:
                    case BiomeType::SnowyMountains:
                        finalHeight += 20.0f * terrainScale; break;
                    case BiomeType::GravellyMountains:
                        finalHeight += 15.0f * terrainScale; break;
                    case BiomeType::Jungle:
                        finalHeight += 8.0f * terrainScale;  break;
                    case BiomeType::Swamp:
                        finalHeight -= 5.0f * terrainScale;  break;
                    case BiomeType::Ocean:
                    case BiomeType::DeepOcean:
                        finalHeight -= 15.0f * terrainScale; break;
                    default: break;
                }

                // --- 1d. 河流侵蚀 ---
                float erodedHeight = params.riversEnabled
                    ? applyRiverErosion(wx, wz, finalHeight, riverNoise, continentalNoise)
                    : finalHeight;
                erodedHeightCache[x][z] = erodedHeight;

                for (unsigned int y = 0; y < CHUNK_SIZE; y++) {
                    int worldY = baseWorldY + (int)y;

                    if (worldY <= 1) {
                        blocks[x][y][z] = (int)BlockType::Bedrock;
                        continue;
                    }

                    // 3D 密度场
                    float vertDensity = (erodedHeight - (float)worldY) * 0.3f;
                    float d1 = densityNoise1->GetNoise(wx, (float)worldY * 0.3f, wz);
                    float d2 = densityNoise2->GetNoise(wx, (float)worldY * 0.5f, wz);
                    float d3 = densityNoise3->GetNoise(wx, (float)worldY, wz);
                    float dNoise = d1 * 0.5f + d2 * 0.3f + d3 * 0.2f;
                    float density = vertDensity + dNoise;

                    if (density <= 0.0f) {
                        blocks[x][y][z] = (int)BlockType::Air;
                    } else {
                        int depthBelowSurface = (int)(erodedHeight - worldY);
                        if (depthBelowSurface <= 1) {
                            blocks[x][y][z] = getSurfaceBlockAt(biome, worldY, waterLevel);
                        } else if (depthBelowSurface <= 6) {
                            blocks[x][y][z] = (int)BlockType::Dirt;
                        } else if (depthBelowSurface <= 25) {
                            blocks[x][y][z] = (int)BlockType::Stone;
                        } else {
                            blocks[x][y][z] = (int)BlockType::DeepStone;
                        }
                    }
                }
            }
        }

        // ====== Pass 2: 洞穴掏空 ======
        if (params.cavesEnabled) {
            int maxWorldY = baseWorldY + CS;
            for (unsigned int x = 0; x < CHUNK_SIZE; x++) {
                for (unsigned int z = 0; z < CHUNK_SIZE; z++) {
                    for (unsigned int y = 0; y < CHUNK_SIZE; y++) {
                        if (blocks[x][y][z] == (int)BlockType::Air) continue;
                        if (blocks[x][y][z] == (int)BlockType::Bedrock) continue;

                        float wx = (float)(baseWorldX + (int)x);
                        float wy = (float)(baseWorldY + (int)y);
                        float wz = (float)(baseWorldZ + (int)z);

                        // 应用域扭曲强度参数
                        float wwx = wx, wwy = wy, wwz = wz;
                        if (params.caveWarpStrength > 0.0f) {
                            wwx += caveWarpX->GetNoise(wx, wy, wz) * params.caveWarpStrength * 15.0f;
                            wwy += caveWarpY->GetNoise(wx, wy, wz) * params.caveWarpStrength * 15.0f;
                            wwz += caveWarpZ->GetNoise(wx, wy, wz) * params.caveWarpStrength * 15.0f;
                        }

                        if (isCheeseCave(wwx, wwy, wwz, cheeseCaveNoise,
                                         caveWarpX, caveWarpY, caveWarpZ, maxWorldY) ||
                            isSpaghettiCave(wwx, wwy, wwz, spaghettiCaveNoise,
                                            caveWarpX, caveWarpY, caveWarpZ) ||
                            isNoodleCave(wwx, wwy, wwz, noodleCaveNoise,
                                         caveWarpX, caveWarpY, caveWarpZ) ||
                            isRavine(wwx, wwy, wwz, ravineNoise, maxWorldY)) {
                            blocks[x][y][z] = (int)BlockType::Air;
                        }
                    }
                }
            }
        }

        // ====== Pass 3: 统一表层水填充（全球海平面） ======
        // 所有低于 WATER_LEVEL 的空气方块统一填充为水
        // 这保证了同一区域（甚至全局）水面高度统一
        for (unsigned int x = 0; x < CHUNK_SIZE; x++) {
            for (unsigned int z = 0; z < CHUNK_SIZE; z++) {
                for (unsigned int y = 0; y < CHUNK_SIZE; y++) {
                    int& bt = blocks[x][y][z];
                    if (bt != (int)BlockType::Air) continue;
                    int worldY = baseWorldY + (int)y;
                    if (worldY <= waterLevel) {
                        bt = (int)BlockType::Water;
                    }
                }
            }
        }

        // ====== Pass 4: 含水层填充（仅地表以下、海平面以上的洞穴水体） ======
        if (params.aquifersEnabled) {
            for (unsigned int x = 0; x < CHUNK_SIZE; x++) {
                for (unsigned int z = 0; z < CHUNK_SIZE; z++) {
                    float terrainSurface = erodedHeightCache[x][z];
                    for (unsigned int y = 0; y < CHUNK_SIZE; y++) {
                        int& bt = blocks[x][y][z];
                        if (bt != (int)BlockType::Air) continue;
                        float wx = (float)(baseWorldX + (int)x);
                        float wy = (float)(baseWorldY + (int)y);
                        float wz = (float)(baseWorldZ + (int)z);
                        if (wy >= terrainSurface) continue;
                        fillAquifers(wx, wy, wz, aquiferNoise, aquiferBarrierNoise,
                                     continentalNoise, bt);
                    }
                }
            }
        }

        // ====== Pass 5: 河流水填充 ======
        if (params.riversEnabled) {
            for (unsigned int x = 0; x < CHUNK_SIZE; x++) {
                for (unsigned int z = 0; z < CHUNK_SIZE; z++) {
                    float wx = (float)(baseWorldX + (int)x);
                    float wz = (float)(baseWorldZ + (int)z);
                    float baseHeight = erodedHeightCache[x][z];
                    for (unsigned int y = 0; y < CHUNK_SIZE; y++) {
                        int& bt = blocks[x][y][z];
                        if (bt != (int)BlockType::Air) continue;
                        float wy = (float)(baseWorldY + (int)y);
                        fillRiverWater(wx, wy, wz, baseHeight,
                                       riverNoise, continentalNoise, bt);
                    }
                }
            }
        }

        // ====== Pass 6: 树木生成 ======
        // 只在距离区块边界至少 3 格的位置生成，避免跨区块问题
        if (params.treesEnabled) {
        for (unsigned int x = 3; x < CHUNK_SIZE - 3; x++) {
            for (unsigned int z = 3; z < CHUNK_SIZE - 3; z++) {
                // 找地表 Y：从上往下扫描，找到第一个非空气、非水、非树木类方块
                // 跳过树木类方块（Log/Leaves/Cactus），避免在已有的树上再堆树
                int surfaceY = -1;
                for (int y = CS - 1; y >= 0; y--) {
                    int bt = blocks[x][y][z];
                    if (bt == (int)BlockType::Air || bt == (int)BlockType::Water
                        || bt == (int)BlockType::Log
                        || bt == (int)BlockType::Leaves
                        || bt == (int)BlockType::Cactus) {
                        continue;
                    }
                    surfaceY = y;
                    break;
                }
                if (surfaceY < 0) continue;

                float wx = (float)(baseWorldX + (int)x);
                float wz = (float)(baseWorldZ + (int)z);
                float terrainH = (float)(baseWorldY + surfaceY);
                BiomeType biome = biomeCache[x][z];

                if (shouldPlaceTree(wx, wz, biome, detailNoise, terrainH, waterLevel)) {
                    float treeRnd = normalizeNoise(detailNoise->GetNoise(wx * 100.0f, wz * 100.0f));
                    TreeType treeType = getTreeTypeForBiome(biome, treeRnd);

                    switch (treeType) {
                        case TreeType::Oak:
                        case TreeType::Birch:
                        case TreeType::SwampOak:
                            placeOakTree((int)x, surfaceY, (int)z, blocks, detailNoise);
                            break;
                        case TreeType::Spruce:
                            placeSpruceTree((int)x, surfaceY, (int)z, blocks, detailNoise);
                            break;
                        case TreeType::CactusTree:
                            placeCactusPlant((int)x, surfaceY, (int)z, blocks, detailNoise);
                            break;
                        default:
                            placeOakTree((int)x, surfaceY, (int)z, blocks, detailNoise);
                    }
                }
            }
        }
        }

        generated = true;
    }
};

// ============================================================================
// 生物群系实现
// ============================================================================
inline BiomeType ChunkData::getBiomeAt(float continental, float erosion,
                                       float temp, float humid, float weird) {
    if (continental < 0.35f) {
        if (continental < 0.2f && weird > 0.6f) return BiomeType::DeepOcean;
        if (temp > 0.7f) return BiomeType::WarmOcean;
        return BiomeType::Ocean;
    }
    if (continental < 0.4f) return BiomeType::Plains; // 沙滩由地表方块处理

    if (weird > 0.9f && humid > 0.6f) return BiomeType::MushroomFields;

    if (temp < 0.2f) {
        if (continental > 0.6f && weird > 0.7f) return BiomeType::IceSpikes;
        if (humid > 0.5f) return BiomeType::SnowyTaiga;
        return BiomeType::SnowyTundra;
    }
    if (temp < 0.4f) {
        if (humid > 0.4f) return BiomeType::Taiga;
        if (continental > 0.65f) return BiomeType::SnowyMountains;
        return BiomeType::Plains;
    }
    if (temp > 0.8f) {
        if (humid < 0.3f) {
            if (erosion < 0.3f && weird > 0.8f) return BiomeType::Badlands;
            return BiomeType::Desert;
        }
        if (humid > 0.6f) return BiomeType::Jungle;
        return BiomeType::Savanna;
    }
    if (humid > 0.7f) {
        if (temp > 0.5f && weird > 0.6f) return BiomeType::Jungle;
        return BiomeType::Swamp;
    }
    if (continental > 0.65f) {
        if (temp > 0.5f) return BiomeType::Mountains;
        if (erosion > 0.5f) return BiomeType::GravellyMountains;
        return BiomeType::Mountains;
    }
    if (humid > 0.5f) return BiomeType::Forest;
    if (weird > 0.7f && humid > 0.3f) return BiomeType::FlowerForest;
    return BiomeType::Plains;
}

inline int ChunkData::getSurfaceBlockAt(BiomeType biome, int worldY, int waterLevel) {
    if (worldY <= waterLevel) return (int)BlockType::Sand;
    switch (biome) {
        case BiomeType::Desert:
        case BiomeType::Savanna:
            return (int)BlockType::Sand;
        case BiomeType::SnowyTundra:
        case BiomeType::SnowyTaiga:
        case BiomeType::SnowyMountains:
        case BiomeType::IceSpikes:
            return (int)BlockType::Snow;
        case BiomeType::Badlands:
            return (int)BlockType::Sand;
        case BiomeType::GravellyMountains:
            return (int)BlockType::Stone;
        default:
            return (int)BlockType::Grass;
    }
}

// ============================================================================
// 洞穴实现
// ============================================================================
inline void ChunkData::warpCoordinate(float& wx, float& wy, float& wz,
                                      FastNoiseLite* wxN, FastNoiseLite* wyN, FastNoiseLite* wzN,
                                      float strength) {
    wx += wxN->GetNoise(wx, wy, wz) * strength;
    wy += wyN->GetNoise(wx, wy, wz) * strength;
    wz += wzN->GetNoise(wx, wy, wz) * strength;
}

inline bool ChunkData::isCheeseCave(float wx, float wy, float wz,
                                    FastNoiseLite* cn,
                                    FastNoiseLite* cwX, FastNoiseLite* cwY, FastNoiseLite* cwZ,
                                    int maxWorldY) {
    float cx = wx * 0.5f, cy = wy * 0.5f, cz = wz * 0.5f;
    warpCoordinate(cx, cy, cz, cwX, cwY, cwZ, 15.0f);
    float v = cn->GetNoise(cx, cy, cz);
    float depthFactor = std::max(0.0f, (float)(maxWorldY - wy) / (float)maxWorldY);
    float threshold = 0.5f - depthFactor * 0.3f;
    return v > threshold;
}

inline bool ChunkData::isSpaghettiCave(float wx, float wy, float wz,
                                       FastNoiseLite* cn,
                                       FastNoiseLite* cwX, FastNoiseLite* cwY, FastNoiseLite* cwZ) {
    float cx = wx, cy = wy, cz = wz;
    warpCoordinate(cx, cy, cz, cwX, cwY, cwZ, 20.0f);
    warpCoordinate(cx, cy, cz, cwX, cwY, cwZ, 10.0f);
    float v = cn->GetNoise(cx, cy, cz);
    return std::abs(v) < 0.15f;
}

inline bool ChunkData::isNoodleCave(float wx, float wy, float wz,
                                    FastNoiseLite* cn,
                                    FastNoiseLite* cwX, FastNoiseLite* cwY, FastNoiseLite* cwZ) {
    if (wy > 30.0f) return false;
    float cx = wx, cy = wy, cz = wz;
    warpCoordinate(cx, cy, cz, cwX, cwY, cwZ, 25.0f);
    warpCoordinate(cx, cy, cz, cwX, cwY, cwZ, 15.0f);
    float v = cn->GetNoise(cx, cy, cz);
    return std::abs(v) < 0.08f;
}

inline bool ChunkData::isRavine(float wx, float wy, float wz,
                                FastNoiseLite* rn, int maxWorldY) {
    float v = rn->GetNoise(wx, wz);
    float v2 = rn->GetNoise(wx + 0.5f, wz + 0.5f);
    bool nearRavine = std::abs(v) < 0.05f && std::abs(v - v2) > 0.1f;
    if (!nearRavine) return false;
    // 裂谷：地表宽、深层窄，y 越高(maxWorldY方向)越宽
    float maxWidth = 4.0f + wy * 0.015f;
    return std::abs(v) * 100.0f < maxWidth;
}

// ============================================================================
// 含水层实现
// ============================================================================
inline float ChunkData::getLocalWaterLevel(float wx, float wy, float wz,
                                           FastNoiseLite* aq, FastNoiseLite* cn) {
    float a = aq->GetNoise(wx * 0.3f, wy * 0.3f, wz * 0.3f);
    float continental = normalizeNoise(cn->GetNoise(wx, wz));
    float cFac = 1.0f - continental;
    return 25.0f + a * 12.0f + cFac * 15.0f;
}

inline bool ChunkData::isAquiferBarrier(float wx, float wy, float wz,
                                        FastNoiseLite* bn) {
    float b = bn->GetNoise(wx * 0.5f, wy * 0.5f, wz * 0.5f);
    return b > 0.3f && wy > 10.0f && wy < 50.0f;
}

inline void ChunkData::fillAquifers(float wx, float wy, float wz,
                                    FastNoiseLite* aq, FastNoiseLite* bn,
                                    FastNoiseLite* cn, int& bt) {
    if (bt != (int)BlockType::Air) return;
    if (wy <= 2.0f) return;
    float lvl = getLocalWaterLevel(wx, wy, wz, aq, cn);
    if (wy < lvl && !isAquiferBarrier(wx, wy, wz, bn))
        bt = (int)BlockType::Water;
}

// ============================================================================
// 河流实现
// ============================================================================
inline float ChunkData::getRiverValue(float wx, float wz,
                                      FastNoiseLite* rn, FastNoiseLite* cn) {
    float continental = normalizeNoise(cn->GetNoise(wx, wz));
    if (continental < 0.35f) return -1.0f;
    float v = rn->GetNoise(wx, wz);
    float rAbs = std::abs(v);
    float dx = rn->GetNoise(wx + 0.5f, wz) - rn->GetNoise(wx - 0.5f, wz);
    float dz = rn->GetNoise(wx, wz + 0.5f) - rn->GetNoise(wx, wz - 0.5f);
    float grad = std::sqrt(dx * dx + dz * dz);
    float threshold = 0.03f + continental * 0.02f;
    if (rAbs < threshold && grad > 0.02f) return threshold - rAbs;
    return -1.0f;
}

inline float ChunkData::applyRiverErosion(float wx, float wz, float h,
                                          FastNoiseLite* rn, FastNoiseLite* cn) {
    float rv = getRiverValue(wx, wz, rn, cn);
    if (rv < 0.0f) return h;
    float erosion = rv * 8.0f / 0.03f;
    erosion = std::min(erosion, 8.0f);
    return h - erosion;
}

inline void ChunkData::fillRiverWater(float wx, float wy, float wz, float h,
                                      FastNoiseLite* rn, FastNoiseLite* cn, int& bt) {
    if (bt != (int)BlockType::Air) return;
    float rv = getRiverValue(wx, wz, rn, cn);
    if (rv < 0.0f) return;
    if (wy < h + 2.0f && wy >= h - 1.0f)
        bt = (int)BlockType::Water;
}

// ============================================================================
// 树木实现
// ============================================================================
inline TreeType ChunkData::getTreeTypeForBiome(BiomeType biome, float r) {
    switch (biome) {
        case BiomeType::Forest:      return r > 0.2f ? TreeType::Oak : TreeType::Birch;
        case BiomeType::FlowerForest:return TreeType::Oak;
        case BiomeType::Taiga:
        case BiomeType::SnowyTaiga:  return TreeType::Spruce;
        case BiomeType::Jungle:      return TreeType::Jungle;
        case BiomeType::Savanna:     return TreeType::Acacia;
        case BiomeType::Swamp:       return TreeType::SwampOak;
        case BiomeType::Desert:
        case BiomeType::Badlands:    return TreeType::CactusTree;
        case BiomeType::Plains:      return TreeType::Oak;
        default:                     return TreeType::Oak;
    }
}

inline bool ChunkData::shouldPlaceTree(float wx, float wz, BiomeType biome,
                                       FastNoiseLite* dn, float th, int wl) {
    float density = 0.0f;
    switch (biome) {
        case BiomeType::Forest:       density = 0.6f; break;
        case BiomeType::FlowerForest: density = 0.4f; break;
        case BiomeType::Taiga:        density = 0.7f; break;
        case BiomeType::Jungle:       density = 0.8f; break;
        case BiomeType::Swamp:        density = 0.3f; break;
        case BiomeType::Savanna:      density = 0.1f; break;
        case BiomeType::Plains:       density = 0.02f; break;
        case BiomeType::Desert:
        case BiomeType::Badlands:     density = 0.05f; break;
        default: return false;
    }
    if (density <= 0.0f) return false;
    float n = dn->GetNoise(wx * 10.0f, wz * 10.0f);
    return normalizeNoise(n) < density && th > (float)(wl + 2);
}

static void setSafe(int x, int y, int z, int bt,
                    int blocks[ChunkData::CHUNK_SIZE][ChunkData::CHUNK_SIZE][ChunkData::CHUNK_SIZE]) {
    const int CS = (int)ChunkData::CHUNK_SIZE;
    if (x < 0 || x >= CS || y < 0 || y >= CS || z < 0 || z >= CS) return;
    if (blocks[x][y][z] == (int)BlockType::Air)
        blocks[x][y][z] = bt;
}

inline void ChunkData::placeOakTree(int bx, int by, int bz,
                                    int blocks[CHUNK_SIZE][CHUNK_SIZE][CHUNK_SIZE],
                                    FastNoiseLite* dn) {
    float hn = dn->GetNoise((float)bx * 0.1f, (float)bz * 0.1f);
    int trunkH = 4 + (int)((hn + 1.0f) * 1.5f);
    for (int dy = 0; dy < trunkH; dy++)
        setSafe(bx, by + dy + 1, bz, (int)BlockType::Log, blocks);

    struct LeafLayer { int yo; int rx; int rz; };
    std::vector<LeafLayer> layers = {
        {trunkH - 1, 2, 2}, {trunkH, 2, 2}, {trunkH + 1, 1, 1}, {trunkH + 2, 0, 0}
    };
    for (auto& l : layers) {
        int cy = by + l.yo + 1;
        for (int dx = -l.rx; dx <= l.rx; dx++)
            for (int dz = -l.rz; dz <= l.rz; dz++) {
                float dist = std::sqrt((float)(dx*dx + dz*dz));
                if (dist > (float)l.rx) continue;
                if (l.yo >= trunkH - 1 && dist > (float)(l.rx - 0.5f)) continue;
                setSafe(bx + dx, cy, bz + dz, (int)BlockType::Leaves, blocks);
            }
    }
}

inline void ChunkData::placeSpruceTree(int bx, int by, int bz,
                                       int blocks[CHUNK_SIZE][CHUNK_SIZE][CHUNK_SIZE],
                                       FastNoiseLite* dn) {
    float hn = dn->GetNoise((float)bx * 0.1f, (float)bz * 0.1f);
    int trunkH = 6 + (int)((hn + 1.0f) * 1.5f);
    for (int dy = 0; dy < trunkH; dy++)
        setSafe(bx, by + dy + 1, bz, (int)BlockType::Log, blocks);

    int leafStart = 2;
    for (int dy = leafStart; dy < trunkH; dy++) {
        int rad = (trunkH - dy) / 2 + 1;
        int cy = by + dy + 1;
        for (int dx = -rad; dx <= rad; dx++)
            for (int dz = -rad; dz <= rad; dz++) {
                if (std::abs(dx) + std::abs(dz) > rad + 1) continue;
                setSafe(bx + dx, cy, bz + dz, (int)BlockType::Leaves, blocks);
            }
    }
    setSafe(bx, by + trunkH + 1, bz, (int)BlockType::Leaves, blocks);
}

inline void ChunkData::placeCactusPlant(int bx, int by, int bz,
                                        int blocks[CHUNK_SIZE][CHUNK_SIZE][CHUNK_SIZE],
                                        FastNoiseLite* dn) {
    float hn = dn->GetNoise((float)bx * 0.1f, (float)bz * 0.1f);
    int h = 2 + (int)((hn + 1.0f) * 1.0f);
    h = std::min(h, 3);
    for (int dy = 0; dy < h; dy++)
        setSafe(bx, by + dy + 1, bz, (int)BlockType::Cactus, blocks);
}

} // namespace GAME