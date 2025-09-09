#include "BaseGrid.hpp"
#include "BaseCalculate.hpp"

namespace PhysicsBlock
{

    BaseGrid::BaseGrid(const unsigned int Width, const unsigned int Height) : width(Width), height(Height)
    {
        // 申请网格
        Grid = new GridBlock[width * height];
        NewBool = true;
    }

    BaseGrid::BaseGrid(const unsigned int Width, const unsigned int Height, GridBlock *Gridptr) : width(Width), height(Height), Grid(Gridptr)
    {
        NewBool = false;
    }

    BaseGrid::~BaseGrid()
    {
        // 释放网格
        if (NewBool)
            delete Grid;
    }

    CollisionInfoI BaseGrid::BresenhamDetection(glm::ivec2 start, glm::ivec2 end)
    {
        CollisionInfoI info;
        int dx = abs(end.x - start.x);
        int dy = abs(end.y - start.y);
        int sx = (start.x < end.x) ? 1 : -1;
        int sy = (start.y < end.y) ? 1 : -1;
        int err = dx - dy;
        int e2;
        while (true)
        {
            if (GetCollision(start))
            {
                info.Collision = true;
                break;
            }
            if (end.x == start.x && end.y == start.y)
            {
                info.Collision = false;
                return info;
            }
            e2 = 2 * err;
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
        info.pos = start;
        info.Friction = at(start).FrictionFactor;
        return info;
    }

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
            // 计算出精准位置
            Collisioninfo.Collision = true;
            Collisioninfo.pos = info.pos;
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
            }else{
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
