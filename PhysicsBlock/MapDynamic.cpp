#include "MapDynamic.hpp"
#include <algorithm>

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

    /**
     * @brief 构造函数 - 创建动态地图，初始化板块网格和移动控制台
     * @param Width 板块 X 方向数量
     * @param Height 板块 Y 方向数量
     * @note 使用 Morton 编码优化相邻板块的内存局部性 */
    MapDynamic::MapDynamic(const unsigned int Width, const unsigned int Height):
        width(Width), height(Height), mMovePlate(Width, Height, PixelBlockEdgeSize, Width / 2, Height / 2), 
        centrality({Width / 2 * PixelBlockEdgeSize, Height / 2 * PixelBlockEdgeSize}),
        BaseGrid(Width * PixelBlockEdgeSize, Height * PixelBlockEdgeSize, nullptr)
    {
        type = PhysicsObjectEnum::_MapDynamic;
        BaseGridBuffer = (BaseGrid *)new char[width * height * sizeof(BaseGrid)];
        GridBuffer = new GridBlock[width * height * PixelBlockEdgeSize * PixelBlockEdgeSize];
        for (size_t i = 0; i < (width * height); ++i)
        {
            new(&BaseGridBuffer[i]) BaseGrid(PixelBlockEdgeSize, PixelBlockEdgeSize, &GridBuffer[PixelBlockEdgeSize * PixelBlockEdgeSize * i]);
        }
        bool *DataBool = new bool[width * height];
        for (size_t i = 0; i < (width * height); ++i)
        {
            DataBool[i] = false;
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
                i = static_cast<int>(1) << i;
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

    /**
     * @brief 析构函数 - 释放所有板块网格资源 */
    MapDynamic::~MapDynamic()
    {
        for (size_t i = 0; i < (width * height); ++i)
        {
            BaseGridBuffer[i].~BaseGrid();
        }
        delete (char *)BaseGridBuffer;
        delete GridBuffer;
    }

    /**
     * @brief 整数坐标 Bresenham 线段碰撞检测
     * @param start 起始网格坐标
     * @param end 结束网格坐标
     * @return 碰撞信息（包含是否碰撞、碰撞位置、摩擦系数） */
    CollisionInfoI MapDynamic::FMBresenhamDetection(glm::ivec2 start, glm::ivec2 end)
    {
        int sx = (start.x < end.x) ? 1 : -1;
        int sy = (start.y < end.y) ? 1 : -1;
        int dx = abs(end.x - start.x);
        int dy = -abs(end.y - start.y);
        int err = dx + dy;
        while (true)
        {
            GridBlock& block = at(start.x, start.y);
            if (block.Collision)
            {
                return { true, start, block.FrictionFactor };
            }
            if (end.x == start.x && end.y == start.y)
            {
                return {false, glm::ivec2{0, 0}, FLOAT_{}};
            }
            int e2 = err << 1;
            if (e2 >= dy)
            {
                err += dy;
                start.x += sx;
            }
            if (e2 <= dx)
            {
                err += dx;
                start.y += sy;
            }
        }
    }

    /**
     * @brief 浮点坐标 Bresenham 线段碰撞检测
     * @param start 起始世界坐标
     * @param end 结束世界坐标
     * @return 碰撞信息（包含是否碰撞、精确碰撞位置、碰撞方向、摩擦系数）
     * @note 在整数检测基础上，通过插值计算精确碰撞点和碰撞方向 */
    CollisionInfoD MapDynamic::FMBresenhamDetection(Vec2_ start, Vec2_ end)
    {
            CollisionInfoI infoI = FMBresenhamDetection(ToInt(start), ToInt(end));
            if (infoI.Collision)
            {
                CollisionInfoD info;
                info.Friction = infoI.Friction;
                info.Collision = true;
                info.pos = infoI.pos;
                FLOAT_ Difference = (end.x - start.x) / (start.y - end.y);
                FLOAT_ invDifference = 1.0 / Difference;
                FLOAT_ val = start.x - end.x;
                if (val < 0)
                {
                    info.pos = {infoI.pos.x, ((end.x - infoI.pos.x) * invDifference) + end.y};
                    info.Direction = CheckDirection::Left;
                }
                else if (val > 0)
                {
                    info.pos = {infoI.pos.x + 1, ((end.x - infoI.pos.x - 1) * invDifference) + end.y};
                    info.Direction = CheckDirection::Right;
                }
                if ((val == 0) || (info.pos.y < infoI.pos.y) || (info.pos.y > (infoI.pos.y + 1)))
                {
                    val = start.y - end.y;
                    if (val < 0)
                    {
                        info.pos = {(Difference * (end.y - infoI.pos.y)) + end.x, infoI.pos.y};
                        info.Direction = CheckDirection::Down;
                    }
                    else if (val > 0)
                    {
                        info.pos = {(Difference * (end.y - infoI.pos.y - 1)) + end.x, infoI.pos.y + 1};
                        info.Direction = CheckDirection::Up;
                    }
                    if (at(infoI.pos.x, infoI.pos.y + (CheckDirection::Down == info.Direction ? -1 : 1)).Collision)
                    {
                        val = start.x - end.x;
                        if (val < 0)
                        {
                            info.pos = {infoI.pos.x, ((end.x - infoI.pos.x) * invDifference) + end.y};
                            info.Direction = CheckDirection::Left;
                        }
                        else if (val > 0)
                        {
                            ++infoI.pos.x;
                            info.pos = {infoI.pos.x, ((end.x - infoI.pos.x) * invDifference) + end.y};
                            info.Direction = CheckDirection::Right;
                        }
                    }
                }
                else
                {
                    if (at(infoI.pos.x + (CheckDirection::Left == info.Direction ? -1 : 1), infoI.pos.y).Collision)
                    {
                        val = start.y - end.y;
                        if (val < 0)
                        {
                            info.pos = {(Difference * (end.y - infoI.pos.y)) + end.x, infoI.pos.y};
                            info.Direction = CheckDirection::Down;
                        }
                        else if (val > 0)
                        {
                            ++infoI.pos.y;
                            info.pos = {(Difference * (end.y - infoI.pos.y)) + end.x, infoI.pos.y};
                            info.Direction = CheckDirection::Up;
                        }
                    }
                }
                return info;
            }
        return {false};
    }

    /**
     * @brief 安全 Bresenham 线段碰撞检测（边界裁剪）
     * @param start 起始网格坐标
     * @param end 结束网格坐标
     * @return 碰撞信息
     * @note 坐标会被裁剪到地图有效范围内，防止越界访问 */
    CollisionInfoI MapDynamic::FMSafeBresenhamDetection(glm::ivec2 start, glm::ivec2 end)
    {
        start.x = std::max(0, std::min(start.x, (int)BaseGrid::width - 1));
        start.y = std::max(0, std::min(start.y, (int)BaseGrid::height - 1));
        end.x = std::max(0, std::min(end.x, (int)BaseGrid::width - 1));
        end.y = std::max(0, std::min(end.y, (int)BaseGrid::height - 1));
        return FMBresenhamDetection(start, end);
    }

    std::vector<MapOutline> MapDynamic::GetLightweightOutline(int x_, int y_, int w_, int h_)
    {
        std::vector<MapOutline> Outline;

        for (int x = x_; x < w_; ++x)
        {
            for (int y = y_; y < h_; ++y)
            {
                if (!at(x, y).Entity)
                {
                    continue;
                }
                if (!at(x - 1, y - 1).Entity)
                {
                    if (at(x - 1, y).Entity == at(x, y - 1).Entity)
                    {
                        Outline.push_back({Vec2_{x, y}, Vec2_{-1, -1}, at(x, y).FrictionFactor});
                    }
                }
                else if (!at(x - 1, y).Entity || !at(x, y - 1).Entity)
                {
                    Outline.push_back({Vec2_{x, y}, Vec2_{-1, -1}, at(x, y).FrictionFactor});
                }
                if (!at(x, y + 1).Entity)
                {
                    if (!(at(x - 1, y).Entity && !at(x - 1, y + 1).Entity))
                    {
                        Outline.push_back({Vec2_{x, y + 1}, Vec2_{-1, 1}, at(x, y).FrictionFactor});
                    }
                }
                if (!at(x + 1, y).Entity)
                {
                    if ((!at(x, y - 1).Entity && !at(x + 1, y - 1).Entity))
                    {
                        Outline.push_back({Vec2_{x + 1, y}, Vec2_{1, -1}, at(x, y).FrictionFactor});
                    }
                }
                if (!at(x + 1, y + 1).Entity)
                {
                    if ((at(x + 1, y).Entity == at(x, y + 1).Entity) && !at(x + 1, y).Entity)
                    {
                        Outline.push_back({Vec2_{x + 1, y + 1}, Vec2_{1, 1}, at(x, y).FrictionFactor});
                    }
                }
            }
        }
        return Outline;
    }

    std::vector<MapOutline> MapDynamic::GetOutline(int x_, int y_, int w_, int h_)
    {
        std::vector<MapOutline> Outline;

        for (int x = x_; x < w_; ++x)
        {
            for (int y = y_; y < h_; ++y)
            {
                if (!at(x, y).Entity)
                {
                    continue;
                }

                if (!at(x - 1, y).Entity || !at(x, y - 1).Entity || !at(x - 1, y - 1).Entity)
                {
                    Outline.push_back({Vec2_{x, y}, Vec2_{-1, -1}, at(x, y).FrictionFactor});
                }
                if (!at(x, y + 1).Entity)
                {
                    Outline.push_back({Vec2_{x, y + 1}, Vec2_{-1, 1}, at(x, y).FrictionFactor});
                }
                if (!(at(x + 1, y - 1).Entity || at(x + 1, y).Entity))
                {
                    Outline.push_back({Vec2_{x + 1, y}, Vec2_{1, -1}, at(x, y).FrictionFactor});
                }
                if (!(at(x + 1, y).Entity || at(x + 1, y + 1).Entity || at(x, y + 1).Entity))
                {
                    Outline.push_back({Vec2_{x + 1, y + 1}, Vec2_{1, 1}, at(x, y).FrictionFactor});
                }
            }
        }
        return Outline;
    }

    /**
     * @brief 安全设置碰撞状态（带越界检查和回调通知） */
    bool MapDynamic::SafeSetCollision(glm::ivec2 pos, bool state)
    {
        if (pos.x < 0 || pos.y < 0 || (unsigned int)pos.x >= BaseGrid::width || (unsigned int)pos.y >= BaseGrid::height)
            return false;
        GridBlock &block = at(pos.x, pos.y);
        if (block.Collision != state) {
            block.Collision = state;
            if (!state) {
                block.Entity = false;
                block.Healthpoint = 0;
            }
            // 通知碰撞状态变化（通过 MapFormwork 桥接器转发到 PhysicsCollision）
            if (mCollisionChangeNotifier) {
                mCollisionChangeNotifier(pos, state);
            }
            return true;
        }
        return false;
    }

    /**
     * @brief 设置指定位置的摩擦力系数（带越界检查） */
    void MapDynamic::SetFriction(int x, int y, FLOAT_ friction)
    {
        if (x < 0 || y < 0 || (unsigned int)x >= BaseGrid::width || (unsigned int)y >= BaseGrid::height)
            return;
        at(x, y).FrictionFactor = friction;
    }

    /**
     * @brief 获取指定位置的摩擦力系数（带越界检查） */
    FLOAT_ MapDynamic::GetFriction(int x, int y)
    {
        if (x < 0 || y < 0 || (unsigned int)x >= BaseGrid::width || (unsigned int)y >= BaseGrid::height)
            return FLOAT_{};
        return at(x, y).FrictionFactor;
    }

}
