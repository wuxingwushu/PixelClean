// ─────────────────────────────────────────────────────────────
// PhysicsLiquid3D —— Position Based Fluids (PBF)，3D 版
//
// 结构移植自 PixelClean/PhysicsBlock/PhysicsLiquid.cpp（2D PBF）。
// 算法与参数来自参考项目 Floaty-main：
//   src/lib.rs   solve_fluid_jacobi     → SolveFluidJacobi（λ/Δp Jacobi 求解）
//                NeighborLinkedList    → FindNeighbors（哈希链表邻居搜索）
//   solver.ts    PBD.simulate          → Update（子步循环）
//                PBD.solveFluid        → SolveFluidJacobi + SolveMapBoundaries + ApplyDeltas
//                PBD.enforceCFL        → EnforceCFL（位移钳制 + v = Δx/dt）
//   main.ts      SimulationParams      → Params
//
// ★ 阶段 A（结构 + 参数层）+ 阶段 B（算法）均已完成。
//   阶段 B = Update 的 14 个重求解阶段，逐式移植自 2D 参考
//   PhysicsLiquid.cpp:346-1264，并做数处维度改写：
//     · 邻居邻域 3×3 → 3×3×3 = 27（哈希混入第三轴奇数素数）
//     · 边界探测 8 向 → 26 向（{-1,0,1}³\0，全部归一化为单位向量）
//     · 刚体拟合 标量叉积 → L = Σ r×d 与 3×3 对称求逆 dθ = I⁻¹L
//     · 圆盘同心环 → 球体同心球壳（Fibonacci 球面均布）；胶囊 → PhysicsLine3D::GetSegment 轴段
//     · 标定幂次：eps ∝ k⁻²、cohesion ∝ k¹ 与 2D 相同；curvature 由 2D 的 k⁴ → k^(d+2) = k⁵
//     · restDensity 由 3D 点阵（7×7×7、间距 1.6r、d²<h²）精确标定 = 71.9514618
//       （2D 的 16.66667 只对二维体积元成立，沿用会导致 C≈3.3 直接爆开）
//   另：3D 的 SpikyScale 已是**梯度**幅度（2D 的 10/(πh⁵) 是核幅度），
//   故 2D use site 上的 ×3 因子在此去掉——见 SolveFluidJacobi 上方长注释。
// ─────────────────────────────────────────────────────────────

#include "PhysicsLiquid3D.hpp"
// 需要 PhysicsWorld3D 的完整定义（AddObject / RemoveObject / GetMapFormwork /
// GetLiquid / SetLiquid）。放在本头之后：本头只用前置声明，不反向依赖世界。
#include "PhysicsWorld3D.hpp"

#include <algorithm>
#include <cmath>
#include <functional> // ParallelRange3D 的 std::function
#include <future>     // ThreadPool::enqueue 返回的 std::future
#include <thread>     // std::thread::hardware_concurrency

namespace PhysicsBlock3D
{
    // ═════════════════════════════════════════════════════════
    // 3D 核常数（Floaty lib.rs；2D 为 4/(π·h⁸) 与 10/(π·h⁵)）
    // ═════════════════════════════════════════════════════════
    static const FLOAT_ kPi = FLOAT_(3.14159265358979);

    // 三维 poly6 核幅度 315/(64·π·h⁹)。
    // 参数标定（SyncFromParticleRadius）与 λ 求解共用同一公式，只此一处。
    FLOAT_ PhysicsLiquid3D::Poly6Scale(FLOAT_ h)
    {
        const FLOAT_ h2 = h * h;
        const FLOAT_ h9 = h2 * h2 * h2 * h2 * h; // h⁸·h
        return FLOAT_(315.0) / (FLOAT_(64.0) * kPi * h9);
    }

    // 三维 spiky 梯度核幅度 45/(π·h⁶)（∇W = −spikyScale·(h−r)²·r̂）。
    FLOAT_ PhysicsLiquid3D::SpikyScale(FLOAT_ h)
    {
        const FLOAT_ h2 = h * h;
        const FLOAT_ h6 = h2 * h2 * h2;
        return FLOAT_(45.0) / (kPi * h6);
    }

    // poly6 核权重 (h²−r²)³（Floaty 核函数本体；自贡献 = r2 取 0）。
    // 密度求和（参数标定 / λ 求解）与 XSPH 归一化共用，避免三次方展开写两遍。
    FLOAT_ PhysicsLiquid3D::Poly6Weight(FLOAT_ h2, FLOAT_ r2)
    {
        const FLOAT_ t = h2 - r2;
        return t * t * t;
    }

    // ── 位移上限钳制（Floaty enforceCFL：|Δx| ≤ maxLen，超限按比例缩回） ──
    // 边界推出 / 刚体拟合 / CFL 回代三处共用同一规则，公式只此一份。
    static inline void ClampDisplacement3D(Vec3_ &v, FLOAT_ maxLen)
    {
        const FLOAT_ len2 = ModulusLength(v); // 注意：3D 版返回**平方**长度
        if (len2 > maxLen * maxLen)
        {
            v *= maxLen / SQRT_(len2);
        }
    }

    // ═════════════════════════════════════════════════════════
    // Params
    // ═════════════════════════════════════════════════════════

    void PhysicsLiquid3D::Params::SyncFromParticleRadius()
    {
        // Floaty main.ts：kernelRadius = 3.0 * particleRadius。
        const FLOAT_ r = (particleRadius > FLOAT_(1e-4)) ? particleRadius : FLOAT_(0.15);
        particleRadius = r;
        kernelRadius = FLOAT_(3.0) * r;

        // 静息密度 = 静息间距（s = 1.6r，Floaty initSpacing）**三维**方点阵的实际核密度。
        // 2D 版用 7×7 覆盖 h=3r（s=1.6r 时 3 环即 4.8r > h），3D 用 7×7×7 同一覆盖半径。
        // 自贡献 + Σ_{i,j,k ∈ [−3,3]³ \ 0} poly6(d²)，仅累加 d² < h² 的邻居。
        // 自贡献必须与 SolveFluidJacobi 的约定一致（后者在 r2=0 处取到同一值）。
        const FLOAT_ s = FLOAT_(1.6) * r;
        const FLOAT_ h2 = kernelRadius * kernelRadius;
        const FLOAT_ poly6Scale = Poly6Scale(kernelRadius);
        FLOAT_ rho = poly6Scale * Poly6Weight(h2, FLOAT_(0)); // 自贡献
        for (int i = -3; i <= 3; ++i)
        {
            for (int j = -3; j <= 3; ++j)
            {
                for (int k = -3; k <= 3; ++k)
                {
                    if (i == 0 && j == 0 && k == 0)
                        continue;
                    const FLOAT_ dx = i * s;
                    const FLOAT_ dy = j * s;
                    const FLOAT_ dz = k * s;
                    const FLOAT_ d2 = dx * dx + dy * dy + dz * dz;
                    if (d2 < h2)
                    {
                        rho += poly6Scale * Poly6Weight(h2, d2);
                    }
                }
            }
        }
        restDensity = rho;

        // —— 有量纲常数按 Floaty 参考尺度（r0 = 0.15）不变量化 ——
        // 由核的量纲推导（Δp 各项量纲必须 = L；详见 PhysicsLiquid3D.hpp 中
        // SyncFromParticleRadius 的文档注释）：
        //   eps   ：∇C = ∇W/ρ0 量纲 L⁻¹（**与维度无关**）⇒ [eps] = L⁻² ⇒ eps ∝ k⁻²（同 2D）
        //   内聚  ：u 无量纲 ⇒ [cohesion] = L ⇒ ∝ k¹（同 2D）
        //   曲率  ：[spikyScale·w³] 量纲 L⁻ᵈ ⇒ [curvature] = L^(d+2)；
        //           2D 的 k⁴ 正是 d=2 的取值（交叉验证成立），3D d=3 ⇒ **k⁵**
        const FLOAT_ k = r / FLOAT_(0.15); // 尺度比 s/s0
        eps = FLOAT_(50.0) / (k * k);                    // Floaty: 50   @ r=0.15
        cohesion = FLOAT_(0.000025) * k;                 // 水预设 = Floaty 0.01²×25%
        curvature = FLOAT_(0.00002) * k * k * k * k * k; // Floaty: 0.001*0.02；3D 指数 d+2 = 5
    }

    void PhysicsLiquid3D::Params::Sanitize()
    {
        if (particleRadius <= FLOAT_(0))
            particleRadius = FLOAT_(0.15);
        if (kernelRadius <= FLOAT_(1e-4))
            kernelRadius = FLOAT_(3.0) * particleRadius;
        if (restDensity <= FLOAT_(1e-4))
            SyncFromParticleRadius();
        if (substeps < 1)
            substeps = 1;
        if (substeps > 100)
            substeps = 100;
        if (eps < FLOAT_(1e-4))
        {
            const FLOAT_ k = particleRadius / FLOAT_(0.15); // 与 SyncFromParticleRadius 同一尺度比
            eps = FLOAT_(50) / (k * k);
        }
        if (cfl <= FLOAT_(0))
            cfl = FLOAT_(0.12);
        if (cohesion < FLOAT_(0))
            cohesion = FLOAT_(0);
        if (curvature < FLOAT_(0))
            curvature = FLOAT_(0);
        if (viscosity < FLOAT_(0))
            viscosity = FLOAT_(0);
        if (viscosity > FLOAT_(1))
            viscosity = FLOAT_(1);
    }

