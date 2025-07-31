#include "MapStatic.hpp"
#include "BaseCalculate.hpp"

namespace PhysicsBlock
{

    void MapStatic::SetCentrality(Vec2_ Centrality)
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

    CollisionInfoD MapStatic::FMBresenhamDetection(Vec2_ start, Vec2_ end)
    {
        // 偏移中心位置，对其网格坐标系
        start += centrality;
        end += centrality;

        // 裁剪线段 让线段都在矩形内
        PhysicsBlock::SquareFocus data = PhysicsBlock::LineSquareFocus(start, end, width, height);
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


    std::vector<MapOutline> MapStatic::GetLightweightOutline(int x_, int y_, int w_, int h_)
    {
        x_ += centrality.x;
        y_ += centrality.y;
        w_ += centrality.x;
        h_ += centrality.y;

        std::vector<MapOutline> Outline;

        for (int x = x_; x < w_; ++x)
        {
            for (int y = y_; y < h_; ++y)
            {
                if (!GetCollision(x, y)) {
                    continue;
                }
                // 左上角
                if (!GetCollision(x - 1, y - 1))
                {
                    if (GetCollision(x - 1, y) == GetCollision(x, y - 1))
                    {
                        Outline.push_back({Vec2_{x, y}, at(x, y).FrictionFactor});
                    }
                }
                else if (!GetCollision(x - 1, y) || !GetCollision(x, y - 1))
                {
                    Outline.push_back({Vec2_{x, y}, at(x, y).FrictionFactor});
                }
                // 左下角
                if (!GetCollision(x, y + 1)) {
                    if (!(GetCollision(x - 1, y) && !GetCollision(x - 1, y + 1))) {
                        Outline.push_back({Vec2_{x, y+1}, at(x, y).FrictionFactor});
                    }
                }
                // 右上角
                if (!GetCollision(x + 1, y)) {
                    if ((!GetCollision(x, y - 1) && !GetCollision(x + 1, y - 1))) {
                        Outline.push_back({Vec2_{x+1, y}, at(x, y).FrictionFactor});
                    }
                }
                // 右下角
                if (!GetCollision(x + 1, y + 1)) {
                    if ((GetCollision(x + 1, y) == GetCollision(x, y + 1)) && !GetCollision(x + 1, y))
                    {
                        Outline.push_back({Vec2_{x+1, y+1}, at(x, y).FrictionFactor});
                    }
                }
            }
            
        }
        return Outline;
    }

}