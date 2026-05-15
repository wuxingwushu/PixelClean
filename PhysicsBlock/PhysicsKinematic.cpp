#include "PhysicsKinematic.hpp"
#include "PhysicsParticle.hpp"
#include "PhysicsAngle.hpp"
#include "PhysicsShape.hpp"
#include "PhysicsCircle.hpp"
#include "PhysicsLine.hpp"
#include <cmath>
#include <algorithm>

namespace PhysicsBlock
{

    void PhysicsKinematic::SetIsKinematic(PhysicsFormwork *object, bool kinematic)
    {
        if (object == nullptr)
        {
            throw ArgumentNullException("object");
        }

        std::lock_guard<std::mutex> lock(mMutex);
        KinematicData &data = GetOrCreateData(object);

        if (data.IsKinematic == kinematic)
        {
            return;
        }

        if (kinematic)
        {
            data.SavedSpeed = object->PFSpeed();
            data.SavedAngleSpeed = object->PFAngleSpeed();

            object->PFSpeed() = Vec2_{0.0f, 0.0f};
            object->PFAngleSpeed() = 0.0f;

            data.IsKinematic = true;
        }
        else
        {
            data.IsKinematic = false;

            object->PFSpeed() = data.SavedSpeed;
            object->PFAngleSpeed() = data.SavedAngleSpeed;

            data.Motion = KinematicMotion{};
        }

        SynchronizePhysicsStateImpl(object);
    }

    bool PhysicsKinematic::GetIsKinematic(PhysicsFormwork *object) const
    {
        if (object == nullptr)
        {
            throw ArgumentNullException("object");
        }
        std::lock_guard<std::mutex> lock(mMutex);
        return GetData(object).IsKinematic;
    }

    void PhysicsKinematic::MoveTo(PhysicsFormwork *object, const Vec2_ &position, FLOAT_ duration)
    {
        if (object == nullptr)
        {
            throw ArgumentNullException("object");
        }
        if (duration <= 0.0f)
        {
            throw ArgumentOutOfRangeException("duration",
                "Duration must be greater than 0. Received: " + std::to_string(duration));
        }

        std::lock_guard<std::mutex> lock(mMutex);
        KinematicData &data = GetOrCreateData(object);

        if (!data.IsKinematic)
        {
            throw PhysicsException(
                "MoveTo requires the object to be kinematic. "
                "Call SetIsKinematic(object, true) first.");
        }

        data.Motion.IsMoving = true;
        data.Motion.MoveStartPos = object->PFGetPos();
        data.Motion.MoveTargetPos = position;
        data.Motion.MoveDuration = duration;
        data.Motion.MoveElapsed = 0.0f;
    }

    void PhysicsKinematic::RotateTo(PhysicsFormwork *object, FLOAT_ rotation, FLOAT_ duration)
    {
        if (object == nullptr)
        {
            throw ArgumentNullException("object");
        }
        if (duration <= 0.0f)
        {
            throw ArgumentOutOfRangeException("duration",
                "Duration must be greater than 0. Received: " + std::to_string(duration));
        }

        std::lock_guard<std::mutex> lock(mMutex);
        KinematicData &data = GetOrCreateData(object);

        if (!data.IsKinematic)
        {
            throw PhysicsException(
                "RotateTo requires the object to be kinematic. "
                "Call SetIsKinematic(object, true) first.");
        }

        FLOAT_ currentAngle = 0.0f;
        PhysicsObjectEnum objType = object->PFGetType();
        if (objType == PhysicsObjectEnum::shape ||
            objType == PhysicsObjectEnum::circle ||
            objType == PhysicsObjectEnum::line)
        {
            currentAngle = static_cast<PhysicsAngle *>(object)->angle;
        }

        data.Motion.IsRotating = true;
        data.Motion.RotateStartAngle = currentAngle;
        data.Motion.RotateTargetAngle = rotation;
        data.Motion.RotateDuration = duration;
        data.Motion.RotateElapsed = 0.0f;
    }

    void PhysicsKinematic::SetMoveMode(PhysicsFormwork *object, KinematicMoveMode mode)
    {
        if (object == nullptr)
        {
            throw ArgumentNullException("object");
        }
        std::lock_guard<std::mutex> lock(mMutex);
        GetOrCreateData(object).Motion.MoveMode = mode;
    }

    void PhysicsKinematic::SetRotateMode(PhysicsFormwork *object, KinematicMoveMode mode)
    {
        if (object == nullptr)
        {
            throw ArgumentNullException("object");
        }
        std::lock_guard<std::mutex> lock(mMutex);
        GetOrCreateData(object).Motion.RotateMode = mode;
    }

    void PhysicsKinematic::SynchronizePhysicsState(PhysicsFormwork *object)
    {
        if (object == nullptr)
        {
            throw ArgumentNullException("object");
        }

        std::lock_guard<std::mutex> lock(mMutex);
        SynchronizePhysicsStateImpl(object);
    }

