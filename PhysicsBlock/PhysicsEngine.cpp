#include "PhysicsEngine.hpp"

namespace PhysicsBlock
{

    PhysicsEngine::PhysicsEngine(const Vec2_ &gravityAcceleration, bool wind)
        : mWorld(new PhysicsWorld(gravityAcceleration, wind))
    {
    }

    PhysicsEngine::~PhysicsEngine()
    {
        mTrigger.Clear();
        mCollision.Clear();
        mKinematic.Clear();

        if (mWorld != nullptr)
        {
            delete mWorld;
            mWorld = nullptr;
        }
    }

    void PhysicsEngine::AddObject(PhysicsParticle *object)
    {
        if (object == nullptr)
        {
            throw ArgumentNullException("object");
        }
        mWorld->AddObject(object);
    }

    void PhysicsEngine::AddObject(PhysicsShape *object)
    {
        if (object == nullptr)
        {
            throw ArgumentNullException("object");
        }
        mWorld->AddObject(object);
    }

    void PhysicsEngine::AddObject(PhysicsCircle *object)
    {
        if (object == nullptr)
        {
            throw ArgumentNullException("object");
        }
        mWorld->AddObject(object);
    }

    void PhysicsEngine::AddObject(PhysicsLine *object)
    {
        if (object == nullptr)
        {
            throw ArgumentNullException("object");
        }
        mWorld->AddObject(object);
    }

    void PhysicsEngine::AddObject(PhysicsJoint *object)
    {
        if (object == nullptr)
        {
            throw ArgumentNullException("object");
        }
        mWorld->AddObject(object);
    }

    void PhysicsEngine::AddObject(BaseJunction *object)
    {
        if (object == nullptr)
        {
            throw ArgumentNullException("object");
        }
        mWorld->AddObject(object);
    }

    void PhysicsEngine::AddObject(PhysicsAssembly *object)
    {
        if (object == nullptr)
        {
            throw ArgumentNullException("object");
        }
        mWorld->AddObject(object);
    }

    void PhysicsEngine::RemoveObject(PhysicsFormwork *object)
    {
        if (object == nullptr)
        {
            throw ArgumentNullException("object");
        }

        mTrigger.RemoveAllTriggerListeners(object);
        mCollision.RemoveAllCollisionListeners(object);
        mKinematic.RemoveKinematicData(object);

        mWorld->RemoveObject(object);
    }

    void PhysicsEngine::RemoveObject(PhysicsAssembly *object)
    {
        if (object == nullptr)
        {
            throw ArgumentNullException("object");
        }

        const auto &children = object->GetChildren();
        for (auto *child : children)
        {
            mTrigger.RemoveAllTriggerListeners(child);
            mCollision.RemoveAllCollisionListeners(child);
            mKinematic.RemoveKinematicData(child);
        }

        mWorld->RemoveObject(object);
    }

    void PhysicsEngine::SetMapFormwork(MapFormwork *mapFormwork)
    {
        if (mapFormwork == nullptr)
        {
            throw ArgumentNullException("mapFormwork");
        }
        mWorld->SetMapFormwork(mapFormwork);
    }

    MapFormwork *PhysicsEngine::GetMapFormwork()
    {
        return mWorld->GetMapFormwork();
    }

    PhysicsFormwork *PhysicsEngine::Get(const Vec2_ &pos)
    {
        return mWorld->Get(pos);
    }

    void PhysicsEngine::Update(FLOAT_ deltaTime)
    {
        if (deltaTime <= 0.0f)
        {
            throw ArgumentOutOfRangeException("deltaTime",
                "deltaTime must be greater than 0. Received: " + std::to_string(deltaTime));
        }

        mKinematic.UpdateKinematicMotion(deltaTime);

        mWorld->PhysicsEmulator(deltaTime);

        mCollision.ProcessCollisions(mWorld->CollideGroupVector);

        mTrigger.ProcessTriggers(CollectActiveObjects());
    }

    FLOAT_ PhysicsEngine::GetWorldEnergy()
    {
        return mWorld->GetWorldEnergy();
    }

    size_t PhysicsEngine::GetActiveCollisionCount() const
    {
        return mWorld->CollideGroupVector.size();
    }

    size_t PhysicsEngine::GetObjectCount() const
    {
        return mWorld->ObjectSize;
    }

    std::vector<PhysicsFormwork *> PhysicsEngine::CollectActiveObjects() const
    {
        std::vector<PhysicsFormwork *> objects;

        for (auto *s : mWorld->PhysicsShapeS)
        {
            objects.push_back(static_cast<PhysicsFormwork *>(s));
        }
        for (auto *p : mWorld->PhysicsParticleS)
        {
            objects.push_back(static_cast<PhysicsFormwork *>(p));
        }
        for (auto *c : mWorld->PhysicsCircleS)
        {
            objects.push_back(static_cast<PhysicsFormwork *>(c));
        }
        for (auto *l : mWorld->PhysicsLineS)
        {
            objects.push_back(static_cast<PhysicsFormwork *>(l));
        }

        return objects;
    }

}
