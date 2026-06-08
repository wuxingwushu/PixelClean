#pragma once

#include "../../Tool/FastNoiseLite.h"
#include <cstring>

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
    Snow = 8
};

struct ChunkData {
    static constexpr unsigned int CHUNK_SIZE = 32;
    static constexpr int WATER_LEVEL = 32;
    static constexpr int MAX_HEIGHT = CHUNK_SIZE * 3;

    int worldX, worldY, worldZ;
    bool generated = false;

    int blocks[CHUNK_SIZE][CHUNK_SIZE][CHUNK_SIZE]{};

    ChunkData(int wx, int wy, int wz)
        : worldX(wx), worldY(wy), worldZ(wz) {
        std::memset(blocks, 0, sizeof(blocks));
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

    void generateTerrain(
        FastNoiseLite* continentalNoise,
        FastNoiseLite* hillsNoise,
        FastNoiseLite* detailNoise,
        FastNoiseLite* caveNoise,
        FastNoiseLite* erosionNoise,
        int seed)
    {
        // 将种子混入噪声，让每个区块生成不同地形
        float seedOffsetX = (float)((seed * 1103515245u) & 0x7FFFFFFF) * 0.0001f;
        float seedOffsetZ = (float)((seed * 1234567891u) & 0x7FFFFFFF) * 0.0001f;

        for (unsigned int x = 0; x < CHUNK_SIZE; x++) {
            for (unsigned int z = 0; z < CHUNK_SIZE; z++) {
                float wx = (float)(worldX * (int)CHUNK_SIZE + (int)x) + seedOffsetX;
                float wz = (float)(worldZ * (int)CHUNK_SIZE + (int)z) + seedOffsetZ;

                // 大陆噪声：决定基础高度，放大到 [0, 70]
                float continental = continentalNoise->GetNoise(wx, wz);

                // 丘陵噪声：叠加中等起伏
                float hills = hillsNoise->GetNoise(wx, wz) * 0.35f;

                // 侵蚀噪声：降低某些区域的高度
                float erosion = erosionNoise->GetNoise(wx, wz) * 0.2f;

                // 合并计算地形高度，范围大约 [3, 75]
                float heightF = ((continental + 1.0f) * 0.5f * 60.0f)
                              + (hills * 20.0f)
                              - (std::abs(erosion) * 12.0f)
                              + 10.0f;
                int terrainHeight = (int)heightF;
                terrainHeight = std::max(1, terrainHeight);

                for (unsigned int y = 0; y < CHUNK_SIZE; y++) {
                    int chunkWorldY = (int)(worldY * (int)CHUNK_SIZE + (int)y);

                    // 细节噪声：用于打破水平分层的3D扰动
                    float detail = detailNoise->GetNoise(wx, (float)chunkWorldY * 0.5f, wz);

                    // 洞穴噪声（3D）：用于挖洞
                    float cave3D = caveNoise->GetNoise(wx * 0.4f, (float)chunkWorldY * 0.5f, wz * 0.4f);

                    // 使用detail噪声扰动层边界，打破完美的水平分层
                    float perturbedHeight = terrainHeight + detail * 2.5f;

                    int blockType;

                    if (chunkWorldY > perturbedHeight) {
                        blockType = (int)BlockType::Air;
                    }
                    else if (chunkWorldY >= (int)perturbedHeight - 1) {
                        // 地表层：根据高度和噪声决定
                        if (chunkWorldY <= WATER_LEVEL) {
                            blockType = (int)BlockType::Sand;
                        } else if (chunkWorldY > 55) {
                            blockType = (int)BlockType::Snow;
                        } else if (terrainHeight > 18 + detail * 3.0f) {
                            blockType = (int)BlockType::Grass;
                        } else {
                            blockType = (int)BlockType::Sand;
                        }
                    }
                    else if (chunkWorldY >= (int)perturbedHeight - 7) {
                        // 泥土层：加厚并加入变化
                        blockType = (int)BlockType::Dirt;
                    }
                    else if (chunkWorldY >= (int)perturbedHeight - 18) {
                        // 石头层：用洞穴噪声挖洞
                        if (cave3D > 0.25f && chunkWorldY < perturbedHeight - 5) {
                            blockType = (int)BlockType::Air;
                        } else {
                            blockType = (int)BlockType::Stone;
                        }
                    }
                    else if (chunkWorldY >= (int)perturbedHeight - 30) {
                        // 深层石头
                        if (cave3D > 0.35f && chunkWorldY < perturbedHeight - 7) {
                            blockType = (int)BlockType::Air;
                        } else {
                            blockType = (int)BlockType::DeepStone;
                        }
                    }
                    else if (chunkWorldY <= 1) {
                        blockType = (int)BlockType::Bedrock;
                    }
                    else {
                        // 最深层：更多洞穴
                        if (cave3D > 0.15f) {
                            blockType = (int)BlockType::Air;
                        } else {
                            blockType = (int)BlockType::DeepStone;
                        }
                    }

                    // 水填充：低于海平面的空气变成水
                    if (chunkWorldY <= WATER_LEVEL && blockType == (int)BlockType::Air) {
                        blockType = (int)BlockType::Water;
                    }

                    blocks[x][y][z] = blockType;
                }
            }
        }

        generated = true;
    }
};

} // namespace GAME