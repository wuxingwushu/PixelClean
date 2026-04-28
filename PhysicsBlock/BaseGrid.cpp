#include "BaseGrid.hpp"
#include "BaseCalculate.hpp"

namespace PhysicsBlock
{

    /**
     * @brief 构造函数（内部创建网格）
     * @param Width 网格宽度
     * @param Height 网格高度
     * @details 1. 初始化宽度和高度
     * 2. 在内部申请 width * height 大小的 GridBlock 数组
     * 3. 设置 NewBool 为 true，表示网格由内部管理
     */
    BaseGrid::BaseGrid(const unsigned int Width, const unsigned int Height) : width(Width), height(Height)
    {
        Grid = new GridBlock[width * height];
        NewBool = true;
    }

    /**
     * @brief 构造函数（外部提供网格）
     * @param Width 网格宽度
     * @param Height 网格高度
     * @param Gridptr 外部提供的网格数据指针
     * @details 1. 初始化宽度和高度
     * 2. 使用外部提供的网格数据
     * 3. 设置 NewBool 为 false，表示网格由外部管理
     */
    BaseGrid::BaseGrid(const unsigned int Width, const unsigned int Height, GridBlock *Gridptr) : width(Width), height(Height), Grid(Gridptr)
    {
        NewBool = false;
    }

    /**
     * @brief 析构函数
     * @details 如果网格是内部创建的（NewBool 为 true），则释放网格内存
     */
    BaseGrid::~BaseGrid()
    {
        if (NewBool)
            delete Grid;
    }

    /**
     * @brief 线段碰撞检测（整数坐标）
     * @param start 起始位置（网格坐标）
     * @param end 结束位置（网格坐标）
     * @return 碰撞结果信息
     * @details 使用 Bresenham 算法从起点到终点遍历线段经过的网格
     * 返回第一个遇到碰撞的网格位置
     */
    CollisionInfoI BaseGrid::BresenhamDetection(glm::ivec2 start, glm::ivec2 end)
    {
        int dx = abs(end.x - start.x);
        int dy = abs(end.y - start.y);
        int sx = (start.x < end.x) ? 1 : -1;
        int sy = (start.y < end.y) ? 1 : -1;
        int err = dx - dy;
        int e2;
        const int w = width;
        const int h = height;
        while (true)
        {
            if (start.x < w && start.y < h)
            {
                GridBlock& block = Grid[start.x * h + start.y];
                if (block.Collision)
                {
                    return {true, start, block.FrictionFactor};
                }
            }

            if (end.x == start.x && end.y == start.y)
            {
                return {false, glm::ivec2{0, 0}, FLOAT_{}};
            }

            e2 = err << 1;
            if (e2 > -dy)
            {
                err -= dy;
                start.x += sx;
            }
            if (e2 < dx)
            {
                err += dx;
                start.y += sy;
            }
        }
    }
    
