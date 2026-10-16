#include "PhysicsKinematic3D.hpp"
#include "PhysicsParticle3D.hpp"
#include "PhysicsAngle3D.hpp"
#include "PhysicsShape3D.hpp"
#include "PhysicsCircle3D.hpp"
#include "PhysicsLine3D.hpp"
#include "BaseCalculate3D.hpp"
#include <cmath>
#include <algorithm>

namespace PhysicsBlock3D
{
    /**
     * @brief 该物体是否具备转动自由度
     * @details 只有 shape/circle/line（`PhysicsAngle3D` 派生）才有角速度；
     *          裸粒子调用 `PFAngleSpeed()` 会触发断言。2D 版在 `SetIsKinematic`
     *          里无条件调用 `PFAngleSpeed()`，对粒子会踩断言——3D 在此修正。
     */
    inline bool HasRotation3D(PhysicsFormwork3D *object)
    {
        if (object == nullptr)
        {
            return false;
        }
        const PhysicsObjectEnum t = object->PFGetType();
        return t == shape || t == circle || t == line;
    }


    void PhysicsKinematic3D::SetIsKinematic(PhysicsFormwork3D *object, bool kinematic)
    {
        if (object == nullptr)
        {
            throw ArgumentNullException("object");
        }

        std::lock_guard<std::mutex> lock(mMutex);
        KinematicData3D &data = GetOrCreateData(object);

        if (data.IsKinematic == kinematic)
        {
            return;
        }

        if (kinematic)
        {
            data.SavedSpeed = object->PFSpeed();

            // 清零速度（2D 同样在此处清零，而非在 SynchronizePhysicsState 中）
            object->PFSpeed() = Vec3_(0.0f);

            if (HasRotation3D(object))
            {
                PhysicsAngle3D *angleObj = static_cast<PhysicsAngle3D *>(object);
                data.SavedAngleSpeed = angleObj->PFAngleSpeed();
                angleObj->PFAngleSpeed() = Vec3_(0.0f);
            }
            else
            {
                data.SavedAngleSpeed = Vec3_(0.0f);
            }

            data.IsKinematic = true;
        }
        else
        {
            data.IsKinematic = false;

            object->PFSpeed() = data.SavedSpeed;

            if (HasRotation3D(object))
            {
                static_cast<PhysicsAngle3D *>(object)->PFAngleSpeed() = data.SavedAngleSpeed;
            }

            data.Motion = KinematicMotion3D{};
        }

        SynchronizePhysicsStateImpl(object);
    }

    bool PhysicsKinematic3D::GetIsKinematic(PhysicsFormwork3D *object) const
    {
        if (object == nullptr)
        {
            throw ArgumentNullException("object");
        }
        std::lock_guard<std::mutex> lock(mMutex);
        return GetData(object).IsKinematic;
    }

    void PhysicsKinematic3D::MoveTo(PhysicsFormwork3D *object, const Vec3_ &position, FLOAT_ duration)
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
        KinematicData3D &data = GetOrCreateData(object);

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

    void PhysicsKinematic3D::RotateTo(PhysicsFormwork3D *object, const Quat_ &rotation, FLOAT_ duration)
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
        KinematicData3D &data = GetOrCreateData(object);

        if (!data.IsKinematic)
        {
            throw PhysicsException(
                "RotateTo requires the object to be kinematic. "
                "Call SetIsKinematic(object, true) first.");
        }

        // 只有具备朝向的物体（shape/circle/line）才可旋转
        Quat_ currentRotation(1.0f, 0.0f, 0.0f, 0.0f);
        PhysicsObjectEnum objType = object->PFGetType();
        if (objType == PhysicsObjectEnum::shape ||
            objType == PhysicsObjectEnum::circle ||
            objType == PhysicsObjectEnum::line)
        {
            currentRotation = static_cast<PhysicsAngle3D *>(object)->rotation;
        }

