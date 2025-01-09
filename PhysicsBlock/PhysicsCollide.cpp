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
        double ContactAccount[2]{ 0, 0 };
        unsigned int ContactSize[2]{ 1, 1 };
        unsigned char ContactDirection[2]{ 255, 255 };// 碰撞到那个边了（255代表没有被使用）
        glm::dvec2 Drop, DropPos;// 骨骼点
        double val;
        for (size_t i = 0; i < A->OutlineSize; ++i){
            Drop = A->OutlineSet[i] - A->CentreMass;
            Drop = vec2angle(Drop, A->angle);
            DropPos = A->pos + Drop;
            CollisionInfoD info = B->FMBresenhamDetection(A->OldPos, DropPos);
            if(info.Collision){
                if(info.Direction == ContactDirection[0]){
                    ++ContactSize[0];
                    if (contacts[0].w_side & 0x1) {
                        val = abs(info.pos.y - DropPos.y);
                    }
                    else {
                        val = abs(info.pos.x - DropPos.x);
                    }
                    contacts[0].position += info.pos * val;
                    ContactAccount[0] += val;
                }else if(info.Direction == ContactDirection[1]){ 
                    ++ContactSize[1];
                    if (contacts[1].w_side & 0x1) {
                        val = abs(info.pos.y - DropPos.y);
                    }
                    else {
                        val = abs(info.pos.x - DropPos.x);
                    }
                    contacts[1].position += info.pos * val;
                    ContactAccount[1] += val;
                }
                else {
                    if (255 == ContactDirection[0]) {
                        ContactDirection[0] = info.Direction;
                        if(info.Direction & 0x1){
                            contacts[0].separation = info.pos.y - DropPos.y;
                        }else{
                            contacts[0].separation = info.pos.x - DropPos.x;
                        }
                        contacts[0].separation = abs(contacts[0].separation);
                        ContactAccount[0] = contacts[0].separation;
                        contacts[0].position = info.pos * contacts[0].separation;
                    }
                    else if (255 == ContactDirection[1]) {
                        ContactDirection[1] = info.Direction;
                        if(info.Direction & 0x1){
                            contacts[1].separation = info.pos.y - DropPos.y;
                        }else{
                            contacts[1].separation = info.pos.x - DropPos.x;
                        }
                        contacts[1].separation = abs(contacts[1].separation);
                        ContactAccount[1] = contacts[1].separation;
                        contacts[1].position = info.pos * contacts[1].separation;
                    }
                    else {
                        assert(0 && "[Error]: 出现不合理现象!");
                    }
                }
            }
        }

        contacts[0].position /= ContactAccount[0];
        contacts[0].w_side = ContactDirection[0];
        /*if (contacts[0].w_side & 0x1) {
            contacts[0].position.y = A->pos.y + A->pos.y - contacts[0].position.y;
        }
        else {
            contacts[0].position.x = A->pos.x + A->pos.x - contacts[0].position.x;
        }*/
        contacts[0].normal = vec2angle({ -1, 0 }, ContactDirection[0] * 3.14159265359 / 2);// 地形不会旋转
        contacts[0].separation = ContactAccount[0] / ContactSize[0];// 暂时不理

        contacts[1].position /= ContactAccount[1];
        contacts[1].w_side = ContactDirection[1];
        /*if (contacts[1].w_side & 0x1) {
            contacts[1].position.y = A->pos.y + A->pos.y - contacts[1].position.y;
        }
        else {
            contacts[1].position.x = A->pos.x + A->pos.x - contacts[1].position.x;
        }*/
        contacts[1].normal = vec2angle({ -1, 0 }, ContactDirection[1] * 3.14159265359 / 2);// 地形不会旋转
        contacts[1].separation = ContactAccount[1] / ContactSize[1];// 暂时不理

        return ContactDirection[0] == 255 ? 0 : (ContactDirection[1] == 255 ? 1 : 2);
    }

    int Collide(Contact *contacts, PhysicsParticle *A, MapFormwork *B)
    {
        return 0;
    }

}