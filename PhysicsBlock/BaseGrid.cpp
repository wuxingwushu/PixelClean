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
            if (at(start).Collision)
            {
                info.Collision = true;
                break;
            }
            if (end.x == start.x && end.y == start.y)
            {
                info.Collision = false;
                break;
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
        return info;
    }

    CollisionInfoD BaseGrid::BresenhamDetection(glm::dvec2 start, glm::dvec2 end)
    {
        CollisionInfoD Collisioninfo{false};
        // 线段碰撞检测
        CollisionInfoI info = BresenhamDetection(ToInt(start), ToInt(end));
        if (info.Collision)
        {
            // 计算出精准位置
            Collisioninfo.Collision = true;
            Collisioninfo.pos = info.pos;
            double val = start.x - end.x;
            if (val < 0)
            {
                Collisioninfo.pos = LineXToPos(start, end, info.pos.x);
                Collisioninfo.Direction = CheckDirection::Left;
            }
            else if (val > 0)
            {
                Collisioninfo.pos = LineXToPos(start, end, info.pos.x + 1);
                Collisioninfo.Direction = CheckDirection::Right;
            }
            if ((Collisioninfo.pos.y < info.pos.y) || (Collisioninfo.pos.y > (info.pos.y + 1)))
            {
                val = start.y - end.y;
                if (val < 0)
                {
                    Collisioninfo.pos = LineYToPos(start, end, info.pos.y);
                    Collisioninfo.Direction = CheckDirection::Down;
                }
                else if (val > 0)
                {
                    Collisioninfo.pos = LineYToPos(start, end, info.pos.y + 1);
                    Collisioninfo.Direction = CheckDirection::Up;
                }
            }
            switch (Collisioninfo.Direction)
            {
            case CheckDirection::Left:
                if(GetCollision(info.pos.x - 1, info.pos.y)){
                    val = start.y - end.y;
                    if (val < 0) {
                        Collisioninfo.pos = LineYToPos(start, end, info.pos.y);
                        Collisioninfo.Direction = CheckDirection::Down;
                    }
                    else if (val > 0) {
                        Collisioninfo.pos = LineYToPos(start, end, info.pos.y + 1);
                        Collisioninfo.Direction = CheckDirection::Up;
                    }
                }
                break;
            case CheckDirection::Right:
                if(GetCollision(info.pos.x + 1, info.pos.y)){
                    val = start.y - end.y;
                    if (val < 0) {
                        Collisioninfo.pos = LineYToPos(start, end, info.pos.y);
                        Collisioninfo.Direction = CheckDirection::Down;
                    }
                    else if (val > 0) {
                        Collisioninfo.pos = LineYToPos(start, end, info.pos.y + 1);
                        Collisioninfo.Direction = CheckDirection::Up;
                    }
                }
                break;
                    
            case CheckDirection::Up:
                if(GetCollision(info.pos.x, info.pos.y - 1)){
                    val = start.x - end.x;
                    if (val < 0) {
                        Collisioninfo.pos = LineXToPos(start, end, info.pos.x);
                        Collisioninfo.Direction = CheckDirection::Left;
                    }
                    else if (val > 0) {
                        Collisioninfo.pos = LineXToPos(start, end, info.pos.x + 1);
                        Collisioninfo.Direction = CheckDirection::Right;
                    }
                }
                break;
            case CheckDirection::Down:
                if(GetCollision(info.pos.x, info.pos.y + 1)){
                    val = start.x - end.x;
                    if (val < 0) {
                        Collisioninfo.pos = LineXToPos(start, end, info.pos.x);
                        Collisioninfo.Direction = CheckDirection::Left;
                    }
                    else if (val > 0) {
                        Collisioninfo.pos = LineXToPos(start, end, info.pos.x + 1);
                        Collisioninfo.Direction = CheckDirection::Right;
                    }
                }
                break;
                
            default:
                break;
            }
        }
        return Collisioninfo;
    }

}