    // ═════════════════════════════════════════════════════════
    // 生命周期 / 粒子管理
    // ═════════════════════════════════════════════════════════

    PhysicsLiquid3D::PhysicsLiquid3D(PhysicsWorld3D *World, const Params &P) : mWorld(World), param(P)
    {
    }

    PhysicsLiquid3D::~PhysicsLiquid3D()
    {
        // 主动把液体粒子从物理世界摘除（Clear 内部逐个 RemoveObject）。
        // 这样无论"世界析构先删液体"还是"用户手动 delete 液体"，
        // 都不会留下 IsLiquidParticle=true 却无人更新的孤儿粒子。
        Clear();
        // GetLiquid/SetLiquid 只在液体子系统开启时存在（PhysicsWorld3D.hpp:197-201，
        // 位于 #if PhysicsLiquid3DBool3D 内），故回指注销同样受开关保护。
#if PhysicsLiquid3DBool3D
        if (mWorld != nullptr && mWorld->GetLiquid() == this)
        {
            mWorld->SetLiquid(nullptr);
        }
#endif
    }

    PhysicsParticle3D *PhysicsLiquid3D::AddParticle(Vec3_ pos, FLOAT_ mass, FLOAT_ friction)
    {
        PhysicsParticle3D *p = new PhysicsParticle3D(pos, mass, friction);
        p->IsLiquidParticle = true; // 液体粒子跳过引擎仲裁器（固液交互由本系统按 PBF 自理）
        mWorld->AddObject(p);       // 注册到世界：网格搜索/渲染统计/拾取复用
        mParticles.push_back(p);
        mDensity.push_back(FLOAT_(0));
        return p;
    }

    void PhysicsLiquid3D::AddGrid(Vec3_ center, int numX, int numY, int numZ, FLOAT_ spacing,
                                  FLOAT_ mass, FLOAT_ friction)
    {
        // Floaty main.ts: initSpacing = 0.8 * particleDiameter ⇒ r = spacing / 1.6。
        // 由间距反推粒子尺度并按 Floaty 公式同步 kernelRadius / restDensity，
        // 使任何场景间距都保持 Floaty 的无量纲参数组（h=3r、ρ0=三维点阵核密度）。
        if (spacing > FLOAT_(0) && mParticles.empty()) // R15：只在空液体（首次填充）时同步
        {
            param.particleRadius = spacing / FLOAT_(1.6);
            param.SyncFromParticleRadius();
        }
        if (numX <= 0 || numY <= 0 || numZ <= 0)
        {
            return; // 3D 新增轴：任一分量为 0 则无粒子
        }
        MapFormwork3D *map = mWorld->GetMapFormwork(); // 不把水生成进地形实体内部
        const FLOAT_ ox = (FLOAT_)(numX - 1) * FLOAT_(0.5);
        const FLOAT_ oy = (FLOAT_)(numY - 1) * FLOAT_(0.5);
        const FLOAT_ oz = (FLOAT_)(numZ - 1) * FLOAT_(0.5);
        for (int x = 0; x < numX; ++x)
        {
            for (int y = 0; y < numY; ++y)
            {
                for (int z = 0; z < numZ; ++z)
                {
                    Vec3_ pos = center + Vec3_(
                        ((FLOAT_)x - ox) * spacing,
                        ((FLOAT_)y - oy) * spacing,
                        ((FLOAT_)z - oz) * spacing);
                    // 深陷粒子只能靠边界推出逃逸，会冲击邻近水体
                    if (map != nullptr && map->FMGetCollide(pos))
                    {
                        continue;
                    }
                    AddParticle(pos, mass, friction);
                }
            }
        }
    }

    void PhysicsLiquid3D::Clear()
    {
        // 先把粒子列表整体摘下再逐个删除：
        // PhysicsWorld3D::RemoveObject 会回调 NotifyParticleRemoved（在列表里做尾交换删除），
        // 若边遍历边删除会导致迭代器失效。
        std::vector<PhysicsParticle3D *> pending;
        pending.swap(mParticles);
        for (auto *p : pending)
        {
            if (p == nullptr)
            {
                continue;
            }
            if (mWorld != nullptr)
            {
                mWorld->RemoveObject(p); // RemoveObject 内部会 delete
            }
            else
            {
                delete p;
            }
        }
        mDensity.clear();
        mFluidCount = 0;
        mPos.clear();
        mVel.clear();
        mPrevPos.clear();
        mGrad.clear();
        mDelta.clear();
        mBoundaryDisp.clear();
        mLambda.clear();
        mInside.clear();
        mNearSolid.clear();
        mNext.clear();
        mFirst.clear();
        mNeighbors.clear();
        mPairDir.clear();
        mPairW.clear();
        mSamples.clear();
        mSampleWeight.clear();
        mSolids.clear();
        mStaticBodies.clear();
        mSolidXf.clear();
    }

    void PhysicsLiquid3D::NotifyParticleRemoved(PhysicsParticle3D *p)
    {
        const auto it = std::find(mParticles.begin(), mParticles.end(), p);
        if (it == mParticles.end())
        {
            return;
        }
        // 尾部交换删除（mParticles 与平行数组 mDensity 同步维护）
        const size_t i = (size_t)(it - mParticles.begin());
        const size_t last = mParticles.size() - 1;
        mParticles[i] = mParticles[last];
        if (i < mDensity.size() && last < mDensity.size())
        {
            mDensity[i] = mDensity[last];
        }
        mParticles.pop_back();
        if (mDensity.size() > mParticles.size())
        {
            mDensity.pop_back();
        }
    }

    // ═════════════════════════════════════════════════════════
    // 刚体采样点（Floaty 的 blob 粒子角色）
    // ═════════════════════════════════════════════════════════

    FLOAT_ PhysicsLiquid3D::SampleWeight(size_t combinedIndex) const
    {
        if (combinedIndex < mFluidCount)
        {
            return FLOAT_(0); // 流体粒子不占采样权重位（只有采样点区间才有权重）
        }
        const size_t s = combinedIndex - mFluidCount;
        return (s < mSampleWeight.size()) ? mSampleWeight[s] : FLOAT_(0);
    }

    // 采样点重新锚定到刚体位姿：world = body->pos + QuatRotate(body->rotation, local)。
    // 2D 版是 pos + vec2angle(local, body->angle)；3D 用四元数旋转。
    void PhysicsLiquid3D::SyncSamplePositions()
    {
        const size_t n = mFluidCount;
        const size_t total = std::min(mSamples.size(), mPos.size() - std::min(mPos.size(), n));
        for (size_t s = 0; s < total; ++s)
        {
            const SolidSample &sm = mSamples[s];
            if (sm.body == nullptr)
            {
                continue;
            }
            mPos[n + s] = sm.body->pos + QuatRotate(sm.body->rotation, sm.local);
        }
    }

    // 刚体是否包含该点（形状逐体素 / 球平方距离；线段太薄视为不包含）。
    // 走变换缓存：AABB 快筛 + 四元数逆旋转 + 无三角函数 + 球不开方。
    bool PhysicsLiquid3D::SolidContains(const SolidXf &xf, const Vec3_ &p)
    {
        const Vec3_ d = p - xf.pos;
        if (d.x < -xf.radius || d.x > xf.radius ||
            d.y < -xf.radius || d.y > xf.radius ||
            d.z < -xf.radius || d.z > xf.radius)
        {
            return false; // AABB 快筛：绝大多数查询一步退出
        }
        if (xf.shape != nullptr)
        {
            // 逆旋转到局部系后加上质心偏移，按体素网格逐格判定
            const Vec3_ local = QuatInverseRotate(xf.rotation, d) + xf.shape->CentreMass;
            const int cx = (int)floorf(local.x);
            const int cy = (int)floorf(local.y);
            const int cz = (int)floorf(local.z);
            if (!xf.shape->InBounds(cx, cy, cz))
                return false;
            return xf.shape->GridAt(cx, cy, cz).Entity;
        }
        if (xf.radius2 > FLOAT_(0)) // 球：平方距离（等价 Modulus < 0.9r）
        {
            return ModulusLength(d) < xf.radius2;
        }
        return false;
    }

