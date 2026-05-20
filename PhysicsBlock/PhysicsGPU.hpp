#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <chrono>

namespace VulKan {
    class Device;
    class Buffer;
    class Calculate;
    class CommandPool;
    class CommandBuffer;
    struct CalculateStruct;
}

namespace PhysicsBlock {

class PhysicsWorld;
class BaseArbiter;

class PhysicsGPU {
public:
    PhysicsGPU(VulKan::Device* device, PhysicsWorld* world);
    ~PhysicsGPU();

    void Initialize();

    void ExecuteGPUApplyImpulse(float inv_dt, unsigned int impulseSize);

    bool IsReady() const { return mReady; }

    float GetLastFrameTimeMS() const { return mLastFrameTimeMS; }

private:
    void PackBodyBuffer();
    void PackArbiterBuffer();
    void PackJointBuffer();
    void PackJunctionBuffer();

    void UnpackBodyBuffer();
    void UnpackArbiterPnPt();

    uint32_t GetBodyIndex(void* objPtr);
    uint32_t GetArbiterGPUType(BaseArbiter* arb);

    void EnsureBodyCapacity(size_t N);
    void EnsureArbiterCapacity(size_t N);
    void EnsureJointCapacity(size_t N);
    void EnsureJunctionCapacity(size_t N);

    void RecreateCalculates();

    VulKan::Device* mDevice = nullptr;
    PhysicsWorld*  mWorld  = nullptr;
    bool mReady = false;

    VulKan::Buffer* mBodyBuffer      = nullptr;
    VulKan::Buffer* mArbiterBuffer   = nullptr;
    VulKan::Buffer* mJointBuffer     = nullptr;
    VulKan::Buffer* mJunctionBuffer  = nullptr;

    VulKan::Buffer* mArbiterCount    = nullptr;
    VulKan::Buffer* mJointCount      = nullptr;
    VulKan::Buffer* mJunctionCount   = nullptr;

    VulKan::Calculate* mCalculateArbiter  = nullptr;
    VulKan::Calculate* mCalculateJoint    = nullptr;
    VulKan::Calculate* mCalculateJunction = nullptr;

    VulKan::CommandPool*   mSharedCmdPool   = nullptr;
    VulKan::CommandBuffer* mSharedCmdBuffer = nullptr;

    size_t mBodyCapacity     = 0;
    size_t mArbiterCapacity  = 0;
    size_t mJointCapacity    = 0;
    size_t mJunctionCapacity = 0;

    float mLastFrameTimeMS = 0.0f;
};

} // namespace PhysicsBlock