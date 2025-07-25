#include "MapDynamic.hpp"

namespace PhysicsBlock
{
    /**
     * @brief 获取最大2幂是几次幂
     * @param val 值
     * @return 几次幂 */
    int Power(unsigned int val, int i = -1)
    {
        return val > 0 ? Power(val >>= 1, ++i) : i;
    }

    MapDynamic::MapDynamic(const unsigned int Width, const unsigned int Height):
        width(Width), height(Height), mMovePlate(Width, Height, PixelBlockEdgeSize, Width / 2, Height / 2), 
        centrality({Width / 2 * PixelBlockEdgeSize, Height / 2 * PixelBlockEdgeSize}),
        BaseGrid(Width, Height, nullptr)
    {
        BaseGridBuffer = (BaseGrid *)new char[width * height * sizeof(BaseGrid)];
        GridBuffer = new GridBlock[width * height * PixelBlockEdgeSize * PixelBlockEdgeSize];
        bool *DataBool = new bool[width * height];
        for (size_t i = 0; i < (width * height); ++i)
        {
            DataBool[i] = false;
            //new(&BaseGridBuffer[i])BaseGrid(PixelBlockEdgeSize, PixelBlockEdgeSize, &GridBuffer[PixelBlockEdgeSize * PixelBlockEdgeSize * i]);
        }


        /* 将网格分配到移动板块控制中心，让相邻的坐标内存也尽可能的相邻（提高内存命中率） */

        unsigned int GetNum = 0;
        unsigned int wh = width > height ? height : width;
        for (size_t y = 0; y < height; ++y)
        {
            for (size_t x = 0; x < width; ++x)
            {
                if (DataBool[x * height + y])
                    continue;
                int e;
                for (e = 1; e < wh; ++e)
                {
                    bool OK = false;
                    int fx = (x + e);
                    int fy = (y + e);
                    if ((fx > width) || (fy > height))
                    {
                        break;
                    }
                    for (size_t xx = x; xx <= fx; ++xx)
                    {
                        if (DataBool[xx * height + fy])
                        {
                            OK = true;
                            break;
                        }
                    }
                    if (OK)
                    {
                        break;
                    }
                    for (size_t xy = y; xy < fy; ++xy)
                    {
                        if (DataBool[fx * height + xy])
                        {
                            OK = true;
                            break;
                        }
                    }
                    if (OK)
                    {
                        break;
                    }
                }
                if (e < 2)
                {
                    *mMovePlate.PlateAt(x, y) = &BaseGridBuffer[GetNum];
                    ++GetNum;
                    continue;
                }
                int i = Power(e);
                i = (int)1 << i;
                unsigned int PY = GetNum;
                for (size_t posx = 0; posx < i; ++posx)
                {
                    for (size_t posy = 0; posy < i; ++posy)
                    {
                        ++GetNum;
                        unsigned int MortonVal = PY + Morton2D(posx, posy);
                        assert((MortonVal < (width * height)) && "BaseDefine 的 PixelBlockEdgeSize 只可以是 2 的幂次方");
                        assert(!DataBool[(x + posx) * height + (y + posy)] && "BaseDefine 的 PixelBlockEdgeSize 只可以是 2 的幂次方");
                        DataBool[(x + posx) * height + (y + posy)] = true;
                        *mMovePlate.PlateAt(x + posx, y + posy) = &BaseGridBuffer[MortonVal];
                    }
                }
            }
        }
        delete DataBool;
    }

    MapDynamic::~MapDynamic()
    {
        delete (char *)BaseGridBuffer;
        delete GridBuffer;
    }

    CollisionInfoI MapDynamic::FMBresenhamDetection(glm::ivec2 start, glm::ivec2 end)
    {
        return BresenhamDetection(start, end);
    }

    CollisionInfoD MapDynamic::FMBresenhamDetection(Vec2_ start, Vec2_ end)
    {
        // 偏移中心位置，对其网格坐标系
        start += centrality;
        end += centrality;

        // 裁剪线段 让线段都在矩形内
        PhysicsBlock::SquareFocus data = PhysicsBlock::LineSquareFocus(start, end, width - 0.0001, height - 0.0001);
        if (data.Focus)
        {
            // 线段碰撞检测
            CollisionInfoD info = BresenhamDetection(data.start, data.end);
            if (info.Collision)
            {
                // 返回物理坐标系
                info.pos -= centrality;
                return info;
            }
        }
        return {false};
    }

}