    // 重建刚体变换缓存（BuildSolidSamples 末尾调用）。
    // 内容 = 动态体（[0, mSolids.size())，可采样、可投影）+ 静态体（纯几何钳制，R2-A）。
    // 动态体在前保证 xfIndex == mSolids 下标，ProjectSolids 可就地按下标刷新（不重排）。
    void PhysicsLiquid3D::RefreshSolidXform()
    {
        mSolidXf.clear();
        mSolidXf.reserve(mSolids.size() + mStaticBodies.size());

        auto MakeXf = [](PhysicsAngle3D *body) -> SolidXf
        {
            SolidXf xf;
            xf.body = body;
            if (body == nullptr)
            {
                return xf;
            }
            xf.pos = body->pos;
            xf.rotation = body->rotation;
            xf.radius = body->PFGetCollisionR();
            xf.dynamic = (body->invMass > FLOAT_(0));
            if (body->PFGetType() == PhysicsObjectEnum::shape)
            {
                xf.shape = (PhysicsShape3D *)body;
            }
            else if (body->PFGetType() == PhysicsObjectEnum::circle)
            {
                const FLOAT_ r9 = ((PhysicsCircle3D *)body)->radius * FLOAT_(0.9);
                xf.radius2 = r9 * r9;
            }
            return xf;
        };

        for (const SolidTrack &st : mSolids)
        {
            mSolidXf.push_back(MakeXf(st.body));
        }
        // 静态体：只提供几何钳制；位姿永不变化，帧内无需再刷新
        for (const SolidTrack &st : mStaticBodies)
        {
            mSolidXf.push_back(MakeXf(st.body));
        }
    }

    // ═════════════════════════════════════════════════════════
    // 重求解阶段（阶段 B：逐式移植自 2D 参考 PhysicsLiquid.cpp）
    //
    // 维度改写要点（见各函数上方注释）：
    //   · 邻域 3×3 → 3×3×3 = 27；哈希第三轴混入奇数素数
    //   · 边界探测 8 向 → 26 向（{-1,0,1}³\0，归一化为单位向量）
    //   · 刚体拟合 标量叉积 → L = Σ r×d 与 3×3 对称求逆 dθ = I⁻¹L
    // ═════════════════════════════════════════════════════════

    // ── 数据并行分片（λ/Δp 求解按 i 独立读写、无写冲突 → 结果与串行逐位一致、确定性） ──
    // 对应 2D PhysicsLiquid.cpp:794-819 的 ParallelRange；ThreadPool 在全局命名空间
    // （Tool/ThreadPool.h，经 PhysicsWorld3D.hpp 引入）。
    // 注：2D 版借用 solvers 自己的线程池；3D 的 PhysicsWorld3D::mThreadPool 是 **private**
    //     （PhysicsWorld3D.hpp:359 起为 private 段，且该头文件不在本次可改文件之列），
    //     故改用函数级 static 池：进程内共享、首次需要时才构造（默认构造 =
    //     hardware_concurrency 个 worker）。行为与 2D 等价，仅池的所有者不同。
    static void ParallelRange3D(size_t count, const std::function<void(size_t, size_t)> &fn)
    {
        const unsigned int T = std::min(std::thread::hardware_concurrency(), 8u);
        if (T <= 1 || count < 1024) // 小规模直接串行（分发开销 > 收益；T==0 也走这里）
        {
            fn(0, count);
            return;
        }
        static ThreadPool sPool; // C++11 魔法静态：首次进入此处才构造
        std::vector<std::future<void>> fs;
        fs.reserve(T);
        for (unsigned int t = 0; t < T; ++t)
        {
            const size_t b = count * t / T;
            const size_t e = count * (t + 1) / T;
            if (b >= e)
            {
                continue;
            }
            fs.push_back(sPool.enqueue([fn, b, e]() { fn(b, e); }));
        }
        for (auto &f : fs)
        {
            f.wait();
        }
    }

    // ── 外积 a ⊗ b（列主序：第 j 列 = a·b[j]）──
    // 刚体拟合的惯量张量 Σ(|r|²E − r⊗r) 需要，glm 的 outerProduct 不保证可用，自写更稳。
    static inline Mat3_ OuterProduct3D(const Vec3_ &a, const Vec3_ &b)
    {
        Mat3_ m;
        m[0] = Vec3_(a.x * b.x, a.y * b.x, a.z * b.x);
        m[1] = Vec3_(a.x * b.y, a.y * b.y, a.z * b.y);
        m[2] = Vec3_(a.x * b.z, a.y * b.z, a.z * b.z);
        return m;
    }

    // ═════════════════════════════════════════════════════════
    // 刚体采样点（Floaty 的 blob 粒子角色）
    // 2D 见 PhysicsLiquid.cpp:346-514
    // ═════════════════════════════════════════════════════════

    void PhysicsLiquid3D::BuildSolidSamples()
    {
        mSamples.clear();
        mSampleWeight.clear();
        mSolids.clear();
        mStaticBodies.clear();
        mSamples.reserve(kMaxSolidSamples);
        mSampleWeight.reserve(kMaxSolidSamples);
        mSolids.reserve(16);
        mStaticBodies.reserve(16);

        const size_t n = mParticles.size();
        if (n == 0)
        {
            RefreshSolidXform(); // 无流体也要刷新（清空）变换缓存，否则残留上一帧刚体
            return;
        }

        // 流体包围盒（粗剔除远处刚体）—— 2D 是矩形，3D 是长方体
        FLOAT_ minX = mParticles[0]->pos.x, maxX = minX;
        FLOAT_ minY = mParticles[0]->pos.y, maxY = minY;
        FLOAT_ minZ = mParticles[0]->pos.z, maxZ = minZ;
        for (size_t i = 1; i < n; ++i)
        {
            const Vec3_ &p = mParticles[i]->pos;
            minX = std::min(minX, p.x);
            maxX = std::max(maxX, p.x);
            minY = std::min(minY, p.y);
            maxY = std::max(maxY, p.y);
            minZ = std::min(minZ, p.z);
            maxZ = std::max(maxZ, p.z);
        }

        // Floaty initSpacing = 0.8 * particleDiameter = 1.6r：采样点同间距布点
        const FLOAT_ spacing = std::max(param.particleRadius * FLOAT_(1.6), FLOAT_(0.05));
        const FLOAT_ margin = param.kernelRadius * FLOAT_(3);

        auto NearFluid = [&](PhysicsAngle3D *body) -> bool
        {
            const FLOAT_ r = body->PFGetCollisionR() + margin;
            return !(body->pos.x + r < minX || body->pos.x - r > maxX ||
                     body->pos.y + r < minY || body->pos.y - r > maxY ||
                     body->pos.z + r < minZ || body->pos.z - r > maxZ);
        };

        auto PushSample = [&](PhysicsAngle3D *body, const Vec3_ &local) -> bool
        {
            if ((int)mSamples.size() >= kMaxSolidSamples)
            {
                return false;
            }
            mSamples.push_back(SolidSample{body, local});
            return true;
        };

        // shape / circle / line 都派生自 PhysicsAngle3D
        std::vector<PhysicsAngle3D *> candidates;
        candidates.reserve(mWorld->PhysicsShapeS.size() + mWorld->PhysicsCircleS.size() +
                           mWorld->PhysicsLineS.size());
        auto Collect = [&candidates](const auto &bodies)
        {
            for (auto *b : bodies)
            {
                candidates.push_back((PhysicsAngle3D *)b);
            }
        };
        Collect(mWorld->PhysicsShapeS);
        Collect(mWorld->PhysicsCircleS);
        Collect(mWorld->PhysicsLineS);

        for (PhysicsAngle3D *body : candidates)
        {
            // 质量/惯量双零 = 既不可移动也不参与求解 → 完全隐形体（水直接穿过），显式排除
            if (body == nullptr || (body->PFGetMass() <= FLOAT_(0) && body->invMass <= FLOAT_(0)))
            {
                continue;
            }
            if (!NearFluid(body))
            {
                continue;
            }

            // ── R2-A：静态刚体（invMass==0 && mass>0）退出密度采样 ──
            // 静态体只登记为纯几何钳制体（不布点、不进 mPos/邻居系统），挡水职责统一交给
            // SolveMapBoundaries 的几何挤出（与地形同一条路径）。
            if (body->invMass <= FLOAT_(0))
            {
                mStaticBodies.push_back(SolidTrack(body, 0, 0, 0));
                continue;
            }
            if (body->PFGetMass() <= FLOAT_(0))
            {
                continue; // 防御：invMass>0 却 mass<=0（退化构造）会让下面的 w 除零
            }

            const int begin = (int)mSamples.size();
            switch (body->PFGetType())
            {
            case PhysicsObjectEnum::shape:
            {
                // 体素形状：每个实体格按间距沿**三轴**细分采样（2D 是 x/y 两轴）
                PhysicsShape3D *s = (PhysicsShape3D *)body;
                const int per = std::max(1, (int)floor(FLOAT_(1) / spacing + FLOAT_(0.5)));
                const FLOAT_ step = FLOAT_(1) / (FLOAT_)per;
                bool overflow = false;
                for (unsigned int x = 0; x < s->width && !overflow; ++x)
                {
                    for (unsigned int y = 0; y < s->height && !overflow; ++y)
                    {
                        for (unsigned int z = 0; z < s->depth && !overflow; ++z)
                        {
                            if (!s->GridAt((int)x, (int)y, (int)z).Entity)
                            {
                                continue;
                            }
                            for (int i = 0; i < per && !overflow; ++i)
                            {
                                for (int j = 0; j < per && !overflow; ++j)
                                {
                                    for (int k = 0; k < per && !overflow; ++k)
                                    {
                                        const Vec3_ local =
                                            Vec3_((FLOAT_)x + ((FLOAT_)i + FLOAT_(0.5)) * step,
                                                  (FLOAT_)y + ((FLOAT_)j + FLOAT_(0.5)) * step,
                                                  (FLOAT_)z + ((FLOAT_)k + FLOAT_(0.5)) * step) -
                                            s->CentreMass;
                                        overflow = !PushSample(body, local);
                                    }
                                }
                            }
                        }
                    }
                }
                break;
            }
            case PhysicsObjectEnum::circle:
            {
                // 球体：2D 的"同心环"（半径 ring 的圆环上按 周长/spacing 布点）
                //        → 3D 的**同心球壳**（半径 ring 的球面上按 4π·ring²/spacing² 布点）。
                // 逐壳用 Fibonacci 球面（z 等分 + 黄金角）避免经纬格网的极点聚集；
                // 壳间距 = 流体间距 spacing ⇒ 采样总数 ≈ V/spacing³，与流体的体积数密度
                // 一致，质量加权 PBD（w = 采样数/m）的密度一致性由此保证。
                PhysicsCircle3D *c = (PhysicsCircle3D *)body;
                const FLOAT_ rad = c->radius;
                PushSample(body, Vec3_(0, 0, 0)); // 球心（2D 同样先压入环心）
                bool overflow = false;
                for (FLOAT_ ring = spacing; ring <= rad && !overflow; ring += spacing)
                {
                    // 壳层面积 4π·ring² ÷ 单点占位 spacing² = 该层点数
                    const int cnt = std::max(4, (int)floor(FLOAT_(4.0) * kPi * ring * ring /
                                                       (spacing * spacing) + FLOAT_(0.5)));
                    for (int k = 0; k < cnt && !overflow; ++k)
                    {
                        const FLOAT_ z = FLOAT_(1.0) - (FLOAT_(2.0) * (FLOAT_)k + FLOAT_(1.0)) /
                                                       (FLOAT_)cnt;
                        const FLOAT_ ringXY = SQRT_(std::max(FLOAT_(0.0), FLOAT_(1.0) - z * z));
                        const FLOAT_ phi = FLOAT_(2.39996322972865332) * (FLOAT_)k; // 黄金角 (3-sqrt5)pi
                        overflow = !PushSample(body,
                                               Vec3_(cosf(phi) * ringXY * ring,
                                                     sinf(phi) * ringXY * ring, z * ring));
                    }
                }
                break;
            }
            case PhysicsObjectEnum::line:
            {
                // 胶囊：取胶囊轴段（PhysicsLine3D::GetSegment，已含 rotation 与半长），
                // 在**世界系**均布后换算回局部系：世界系端点对任意旋转都取得到真实轴，
                // 不必依赖 rotation 相对局部轴的正负约定。
                PhysicsLine3D *l = (PhysicsLine3D *)body;
                Vec3_ segB(0), segE(0);
                l->GetSegment(segB, segE);
                const Vec3_ axis = segE - segB;
                const FLOAT_ len = SQRT_(ModulusLength(axis)); // ModulusLength 是平方长度
                if (len <= FLOAT_(1e-6))
                {
                    // 退化线段（零长 / 未 UpdateOutline）：回退到 2D 的"沿局部 X 轴均布"
                    const FLOAT_ half = (l->HalfLength > FLOAT_(0)) ? l->HalfLength : l->radius;
                    const int cnt = std::max(2, (int)floor(FLOAT_(2.0) * half / spacing +
                                                        FLOAT_(0.5)));
                    for (int k = 0; k <= cnt; ++k)
                    {
                        const FLOAT_ t = -half + FLOAT_(2.0) * half * (FLOAT_)k / (FLOAT_)cnt;
                        if (!PushSample(body, Vec3_(t, 0, 0)))
                        {
                            break;
                        }
                    }
                    break;
                }
                const int cnt = std::max(2, (int)floor(len / spacing + FLOAT_(0.5)));
                for (int k = 0; k <= cnt; ++k)
                {
                    const Vec3_ world = segB + axis * ((FLOAT_)k / (FLOAT_)cnt);
                    const Vec3_ local = QuatInverseRotate(body->rotation, world - body->pos);
                    if (!PushSample(body, local))
                    {
                        break;
                    }
                }
                break;
            }
            default:
                break;
            }

            const int end = (int)mSamples.size();
            if (end == begin)
            {
                continue;
            }
            // Floaty 质量加权 PBD：w = 1/m_采样 = 采样数/刚体总质量
            const FLOAT_ w = (FLOAT_)(end - begin) / body->PFGetMass();
            mSampleWeight.insert(mSampleWeight.end(), (size_t)(end - begin), w);
            // xfIndex = 本条目在 mSolids 中的下标；RefreshSolidXform 先排动态体，
            // 故它同时也是该体在 mSolidXf 中的下标（ProjectSolids 就地刷新用）
            mSolids.push_back(SolidTrack(body, begin, end, (int)mSolids.size()));
        }

        RefreshSolidXform(); // 刚体查询缓存：动态体 + 静态体（纯几何钳制）
    }

