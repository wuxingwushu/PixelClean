#include "PhysicsCollide.hpp"
#include "BaseCalculate.hpp"
#include <cmath>

namespace PhysicsBlock
{

    int Collide(Contact *contacts, PhysicsShape *A, PhysicsShape *B)
    {
        return 0;
    }

    int Collide(Contact *contacts, PhysicsShape *A, PhysicsParticle *B)
    {
        return 0;
    }

    int Collide(Contact *contacts, PhysicsShape *A, MapFormwork *B)
    {
        unsigned int ContactSize[2]{ 0, 0 };
        unsigned char ContactDirection[2]{ 255, 255 };// 碰撞到那个边了（255代表没有被使用）
        glm::dvec2 JiuPos;// 上一刻 位置
        glm::dvec2 Drop, DropPos;// 骨骼点
        for (size_t i = 0; i < A->OutlineSize; ++i){
            Drop = A->OutlineSet[i] - A->CentreMass;
            Drop = vec2angle(Drop, A->angle);
            DropPos = A->pos + Drop;
            CollisionInfoD info = B->FMBresenhamDetection(JiuPos + Drop, DropPos);
            if(info.Collision){
                if(info.Direction == ContactDirection[0]){
                    contacts[0].position += info.pos;
                    ++ContactSize[0];
                }else if(info.Direction == ContactDirection[1]){
                    contacts[1].position += info.pos;
                    ++ContactSize[1];
                }
                else {
                    if (255 == ContactDirection[0]) {
                        contacts[0].position = info.pos;
                        ContactDirection[0] = info.Direction;
                        ++ContactSize[0];
                        if(info.Direction & 0x1){
                            contacts[0].separation = info.pos.x - DropPos.x;
                        }else{
                            contacts[0].separation = info.pos.y - DropPos.y;
                        }
                        contacts[0].separation = abs(contacts[0].separation);
                    }
                    else if (255 == ContactDirection[1]) {
                        contacts[1].position = info.pos;
                        ContactDirection[1] = info.Direction;
                        ++ContactSize[1];
                        if(info.Direction & 0x1){
                            contacts[0].separation = info.pos.x - DropPos.x;
                        }else{
                            contacts[0].separation = info.pos.y - DropPos.y;
                        }
                        contacts[0].separation = abs(contacts[0].separation);
                    }
                    else {
                        assert(0 && "[Error]: 出现不合理现象!");
                    }
                }
            }
        }

        contacts[0].position /= ContactSize[0];
        contacts[0].w_side = ContactDirection[0];
        contacts[0].normal = vec2angle({1, 0}, ContactDirection[0] * 3.14159265359 / 2);// 地形不会旋转
        contacts[0].separation = 0.1;// 暂时不理

        contacts[1].position /= ContactSize[1];
        contacts[1].w_side = ContactDirection[1];
        contacts[1].normal = vec2angle({ 1, 0 }, ContactDirection[1] * 3.14159265359 / 2);// 地形不会旋转
        contacts[1].separation = 0.1;// 暂时不理

        return ContactDirection[0] == 255 ? 0 : (ContactDirection[1] == 255 ? 1 : 2);
    }

    int Collide(Contact *contacts, PhysicsParticle *A, MapFormwork *B)
    {
        return 0;
    }

}