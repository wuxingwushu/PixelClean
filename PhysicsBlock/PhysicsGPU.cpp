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
#include <algorithm>

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
    delete mBodyStaging;
    delete mArbiterStaging;
    delete mArbiterCount;
    delete mJointCount;
    delete mJunctionCount;
}

uint32_t PhysicsGPU::GetBodyIndex(void* objPtr) {
    auto it = mBodyIndexMap.find(objPtr);
    if (it != mBodyIndexMap.end()) {
        return it->second;
    }
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

void PhysicsGPU::PackBodyStatic() {
    auto& shapes    = mWorld->PhysicsShapeS;
    auto& circles   = mWorld->PhysicsCircleS;
    auto& particles = mWorld->PhysicsParticleS;
    auto& lines     = mWorld->PhysicsLineS;

    size_t dynCount = shapes.size() + circles.size() + particles.size() + lines.size();
    bool hasMap = (mWorld->GetMapFormwork() != nullptr);
    size_t N = dynCount + (hasMap ? 1 : 0);
    if (N == 0) return;

    EnsureBodyCapacity(N);

    float* dst = reinterpret_cast<float*>(mBodyBuffer->getPersistentMappedPtr());
    size_t offset = 0;

    auto writeStaticAngle = [&](PhysicsAngle* obj) {
        dst[offset + 3] = obj->invMass;
        dst[offset + 4] = obj->invMomentInertia;
        dst[offset + 5] = obj->mass;
        dst[offset + 6] = obj->friction;
        dst[offset + 7] = 0.0f;
        offset += BODY_FLOATS;
    };

    auto writeStaticParticle = [&](PhysicsParticle* obj) {
        dst[offset + 3] = obj->invMass;
        dst[offset + 4] = 0.0f;
        dst[offset + 5] = obj->mass;
        dst[offset + 6] = obj->friction;
        dst[offset + 7] = 0.0f;
        offset += BODY_FLOATS;
    };

    for (auto* s : shapes)    writeStaticAngle(s);
    for (auto* c : circles)   writeStaticAngle(c);
    for (auto* p : particles) writeStaticParticle(p);
    for (auto* l : lines)     writeStaticAngle(l);

    if (hasMap) {
        dst[offset + 3] = 0.0f;
        dst[offset + 4] = 0.0f;
        dst[offset + 5] = 0.0f;
        dst[offset + 6] = 0.0f;
        dst[offset + 7] = 0.0f;
    }
}

void PhysicsGPU::PackBodyDynamic() {
    auto& shapes    = mWorld->PhysicsShapeS;
    auto& circles   = mWorld->PhysicsCircleS;
    auto& particles = mWorld->PhysicsParticleS;
    auto& lines     = mWorld->PhysicsLineS;

    size_t dynCount = shapes.size() + circles.size() + particles.size() + lines.size();
    bool hasMap = (mWorld->GetMapFormwork() != nullptr);
    size_t N = dynCount + (hasMap ? 1 : 0);

    mBodyIndexMap.clear();
    if (dynCount == 0) return;
    mBodyIndexMap.reserve(N);

    EnsureBodyCapacity(N);

    float* dst = reinterpret_cast<float*>(mBodyBuffer->getPersistentMappedPtr());

    const uint32_t shapesStart    = 0;
    const uint32_t circlesStart   = shapesStart    + (uint32_t)shapes.size();
    const uint32_t particlesStart = circlesStart   + (uint32_t)circles.size();
    const uint32_t linesStart     = particlesStart + (uint32_t)particles.size();

    const int xThreadNum = (int)std::thread::hardware_concurrency();
    std::vector<std::future<std::unordered_map<void*, uint32_t>>> futures;

    const auto PackChunk = [&](int threadIdx, int totalThreads)
            -> std::unordered_map<void*, uint32_t> {
        std::unordered_map<void*, uint32_t> localMap;
        int start, end;

        ThreadTaskAllot(start, end, (int)shapes.size(), threadIdx, totalThreads);
        for (int i = start; i < end; ++i) {
            PhysicsAngle* obj = shapes[i];
            uint32_t idx = shapesStart + i;
            localMap[obj] = idx;
            size_t off = idx * BODY_FLOATS;
            dst[off + 0] = obj->speed.x;
            dst[off + 1] = obj->speed.y;
            dst[off + 2] = obj->angleSpeed;
        }

        ThreadTaskAllot(start, end, (int)circles.size(), threadIdx, totalThreads);
        for (int i = start; i < end; ++i) {
            PhysicsAngle* obj = circles[i];
            uint32_t idx = circlesStart + i;
            localMap[obj] = idx;
            size_t off = idx * BODY_FLOATS;
            dst[off + 0] = obj->speed.x;
            dst[off + 1] = obj->speed.y;
            dst[off + 2] = obj->angleSpeed;
        }

        ThreadTaskAllot(start, end, (int)particles.size(), threadIdx, totalThreads);
        for (int i = start; i < end; ++i) {
            PhysicsParticle* obj = particles[i];
            uint32_t idx = particlesStart + i;
            localMap[obj] = idx;
            size_t off = idx * BODY_FLOATS;
            dst[off + 0] = obj->speed.x;
            dst[off + 1] = obj->speed.y;
            dst[off + 2] = 0.0f;
        }

        ThreadTaskAllot(start, end, (int)lines.size(), threadIdx, totalThreads);
        for (int i = start; i < end; ++i) {
            PhysicsAngle* obj = lines[i];
            uint32_t idx = linesStart + i;
            localMap[obj] = idx;
            size_t off = idx * BODY_FLOATS;
            dst[off + 0] = obj->speed.x;
            dst[off + 1] = obj->speed.y;
            dst[off + 2] = obj->angleSpeed;
        }

        return localMap;
    };

    for (int i = 0; i < xThreadNum; ++i)
        futures.push_back(std::async(std::launch::async, PackChunk, i, xThreadNum));

    for (auto& f : futures) {
        auto localMap = f.get();
        mBodyIndexMap.insert(localMap.begin(), localMap.end());
    }

    if (hasMap) {
        mBodyIndexMap[mWorld->GetMapFormwork()] = (uint32_t)dynCount;
    }
}

void PhysicsGPU::PackBodyBuffer() {
    PackBodyStatic();
    PackBodyDynamic();
}

void PhysicsGPU::PackArbiterBuffer() {
    auto& arbVec = mWorld->CollideGroupVector;
    size_t N = arbVec.size();
    if (N == 0) return;

    EnsureArbiterCapacity(N);

    mCpuArbiterBuffer.assign(N * ARBITER_STRIDE_U32, 0u);
    std::vector<uint32_t>& cpuArbiter = mCpuArbiterBuffer;

    for (size_t i = 0; i < N; ++i) {
        BaseArbiter* arb = arbVec[i];
        uint32_t* slot = cpuArbiter.data() + i * ARBITER_STRIDE_U32;

        slot[0] = GetBodyIndex(arb->mOriginalObject1);
        slot[1] = GetBodyIndex(arb->mOriginalObject2);
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

    memcpy(reinterpret_cast<uint32_t*>(mArbiterBuffer->getPersistentMappedPtr()),
           cpuArbiter.data(), N * ARBITER_STRIDE_BYTES);
}

void PhysicsGPU::PackJointBuffer() {
    auto& jointVec = mWorld->PhysicsJointS;
    size_t N = jointVec.size();
    if (N == 0) return;

    EnsureJointCapacity(N);

    mCpuJointBuffer.resize(N * JOINT_FLOATS);
    std::vector<float>& cpuJoint = mCpuJointBuffer;

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

    memcpy(reinterpret_cast<float*>(mJointBuffer->getPersistentMappedPtr()),
           cpuJoint.data(), N * JOINT_BYTES);
}

void PhysicsGPU::PackJunctionBuffer() {
    auto& junctionVec = mWorld->BaseJunctionS;
    size_t N = junctionVec.size();
    if (N == 0) return;

    EnsureJunctionCapacity(N);

    mCpuJunctionBuffer.resize(N * JUNCTION_FLOATS);
    std::vector<float>& cpuJunction = mCpuJunctionBuffer;

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

    memcpy(reinterpret_cast<float*>(mJunctionBuffer->getPersistentMappedPtr()),
           cpuJunction.data(), N * JUNCTION_BYTES);
}

void PhysicsGPU::PackSingleArbiter(uint32_t arbIdx) {
    BaseArbiter* arb = mWorld->CollideGroupVector[arbIdx];
    uint32_t* dst = reinterpret_cast<uint32_t*>(mArbiterBuffer->getPersistentMappedPtr());
    uint32_t* slot = dst + arbIdx * ARBITER_STRIDE_U32;

    slot[0] = GetBodyIndex(arb->mOriginalObject1);
    slot[1] = GetBodyIndex(arb->mOriginalObject2);
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

void PhysicsGPU::PackSingleJoint(uint32_t jIdx) {
    PhysicsJoint* j = mWorld->PhysicsJointS[jIdx];
    float* dst = reinterpret_cast<float*>(mJointBuffer->getPersistentMappedPtr());
    float* slot = dst + jIdx * JOINT_FLOATS;

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

void PhysicsGPU::PackSingleJunction(uint32_t jnIdx) {
    BaseJunction* jn = mWorld->BaseJunctionS[jnIdx];
    float* dst = reinterpret_cast<float*>(mJunctionBuffer->getPersistentMappedPtr());
    float* slot = dst + jnIdx * JUNCTION_FLOATS;

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

void PhysicsGPU::UnpackBodyBuffer() {
    auto& shapes    = mWorld->PhysicsShapeS;
    auto& circles   = mWorld->PhysicsCircleS;
    auto& particles = mWorld->PhysicsParticleS;
    auto& lines     = mWorld->PhysicsLineS;

    size_t N = shapes.size() + circles.size() + particles.size() + lines.size();
    if (N == 0) return;

    float* mapped = reinterpret_cast<float*>(mBodyStaging->getPersistentMappedPtr());

    const uint32_t shapesStart    = 0;
    const uint32_t circlesStart   = shapesStart    + (uint32_t)shapes.size();
    const uint32_t particlesStart = circlesStart   + (uint32_t)circles.size();
    const uint32_t linesStart     = particlesStart + (uint32_t)particles.size();

    const int xThreadNum = (int)std::thread::hardware_concurrency();
    std::vector<std::future<void>> futures;

    const auto UnpackChunk = [&](int threadIdx, int totalThreads) {
        int start, end;

        ThreadTaskAllot(start, end, (int)shapes.size(), threadIdx, totalThreads);
        for (int i = start; i < end; ++i) {
            PhysicsAngle* obj = shapes[i];
            size_t off = (shapesStart + i) * BODY_FLOATS;
            obj->speed.x    = mapped[off + 0];
            obj->speed.y    = mapped[off + 1];
            obj->angleSpeed = mapped[off + 2];
        }

        ThreadTaskAllot(start, end, (int)circles.size(), threadIdx, totalThreads);
        for (int i = start; i < end; ++i) {
            PhysicsAngle* obj = circles[i];
            size_t off = (circlesStart + i) * BODY_FLOATS;
            obj->speed.x    = mapped[off + 0];
            obj->speed.y    = mapped[off + 1];
            obj->angleSpeed = mapped[off + 2];
        }

        ThreadTaskAllot(start, end, (int)particles.size(), threadIdx, totalThreads);
        for (int i = start; i < end; ++i) {
            PhysicsParticle* obj = particles[i];
            size_t off = (particlesStart + i) * BODY_FLOATS;
            obj->speed.x = mapped[off + 0];
            obj->speed.y = mapped[off + 1];
        }

        ThreadTaskAllot(start, end, (int)lines.size(), threadIdx, totalThreads);
        for (int i = start; i < end; ++i) {
            PhysicsAngle* obj = lines[i];
            size_t off = (linesStart + i) * BODY_FLOATS;
            obj->speed.x    = mapped[off + 0];
            obj->speed.y    = mapped[off + 1];
            obj->angleSpeed = mapped[off + 2];
        }
    };

    for (int i = 0; i < xThreadNum; ++i)
        futures.push_back(std::async(std::launch::async, UnpackChunk, i, xThreadNum));
    for (auto& f : futures) f.wait();
}

void PhysicsGPU::UnpackArbiterPnPt() {
    auto& arbVec = mWorld->CollideGroupVector;
    size_t N = arbVec.size();
    if (N == 0) return;

    uint32_t* mapped = reinterpret_cast<uint32_t*>(mArbiterStaging->getPersistentMappedPtr());

    const int xThreadNum = (int)std::thread::hardware_concurrency();
    std::vector<std::future<void>> futures;

    const auto UnpackChunk = [&](int threadIdx, int totalThreads) {
        int start, end;
        ThreadTaskAllot(start, end, (int)N, threadIdx, totalThreads);
        for (int i = start; i < end; ++i) {
            BaseArbiter* arb = arbVec[i];
            uint32_t* slot = mapped + i * ARBITER_STRIDE_U32;
            float* contacts = reinterpret_cast<float*>(slot + 8);

            for (int ci = 0; ci < arb->numContacts && ci < (int)MAX_CONTACTS; ++ci) {
                float* ct = contacts + ci * CONTACT_FLOATS;
                arb->contacts[ci].Pn = ct[10];
                arb->contacts[ci].Pt = ct[11];
            }
        }
    };

    for (int i = 0; i < xThreadNum; ++i)
        futures.push_back(std::async(std::launch::async, UnpackChunk, i, xThreadNum));
    for (auto& f : futures) f.wait();
}

void PhysicsGPU::EnsureBodyCapacity(size_t N) {
    if (N <= mBodyCapacity) return;
    delete mBodyBuffer;
    mBodyBuffer = new VulKan::Buffer(
        mDevice, N * BODY_BYTES,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    mBodyBuffer->getPersistentMappedPtr();
    mBodyCapacity = N;

    delete mBodyStaging;
    mBodyStaging = new VulKan::Buffer(
        mDevice, N * BODY_BYTES,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT
    );
    mBodyStaging->getPersistentMappedPtr();
    mBodyStagingCapacity = N;

    RecreateCalculateArbiter();
    RecreateCalculateJoint();
    RecreateCalculateJunction();
    PackBodyStatic();
}

void PhysicsGPU::EnsureArbiterCapacity(size_t N) {
    if (N <= mArbiterCapacity) return;
    delete mArbiterBuffer;
    mArbiterBuffer = new VulKan::Buffer(
        mDevice, N * ARBITER_STRIDE_BYTES,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    mArbiterBuffer->getPersistentMappedPtr();
    mArbiterCapacity = N;

    delete mArbiterStaging;
    mArbiterStaging = new VulKan::Buffer(
        mDevice, N * ARBITER_STRIDE_BYTES,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT
    );
    mArbiterStaging->getPersistentMappedPtr();
    mArbiterStagingCapacity = N;

    RecreateCalculateArbiter();
}

void PhysicsGPU::EnsureJointCapacity(size_t N) {
    if (N <= mJointCapacity) return;
    delete mJointBuffer;
    mJointBuffer = new VulKan::Buffer(
        mDevice, N * JOINT_BYTES,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    mJointBuffer->getPersistentMappedPtr();
    mJointCapacity = N;
    RecreateCalculateJoint();
}

void PhysicsGPU::EnsureJunctionCapacity(size_t N) {
    if (N <= mJunctionCapacity) return;
    delete mJunctionBuffer;
    mJunctionBuffer = new VulKan::Buffer(
        mDevice, N * JUNCTION_BYTES,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    mJunctionBuffer->getPersistentMappedPtr();
    mJunctionCapacity = N;
    RecreateCalculateJunction();
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
    delete mBodyStaging; mBodyStaging = nullptr;
    delete mArbiterStaging; mArbiterStaging = nullptr;
    delete mArbiterCount; mArbiterCount = nullptr;
    delete mJointCount; mJointCount = nullptr;
    delete mJunctionCount; mJunctionCount = nullptr;
    mBodyCapacity = mArbiterCapacity = mJointCapacity = mJunctionCapacity = 0;
    mBodyStagingCapacity = mArbiterStagingCapacity = 0;

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
    mArbiterCount->getPersistentMappedPtr();
    mJointCount = new VulKan::Buffer(
        mDevice, COUNT_BUFFER_BYTES,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    mJointCount->getPersistentMappedPtr();
    mJunctionCount = new VulKan::Buffer(
        mDevice, COUNT_BUFFER_BYTES,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    mJunctionCount->getPersistentMappedPtr();

    mSharedCmdPool   = new VulKan::CommandPool(mDevice);
    mSharedCmdBuffer = new VulKan::CommandBuffer(mDevice, mSharedCmdPool);

    auto caps = mDevice->getComputeCapabilities();
    mSMCount = caps.smCount;
    uint32_t sgSize = caps.subgroupSize;
    if (sgSize == 0) sgSize = 32;
    mLocalSizeX = sgSize * 2;
    if (mLocalSizeX < 32)  mLocalSizeX = 32;
    if (mLocalSizeX > 256) mLocalSizeX = 256;

    if (mBodyBuffer && mArbiterBuffer) {
        std::vector<VulKan::CalculateStruct> calcStruct(3);
        calcStruct[0].mBufferInfo = &mBodyBuffer->getBufferInfo();
        calcStruct[1].mBufferInfo = &mArbiterBuffer->getBufferInfo();
        calcStruct[2].mBufferInfo = &mArbiterCount->getBufferInfo();
        mCalculateArbiter = new VulKan::Calculate(
            mDevice, &calcStruct, "shaders/physics_arbiter.comp.spv", mLocalSizeX
        );
    }

    if (mBodyBuffer && mJointBuffer) {
        std::vector<VulKan::CalculateStruct> calcStruct(3);
        calcStruct[0].mBufferInfo = &mBodyBuffer->getBufferInfo();
        calcStruct[1].mBufferInfo = &mJointBuffer->getBufferInfo();
        calcStruct[2].mBufferInfo = &mJointCount->getBufferInfo();
        mCalculateJoint = new VulKan::Calculate(
            mDevice, &calcStruct, "shaders/physics_joint.comp.spv", mLocalSizeX
        );
    }

    if (mBodyBuffer && mJunctionBuffer) {
        std::vector<VulKan::CalculateStruct> calcStruct(3);
        calcStruct[0].mBufferInfo = &mBodyBuffer->getBufferInfo();
        calcStruct[1].mBufferInfo = &mJunctionBuffer->getBufferInfo();
        calcStruct[2].mBufferInfo = &mJunctionCount->getBufferInfo();
        mCalculateJunction = new VulKan::Calculate(
            mDevice, &calcStruct, "shaders/physics_junction.comp.spv", mLocalSizeX
        );
    }

    mReady = true;
}

void PhysicsGPU::RecreateCalculateArbiter() {
    if (mBodyBuffer && mArbiterBuffer && mArbiterCount) {
        delete mCalculateArbiter;
        mCalculateArbiter = nullptr;
        std::vector<VulKan::CalculateStruct> calcStruct(3);
        calcStruct[0].mBufferInfo = &mBodyBuffer->getBufferInfo();
        calcStruct[1].mBufferInfo = &mArbiterBuffer->getBufferInfo();
        calcStruct[2].mBufferInfo = &mArbiterCount->getBufferInfo();
        mCalculateArbiter = new VulKan::Calculate(
            mDevice, &calcStruct, "shaders/physics_arbiter.comp.spv", mLocalSizeX
        );
    }
}

void PhysicsGPU::RecreateCalculateJoint() {
    if (mBodyBuffer && mJointBuffer && mJointCount) {
        delete mCalculateJoint;
        mCalculateJoint = nullptr;
        std::vector<VulKan::CalculateStruct> calcStruct(3);
        calcStruct[0].mBufferInfo = &mBodyBuffer->getBufferInfo();
        calcStruct[1].mBufferInfo = &mJointBuffer->getBufferInfo();
        calcStruct[2].mBufferInfo = &mJointCount->getBufferInfo();
        mCalculateJoint = new VulKan::Calculate(
            mDevice, &calcStruct, "shaders/physics_joint.comp.spv", mLocalSizeX
        );
    }
}

void PhysicsGPU::RecreateCalculateJunction() {
    if (mBodyBuffer && mJunctionBuffer && mJunctionCount) {
        delete mCalculateJunction;
        mCalculateJunction = nullptr;
        std::vector<VulKan::CalculateStruct> calcStruct(3);
        calcStruct[0].mBufferInfo = &mBodyBuffer->getBufferInfo();
        calcStruct[1].mBufferInfo = &mJunctionBuffer->getBufferInfo();
        calcStruct[2].mBufferInfo = &mJunctionCount->getBufferInfo();
        mCalculateJunction = new VulKan::Calculate(
            mDevice, &calcStruct, "shaders/physics_junction.comp.spv", mLocalSizeX
        );
    }
}

void PhysicsGPU::ExecuteGPUApplyImpulse(float inv_dt, unsigned int impulseSize) {
    if (!mReady) return;

    const int xThreadNum = (int)std::thread::hardware_concurrency();

    auto tUploadStart = std::chrono::high_resolution_clock::now();

    PackBodyDynamic();

    EnsureArbiterCapacity(mWorld->CollideGroupVector.size());
    EnsureJointCapacity(mWorld->PhysicsJointS.size());
    EnsureJunctionCapacity(mWorld->BaseJunctionS.size());

    {
        std::vector<std::future<void>> futures;
        const auto PackChunk = [this](int threadIdx, int totalThreads) {
            int start, end;
            ThreadTaskAllot(start, end, (int)mWorld->CollideGroupVector.size(), threadIdx, totalThreads);
            for (int i = start; i < end; ++i) PackSingleArbiter((uint32_t)i);
            ThreadTaskAllot(start, end, (int)mWorld->PhysicsJointS.size(), threadIdx, totalThreads);
            for (int i = start; i < end; ++i) PackSingleJoint((uint32_t)i);
            ThreadTaskAllot(start, end, (int)mWorld->BaseJunctionS.size(), threadIdx, totalThreads);
            for (int i = start; i < end; ++i) PackSingleJunction((uint32_t)i);
        };
        for (int i = 0; i < xThreadNum; ++i)
            futures.push_back(std::async(std::launch::async, PackChunk, i, xThreadNum));
        for (auto& f : futures) f.wait();
    }

    uint32_t N_arbiters  = (uint32_t)mWorld->CollideGroupVector.size();
    uint32_t N_joints    = (uint32_t)mWorld->PhysicsJointS.size();
    uint32_t N_junctions = (uint32_t)mWorld->BaseJunctionS.size();
    memcpy(mArbiterCount->getPersistentMappedPtr(), &N_arbiters, sizeof(uint32_t));
    memcpy(mJointCount->getPersistentMappedPtr(), &N_joints, sizeof(uint32_t));
    memcpy(mJunctionCount->getPersistentMappedPtr(), &N_junctions, sizeof(uint32_t));

    auto tUploadEnd = std::chrono::high_resolution_clock::now();
    mCPUUploadTimeMS = std::chrono::duration<float, std::milli>(tUploadEnd - tUploadStart).count();

    auto tExecuteStart = std::chrono::high_resolution_clock::now();

    mSharedCmdPool->reset();
    mSharedCmdBuffer->begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    VkCommandBuffer cmd = mSharedCmdBuffer->getCommandBuffer();

    VkMemoryBarrier hostToDeviceMemoryBarrier = {};
    hostToDeviceMemoryBarrier.sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    hostToDeviceMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
    hostToDeviceMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;

    VkBufferMemoryBarrier bodyHostBarrier = {};
    bodyHostBarrier.sType         = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    bodyHostBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
    bodyHostBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    bodyHostBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bodyHostBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bodyHostBarrier.buffer        = mBodyBuffer->getBuffer();
    bodyHostBarrier.offset        = 0;
    bodyHostBarrier.size          = VK_WHOLE_SIZE;

    std::vector<VkBufferMemoryBarrier> hostBarriers;
    hostBarriers.push_back(bodyHostBarrier);
    if (mArbiterBuffer) {
        VkBufferMemoryBarrier ab = bodyHostBarrier;
        ab.buffer = mArbiterBuffer->getBuffer();
        hostBarriers.push_back(ab);
    }
    if (mJointBuffer) {
        VkBufferMemoryBarrier jb = bodyHostBarrier;
        jb.buffer = mJointBuffer->getBuffer();
        hostBarriers.push_back(jb);
    }
    if (mJunctionBuffer) {
        VkBufferMemoryBarrier jnb = bodyHostBarrier;
        jnb.buffer = mJunctionBuffer->getBuffer();
        hostBarriers.push_back(jnb);
    }
    if (mArbiterCount) {
        VkBufferMemoryBarrier cb = bodyHostBarrier;
        cb.buffer = mArbiterCount->getBuffer();
        hostBarriers.push_back(cb);
    }

    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_HOST_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0,
        1, &hostToDeviceMemoryBarrier,
        (uint32_t)hostBarriers.size(),
        hostBarriers.data(),
        0, nullptr);

    VkBufferMemoryBarrier bodyBarrier = {};
    bodyBarrier.sType         = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    bodyBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    bodyBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    bodyBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bodyBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bodyBarrier.buffer        = mBodyBuffer->getBuffer();
    bodyBarrier.offset        = 0;
    bodyBarrier.size          = VK_WHOLE_SIZE;

    VkBufferMemoryBarrier arbiterBarrier = bodyBarrier;
    if (mArbiterBuffer) arbiterBarrier.buffer = mArbiterBuffer->getBuffer();

    VkBufferMemoryBarrier jointBarrier = bodyBarrier;
    if (mJointBuffer) jointBarrier.buffer = mJointBuffer->getBuffer();

    const uint32_t MIN_WG = mSMCount;

    uint32_t groupsA = (N_arbiters > 0)
        ? std::max((N_arbiters + mLocalSizeX - 1) / mLocalSizeX, MIN_WG)
        : 0u;
    uint32_t groupsJ = (N_joints > 0)
        ? std::max((N_joints + mLocalSizeX - 1) / mLocalSizeX, MIN_WG)
        : 0u;
    uint32_t groupsJu = (N_junctions > 0)
        ? std::max((N_junctions + mLocalSizeX - 1) / mLocalSizeX, MIN_WG)
        : 0u;

    VkMemoryBarrier memBarrier = {};
    memBarrier.sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    memBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    memBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;

    for (unsigned int iter = 0; iter < impulseSize; ++iter) {
        if (N_arbiters > 0 && mCalculateArbiter) {
            mCalculateArbiter->recordDispatch(cmd, groupsA);
            VkBufferMemoryBarrier ab[2] = { bodyBarrier, arbiterBarrier };
            vkCmdPipelineBarrier(cmd,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                0,
                1, &memBarrier,
                2, ab,
                0, nullptr);
        }

        if (N_joints > 0 && mCalculateJoint) {
            mCalculateJoint->recordDispatch(cmd, groupsJ);
            VkBufferMemoryBarrier jb[2] = { bodyBarrier, jointBarrier };
            vkCmdPipelineBarrier(cmd,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                0,
                1, &memBarrier,
                2, jb,
                0, nullptr);
        }

        if (N_junctions > 0 && mCalculateJunction) {
            mCalculateJunction->recordDispatch(cmd, groupsJu);
            vkCmdPipelineBarrier(cmd,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                0,
                1, &memBarrier,
                1, &bodyBarrier,
                0, nullptr);
        }
    }

    VkMemoryBarrier deviceToHostMemoryBarrier = {};
    deviceToHostMemoryBarrier.sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    deviceToHostMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    deviceToHostMemoryBarrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;

    VkBufferMemoryBarrier bodyDeviceToHostBarrier = {};
    bodyDeviceToHostBarrier.sType         = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    bodyDeviceToHostBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    bodyDeviceToHostBarrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
    bodyDeviceToHostBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bodyDeviceToHostBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bodyDeviceToHostBarrier.buffer        = mBodyBuffer->getBuffer();
    bodyDeviceToHostBarrier.offset        = 0;
    bodyDeviceToHostBarrier.size          = VK_WHOLE_SIZE;

    std::vector<VkBufferMemoryBarrier> readbackBarriers;
    readbackBarriers.push_back(bodyDeviceToHostBarrier);
    if (mArbiterBuffer) {
        VkBufferMemoryBarrier ab = bodyDeviceToHostBarrier;
        ab.buffer = mArbiterBuffer->getBuffer();
        readbackBarriers.push_back(ab);
    }

    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_HOST_BIT,
        0,
        1, &deviceToHostMemoryBarrier,
        (uint32_t)readbackBarriers.size(),
        readbackBarriers.data(),
        0, nullptr);

    if (mBodyBuffer && mBodyStaging) {
        VkBufferMemoryBarrier preCopyBarrier = {};
        preCopyBarrier.sType         = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        preCopyBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        preCopyBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        preCopyBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        preCopyBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        preCopyBarrier.buffer        = mBodyBuffer->getBuffer();
        preCopyBarrier.offset        = 0;
        preCopyBarrier.size          = VK_WHOLE_SIZE;

        vkCmdPipelineBarrier(cmd,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            0, nullptr,
            1, &preCopyBarrier,
            0, nullptr);

        VkBufferCopy bodyCopyRegion = {};
        bodyCopyRegion.size = mBodyCapacity * BODY_BYTES;
        vkCmdCopyBuffer(cmd,
            mBodyBuffer->getBuffer(),
            mBodyStaging->getBuffer(),
            1, &bodyCopyRegion);

        VkBufferMemoryBarrier postCopyBarrier = preCopyBarrier;
        postCopyBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        postCopyBarrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
        postCopyBarrier.buffer        = mBodyStaging->getBuffer();

        vkCmdPipelineBarrier(cmd,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_HOST_BIT,
            0,
            0, nullptr,
            1, &postCopyBarrier,
            0, nullptr);
    }

    if (mArbiterBuffer && mArbiterStaging) {
        VkBufferMemoryBarrier preCopyBarrierA = {};
        preCopyBarrierA.sType         = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        preCopyBarrierA.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        preCopyBarrierA.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        preCopyBarrierA.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        preCopyBarrierA.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        preCopyBarrierA.buffer        = mArbiterBuffer->getBuffer();
        preCopyBarrierA.offset        = 0;
        preCopyBarrierA.size          = VK_WHOLE_SIZE;

        vkCmdPipelineBarrier(cmd,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            0, nullptr,
            1, &preCopyBarrierA,
            0, nullptr);

        VkBufferCopy arbCopyRegion = {};
        arbCopyRegion.size = mArbiterCapacity * ARBITER_STRIDE_BYTES;
        vkCmdCopyBuffer(cmd,
            mArbiterBuffer->getBuffer(),
            mArbiterStaging->getBuffer(),
            1, &arbCopyRegion);

        VkBufferMemoryBarrier postCopyBarrierA = preCopyBarrierA;
        postCopyBarrierA.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        postCopyBarrierA.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
        postCopyBarrierA.buffer        = mArbiterStaging->getBuffer();

        vkCmdPipelineBarrier(cmd,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_HOST_BIT,
            0,
            0, nullptr,
            1, &postCopyBarrierA,
            0, nullptr);
    }

    mSharedCmdBuffer->end();
    mSharedCmdBuffer->submitSync(mDevice->getGraphicQueue());

    auto tExecuteEnd = std::chrono::high_resolution_clock::now();
    mGPUExecuteTimeMS = std::chrono::duration<float, std::milli>(tExecuteEnd - tExecuteStart).count();

    auto tReadbackStart = std::chrono::high_resolution_clock::now();

    UnpackBodyBuffer();
    UnpackArbiterPnPt();

    auto tReadbackEnd = std::chrono::high_resolution_clock::now();
    mGPUReadbackTimeMS = std::chrono::duration<float, std::milli>(tReadbackEnd - tReadbackStart).count();
}

} // namespace PhysicsBlock