    // ═════════════════════════════════════════════════════════
    // 组合位置快照（2D 见 PhysicsLiquid.cpp:534-560）
    // ═════════════════════════════════════════════════════════

    void PhysicsLiquid3D::SyncPositions()
    {
        const size_t n = mFluidCount;
        const size_t N = n + mSamples.size();
        mPos.resize(N);
        mVel.resize(N);
        mPrevPos.resize(N);
        mGrad.resize(N);
        mDelta.resize(N);
        mBoundaryDisp.resize(n);
        mLambda.resize(N);
        mDensity.resize(n);
        for (size_t i = 0; i < n; ++i)
        {
            PhysicsParticle3D *p = mParticles[i];
            p->OldPos = p->pos; // 本步积分前位置（引擎 PhysicsPos 的 OldPos 记账语义）
            mPos[i] = p->pos;
            mVel[i] = p->speed;
            mPrevPos[i] = p->pos;
        }
        SyncSamplePositions();
    }

    // ═════════════════════════════════════════════════════════
    // 邻居搜索（Floaty src/lib.rs NeighborLinkedList.find_neighbors）
    // 2D 见 PhysicsLiquid.cpp:561-647
    // ═════════════════════════════════════════════════════════

    int PhysicsLiquid3D::HashCell(int gx, int gy, int gz)
    {
        // 2D: ((gx * 92837111) ^ (gy * 689287499)) % HASH_SIZE
        // 3D: 沿用 2D 的两个素数，再按同一风格追加第三个奇数素数乘 gz，仍对
        //     kHashSize 取模。无符号回绕保证负坐标一致（Floaty 世界坐标恒正，
        //     本引擎大量使用负坐标）。
        const unsigned int h = (unsigned int)gx * 92837111u ^
                               (unsigned int)gy * 689287499u ^
                               (unsigned int)gz * 83492791u;
        return (int)(h % (unsigned int)kHashSize);
    }

    void PhysicsLiquid3D::FindNeighbors(FLOAT_ gridSpacing)
    {
        const size_t N = mPos.size();
        mFirst.resize(N + 1); // 全部元素在下方两处写入
        if (N == 0)
        {
            mFirst[0] = 0;
            return;
        }
        mNext.resize(N);
        if (mHashFirst.size() != (size_t)kHashSize)
        {
            mHashFirst.assign(kHashSize, -1);
            mHashMark.assign(kHashSize, 0);
        }
        if (mNeighbors.size() < N * (size_t)kMaxNeighbors)
        {
            mNeighbors.resize(N * (size_t)kMaxNeighbors);
            mPairDir.resize(N * (size_t)kMaxNeighbors);
            mPairW.resize(N * (size_t)kMaxNeighbors);
        }

        const FLOAT_ inv = FLOAT_(1) / gridSpacing;

        // 第一遍：把全部粒子挂进哈希链表（epoch 标记免清空）
        ++mEpoch;
        for (size_t i = 0; i < N; ++i)
        {
            const int gx = (int)floor(mPos[i].x * inv);
            const int gy = (int)floor(mPos[i].y * inv);
            const int gz = (int)floor(mPos[i].z * inv);
            const int h = HashCell(gx, gy, gz);
            if (mHashMark[h] != mEpoch)
            {
                mHashMark[h] = mEpoch;
                mHashFirst[h] = -1;
            }
            mNext[i] = mHashFirst[h];
            mHashFirst[h] = (int)i;
        }

        // 第二遍：3×3×3 = 27 邻域收集 ≤ kMaxNeighbors 个邻居（距离 < gridSpacing）
        // 注：全量保存两个半边（j<i 与 j>i 都入表）——λ 的密度和 Σ_j W 需要完整邻域，
        //     若只留 j>i 半边就必须改成对称累加，与 2D 的逐粒子求和不再等价。
        const FLOAT_ h2 = gridSpacing * gridSpacing;
        size_t accum = 0;
        for (size_t i = 0; i < N; ++i)
        {
            mFirst[i] = (int)accum;
            const int gx = (int)floor(mPos[i].x * inv);
            const int gy = (int)floor(mPos[i].y * inv);
            const int gz = (int)floor(mPos[i].z * inv);
            int count = 0;
            for (int ix = gx - 1; ix <= gx + 1; ++ix)
            {
                for (int iy = gy - 1; iy <= gy + 1; ++iy)
                {
                    for (int iz = gz - 1; iz <= gz + 1; ++iz)
                    {
                        const int h = HashCell(ix, iy, iz);
                        if (mHashMark[h] != mEpoch)
                        {
                            continue;
                        }
                        for (int id = mHashFirst[h]; id >= 0; id = mNext[id])
                        {
                            const FLOAT_ r2 = ModulusLength(mPos[id] - mPos[i]);
                            if (r2 < h2 && (size_t)id != i && count < kMaxNeighbors)
                            {
                                mNeighbors[accum + count] = id;
                                ++count;
                            }
                        }
                    }
                }
            }
            accum += count;
        }
        mFirst[N] = (int)accum;
    }