    /**
     * @brief 线段碰撞检测（浮点数坐标）
     * @param start 起始位置（世界坐标）
     * @param end 结束位置（世界坐标）
     * @return 碰撞结果信息（包含精确碰撞位置和方向）
     * @details 1. 将浮点数坐标转换为整数坐标，调用重载的 BresenhamDetection
     * 2. 计算精确的碰撞位置和碰撞方向
     * 3. 处理边界情况
     */
    CollisionInfoD BaseGrid::BresenhamDetection(Vec2_ start, Vec2_ end)
    {
        CollisionInfoD Collisioninfo{false};
        // 线段碰撞检测
        CollisionInfoI info = BresenhamDetection(ToInt(start), ToInt(end));
        if (info.pos.x == width)
        {
            --info.pos.x;
        }
        if (info.pos.y == height)
        {
            --info.pos.y;
        }
        if (info.Collision)
        {
            Collisioninfo.Friction = info.Friction;
            Collisioninfo.Collision = true;
            Collisioninfo.pos = info.pos;
            // 计算精确碰撞位置
            FLOAT_ Difference = (end.x - start.x) / (start.y - end.y);
            FLOAT_ invDifference = 1.0 / Difference;
            FLOAT_ val = start.x - end.x;
            if (val < 0)
            {
                Collisioninfo.pos = {info.pos.x, ((end.x - info.pos.x) * invDifference) + end.y};
                Collisioninfo.Direction = CheckDirection::Left;
            }
            else if (val > 0)
            {
                Collisioninfo.pos = {info.pos.x + 1, ((end.x - info.pos.x - 1) * invDifference) + end.y};
                Collisioninfo.Direction = CheckDirection::Right;
            }
            if ((val == 0) || (Collisioninfo.pos.y < info.pos.y) || (Collisioninfo.pos.y > (info.pos.y + 1)))
            {
                val = start.y - end.y;
                if (val < 0)
                {
                    Collisioninfo.pos = {(Difference * (end.y - info.pos.y)) + end.x, info.pos.y};
                    Collisioninfo.Direction = CheckDirection::Down;
                }
                else if (val > 0)
                {
                    Collisioninfo.pos = {(Difference * (end.y - info.pos.y - 1)) + end.x, info.pos.y + 1};
                    Collisioninfo.Direction = CheckDirection::Up;
                }
                // 检查相邻格子是否有碰撞
                if (GetCollision(info.pos.x, info.pos.y + (CheckDirection::Down == Collisioninfo.Direction ? -1 : 1)))
                {
                    val = start.x - end.x;
                    if (val < 0)
                    {
                        Collisioninfo.pos = {info.pos.x, ((end.x - info.pos.x) * invDifference) + end.y};
                        Collisioninfo.Direction = CheckDirection::Left;
                    }
                    else if (val > 0)
                    {
                        ++info.pos.x;
                        Collisioninfo.pos = {info.pos.x, ((end.x - info.pos.x) * invDifference) + end.y};
                        Collisioninfo.Direction = CheckDirection::Right;
                    }
                }
            }
            else
            {
                // 检查相邻格子是否有碰撞
                if (GetCollision(info.pos.x + (CheckDirection::Left == Collisioninfo.Direction ? -1 : 1), info.pos.y))
                {
                    val = start.y - end.y;
                    if (val < 0)
                    {
                        Collisioninfo.pos = {(Difference * (end.y - info.pos.y)) + end.x, info.pos.y};
                        Collisioninfo.Direction = CheckDirection::Down;
                    }
                    else if (val > 0)
                    {
                        ++info.pos.y;
                        Collisioninfo.pos = {(Difference * (end.y - info.pos.y)) + end.x, info.pos.y};
                        Collisioninfo.Direction = CheckDirection::Up;
                    }
                }
            }
        }
        return Collisioninfo;
    }

#if PhysicsBlock_Serialization
    /**
     * @brief 将网格状态序列化为 JSON
     * @param data 输出的 JSON 数据
     * @details 将网格的宽度、高度、是否内部创建标志和所有网格数据序列化到 JSON 中
     */
    void BaseGrid::JsonSerialization(nlohmann::json_abi_v3_12_0::basic_json<> &data)
    {
        data["width"] = width;
        data["height"] = height;
        data["NewBool"] = NewBool;
        nlohmann::json_abi_v3_12_0::basic_json<> &dataYY = data["Grid"];
        dataYY = dataYY.array();
        for (size_t i = 0; i < (width * height); ++i)
        {
            dataYY[i]["type"] = Grid[i].type;
            dataYY[i]["FrictionFactor"] = Grid[i].FrictionFactor;
            dataYY[i]["Healthpoint"] = Grid[i].Healthpoint;
            dataYY[i]["mass"] = Grid[i].mass;
        }
    }

    /**
     * @brief 从 JSON 数据反序列化网格状态
     * @param data JSON 数据
     * @details 从 JSON 数据中恢复网格的创建标志和所有网格数据
     */
    void BaseGrid::JsonContrarySerialization(const nlohmann::json_abi_v3_12_0::basic_json<> &data)
    {
        NewBool = data["NewBool"];
        auto dataBF = data["Grid"];
        for (size_t i = 0; i < (width * height); ++i)
        {
            Grid[i].type = dataBF[i]["type"];
            Grid[i].FrictionFactor = dataBF[i]["FrictionFactor"];
            Grid[i].Healthpoint = dataBF[i]["Healthpoint"];
            Grid[i].mass = dataBF[i]["mass"];
        }
    }
#endif

}