        data.Motion.IsRotating = true;
        data.Motion.RotateStartRotation = currentRotation;
        data.Motion.RotateTargetRotation = glm::normalize(rotation);
        data.Motion.RotateDuration = duration;
        data.Motion.RotateElapsed = 0.0f;
    }

    void PhysicsKinematic3D::SetMoveMode(PhysicsFormwork3D *object, KinematicMoveMode3D mode)
    {
        if (object == nullptr)
        {
            throw ArgumentNullException("object");
        }
        std::lock_guard<std::mutex> lock(mMutex);
        GetOrCreateData(object).Motion.MoveMode = mode;
    }

    void PhysicsKinematic3D::SetRotateMode(PhysicsFormwork3D *object, KinematicMoveMode3D mode)
    {
        if (object == nullptr)
        {
            throw ArgumentNullException("object");
        }
        std::lock_guard<std::mutex> lock(mMutex);
        GetOrCreateData(object).Motion.RotateMode = mode;
    }

    void PhysicsKinematic3D::SynchronizePhysicsState(PhysicsFormwork3D *object)
    {
        if (object == nullptr)
        {
            throw ArgumentNullException("object");
        }

        std::lock_guard<std::mutex> lock(mMutex);
        SynchronizePhysicsStateImpl(object);
    }

    void PhysicsKinematic3D::SynchronizePhysicsStateImpl(PhysicsFormwork3D *object)
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
            // 运动学：有效质量全部归零，碰撞冲量无法推动它
            if (objType == PhysicsObjectEnum::particle)
            {
                static_cast<PhysicsParticle3D *>(object)->invMass = 0.0f;
            }
            else if (objType == PhysicsObjectEnum::shape ||
                     objType == PhysicsObjectEnum::circle ||
                     objType == PhysicsObjectEnum::line)
            {
                static_cast<PhysicsParticle3D *>(object)->invMass = 0.0f;
                // 3D 惯量是张量，零矩阵表示无限惯量（2D 是标量 0.0f）
                static_cast<PhysicsAngle3D *>(object)->invMomentInertia = Mat3_(0.0f);
            }
        }
        else
        {
            if (objType == PhysicsObjectEnum::particle)
            {
                auto *p = static_cast<PhysicsParticle3D *>(object);
                if (p->mass > 0.0f)
                {
                    p->invMass = 1.0f / p->mass;
                }
            }
            else if (objType == PhysicsObjectEnum::shape ||
                     objType == PhysicsObjectEnum::circle ||
                     objType == PhysicsObjectEnum::line)
            {
                auto *p = static_cast<PhysicsParticle3D *>(object);
                if (p->mass > 0.0f)
                {
                    p->invMass = 1.0f / p->mass;
                }
                auto *pa = static_cast<PhysicsAngle3D *>(object);
                // 3D 惯量张量求逆（2D 是 1.0f / MomentInertia 标量除法）
                const Mat3_ inv = SymmetricInverse(pa->MomentInertia);
                pa->invMomentInertia = inv;
            }
        }
    }

    void PhysicsKinematic3D::UpdateKinematicMotion(FLOAT_ deltaTime)
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

            KinematicMotion3D &motion = data.Motion;

            if (motion.IsMoving)
            {
                motion.MoveElapsed += deltaTime;

                if (motion.MoveElapsed >= motion.MoveDuration)
                {
                    motion.MoveElapsed = motion.MoveDuration;

                    if (motion.MoveMode == KinematicMoveMode3D::PingPong)
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

                // 三维线性插值（2D 是逐分量手写 x/y）
                auto *pp = static_cast<PhysicsParticle3D *>(object);
                pp->pos = Lerp(motion.MoveStartPos, motion.MoveTargetPos, t);
            }

            if (motion.IsRotating)
            {
                motion.RotateElapsed += deltaTime;

                if (motion.RotateElapsed >= motion.RotateDuration)
                {
                    motion.RotateElapsed = motion.RotateDuration;

                    if (motion.RotateMode == KinematicMoveMode3D::PingPong)
                    {
                        std::swap(motion.RotateStartRotation, motion.RotateTargetRotation);
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

                // 四元数球面插值：3D 必须用 slerp，线性插值角度会非匀速且可能绕远路
                static_cast<PhysicsAngle3D *>(object)->rotation =
                    glm::normalize(glm::slerp(motion.RotateStartRotation, motion.RotateTargetRotation, t));
            }
        }
    }

    void PhysicsKinematic3D::RemoveKinematicData(PhysicsFormwork3D *object)
    {
        if (object == nullptr)
        {
            throw ArgumentNullException("object");
        }
        std::lock_guard<std::mutex> lock(mMutex);
        mKinematicData.erase(object);
    }

    void PhysicsKinematic3D::Clear()
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mKinematicData.clear();
    }

    KinematicData3D &PhysicsKinematic3D::GetOrCreateData(PhysicsFormwork3D *object)
    {
        auto it = mKinematicData.find(object);
        if (it == mKinematicData.end())
        {
            it = mKinematicData.emplace(object, KinematicData3D{}).first;
        }
        return it->second;
    }

    const KinematicData3D &PhysicsKinematic3D::GetData(PhysicsFormwork3D *object) const
    {
        static KinematicData3D defaultData;
        auto it = mKinematicData.find(object);
        if (it == mKinematicData.end())
        {
            return defaultData;
        }
        return it->second;
    }

}