    // ═════════════════════════════════════════════════════════
    // inside 标记（Floaty solver.ts PBD.makeIsInsideForFluid）
    // 2D 见 PhysicsLiquid.cpp:648-670
    // ═════════════════════════════════════════════════════════

    void PhysicsLiquid3D::MarkInside()
    {
        const size_t n = mFluidCount;
        mInside.assign(n, 0);
        // mSolidXf = 动态体 [0, mSolids.size()) 在前 + 静态体在后，
        // 故按下标上界遍历即可只扫动态体（静态体的 inside 语义无意义）
        const size_t dyn = std::min(mSolids.size(), mSolidXf.size());
        for (size_t bi = 0; bi < dyn; ++bi)
        {
            const SolidXf &xf = mSolidXf[bi];
            if (!xf.dynamic)
            {
                continue; // 静态刚体由边界钳制处理（Floaty 的 inside 只针对移动软体）
            }
            for (size_t i = 0; i < n; ++i)
            {
                if (!mInside[i] && SolidContains(xf, mPos[i]))
                {
                    mInside[i] = 1;
                }
            }
        }
    }

    // ═════════════════════════════════════════════════════════
    // 近边界候选标记（R3：帧级预筛，子步内边界探测只对候选执行）
    // 2D 见 PhysicsLiquid.cpp:701-780
    // ═════════════════════════════════════════════════════════

    /**
     * @brief 标记"本帧可能触边"的流体粒子（R3）
     * @details 正确性依据 —— **帧内位移上界**（全部由解得参数给出，非经验常数）：
     *      · 每子步净位移 ≤ 0.9·cfl·h（EnforceCFL 的 maxDisp），共 substeps 个子步
     *        → 全帧漂移 ≤ 0.9·cfl·h·substeps；
     *      · 每子步另有边界推出位移与 Δp 修正，二者同受 cfl·h 级钳制，故余量
     *        再叠加一个 cfl·h 即覆盖单步内的位置级修正。
     *      候选取并集：(a) 帧初已在刚体内（复用 MarkInside 结果）；
     *      (b) 与任一近流体刚体包围**球**外扩 margin 相交；(c) margin 邻域内存在实体地图格。
     *      本函数只影响"是否执行探测"，不改变探测结果：候选集是真实触边条件的保守超集，
     *      故 SolveMapBoundaries 的物理行为与未预筛时逐位一致。 */
    void PhysicsLiquid3D::MarkNearSolid()
    {
        const size_t n = mFluidCount;
        mNearSolid.assign(n, 0);
        if (n == 0)
        {
            return;
        }

        // 余量 = 全帧漂移（0.9·cfl·h·substeps）+ 每子步位置级修正上限（cfl·h）
        const FLOAT_ cflDisp = mConst.cflDisp;
        const FLOAT_ margin = mConst.cflDispVel * (FLOAT_)param.substeps + cflDisp;

        MapFormwork3D *map = mWorld->GetMapFormwork();
        const Vec3_ centrality = (map != nullptr) ? map->FMGetCentrality() : Vec3_(0);

        // ── (a) 帧初在动态刚体内 + (b) 刚体包围球外扩 margin ──
        for (size_t i = 0; i < n; ++i)
        {
            if (mInside[i])
            {
                mNearSolid[i] = 1; // (a) 必须走逃逸
                continue;
            }
            const Vec3_ p = mPos[i];
            for (const SolidXf &xf : mSolidXf)
            {
                const FLOAT_ r = xf.radius + margin; // 包围球外扩：保守覆盖形状体角部
                if (ModulusLength(p - xf.pos) <= r * r)
                {
                    mNearSolid[i] = 1;
                    break;
                }
            }
        }

        // ── (c) 地形侧：margin 邻域内的地图格（立方邻域，2D 是正方形） ──
        // 扫描半径 = floor(margin)+1（而非写死 3×3×3）。推导：实体格 (i,j,k) 到 p 的
        // 最近点距离 ≥ (|i|−1, |j|−1, |k|−1) 的欧氏长度 ≥ (⌊margin⌋+1)·√3 > margin，
        // 故 (|i|,|j|,|k|) 全 ≥ 2 的格可跳过。默认参数下 margin ≈ 0.35 → 半径 1。
        // 注意：不能用"格内小数部分"把邻域收窄——粒子恰在格边界时小数部分为 0，
        // 会把真正相邻的实体格误判为"更远格"而漏掉（2D 实测漏判已复现，见 2D:757-759）。
        if (map != nullptr)
        {
            const int scan = (int)floor(margin) + 1;
            for (size_t i = 0; i < n; ++i)
            {
                if (mNearSolid[i])
                {
                    continue;
                }
                const Vec3_ local = mPos[i] + centrality; // 世界 → 地图局部（与 FMGetCollide 同式）
                const IVec3_ g = ToInt(local);
                for (int ox = -scan; ox <= scan && !mNearSolid[i]; ++ox)
                {
                    for (int oy = -scan; oy <= scan && !mNearSolid[i]; ++oy)
                    {
                        for (int oz = -scan; oz <= scan; ++oz)
                        {
                            if (map->FMGetCollide(IVec3_(g.x + ox, g.y + oy, g.z + oz)))
                            {
                                mNearSolid[i] = 1;
                                break;
                            }
                        }
                    }
                }
            }
        }
    }

    // ═════════════════════════════════════════════════════════
    // 子步阶段
    // ═════════════════════════════════════════════════════════

    // 半隐式欧拉（Floaty solver.ts simulate 积分段：v += g·dt; prevPos = pos; pos += v·dt）
    // 2D 见 PhysicsLiquid.cpp:781-792。Floaty 只有重力项；此处把 AddForce 外力并入同一积分式。
    void PhysicsLiquid3D::IntegrateFluid(FLOAT_ dt)
    {
        const size_t n = std::min(mFluidCount, mParticles.size());
        const Vec3_ g = mWorld->GravityAcceleration;
        for (size_t i = 0; i < n; ++i)
        {
            PhysicsParticle3D *p = mParticles[i];
            mVel[i] += dt * (g + p->invMass * p->force);
            mPrevPos[i] = mPos[i];
            mPos[i] += mVel[i] * dt;
        }
    }

