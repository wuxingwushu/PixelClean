#include "MapStatic.hpp"
#include "BaseCalculate.hpp"

namespace PhysicsBlock
{

    void MapStatic::SetCentrality(glm::dvec2 Centrality){
        // 限定点在范围内
        if((Centrality.x < 0) || (Centrality.y < 0) || (Centrality.x > width) || (Centrality.y < height)){
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

        CollisionInfoD Collisioninfo{false};
        // 裁剪线段 让线段都在矩形内
        PhysicsBlock::SquareFocus data = PhysicsBlock::LineSquareFocus(start, end, width-0.01, height - 0.01);
        if(data.Focus){
            // 线段碰撞检测
            CollisionInfoI info = FMBresenhamDetection(ToInt(data.start), ToInt(data.end));
            if(info.Collision){
                // 计算出精准位置
                Collisioninfo.Collision = true;
                Collisioninfo.pos = info.pos;
                double val = start.x - end.x;
                if(val < 0){
                    Collisioninfo.pos = LineXToPos(start, end, info.pos.x);
                    Collisioninfo.Direction = CheckDirection::Left;
                }else if(val > 0){
                    Collisioninfo.pos = LineXToPos(start, end, info.pos.x + 1);
                    Collisioninfo.Direction = CheckDirection::Right;
                }
                if ((Collisioninfo.pos.y < info.pos.y) || (Collisioninfo.pos.y > (info.pos.y + 1))) {
                    val = start.y - end.y;
                    if (val < 0) {
                        Collisioninfo.pos = LineYToPos(start, end, info.pos.y);
                        Collisioninfo.Direction = CheckDirection::Up;
                    }
                    else if (val > 0) {
                        Collisioninfo.pos = LineYToPos(start, end, info.pos.y + 1);
                        Collisioninfo.Direction = CheckDirection::Down;
                    }
                }
                
            }
        }
        // 返回物理坐标系
        Collisioninfo.pos -= centrality;
        return Collisioninfo;
    }

}