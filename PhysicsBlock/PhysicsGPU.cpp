#include "PhysicsGPU.hpp"
#include "PhysicsWorld.hpp"
#include "../VulkanTool/Calculate.h"
#include "../Vulkan/buffer.h"
#include "../Vulkan/device.h"
#include "../Vulkan/commandPool.h"
#include "../Vulkan/commandBuffer.h"
#include "PhysicsBaseArbiter.hpp"
#include <cstring>
#include <cassert>

namespace PhysicsBlock {

static constexpr size_t BODY_FLOATS = 8;
static constexpr size_t BODY_BYTES = BODY_FLOATS * sizeof(float);
static constexpr size_t MAX_CONTACTS = 20;
static constexpr size_t CONTACT_FLOATS = 12;
static constexpr size_t CONTACT_BYTES = CONTACT_FLOATS * sizeof(float);
static constexpr size_t ARBITER_META_BYTES = 32;
static constexpr size_t ARBITER_STRIDE_BYTES = ARBITER_META_BYTES + MAX_CONTACTS * CONTACT_BYTES;
static constexpr size_t ARBITER_STRIDE_U32 = ARBITER_STRIDE_BYTES / sizeof(uint32_t);
static constexpr size_t JOINT_FLOATS = 20;
static constexpr size_t JOINT_BYTES = JOINT_FLOATS * sizeof(float);
static constexpr size_t JUNCTION_FLOATS = 20;
static constexpr size_t JUNCTION_BYTES = JUNCTION_FLOATS * sizeof(float);
static constexpr size_t COUNT_BUFFER_BYTES = 256;
static constexpr uint32_t INVALID_INDEX = 0xFFFFFFFFu;

PhysicsGPU::PhysicsGPU(VulKan::Device* device, PhysicsWorld* world)
    : mDevice(device), mWorld(world) {}

PhysicsGPU::~PhysicsGPU() {
    delete mCalculateArbiter;
    delete mCalculateJoint;
    delete mCalculateJunction;
    delete mSharedCmdBuffer;
    delete mSharedCmdPool;
    delete mBodyBuffer;
    delete mArbiterBuffer;
    delete mJointBuffer;
    delete mJunctionBuffer;
    delete mArbiterCount;
    delete mJointCount;
    delete mJunctionCount;
}

uint32_t PhysicsGPU::GetBodyIndex(void* objPtr) {
    auto& shapes    = mWorld->PhysicsShapeS;
    auto& circles   = mWorld->PhysicsCircleS;
    auto& particles = mWorld->PhysicsParticleS;
    auto& lines     = mWorld->PhysicsLineS;

    for (size_t i = 0; i < shapes.size(); ++i)
        if (shapes[i] == objPtr) return (uint32_t)i;
    size_t offset = shapes.size();
    for (size_t i = 0; i < circles.size(); ++i)
        if (circles[i] == objPtr) return (uint32_t)(offset + i);
    offset += circles.size();
    for (size_t i = 0; i < particles.size(); ++i)
        if (particles[i] == objPtr) return (uint32_t)(offset + i);
    offset += particles.size();
    for (size_t i = 0; i < lines.size(); ++i)
        if (lines[i] == objPtr) return (uint32_t)(offset + i);
    offset += lines.size();

    if (mWorld->GetMapFormwork() == objPtr) return (uint32_t)offset;

    return INVALID_INDEX;
}

uint32_t PhysicsGPU::GetArbiterGPUType(BaseArbiter* arb) {
    switch (arb->key.PoolID) {
        case 0: case 1: case 10: return 1;  // AD
        case 2: case 6: case 7: case 8: case 9: return 0;  // AA
        case 3: case 5: case 11: return 2;  // A
        case 4: return 3;  // D
        default: return 0;
    }
}

void PhysicsGPU::PackBodyBuffer() {
    auto& shapes    = mWorld->PhysicsShapeS;
    auto& circles   = mWorld->PhysicsCircleS;
    auto& particles = mWorld->PhysicsParticleS;
    auto& lines     = mWorld->PhysicsLineS;

    size_t dynCount = shapes.size() + circles.size() + particles.size() + lines.size();
    bool hasMap = (mWorld->GetMapFormwork() != nullptr);
    size_t N = dynCount + (hasMap ? 1 : 0);
    if (N == 0) return;

    EnsureBodyCapacity(N);

    std::vector<float> cpuBody(N * BODY_FLOATS);
    size_t offset = 0;

    auto packAngle = [&](PhysicsAngle* obj) {
        cpuBody[offset + 0] = obj->speed.x;
        cpuBody[offset + 1] = obj->speed.y;
        cpuBody[offset + 2] = obj->angleSpeed;
        cpuBody[offset + 3] = obj->invMass;
        cpuBody[offset + 4] = obj->invMomentInertia;
        cpuBody[offset + 5] = obj->mass;
        cpuBody[offset + 6] = obj->friction;
        cpuBody[offset + 7] = 0.0f;
        offset += BODY_FLOATS;
    };

    auto packParticle = [&](PhysicsParticle* obj) {
        cpuBody[offset + 0] = obj->speed.x;
        cpuBody[offset + 1] = obj->speed.y;
        cpuBody[offset + 2] = 0.0f;
        cpuBody[offset + 3] = obj->invMass;
        cpuBody[offset + 4] = 0.0f;
        cpuBody[offset + 5] = obj->mass;
        cpuBody[offset + 6] = obj->friction;
        cpuBody[offset + 7] = 0.0f;
        offset += BODY_FLOATS;
    };

    for (auto* s : shapes)    packAngle(s);
    for (auto* c : circles)   packAngle(c);
    for (auto* p : particles) packParticle(p);
    for (auto* l : lines)     packAngle(l);

    if (hasMap) {
        cpuBody[offset + 0] = 0.0f;
        cpuBody[offset + 1] = 0.0f;
        cpuBody[offset + 2] = 0.0f;
        cpuBody[offset + 3] = 0.0f;
        cpuBody[offset + 4] = 0.0f;
        cpuBody[offset + 5] = 0.0f;
        cpuBody[offset + 6] = 0.0f;
        cpuBody[offset + 7] = 0.0f;
    }

    mBodyBuffer->updateBufferByMap(cpuBody.data(), N * BODY_BYTES);
}

void PhysicsGPU::PackArbiterBuffer() {
    auto& arbVec = mWorld->CollideGroupVector;
    size_t N = arbVec.size();
    if (N == 0) return;

    EnsureArbiterCapacity(N);

    std::vector<uint32_t> cpuArbiter(N * ARBITER_STRIDE_U32, 0);

    for (size_t i = 0; i < N; ++i) {
        BaseArbiter* arb = arbVec[i];
        uint32_t* slot = cpuArbiter.data() + i * ARBITER_STRIDE_U32;

        slot[0] = GetBodyIndex(arb->key.object1);
        slot[1] = GetBodyIndex(arb->key.object2);
        slot[2] = GetArbiterGPUType(arb);
        slot[3] = (uint32_t)arb->numContacts;

        float* contacts = reinterpret_cast<float*>(slot + 8);
        for (int ci = 0; ci < arb->numContacts && ci < (int)MAX_CONTACTS; ++ci) {
            Contact& c = arb->contacts[ci];
            float* ct = contacts + ci * CONTACT_FLOATS;
            ct[0]  = c.normal.x;
            ct[1]  = c.normal.y;
            ct[2]  = c.r1.x;
            ct[3]  = c.r1.y;
            ct[4]  = c.r2.x;
            ct[5]  = c.r2.y;
            ct[6]  = c.massNormal;
            ct[7]  = c.massTangent;
            ct[8]  = c.bias;
            ct[9]  = c.friction;
            ct[10] = c.Pn;
            ct[11] = c.Pt;
        }
    }

    mArbiterBuffer->updateBufferByMap(cpuArbiter.data(), N * ARBITER_STRIDE_BYTES);
}

void PhysicsGPU::PackJointBuffer() {
    auto& jointVec = mWorld->PhysicsJointS;
    size_t N = jointVec.size();
    if (N == 0) return;

    EnsureJointCapacity(N);

    std::vector<float> cpuJoint(N * JOINT_FLOATS);

    for (size_t i = 0; i < N; ++i) {
        PhysicsJoint* j = jointVec[i];
        float* slot = cpuJoint.data() + i * JOINT_FLOATS;

        uint32_t idxA = GetBodyIndex(j->body1);
        uint32_t idxB = GetBodyIndex(j->body2);
        memcpy(&slot[0], &idxA, sizeof(uint32_t));
        memcpy(&slot[1], &idxB, sizeof(uint32_t));
        slot[2]  = j->r1.x;
        slot[3]  = j->r1.y;
        slot[4]  = j->r2.x;
        slot[5]  = j->r2.y;
        slot[6]  = j->bias.x;
        slot[7]  = j->bias.y;
        slot[8]  = j->M[0][0];
        slot[9]  = j->M[1][0];
        slot[10] = j->M[0][1];
        slot[11] = j->M[1][1];
        slot[12] = j->softness;
        slot[13] = j->P.x;
        slot[14] = j->P.y;
        for (int k = 15; k < (int)JOINT_FLOATS; ++k) slot[k] = 0.0f;
    }

    mJointBuffer->updateBufferByMap(cpuJoint.data(), N * JOINT_BYTES);
}

void PhysicsGPU::PackJunctionBuffer() {
    auto& junctionVec = mWorld->BaseJunctionS;
    size_t N = junctionVec.size();
    if (N == 0) return;

    EnsureJunctionCapacity(N);

    std::vector<float> cpuJunction(N * JUNCTION_FLOATS);

    for (size_t i = 0; i < N; ++i) {
        BaseJunction* jn = junctionVec[i];
        float* slot = cpuJunction.data() + i * JUNCTION_FLOATS;

        uint32_t bodyA = INVALID_INDEX;
        uint32_t bodyB = INVALID_INDEX;
        float fixedAX = 0, fixedAY = 0, fixedBX = 0, fixedBY = 0;
        float mR1X = 0, mR1Y = 0, mR2X = 0, mR2Y = 0;
        uint32_t jType = 0;

        switch (jn->ObjectType()) {
            case CordObjectType::JunctionAA: {
                auto* j = static_cast<PhysicsJunctionSS*>(jn);
                bodyA = GetBodyIndex(j->mParticle1);
                bodyB = GetBodyIndex(j->mParticle2);
                mR1X = j->mR1.x; mR1Y = j->mR1.y;
                mR2X = j->mR2.x; mR2Y = j->mR2.y;
                jType = 0;
                break;
            }
            case CordObjectType::JunctionA: {
                auto* j = static_cast<PhysicsJunctionS*>(jn);
                bodyA = GetBodyIndex(j->mParticle);
                fixedBX = j->mRegularDrop.x;
                fixedBY = j->mRegularDrop.y;
                mR1X = j->R.x; mR1Y = j->R.y;
                jType = 1;
                break;
            }
            case CordObjectType::JunctionP: {
                auto* j = static_cast<PhysicsJunctionP*>(jn);
                bodyA = GetBodyIndex(j->mParticle);
                fixedBX = j->mRegularDrop.x;
                fixedBY = j->mRegularDrop.y;
                jType = 2;
                break;
            }
            case CordObjectType::JunctionPP: {
                auto* j = static_cast<PhysicsJunctionPP*>(jn);
                bodyA = GetBodyIndex(j->mParticle1);
                bodyB = GetBodyIndex(j->mParticle2);
                jType = 3;
                break;
            }
            default:
                break;
        }

        memcpy(&slot[0], &bodyA, sizeof(uint32_t));
        memcpy(&slot[1], &bodyB, sizeof(uint32_t));
        slot[2]  = fixedAX;
        slot[3]  = fixedAY;
        slot[4]  = fixedBX;
        slot[5]  = fixedBY;
        slot[6]  = jn->Normal.x;
        slot[7]  = jn->Normal.y;
        slot[8]  = jn->bias;
        slot[9]  = jn->k;
        slot[10] = jn->Length;
        memcpy(&slot[11], &jType, sizeof(uint32_t));
        slot[12] = jn->ElasticityType();
        slot[13] = mR1X;
        slot[14] = mR1Y;
        slot[15] = mR2X;
        slot[16] = mR2Y;
        slot[17] = jn->P.x;
        slot[18] = jn->P.y;
        slot[19] = 0.0f;
    }

    mJunctionBuffer->updateBufferByMap(cpuJunction.data(), N * JUNCTION_BYTES);
}

void PhysicsGPU::UnpackBodyBuffer() {
    auto& shapes    = mWorld->PhysicsShapeS;
    auto& circles   = mWorld->PhysicsCircleS;
    auto& particles = mWorld->PhysicsParticleS;
    auto& lines     = mWorld->PhysicsLineS;

    size_t N = shapes.size() + circles.size() + particles.size() + lines.size();
    if (N == 0) return;

    float* mapped = reinterpret_cast<float*>(mBodyBuffer->getupdateBufferByMap());
    size_t offset = 0;

    auto unpackAngle = [&](PhysicsAngle* obj) {
        obj->speed.x   = mapped[offset + 0];
        obj->speed.y   = mapped[offset + 1];
        obj->angleSpeed = mapped[offset + 2];
        offset += BODY_FLOATS;
    };

    auto unpackParticle = [&](PhysicsParticle* obj) {
        obj->speed.x = mapped[offset + 0];
        obj->speed.y = mapped[offset + 1];
        offset += BODY_FLOATS;
    };

    for (auto* s : shapes)    unpackAngle(s);
    for (auto* c : circles)   unpackAngle(c);
    for (auto* p : particles) unpackParticle(p);
    for (auto* l : lines)     unpackAngle(l);

    mBodyBuffer->endupdateBufferByMap();
}

void PhysicsGPU::UnpackArbiterPnPt() {
    auto& arbVec = mWorld->CollideGroupVector;
    size_t N = arbVec.size();
    if (N == 0) return;

    uint32_t* mapped = reinterpret_cast<uint32_t*>(mArbiterBuffer->getupdateBufferByMap());

    for (size_t i = 0; i < N; ++i) {
        BaseArbiter* arb = arbVec[i];
        uint32_t* slot = mapped + i * ARBITER_STRIDE_U32;
        float* contacts = reinterpret_cast<float*>(slot + 8);

        for (int ci = 0; ci < arb->numContacts && ci < (int)MAX_CONTACTS; ++ci) {
            float* ct = contacts + ci * CONTACT_FLOATS;
            arb->contacts[ci].Pn = ct[10];
            arb->contacts[ci].Pt = ct[11];
        }
    }

    mArbiterBuffer->endupdateBufferByMap();
}

void PhysicsGPU::EnsureBodyCapacity(size_t N) {
    if (N <= mBodyCapacity) return;
    delete mBodyBuffer;
    mBodyBuffer = new VulKan::Buffer(
        mDevice, N * BODY_BYTES,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    mBodyCapacity = N;
    RecreateCalculates();
}

void PhysicsGPU::EnsureArbiterCapacity(size_t N) {
    if (N <= mArbiterCapacity) return;
    delete mArbiterBuffer;
    mArbiterBuffer = new VulKan::Buffer(
        mDevice, N * ARBITER_STRIDE_BYTES,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    mArbiterCapacity = N;
    RecreateCalculates();
}

void PhysicsGPU::EnsureJointCapacity(size_t N) {
    if (N <= mJointCapacity) return;
    delete mJointBuffer;
    mJointBuffer = new VulKan::Buffer(
        mDevice, N * JOINT_BYTES,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    mJointCapacity = N;
    RecreateCalculates();
}

void PhysicsGPU::EnsureJunctionCapacity(size_t N) {
    if (N <= mJunctionCapacity) return;
    delete mJunctionBuffer;
    mJunctionBuffer = new VulKan::Buffer(
        mDevice, N * JUNCTION_BYTES,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    mJunctionCapacity = N;
    RecreateCalculates();
}

void PhysicsGPU::Initialize() {
    delete mCalculateArbiter; mCalculateArbiter = nullptr;
    delete mCalculateJoint; mCalculateJoint = nullptr;
    delete mCalculateJunction; mCalculateJunction = nullptr;
    delete mSharedCmdBuffer; mSharedCmdBuffer = nullptr;
    delete mSharedCmdPool; mSharedCmdPool = nullptr;
    delete mBodyBuffer; mBodyBuffer = nullptr;
    delete mArbiterBuffer; mArbiterBuffer = nullptr;
    delete mJointBuffer; mJointBuffer = nullptr;
    delete mJunctionBuffer; mJunctionBuffer = nullptr;
    delete mArbiterCount; mArbiterCount = nullptr;
    delete mJointCount; mJointCount = nullptr;
    delete mJunctionCount; mJunctionCount = nullptr;
    mBodyCapacity = mArbiterCapacity = mJointCapacity = mJunctionCapacity = 0;

    PackBodyBuffer();
    PackArbiterBuffer();
    PackJointBuffer();
    PackJunctionBuffer();

    // Count buffer 必须满足 minStorageBufferOffsetAlignment（通常 64~256），
    // 此处固定 256 字节以适配任何 GPU
    mArbiterCount = new VulKan::Buffer(
        mDevice, COUNT_BUFFER_BYTES,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    mJointCount = new VulKan::Buffer(
        mDevice, COUNT_BUFFER_BYTES,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    mJunctionCount = new VulKan::Buffer(
        mDevice, COUNT_BUFFER_BYTES,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );

    if (mBodyBuffer && mArbiterBuffer) {
        std::vector<VulKan::CalculateStruct> calcStruct(3);
        calcStruct[0].mBufferInfo = &mBodyBuffer->getBufferInfo();
        calcStruct[1].mBufferInfo = &mArbiterBuffer->getBufferInfo();
        calcStruct[2].mBufferInfo = &mArbiterCount->getBufferInfo();
        mCalculateArbiter = new VulKan::Calculate(
            mDevice, &calcStruct, "shaders/physics_arbiter.comp.spv"
        );
    }

    if (mBodyBuffer && mJointBuffer) {
        std::vector<VulKan::CalculateStruct> calcStruct(3);
        calcStruct[0].mBufferInfo = &mBodyBuffer->getBufferInfo();
        calcStruct[1].mBufferInfo = &mJointBuffer->getBufferInfo();
        calcStruct[2].mBufferInfo = &mJointCount->getBufferInfo();
        mCalculateJoint = new VulKan::Calculate(
            mDevice, &calcStruct, "shaders/physics_joint.comp.spv"
        );
    }

    if (mBodyBuffer && mJunctionBuffer) {
        std::vector<VulKan::CalculateStruct> calcStruct(3);
        calcStruct[0].mBufferInfo = &mBodyBuffer->getBufferInfo();
        calcStruct[1].mBufferInfo = &mJunctionBuffer->getBufferInfo();
        calcStruct[2].mBufferInfo = &mJunctionCount->getBufferInfo();
        mCalculateJunction = new VulKan::Calculate(
            mDevice, &calcStruct, "shaders/physics_junction.comp.spv"
        );
    }

    mSharedCmdPool   = new VulKan::CommandPool(mDevice);
    mSharedCmdBuffer = new VulKan::CommandBuffer(mDevice, mSharedCmdPool);

    mReady = true;
}

void PhysicsGPU::RecreateCalculates() {
    if (mBodyBuffer && mArbiterBuffer && mArbiterCount) {
        delete mCalculateArbiter;
        mCalculateArbiter = nullptr;
        std::vector<VulKan::CalculateStruct> calcStruct(3);
        calcStruct[0].mBufferInfo = &mBodyBuffer->getBufferInfo();
        calcStruct[1].mBufferInfo = &mArbiterBuffer->getBufferInfo();
        calcStruct[2].mBufferInfo = &mArbiterCount->getBufferInfo();
        mCalculateArbiter = new VulKan::Calculate(
            mDevice, &calcStruct, "shaders/physics_arbiter.comp.spv"
        );
    }

    if (mBodyBuffer && mJointBuffer && mJointCount) {
        delete mCalculateJoint;
        mCalculateJoint = nullptr;
        std::vector<VulKan::CalculateStruct> calcStruct(3);
        calcStruct[0].mBufferInfo = &mBodyBuffer->getBufferInfo();
        calcStruct[1].mBufferInfo = &mJointBuffer->getBufferInfo();
        calcStruct[2].mBufferInfo = &mJointCount->getBufferInfo();
        mCalculateJoint = new VulKan::Calculate(
            mDevice, &calcStruct, "shaders/physics_joint.comp.spv"
        );
    }

    if (mBodyBuffer && mJunctionBuffer && mJunctionCount) {
        delete mCalculateJunction;
        mCalculateJunction = nullptr;
        std::vector<VulKan::CalculateStruct> calcStruct(3);
        calcStruct[0].mBufferInfo = &mBodyBuffer->getBufferInfo();
        calcStruct[1].mBufferInfo = &mJunctionBuffer->getBufferInfo();
        calcStruct[2].mBufferInfo = &mJunctionCount->getBufferInfo();
        mCalculateJunction = new VulKan::Calculate(
            mDevice, &calcStruct, "shaders/physics_junction.comp.spv"
        );
    }
}

void PhysicsGPU::ExecuteGPUApplyImpulse(float inv_dt, unsigned int impulseSize) {
    if (!mReady) return;

    auto tStart = std::chrono::high_resolution_clock::now();

    PackBodyBuffer();
    PackArbiterBuffer();
    PackJointBuffer();
    PackJunctionBuffer();

    uint32_t N_arbiters  = (uint32_t)mWorld->CollideGroupVector.size();
    uint32_t N_joints    = (uint32_t)mWorld->PhysicsJointS.size();
    uint32_t N_junctions = (uint32_t)mWorld->BaseJunctionS.size();
    mArbiterCount->updateBufferByMap(&N_arbiters, sizeof(uint32_t));
    mJointCount->updateBufferByMap(&N_joints, sizeof(uint32_t));
    mJunctionCount->updateBufferByMap(&N_junctions, sizeof(uint32_t));

    mSharedCmdPool->reset();
    mSharedCmdBuffer->begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    VkCommandBuffer cmd = mSharedCmdBuffer->getCommandBuffer();

    VkMemoryBarrier ssboBarrier = {};
    ssboBarrier.sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    ssboBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    ssboBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;

    uint32_t groupsA = (N_arbiters  + 255) / 256;
    uint32_t groupsJ = (N_joints    + 255) / 256;
    uint32_t groupsJu= (N_junctions + 255) / 256;

    for (unsigned int iter = 0; iter < impulseSize; ++iter) {
        if (N_arbiters > 0 && mCalculateArbiter) {
            mCalculateArbiter->recordDispatch(cmd, groupsA);
            vkCmdPipelineBarrier(cmd,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                0, 1, &ssboBarrier, 0, nullptr, 0, nullptr);
        }

        if (N_joints > 0 && mCalculateJoint) {
            mCalculateJoint->recordDispatch(cmd, groupsJ);
            vkCmdPipelineBarrier(cmd,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                0, 1, &ssboBarrier, 0, nullptr, 0, nullptr);
        }

        if (N_junctions > 0 && mCalculateJunction) {
            mCalculateJunction->recordDispatch(cmd, groupsJu);
            vkCmdPipelineBarrier(cmd,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                0, 1, &ssboBarrier, 0, nullptr, 0, nullptr);
        }
    }

    mSharedCmdBuffer->end();
    mSharedCmdBuffer->submitSync(mDevice->getGraphicQueue());

    UnpackBodyBuffer();
    UnpackArbiterPnPt();

    auto tEnd = std::chrono::high_resolution_clock::now();
    mLastFrameTimeMS = std::chrono::duration<float, std::milli>(tEnd - tStart).count();
}

} // namespace PhysicsBlock