    // λ + Δp 求解（Floaty src/lib.rs solve_fluid_jacobi 逐式移植）
    // 2D 见 PhysicsLiquid.cpp:821-945
    //
    // ⚠ 2D → 3D 唯一一处**非机械**改写：spiky 梯度的 ×3 因子必须去掉。
    //   2D 的 `spikyScale = 10/(πh⁵)` 是 **核**幅度，∇W = −3·spikyScale·w²·r̂，
    //   故 use site 上显式写 `* -3`（2D:881/930）。
    //   3D 的 `SpikyScale(h) = 45/(πh⁶)` 已经是 **梯度**幅度
    //   （见本文件 SpikyScale 的注释与 PhysicsLiquid3D.hpp 中 SpikyScale 的说明：∇W = −spikyScale·w²·r̂），
    //   所以 3D 的因子是 −1。照抄 −3 会把压力梯度放大三倍（|∇C|² 放大九倍）。
    void PhysicsLiquid3D::SolveFluidJacobi()
    {
        const size_t n = mFluidCount;
        const size_t N = mPos.size();
        const FLOAT_ h = mConst.h;
        const FLOAT_ h2 = mConst.h2;
        const FLOAT_ poly6Scale = mConst.poly6Scale;
        const FLOAT_ spikyScale = mConst.spikyScale;
        // 曲率项用的是 spiky **核**幅度 A_k（Akinci 表面张力：Σ(n_i-n_j)·W），
        // 而 3D 的 spikyScale 是**梯度**幅度 A_g = 3·A_k（∇W = -A_g·w²·r̂ = -3A_k·w²·r̂）。
        // 2D 的 use site 直接写 spikyScale(=A_k)·w³；3D 必须除以 3 才是同一物理量，
        // 否则表面张力/曲率被放大 3 倍（k⁵ 幂次不受影响，只是量值）。
        const FLOAT_ spikyKernel = spikyScale * FLOAT_(1.0 / 3.0); // A_k = A_g / 3
        const FLOAT_ restInv = mConst.restInv;
        const FLOAT_ selfRho = mConst.selfRho; // 自贡献（与 SyncFromParticleRadius 同约定）

        // —— λ 求解（Jacobi：λ_i = −C_i / (Σ|∇C|² + eps)，C<0 时 λ=0）——
        // 每 i 独立写 mLambda/mGrad/mDensity/mPair* → 数据并行，结果与串行逐位一致
        ParallelRange3D(N, [&](size_t rangeBegin, size_t rangeEnd)
        {
            for (size_t i = rangeBegin; i < rangeEnd; ++i)
            {
                const bool insideI = (i < n) && (mInside[i] != 0);
                FLOAT_ rho = selfRho;
                Vec3_ gradSum{0};
                FLOAT_ sumGrad2 = 0;

                if (!insideI)
                {
                    const int first = mFirst[i];
                    const int last = mFirst[i + 1];
                    for (int k = first; k < last; ++k)
                    {
                        // 默认写 mPairW[k] < 0 哨兵（无效对），Δp 遍直接跳过——两遍跳过集完全一致。
                        // Floaty：刚体↔刚体相互作用忽略；在刚体内部的流体忽略。
                        mPairW[k] = -FLOAT_(1);
                        const int j = mNeighbors[k];
                        if (j < 0 || (i >= n && (size_t)j >= n) ||
                            ((size_t)j < n && mInside[j]))
                        {
                            continue;
                        }

                        const Vec3_ d = mPos[i] - mPos[j];
                        const FLOAT_ r2 = ModulusLength(d);
                        if (r2 <= FLOAT_(0) || r2 >= h2)
                        {
                            continue;
                        }

                        rho += poly6Scale * Poly6Weight(h2, r2);
                        const FLOAT_ r = SQRT_(r2);
                        const FLOAT_ w = h - r;
                        // 单位方向用"一次倒数 + 乘法"（同 2D：省掉每对两次除法）
                        const Vec3_ u = d * (FLOAT_(1) / r);
                        // 配对缓存：单位方向 + 核距（Δp 遍复用，省掉第二次 √r/核求值）
                        mPairDir[k] = u;
                        mPairW[k] = w;
                        // Floaty: cur_grad = d/r * (spiky_scale * w * w * -3) / restDensity
                        // 3D：×(-1)（见本函数上方注释：SpikyScale 已含导数因子 3）
                        const FLOAT_ gscale = (-spikyScale * w * w) * restInv;
                        const Vec3_ curGrad = u * gscale;
                        sumGrad2 += ModulusLength(curGrad);
                        gradSum += curGrad;
                    }
                }

                // Floaty: sum_grad_2 += (grad_ix² + grad_iy²) * inv_m
                const FLOAT_ invM = (i >= n) ? SampleWeight(i) : FLOAT_(1);
                sumGrad2 += ModulusLength(gradSum) * invM;
                const FLOAT_ c = rho * restInv - FLOAT_(1);
                FLOAT_ lambda = -c / (sumGrad2 + param.eps);
                if (c < FLOAT_(0))
                {
                    lambda = FLOAT_(0); // 只压不拉（Floaty 原样）
                }
                mLambda[i] = lambda;
                mGrad[i] = -gradSum; // Floaty: grad_x = -grad_ix
                if (i < n)
                {
                    mDensity[i] = rho;
                }
            }
        });

        // —— Δp 求解（Jacobi：Δp_i = inv_m · Σ[(λ_i+λ_j)∇W + 内聚 + 曲率]）——
        // 每 i 独立写 mDelta（λ/grad 缓存本遍只读）→ 数据并行。
        // 压力项对"流体粒子"与"刚体采样点"（Floaty blob 分支）同式；只有表面张力
        // （内聚 + 曲率）仅作用于流体↔流体对，故合并为一个循环 + 一处附加分支。
        const bool surfaceTension = param.surfaceTension;
        ParallelRange3D(N, [&](size_t rangeBegin, size_t rangeEnd)
        {
            for (size_t i = rangeBegin; i < rangeEnd; ++i)
            {
                const bool isSample = (i >= n);
                const bool insideI = !isSample && (mInside[i] != 0);
                const FLOAT_ invM = isSample ? SampleWeight(i) : FLOAT_(1);
                Vec3_ delta{0};

                if (!insideI) // 采样点恒参与；在刚体内部的流体粒子跳过（Floaty inside 规则）
                {
                    const int first = mFirst[i];
                    const int last = mFirst[i + 1];
                    for (int k = first; k < last; ++k)
                    {
                        const FLOAT_ w = mPairW[k];
                        if (w < FLOAT_(0))
                        {
                            continue; // λ 遍标记的无效对
                        }
                        const int j = mNeighbors[k];
                        delta += mPairDir[k] *
                                 ((-spikyScale * w * w) * restInv * (mLambda[i] + mLambda[j]));

                        if (!isSample && (size_t)j < n && surfaceTension)
                        {
                            // 表面张力（Akinci 内聚 + 曲率，Floaty lib.rs 流体↔流体分支）：
                            //   cohesion  = d/r * (-0.01*0.01)
                            //   curvature = (grads_i - grads_j) * spiky_scale*w³ * 0.001*0.02
                            delta += mPairDir[k] * (-param.cohesion);
                            delta += (mGrad[i] - mGrad[j]) *
                                     (spikyKernel * w * w * w * param.curvature);
                        }
                    }
                }
                mDelta[i] = delta * invM;
            }
        });
    }

