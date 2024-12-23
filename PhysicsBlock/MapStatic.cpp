#include "MapStatic.hpp"
#include "BaseCalculate.hpp"

namespace PhysicsBlock
{

    void MapStatic::SetCentrality(glm::dvec2 Centrality)
    {
        // 限定点在范围内
        if ((Centrality.x < 0) || (Centrality.y < 0) || (Centrality.x > width) || (Centrality.y > height))
        {
            return;
        }
        centrality = Centrality;
    }

    CollisionInfoI MapStatic::FMBresenhamDetection(glm::ivec2 start, glm::ivec2 end)
    {
        return BresenhamDetection(start, end);
    }

    CollisionInfoD MapStatic::FMBresenhamDetection(glm::dvec2 start, glm::dvec2 end)
    {
        // 偏移中心位置，对其网格坐标系
        start += centrality;
        end += centrality;

        // 裁剪线段 让线段都在矩形内
        PhysicsBlock::SquareFocus data = PhysicsBlock::LineSquareFocus(start, end, width - 0.01, height - 0.01);
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