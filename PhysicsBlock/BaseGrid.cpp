#include "BaseGrid.hpp"

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
        int dx = abs(end.x - start.x);
        int dy = abs(end.y - start.y);
        int sx = (start.x < end.x) ? 1 : -1;
        int sy = (start.y < end.y) ? 1 : -1;
        int err = dx - dy;
        int e2;
        if (at(start).Collision)
        {
            return { true, start };
        }
        while (true)
        {
            if (end.x == start.x && end.y == start.y)
            {
                return {false, start};
            }
            e2 = 2 * err;
            if (e2 > -dy)
            {
                err -= dy;
                start.x += sx;
                if (at(start).Collision)
                {
                    return { true, start };
                }
            }
            if (e2 < dx)
            {
                err += dx;
                start.y += sy;
                if (at(start).Collision)
                {
                    return { true, start };
                }
            }
        }
    }

}