    // 边界钳制（Floaty solver.ts PBD.solveFluidBoundaries 的容器类比）
    // 2D 见 PhysicsLiquid.cpp:947-1069（8 方向 → 3D 26 方向）
    //
    // 容器 = 地图格 + 静态刚体 + **动态刚体内部**，粒子一律沿 26 个方向小步进找表面
    // 并二分精确贴面推出（推出量 = 实际穿透量；推出位移记入 mBoundaryDisp、
    // 不注入速度——穿入速度反而会被推出位移精确抵消，见 EnforceCFL 的 bVel 规则）。
    void PhysicsLiquid3D::SolveMapBoundaries()
    {
        const size_t n = mFluidCount;

        // 本子步的边界修正：全段清零。2D 只在"候选且已命中"的分支里清零
        // （2D:1030），非候选粒子会残留上一帧的值；EnforceCFL 把 mBoundaryDisp
        // 当作**本子步**修正来反推 a = (pos − b) − prevPos，残留值会污染 v（位置因
        // mPos = prevPos + a + b 的代数恒等而幸免）。此处按语义显式清零。
        for (size_t i = 0; i < n; ++i)
        {
            mBoundaryDisp[i] = Vec3_(0);
        }

        MapFormwork3D *map = mWorld->GetMapFormwork();
        const FLOAT_ jitter = mConst.boundaryJitter;
        const FLOAT_ step = mConst.boundaryStep;
        const int maxSteps = 64; // 最远探测 ≈ 3.2 世界单位

        auto PointSolid = [&](const Vec3_ &p) -> bool
        {
            if (map != nullptr && map->FMGetCollide(p))
            {
                return true;
            }
            // 静态（纯几何钳制体，R2-A）+ 动态刚体统一走变换缓存
            //（AABB 快筛；动态刚体内部挤出 = 修复幽灵穿透）
            for (const SolidXf &xf : mSolidXf)
            {
                if (SolidContains(xf, p))
                {
                    return true;
                }
            }
            return false;
        };

        // 26 个方向 = {-1,0,1}³ \ (0,0,0)，全部**归一化**为等长（轴长 1、
        // 面对角 1/√2、体对角 1/√3）——与 2D 把 4 条对角写成 ±0.7071 同理：
        // 只有方向等长，`step·k` 才是真实欧氏距离，26 向之间才可比远近。
        static const Vec3_ kDirs[26] = {
            {1, 0, 0},  {-1, 0, 0}, {0, 1, 0},  {0, -1, 0}, {0, 0, 1},  {0, 0, -1},
            {FLOAT_(0.70710678), FLOAT_(0.70710678), 0},
            {FLOAT_(-0.70710678), FLOAT_(0.70710678), 0},
            {FLOAT_(0.70710678), FLOAT_(-0.70710678), 0},
            {FLOAT_(-0.70710678), FLOAT_(-0.70710678), 0},
            {FLOAT_(0.70710678), 0, FLOAT_(0.70710678)},
            {FLOAT_(-0.70710678), 0, FLOAT_(0.70710678)},
            {FLOAT_(0.70710678), 0, FLOAT_(-0.70710678)},
            {FLOAT_(-0.70710678), 0, FLOAT_(-0.70710678)},
            {0, FLOAT_(0.70710678), FLOAT_(0.70710678)},
            {0, FLOAT_(-0.70710678), FLOAT_(0.70710678)},
            {0, FLOAT_(0.70710678), FLOAT_(-0.70710678)},
            {0, FLOAT_(-0.70710678), FLOAT_(-0.70710678)},
            {FLOAT_(0.57735027), FLOAT_(0.57735027), FLOAT_(0.57735027)},
            {FLOAT_(0.57735027), FLOAT_(0.57735027), FLOAT_(-0.57735027)},
            {FLOAT_(0.57735027), FLOAT_(-0.57735027), FLOAT_(0.57735027)},
            {FLOAT_(0.57735027), FLOAT_(-0.57735027), FLOAT_(-0.57735027)},
            {FLOAT_(-0.57735027), FLOAT_(0.57735027), FLOAT_(0.57735027)},
            {FLOAT_(-0.57735027), FLOAT_(0.57735027), FLOAT_(-0.57735027)},
            {FLOAT_(-0.57735027), FLOAT_(-0.57735027), FLOAT_(0.57735027)},
            {FLOAT_(-0.57735027), FLOAT_(-0.57735027), FLOAT_(-0.57735027)}};

        // 26 方向搜索最近表面（limitSteps 内找到即回）。返回是否找到。
        // ⚠ limitSteps 必须小：CFL 限制每子步最多移动 maxDisp，近表面穿透（绝大多数
        // 粒子的常态——池底/池壁接触层每子步被重力压入 1e-4）用不到 3 步以外的搜索；
        // 一律扫满 64 步会让向墙内方向的空探测占掉整个边界阶段 90% 的耗时。
        auto FindEscape = [&](const Vec3_ &p, int limitSteps, Vec3_ &outDir,
                              FLOAT_ &outLo, FLOAT_ &outHi) -> bool
        {
            FLOAT_ bestDist = FLOAT_(1e30);
            bool found = false;
            for (int d = 0; d < 26; ++d)
            {
                for (int k = 1; k <= limitSteps; ++k)
                {
                    const Vec3_ cand = p + kDirs[d] * (step * (FLOAT_)k);
                    if (!PointSolid(cand))
                    {
                        const FLOAT_ dist = step * (FLOAT_)k;
                        if (dist < bestDist)
                        {
                            bestDist = dist;
                            outDir = kDirs[d];
                            outLo = step * (FLOAT_)(k - 1); // 固体侧端点距离
                            outHi = step * (FLOAT_)k;       // 空位侧端点距离
                            found = true;
                        }
                        break; // 该方向的表面
                    }
                }
            }
            return found;
        };

        const FLOAT_ maxDisp = mConst.cflDisp;
        const int fastSteps = mConst.fastSteps; // ≈3：覆盖每子步实际可移动距离

        for (size_t i = 0; i < n; ++i)
        {
            // R3 预筛：帧初离一切固体表面 > margin 的粒子整个 Update 内不可能触边，
            // substeps 个子步全部免探测（候选取自 MarkNearSolid，是真实触边条件的保守超集）
            if (!mNearSolid[i])
            {
                continue;
            }
            Vec3_ &p = mPos[i];
            if (!PointSolid(p))
            {
                continue;
            }
            // 两段式搜索：近程快路径（常态接触）→ 深陷实体才走全距慢路径
            Vec3_ bestDir{0};
            FLOAT_ bestLo = 0, bestHi = 0;
            if (!FindEscape(p, fastSteps, bestDir, bestLo, bestHi) &&
                !FindEscape(p, maxSteps, bestDir, bestLo, bestHi))
            {
                continue;
            }

            // 二分定位表面（[bestLo 固体, bestHi 空位]），推出量 ≈ 实际穿透量
            //（5 次迭代 = 步长 1/32 精度，足够；更多次白算）
            for (int iter = 0; iter < 5; ++iter)
            {
                const FLOAT_ mid = (bestLo + bestHi) * FLOAT_(0.5);
                if (PointSolid(p + bestDir * mid))
                {
                    bestLo = mid;
                }
                else
                {
                    bestHi = mid;
                }
            }
            Vec3_ target = p + bestDir * bestHi;
            // 大幅推出（深陷实体逃逸，bestHi = 表面距离）才抖动松解；接触级微修正不抖动
            if (bestHi > mConst.h)
            {
                target += Vec3_(Random(-jitter, jitter), Random(-jitter, jitter),
                                Random(-jitter, jitter));
            }
            // 推出量受 CFL 位移上限：深陷实体（如初始就压在地形里的水）逐子步
            // 渐进逃逸。表面可以一次找到，但位移必须分帧摊销——整段应用会让
            // 逃逸粒子瞬间砸进邻近水体（局部密度 +150% 的冲击波）。
            Vec3_ move = target - p;
            ClampDisplacement3D(move, maxDisp);
            mBoundaryDisp[i] = move;
            p += move;
        }
    }

    // Δp 应用 + 刚体拟合投影（Floaty 的软体约束投影槽位 → 刚体最小二乘拟合）
    // 2D 见 PhysicsLiquid.cpp:1071-1081
    void PhysicsLiquid3D::ApplyDeltas()
    {
        // 流体与刚体采样点同为 p += Δp（mDelta 已含各自的 inv_m 权重）
        // 注意：mBoundaryDisp **不在这里**加——它是位置级约束，已在
        // SolveMapBoundaries 内直接作用到 mPos（2D 同序）。
        const size_t N = mPos.size();
        for (size_t i = 0; i < N; ++i)
        {
            mPos[i] += mDelta[i];
        }
        ProjectSolids();
    }

    // Δp 最小二乘刚体拟合 → 位姿（2D 见 PhysicsLiquid.cpp:1083-1136）
    // 2D：t = mean(Δp)；dθ = Σ(r×d)/Σ|r|²（标量）
    // 3D：t = mean(Δp)；L = Σ(r×d)（Vec3_）；I = Σ(|r|²E − r⊗r)（Mat3_）；
    //     dθ = SymmetricInverse(I)·L（轴-角向量，模 = 转角）
    void PhysicsLiquid3D::ProjectSolids()
    {
        const size_t n = mFluidCount;
        const FLOAT_ maxDisp = mConst.cflDisp;
        for (size_t bi = 0; bi < mSolids.size(); ++bi)
        {
            SolidTrack &st = mSolids[bi];
            PhysicsAngle3D *body = st.body;
            const int cnt = st.end - st.begin;
            if (body == nullptr || cnt <= 0 || body->invMass <= FLOAT_(0))
            {
                continue; // 静态刚体不动（w = 0，采样点也不需重锚定）
            }

            const Quat_ rot = body->rotation;
            Vec3_ t{0};
            Vec3_ L{0};
            Mat3_ inertia(0.0f); // 零矩阵（glm 的标量构造是**对角**构造，0 = 全零）
            for (int s = st.begin; s < st.end; ++s)
            {
                const Vec3_ d = mDelta[n + s];
                t += d;
                const Vec3_ r = rot * mSamples[s].local; // 采样点当前力臂（世界系）
                L += Cross(r, d);
                inertia += (ModulusLength(r) * Mat3_(1.0f) - OuterProduct3D(r, r));
            }
            t /= (FLOAT_)cnt;
            Vec3_ dTheta = SymmetricInverse(inertia) * L;

            // 位移钳制（Floaty enforceCFL 的 blob 槽位：每子步 ≤ cfl·h）
            ClampDisplacement3D(t, maxDisp);
            ClampDisplacement3D(dTheta, maxDisp);

            body->pos += t;
            // dθ 是世界系轴-角向量 → 用**世界系前乘**增量四元数，与引擎
            // PhysicsAngle3D::PhysicsPos（rotation = dq·rotation，PhysicsAngle3D.cpp:155-156）同约定
            const Quat_ dq = QuatFromAngularVelocity(dTheta, FLOAT_(1));
            body->rotation = glm::normalize(dq * body->rotation);
            st.accumT += t;
            st.accumR += dTheta;

            // 本刚体采样点重锚定 + 变换缓存就地刷新
            //（替代整表 SyncSamplePositions / 全量重建；静态体从不移动无需重锚定）
            const Quat_ rotNew = body->rotation;
            for (int s = st.begin; s < st.end; ++s)
            {
                mPos[n + s] = body->pos + (rotNew * mSamples[s].local);
            }
            const size_t xi = (size_t)st.xfIndex; // 动态体在 mSolidXf 的下标前缀内
            if (xi < mSolidXf.size())
            {
                mSolidXf[xi].pos = body->pos;
                mSolidXf[xi].rotation = rotNew;
            }
        }
    }

    // 位移 CFL 钳制 + 速度回代（Floaty solver.ts PBD.enforceCFL：v = Δx/dt）
    // 2D 见 PhysicsLiquid.cpp:1138-1165
    // 边界修正的处理（关键稳定性规则）：
    //   · 修正量 ≤ 来流运动量（正常接触，b ≈ −a 的法向分量）→ 全额计入 Δx：
    //     接触把法向速度自然消掉（否则重力速度在贴地时无限累积，几十帧后整池弹飞）；
    //   · 修正量 > 来流运动量（深陷实体逃逸）→ 最多只计入 |a|：位置照常逃逸，
    //     但不注入逃逸速度（否则推出修正经 v = Δx/dt 变成千倍假速度）。
    void PhysicsLiquid3D::EnforceCFL(FLOAT_ dt)
    {
        const size_t n = std::min(mFluidCount, mParticles.size());
        const FLOAT_ maxDisp = mConst.cflDispVel; // Floaty: 0.9 * maxVel
        const FLOAT_ invDt = FLOAT_(1) / dt;
        for (size_t i = 0; i < n; ++i)
        {
            const Vec3_ b = mBoundaryDisp[i];            // 本子步边界修正
            const Vec3_ a = (mPos[i] - b) - mPrevPos[i]; // 边界修正前的运动量
            const FLOAT_ aLen2 = ModulusLength(a);       // 平方长度：常态（无边界修正）零开方
            const FLOAT_ bLen2 = ModulusLength(b);
            Vec3_ bVel = b;
            if (bLen2 > aLen2 && bLen2 > FLOAT_(0))
            {
                bVel = b * (SQRT_(aLen2) / SQRT_(bLen2)); // 逃逸修正不注入速度
            }
            Vec3_ d = a + bVel;
            ClampDisplacement3D(d, maxDisp);
            mVel[i] = d * invDt;
            mPos[i] = mPrevPos[i] + d + (b - bVel); // 位置保留全部边界修正
        }
    }

