#pragma once
#include "PhysicsWorld.hpp"
#include "PhysicsTrigger.hpp"
#include "PhysicsCollision.hpp"
#include "PhysicsKinematic.hpp"
#include "PhysicsBlockTypes.hpp"
#include <memory>

namespace PhysicsBlock
{

    class PhysicsEngine
    {
    public:
        PhysicsEngine(const Vec2_ &gravityAcceleration, bool wind = false);

        ~PhysicsEngine();

        PhysicsEngine(const PhysicsEngine &) = delete;
        PhysicsEngine &operator=(const PhysicsEngine &) = delete;

        void AddObject(PhysicsParticle *object);
        void AddObject(PhysicsShape *object);
        void AddObject(PhysicsCircle *object);
        void AddObject(PhysicsLine *object);
        void AddObject(PhysicsJoint *object);
        void AddObject(BaseJunction *object);
        void AddObject(PhysicsAssembly *object);

        void RemoveObject(PhysicsFormwork *object);
        void RemoveObject(PhysicsAssembly *object);

        void SetMapFormwork(MapFormwork *mapFormwork);
        MapFormwork *GetMapFormwork();

        PhysicsFormwork *Get(const Vec2_ &pos);

        void Update(FLOAT_ deltaTime);

        PhysicsTrigger &GetTrigger() { return mTrigger; }
        const PhysicsTrigger &GetTrigger() const { return mTrigger; }

        PhysicsCollision &GetCollision() { return mCollision; }
        const PhysicsCollision &GetCollision() const { return mCollision; }

        PhysicsKinematic &GetKinematic() { return mKinematic; }
        const PhysicsKinematic &GetKinematic() const { return mKinematic; }

        PhysicsWorld &GetWorld() { return *mWorld; }
        const PhysicsWorld &GetWorld() const { return *mWorld; }

        FLOAT_ GetWorldEnergy();
        size_t GetActiveCollisionCount() const;
        size_t GetObjectCount() const;

    private:
        std::vector<PhysicsFormwork *> CollectActiveObjects() const;

        PhysicsWorld *mWorld;
        PhysicsTrigger mTrigger;
        PhysicsCollision mCollision;
        PhysicsKinematic mKinematic;
    };

}
