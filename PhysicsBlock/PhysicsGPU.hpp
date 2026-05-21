#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <chrono>
#include <unordered_map>

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

    float GetCPUUploadTimeMS()   const { return mCPUUploadTimeMS; }
    float GetGPUExecuteTimeMS()  const { return mGPUExecuteTimeMS; }
    float GetGPUReadbackTimeMS() const { return mGPUReadbackTimeMS; }

private:
    void PackBodyBuffer();
    void PackBodyStatic();
    void PackBodyDynamic();
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

    void RecreateCalculateArbiter();
    void RecreateCalculateJoint();
    void RecreateCalculateJunction();

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

    float mCPUUploadTimeMS   = 0.0f;
    float mGPUExecuteTimeMS  = 0.0f;
    float mGPUReadbackTimeMS = 0.0f;

    std::unordered_map<void*, uint32_t> mBodyIndexMap;

    std::vector<uint32_t> mCpuArbiterBuffer;
    std::vector<float>    mCpuJointBuffer;
    std::vector<float>    mCpuJunctionBuffer;
};

} // namespace PhysicsBlock