    void PhysicsKinematic::SynchronizePhysicsStateImpl(PhysicsFormwork *object)
    {
        auto it = mKinematicData.find(object);
        if (it == mKinematicData.end())
        {
            return;
        }

        const bool isKinematic = it->second.IsKinematic;
        PhysicsObjectEnum objType = object->PFGetType();

        if (isKinematic)
        {
            if (objType == PhysicsObjectEnum::particle)
            {
                static_cast<PhysicsParticle *>(object)->invMass = 0.0f;
            }
            else if (objType == PhysicsObjectEnum::shape ||
                     objType == PhysicsObjectEnum::circle ||
                     objType == PhysicsObjectEnum::line)
            {
                static_cast<PhysicsParticle *>(object)->invMass = 0.0f;
                static_cast<PhysicsAngle *>(object)->invMomentInertia = 0.0f;
            }
        }
        else
        {
            if (objType == PhysicsObjectEnum::particle)
            {
                auto *p = static_cast<PhysicsParticle *>(object);
                if (p->mass > 0.0f)
                {
                    p->invMass = 1.0f / p->mass;
                }
            }
            else if (objType == PhysicsObjectEnum::shape ||
                     objType == PhysicsObjectEnum::circle ||
                     objType == PhysicsObjectEnum::line)
            {
                auto *p = static_cast<PhysicsParticle *>(object);
                if (p->mass > 0.0f)
                {
                    p->invMass = 1.0f / p->mass;
                }
                auto *pa = static_cast<PhysicsAngle *>(object);
                if (pa->MomentInertia > 0.0f)
                {
                    pa->invMomentInertia = 1.0f / pa->MomentInertia;
                }
            }
        }
    }

    void PhysicsKinematic::UpdateKinematicMotion(FLOAT_ deltaTime)
    {
        if (deltaTime <= 0.0f)
        {
            throw ArgumentOutOfRangeException("deltaTime",
                "deltaTime must be greater than 0. Received: " + std::to_string(deltaTime));
        }

        std::lock_guard<std::mutex> lock(mMutex);

        for (auto &[object, data] : mKinematicData)
        {
            if (object == nullptr || !data.IsKinematic)
            {
                continue;
            }

            KinematicMotion &motion = data.Motion;

            if (motion.IsMoving)
            {
                motion.MoveElapsed += deltaTime;

                if (motion.MoveElapsed >= motion.MoveDuration)
                {
                    motion.MoveElapsed = motion.MoveDuration;

                    if (motion.MoveMode == KinematicMoveMode::PingPong)
                    {
                        std::swap(motion.MoveStartPos, motion.MoveTargetPos);
                        motion.MoveElapsed = 0.0f;
                    }
                    else
                    {
                        motion.IsMoving = false;
                    }
                }

                FLOAT_ t = (motion.MoveDuration > 0.0f)
                               ? motion.MoveElapsed / motion.MoveDuration
                               : 1.0f;
                t = (t > 1.0f) ? 1.0f : ((t < 0.0f) ? 0.0f : t);

                auto *pp = static_cast<PhysicsParticle *>(object);
                pp->pos.x = motion.MoveStartPos.x + (motion.MoveTargetPos.x - motion.MoveStartPos.x) * t;
                pp->pos.y = motion.MoveStartPos.y + (motion.MoveTargetPos.y - motion.MoveStartPos.y) * t;
            }

            if (motion.IsRotating)
            {
                motion.RotateElapsed += deltaTime;

                if (motion.RotateElapsed >= motion.RotateDuration)
                {
                    motion.RotateElapsed = motion.RotateDuration;

                    if (motion.RotateMode == KinematicMoveMode::PingPong)
                    {
                        std::swap(motion.RotateStartAngle, motion.RotateTargetAngle);
                        motion.RotateElapsed = 0.0f;
                    }
                    else
                    {
                        motion.IsRotating = false;
                    }
                }

                FLOAT_ t = (motion.RotateDuration > 0.0f)
                               ? motion.RotateElapsed / motion.RotateDuration
                               : 1.0f;
                t = (t > 1.0f) ? 1.0f : ((t < 0.0f) ? 0.0f : t);

                static_cast<PhysicsAngle *>(object)->angle =
                    motion.RotateStartAngle + (motion.RotateTargetAngle - motion.RotateStartAngle) * t;
            }
        }
    }

    void PhysicsKinematic::RemoveKinematicData(PhysicsFormwork *object)
    {
        if (object == nullptr)
        {
            throw ArgumentNullException("object");
        }
        std::lock_guard<std::mutex> lock(mMutex);
        mKinematicData.erase(object);
    }

    void PhysicsKinematic::Clear()
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mKinematicData.clear();
    }

    KinematicData &PhysicsKinematic::GetOrCreateData(PhysicsFormwork *object)
    {
        auto it = mKinematicData.find(object);
        if (it == mKinematicData.end())
        {
            it = mKinematicData.emplace(object, KinematicData{}).first;
        }
        return it->second;
    }

    const KinematicData &PhysicsKinematic::GetData(PhysicsFormwork *object) const
    {
        static KinematicData defaultData;
        auto it = mKinematicData.find(object);
        if (it == mKinematicData.end())
        {
            return defaultData;
        }
        return it->second;
    }

}