    // XSPH 速度平滑黏性（Macklin《Position Based Fluids》论文的可选黏性项）：
    //   v_i += viscosity · Σ_j (v_j − v_i)·W(r_ij) / Σ_j W(r_ij)
    // 2D 见 PhysicsLiquid.cpp:1167-1213。物理黏性：抹平速度高频噪声、让晃动自然衰减
    // ——但没有任何黏连力，因此是"水"的手感；胶感来自表面张力（内聚），两者不同。
    // 注意：每次 Update 只调用**一次**（帧级系数语义）。
    void PhysicsLiquid3D::ApplyXSPH()
    {
        const FLOAT_ c = param.viscosity;
        if (c <= FLOAT_(0))
        {
            return;
        }
        const size_t n = std::min(mFluidCount, mParticles.size());
        const FLOAT_ h2 = mConst.h2;
        for (size_t i = 0; i < n; ++i)
        {
            if (mInside[i])
            {
                continue;
            }
            Vec3_ avg{0};
            FLOAT_ wsum = 0;
            const int first = mFirst[i];
            const int last = mFirst[i + 1];
            for (int k = first; k < last; ++k)
            {
                const int j = mNeighbors[k];
                if (j < 0 || (size_t)j >= n || mInside[j])
                {
                    continue;
                }
                const FLOAT_ r2 = ModulusLength(mPos[i] - mPos[j]);
                if (r2 < h2 && r2 > FLOAT_(0))
                {
                    const FLOAT_ w = Poly6Weight(h2, r2); // poly6 权重（归一化后无量纲）
                    avg += (mVel[j] - mVel[i]) * w;
                    wsum += w;
                }
            }
            if (wsum > FLOAT_(0))
            {
                mVel[i] += avg * (c / wsum);
            }
        }
    }

    // 刚体速度回代：本帧累计的位移修正按帧时间折算为速度增量（钳制）
    // 2D 见 PhysicsLiquid.cpp:1216-1236。3D 角速度是轴-角向量 → 逐分量钳制改为模长钳制。
    void PhysicsLiquid3D::CommitSolids(FLOAT_ time)
    {
        const FLOAT_ dtSub = time / (FLOAT_)param.substeps;
        const FLOAT_ maxSpeed = (dtSub > FLOAT_(0)) ? (mConst.cflDisp / dtSub) : FLOAT_(0);
        for (SolidTrack &st : mSolids)
        {
            PhysicsAngle3D *body = st.body;
            if (body != nullptr && body->invMass > FLOAT_(0))
            {
                Vec3_ dv = st.accumT / time;
                Vec3_ dw = st.accumR / time;
                ClampDisplacement3D(dv, maxSpeed);
                ClampDisplacement3D(dw, maxSpeed);
                body->speed += dv;
                body->angleSpeed += dw;
                body->StaticNum = 0; // 唤醒
            }
            st.accumT = Vec3_(0); // 静态体同样清零，下次本帧累计从 0 起算
            st.accumR = Vec3_(0);
        }
    }

    // 回写粒子（对应引擎 PhysicsPos 的 pos/speed/OldPos/StaticNum 记账）
    // 2D 见 PhysicsLiquid.cpp:1239-1258
    void PhysicsLiquid3D::CommitFluid()
    {
        // 物理世界可能在 Update 期间删过粒子 → 以 mParticles.size() 与 mFluidCount 的较小值为界
        const size_t n = std::min(mFluidCount, mParticles.size());
        for (size_t i = 0; i < n; ++i)
        {
            PhysicsParticle3D *p = mParticles[i];
            p->pos = mPos[i];
            p->speed = mVel[i];
            p->force = Vec3_(0);
            // OldPos 已在 SyncPositions 记为**帧初**位置（2D 同约定），此处只做帧级静止计数
            if (p->OldPos == p->pos)
            {
                if (p->StaticNum < 200)
                {
                    ++p->StaticNum;
                }
            }
            else
            {
                p->StaticNum = 0;
            }
        }
        // mDensity 已在 SolveFluidJacobi 的第一遍按 i < n 写入（与 2D 同），Density() 直接可用
    }

    // ═════════════════════════════════════════════════════════
    // 主循环（Floaty solver.ts PBD.simulate）
    // 2D 见 PhysicsLiquid.cpp:1265-1302
    // ═════════════════════════════════════════════════════════

    void PhysicsLiquid3D::Update(FLOAT_ time)
    {
        const size_t n = mParticles.size();
        if (n == 0 || time <= FLOAT_(0) || mWorld == nullptr)
        {
            return;
        }
        param.Sanitize();
        mFluidCount = n;

        // 0) 子步循环不变量（SolverConst）：h / 核归一化 / CFL 上限等整个 Update 内不变。
        //    2D 版在各阶段内各自重算 param.*，数学完全等价；集中求值一次更省、且便于调参。
        mConst.h = param.kernelRadius;
        mConst.h2 = mConst.h * mConst.h;
        mConst.poly6Scale = Poly6Scale(mConst.h);
        mConst.spikyScale = SpikyScale(mConst.h);
        mConst.restInv = FLOAT_(1) / param.restDensity;
        mConst.selfRho = mConst.poly6Scale * Poly6Weight(mConst.h2, FLOAT_(0));
        mConst.cflDisp = param.cfl * mConst.h;
        mConst.cflDispVel = FLOAT_(0.9) * mConst.cflDisp;
        mConst.boundaryStep = std::max(mConst.h * FLOAT_(0.05), FLOAT_(0.02));
        mConst.boundaryJitter = mConst.h * FLOAT_(0.01);
        mConst.gridSpacing = FLOAT_(1.3) * mConst.h;
        mConst.fastSteps = std::max(2, (int)(mConst.cflDisp / mConst.boundaryStep) + 1);

        // 1) 刚体布点（Floaty 的 blob 粒子）→ 组合位置 → 邻居搜索 → inside 标记
        //    → 近边界候选标记（R3 帧级预筛；必须在 MarkInside 之后，因为它复用 inside 结果）
        BuildSolidSamples();
        SyncPositions();
        FindNeighbors(mConst.gridSpacing); // Floaty: gridSpacing = 1.3 * kernelRadius
        MarkInside();
        MarkNearSolid();

        // 2) 子步循环（Floaty: dt = dtFrame / substeps）
        const int substeps = param.substeps;
        const FLOAT_ dt = time / (FLOAT_)substeps; // 子步步长（循环不变量）
        for (int s = 0; s < substeps; ++s)
        {
            IntegrateFluid(dt);   // v += dt·(g + F/m); prevPos = pos; pos += v·dt
            SolveFluidJacobi();   // λ + Δp（含内聚/曲率、刚体采样点）
            ApplyDeltas();        // pos += Δp + 刚体拟合投影
            SolveMapBoundaries(); // 边界钳制（精确贴面，位移不入速度）
            EnforceCFL(dt);       // 位移钳制 + v = Δx/dt
        }

        // XSPH 黏性：**每次 Update 只调用一次**（帧级小系数）。
        // 放进子步循环 = 每秒平滑上千次，(1-c)^1000 → 相对速度全灭 = 糖浆质感！
        ApplyXSPH();

        // 3) 回代
        CommitSolids(time);
        CommitFluid();
    }

    // ═════════════════════════════════════════════════════════
    // 着色
    // ═════════════════════════════════════════════════════════

    Vec4_ PhysicsLiquid3D::ColorByDensity(FLOAT_ density, const Params &param)
    {
        // PBF 密度围绕 ρ0 波动（λ 只压不拉，压缩时 > ρ0）。
        // 配色按"水"调校：静息水体 = 饱和水蓝，稀疏水花 = 深蓝，压缩浪尖/泡沫 = 亮青白。
        const FLOAT_ low = param.restDensity * FLOAT_(0.7);
        const FLOAT_ high = param.restDensity * FLOAT_(1.45);
        const FLOAT_ span = high - low;
        FLOAT_ t = FLOAT_(0);
        if (span > FLOAT_(1e-6))
        {
            t = (density - low) / span;
        }
        t = Clamp(t, FLOAT_(0), FLOAT_(1));
        // 深蓝(稀疏) → 饱和水蓝(静息) → 亮青白(压缩/泡沫)
        return Vec4_(
            FLOAT_(0.02) + FLOAT_(0.73) * t,
            FLOAT_(0.15) + FLOAT_(0.77) * t,
            FLOAT_(0.65) + FLOAT_(0.35) * t,
            FLOAT_(0.90));
    }

}
