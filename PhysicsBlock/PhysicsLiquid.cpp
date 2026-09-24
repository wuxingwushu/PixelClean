// ─────────────────────────────────────────────────────────────
// PhysicsLiquid —— Position Based Fluids (PBF)
//
// 算法与参数完整移植自参考项目 Floaty-main：
//   src/lib.rs   solve_fluid_jacobi     → SolveFluidJacobi（λ/Δp Jacobi 求解）
//                NeighborLinkedList    → FindNeighbors（哈希链表邻居搜索）
//   solver.ts    PBD.simulate          → Update（子步循环）
//                PBD.solveFluid        → SolveFluidJacobi + SolveMapBoundaries + ApplyDeltas
//                PBD.solveFluidBoundaries → SolveMapBoundaries（容器类比：地形）
//                PBD.enforceCFL        → EnforceCFL（位移钳制 + v = Δx/dt）
//   main.ts      SimulationParams      → Params（substeps/eps/cfl/restDensity 等默认值）
//   common.ts    maxNeighbors=64       → kMaxNeighbors
//
// 固液双向耦合 = Floaty README "Two-Way coupling"（Unified Particle Physics 的
// 质量加权 PBD）：Floaty 的软体 blob 粒子在此由刚体采样点扮演（详见头档注释）。
// ─────────────────────────────────────────────────────────────

#include "PhysicsLiquid.hpp"
#include "BaseCalculate.hpp"
#include <algorithm>
#include <cmath>
#include <functional>
#include <thread>

namespace PhysicsBlock
{
    // ═════════════════════════════════════════════════════════
    // 常数（Floaty src/lib.rs：let pi = 3.141592）
    // ═════════════════════════════════════════════════════════
    static const FLOAT_ kPi = FLOAT_(3.141592);
    static const FLOAT_ kTau = FLOAT_(6.2831853);
    // 邻居哈希桶数（Floaty NeighborLinkedList::HASH_SIZE；与类内 kHashSize 同值，
    // 文件级常量供自由函数 HashCell 使用）
    static const int kHashBuckets = 370111;

    // ═════════════════════════════════════════════════════════
    // Params
    // ═════════════════════════════════════════════════════════

    void PhysicsLiquid::Params::SyncFromParticleRadius()
    {
        // Floaty main.ts：
        //   particleDiameter = 2.0 * particleRadius
        //   kernelRadius     = 3.0 * particleRadius
        //   restDensity      = 1.0 / (particleDiameter * particleDiameter) * 1.5  ← 近似公式
        const FLOAT_ r = (particleRadius > FLOAT_(1e-4)) ? particleRadius : FLOAT_(0.15);
        particleRadius = r;
        kernelRadius = FLOAT_(3.0) * r;

        // 静息密度 = 静息间距（s = 1.6r，Floaty initSpacing）方点阵的**实际核密度**
        // （自贡献 + 邻居核和）。Floaty 的 1.5/(2r)² 只是近似：比点阵实际值低 ~5%，
        // 这 5% 的"初始压缩"经 v = Δx/dt 放大成**出生喷泉**（能量泵）——
        // 粒子一经生成就自己炸开再塌回，水体无故翻腾。点阵精确取值后 C=0 出生，喷泉消失。
        const FLOAT_ s = FLOAT_(1.6) * r;
        const FLOAT_ h = kernelRadius;
        const FLOAT_ h2 = h * h;
        const FLOAT_ poly6Scale = FLOAT_(4.0) / (kPi * h2 * h2 * h2 * h2);
        FLOAT_ rho = poly6Scale * h2 * h2 * h2; // 自贡献（与 SolveFluidJacobi 一致）
        for (int i = -3; i <= 3; ++i)
        {
            for (int j = -3; j <= 3; ++j)
            {
                if (i == 0 && j == 0)
                    continue;
                const FLOAT_ dx = i * s;
                const FLOAT_ dy = j * s;
                const FLOAT_ d2 = dx * dx + dy * dy;
                if (d2 < h2)
                {
                    const FLOAT_ t = h2 - d2;
                    rho += poly6Scale * t * t * t;
                }
            }
        }
        restDensity = rho;

        // —— 有量纲常数按 Floaty 参考尺度（r0 = 0.15）不变量化 ——
        // Floaty 的 eps / 内聚 / 曲率数值绑定其粒子间距（s0 = 0.24），直接搬数值
        // 会随场景尺度失真（如 eps [1/L²] 搬到 2× 间距 = 松弛软 4 倍，压力托不住刚体）。
        // 保持下列无量纲组合不变：
        //   eps·s²（s = 粒子间距）、内聚/s、曲率/s⁴（曲率项 = Δ∇C·spiky·w³·curv，量纲 L⁴）
        const FLOAT_ k = r / FLOAT_(0.15); // 尺度比 s/s0
        eps = FLOAT_(50.0) / (k * k);                  // Floaty: 50   @ r=0.15
        cohesion = FLOAT_(0.000025) * k;               // 水预设 = Floaty 0.01²×25%（史莱姆用 0.0001*k）
        curvature = FLOAT_(0.00002) * k * k * k * k;   // Floaty: 0.001*0.02
    }

    void PhysicsLiquid::Params::Sanitize()
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
            eps = FLOAT_(50) / powf(particleRadius / FLOAT_(0.15), 2.0f);
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

    PhysicsLiquid::PhysicsLiquid(PhysicsWorld *World, const Params &P) : mWorld(World), param(P)
    {
    }

    PhysicsLiquid::~PhysicsLiquid()
    {
        // 主动把液体粒子从物理世界摘除（Clear 内部逐个 RemoveObject）。
        // 这样无论是"世界析构先删液体"还是"用户手动 delete 液体"，
        // 都不会留下 IsLiquidParticle=true 却无人更新的孤儿粒子。
        Clear();
        if (mWorld != nullptr && mWorld->GetLiquid() == this)
        {
            mWorld->SetLiquid(nullptr);
        }
    }

    PhysicsParticle *PhysicsLiquid::AddParticle(Vec2_ pos, FLOAT_ mass, FLOAT_ friction)
    {
        PhysicsParticle *p = new PhysicsParticle(pos, mass, friction);
        p->IsLiquidParticle = true; // 液体粒子跳过引擎仲裁器（固液交互由本系统按 PBF 自理）
        mWorld->AddObject(p);       // 注册到世界：网格搜索/渲染统计/拾取复用
        mParticles.push_back(p);
        mDensity.push_back(FLOAT_(0));
        return p;
    }

    void PhysicsLiquid::AddGrid(Vec2_ center, int numX, int numY, FLOAT_ spacing, FLOAT_ mass, FLOAT_ friction)
    {
        // Floaty main.ts: initSpacing = 0.8 * particleDiameter ⇒ r = spacing / 1.6。
        // 由间距反推粒子尺度并按 Floaty 公式同步 kernelRadius / restDensity，
        // 使任何场景间距都保持 Floaty 的无量纲参数组（h=3r、ρ0=1.5/(2r)²）。
        if (spacing > FLOAT_(0))
        {
            param.particleRadius = spacing / FLOAT_(1.6);
            param.SyncFromParticleRadius();
        }
        MapFormwork *map = mWorld->GetMapFormwork(); // 不把水生成进地形实体内部
        for (int x = 0; x < numX; ++x)
        {
            for (int y = 0; y < numY; ++y)
            {
                Vec2_ pos = center + Vec2_(
                    (x - (numX - 1) * 0.5f) * spacing,
                    (y - (numY - 1) * 0.5f) * spacing);
                // 深陷粒子只能靠边界推出逃逸，会冲击邻近水体
                if (map != nullptr && map->FMGetCollide(pos))
                {
                    continue;
                }
                AddParticle(pos, mass, friction);
            }
        }
    }

    void PhysicsLiquid::Clear()
    {
        // 先把粒子列表整体摘下再逐个删除：
        // PhysicsWorld::RemoveObject 会回调 NotifyParticleRemoved（在列表里做尾交换删除），
        // 若边遍历边删除会导致迭代器失效。
        std::vector<PhysicsParticle *> pending;
        pending.swap(mParticles);
        if (mWorld != nullptr)
        {
            for (auto *p : pending)
            {
                if (p != nullptr)
                {
                    mWorld->RemoveObject(p); // RemoveObject 内部会 delete
                }
            }
        }
        else
        {
            for (auto *p : pending)
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
        mLambda.clear();
        mInside.clear();
        mNext.clear();
        mFirst.clear();
        mNeighbors.clear();
        mPairDir.clear();
        mPairW.clear();
        mSamples.clear();
        mSampleWeight.clear();
        mSolids.clear();
        mStatics.clear();
        mSolidXf.clear();
    }

    void PhysicsLiquid::NotifyParticleRemoved(PhysicsParticle *p)
    {
        if (p == nullptr)
        {
            return;
        }
        for (size_t i = 0; i < mParticles.size(); ++i)
        {
            if (mParticles[i] != p)
            {
                continue;
            }
            // 尾部交换删除（mParticles 与平行数组同步维护）
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
            return;
        }
    }

    // ═════════════════════════════════════════════════════════
    // 刚体采样点（Floaty 的 blob 粒子角色）
    // ═════════════════════════════════════════════════════════

    FLOAT_ PhysicsLiquid::SampleWeight(size_t combinedIndex) const
    {
        const size_t s = combinedIndex - mFluidCount;
        return (s < mSampleWeight.size()) ? mSampleWeight[s] : FLOAT_(0);
    }

    // ── 旋转内联（等价 vec2angle / AngleMat::Anticlockwise，但吃预计算 {cos,sin}） ──
    static inline Vec2_ RotateCS(const Vec2_ &p, const Vec2_ &cs)
    {
        return Vec2_{p.x * cs.x - p.y * cs.y, p.x * cs.y + p.y * cs.x};
    }
    static inline Vec2_ RotateInvCS(const Vec2_ &p, const Vec2_ &cs)
    {
        return Vec2_{p.x * cs.x + p.y * cs.y, -p.x * cs.y + p.y * cs.x};
    }

    // 刚体是否包含该点（形状逐格 / 圆平方距离；线段太薄视为不包含）。
    // 与旧 BodySolidAt 数学等价，但走变换缓存：AABB 快筛 + 无三角函数 + 圆不开方。
    //（旧版每次判定构造 AngleMat = 一对 cos/sin，MarkInside/边界投影每帧数万次判定）
    bool PhysicsLiquid::SolidContains(const SolidXf &xf, const Vec2_ &p)
    {
        const FLOAT_ dx = p.x - xf.pos.x;
        const FLOAT_ dy = p.y - xf.pos.y;
        if (dx < -xf.radius || dx > xf.radius || dy < -xf.radius || dy > xf.radius)
        {
            return false; // AABB 快筛：绝大多数查询一步退出
        }
        if (xf.shape != nullptr)
        {
            const Vec2_ local = RotateInvCS(Vec2_{dx, dy}, xf.cs) + xf.shape->CentreMass;
            const int cx = (int)floor(local.x);
            const int cy = (int)floor(local.y);
            if (cx < 0 || cy < 0 || cx >= (int)xf.shape->width || cy >= (int)xf.shape->height)
                return false;
            return xf.shape->at(cx, cy).Entity;
        }
        if (xf.radius2 > FLOAT_(0)) // 圆：平方距离（等价 Modulus < 0.9r）
        {
            return dx * dx + dy * dy < xf.radius2;
        }
        return false;
    }

    // 重建刚体变换缓存（与 mSolids 同序；BuildSolidSamples 末尾调用）
    void PhysicsLiquid::RefreshSolidXform()
    {
        mSolidXf.clear();
        mSolidXf.reserve(mSolids.size());
        for (const SolidTrack &st : mSolids)
        {
            PhysicsAngle *body = st.body;
            SolidXf xf;
            xf.body = body;
            xf.pos = body->pos;
            xf.cs = Vec2_{(FLOAT_)cos(body->angle), (FLOAT_)sin(body->angle)};
            xf.radius = body->PFGetCollisionR();
            xf.dynamic = (body->invMass > FLOAT_(0));
            if (body->PFGetType() == PhysicsObjectEnum::shape)
            {
                xf.shape = (PhysicsShape *)body;
            }
            else if (body->PFGetType() == PhysicsObjectEnum::circle)
            {
                const FLOAT_ r9 = ((PhysicsCircle *)body)->radius * FLOAT_(0.9);
                xf.radius2 = r9 * r9;
            }
            mSolidXf.push_back(xf);
        }
    }

    void PhysicsLiquid::BuildSolidSamples()
    {
        mSamples.clear();
        mSampleWeight.clear();
        mSolids.clear();
        mStatics.clear();
        mSamples.reserve(kMaxSolidSamples);
        mSampleWeight.reserve(kMaxSolidSamples);
        mSolids.reserve(16);
        mStatics.reserve(8);

        const size_t n = mParticles.size();
        if (n == 0)
        {
            return;
        }

        // 流体包围盒（粗剔除远处刚体）
        FLOAT_ minX = mParticles[0]->pos.x, maxX = minX;
        FLOAT_ minY = mParticles[0]->pos.y, maxY = minY;
        for (size_t i = 1; i < n; ++i)
        {
            const Vec2_ &p = mParticles[i]->pos;
            minX = std::min(minX, p.x);
            maxX = std::max(maxX, p.x);
            minY = std::min(minY, p.y);
            maxY = std::max(maxY, p.y);
        }

        // Floaty initSpacing = 0.8 * particleDiameter = 1.6r：采样点同间距布点
        const FLOAT_ spacing = std::max(param.particleRadius * FLOAT_(1.6), FLOAT_(0.05));
        const FLOAT_ margin = param.kernelRadius * FLOAT_(3);

        auto NearFluid = [&](PhysicsAngle *body) -> bool
        {
            const FLOAT_ r = body->PFGetCollisionR() + margin;
            return !(body->pos.x + r < minX || body->pos.x - r > maxX ||
                     body->pos.y + r < minY || body->pos.y - r > maxY);
        };

        auto PushSample = [&](PhysicsAngle *body, const Vec2_ &local) -> bool
        {
            if ((int)mSamples.size() >= kMaxSolidSamples)
                return false;
            mSamples.push_back(SolidSample{body, local});
            return true;
        };

        // shape / circle / line 都派生自 PhysicsAngle（与 MouseInteraction 同一约定）
        std::vector<PhysicsAngle *> candidates;
        candidates.reserve(mWorld->PhysicsShapeS.size() + mWorld->PhysicsCircleS.size() + mWorld->PhysicsLineS.size());
        for (auto *s : mWorld->PhysicsShapeS) candidates.push_back((PhysicsAngle *)s);
        for (auto *c : mWorld->PhysicsCircleS) candidates.push_back((PhysicsAngle *)c);
        for (auto *l : mWorld->PhysicsLineS) candidates.push_back((PhysicsAngle *)l);

        for (PhysicsAngle *body : candidates)
        {
            if (body == nullptr || body->PFGetMass() <= FLOAT_(0))
            {
                continue;
            }
            if (!NearFluid(body))
            {
                continue;
            }

            const int begin = (int)mSamples.size();
            switch (body->PFGetType())
            {
            case PhysicsObjectEnum::shape:
            {
                // 网格形状：每个实体格按间距细分采样（格子世界尺寸 1×1）
                PhysicsShape *s = (PhysicsShape *)body;
                int per = (int)floor(FLOAT_(1) / spacing + FLOAT_(0.5));
                if (per < 1)
                    per = 1;
                const FLOAT_ step = FLOAT_(1) / per;
                bool overflow = false;
                for (unsigned int x = 0; x < s->width && !overflow; ++x)
                {
                    for (unsigned int y = 0; y < s->height && !overflow; ++y)
                    {
                        if (!s->at(x, y).Entity)
                            continue;
                        for (int i = 0; i < per && !overflow; ++i)
                        {
                            for (int j = 0; j < per && !overflow; ++j)
                            {
                                Vec2_ local = Vec2_{x + (i + FLOAT_(0.5)) * step,
                                                    y + (j + FLOAT_(0.5)) * step} - s->CentreMass;
                                overflow = !PushSample(body, local);
                            }
                        }
                    }
                }
                break;
            }
            case PhysicsObjectEnum::circle:
            {
                // 圆盘：圆心 + 同心环布点（环距 ≈ spacing）
                PhysicsCircle *c = (PhysicsCircle *)body;
                PushSample(body, Vec2_{0, 0});
                for (FLOAT_ ring = spacing; ring <= c->radius; ring += spacing)
                {
                    int cnt = (int)floor(kTau * ring / spacing + FLOAT_(0.5));
                    if (cnt < 3)
                        cnt = 3;
                    for (int k = 0; k < cnt; ++k)
                    {
                        const FLOAT_ a = kTau * k / cnt;
                        if (!PushSample(body, Vec2_{cos(a) * ring, sin(a) * ring}))
                            break;
                    }
                }
                break;
            }
            case PhysicsObjectEnum::line:
            {
                // 线段：沿轴布点（局部 x 轴，半长 = radius）
                PhysicsLine *l = (PhysicsLine *)body;
                int cnt = (int)floor(FLOAT_(2) * l->radius / spacing + FLOAT_(0.5));
                if (cnt < 2)
                    cnt = 2;
                for (int k = 0; k <= cnt; ++k)
                {
                    const FLOAT_ t = -l->radius + FLOAT_(2) * l->radius * k / cnt;
                    if (!PushSample(body, Vec2_{t, 0}))
                        break;
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
            // Floaty 质量加权 PBD：w = 1/m_采样 = 采样数/刚体总质量；静态体 w = 0
            FLOAT_ w = FLOAT_(0);
            if (body->invMass > FLOAT_(0))
            {
                w = (FLOAT_)(end - begin) / body->PFGetMass();
            }
            else
            {
                mStatics.push_back(body);
            }
            for (int s = begin; s < end; ++s)
            {
                mSampleWeight.push_back(w);
            }
            mSolids.push_back(SolidTrack(body, begin, end));
        }

        RefreshSolidXform(); // 刚体查询缓存（与 mSolids 同序）
    }

    void PhysicsLiquid::SyncSamplePositions()
    {
        // 每刚体一对 cos/sin（旧版逐采样点调 vec2angle = 同一角度重复三角函数）
        const size_t n = mFluidCount;
        for (const SolidTrack &st : mSolids)
        {
            PhysicsAngle *body = st.body;
            if (body == nullptr)
            {
                continue;
            }
            const Vec2_ cs = Vec2_{(FLOAT_)cos(body->angle), (FLOAT_)sin(body->angle)};
            for (int s = st.begin; s < st.end; ++s)
            {
                mPos[n + s] = body->pos + RotateCS(mSamples[s].local, cs);
            }
        }
    }

    void PhysicsLiquid::SyncPositions()
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
            PhysicsParticle *p = mParticles[i];
            p->OldPos = p->pos; // 本步积分前位置（引擎 PhysicsPos 的 OldPos 记账语义）
            mPos[i] = p->pos;
            mVel[i] = p->speed;
            mPrevPos[i] = p->pos;
        }
        SyncSamplePositions();
    }

    // ═════════════════════════════════════════════════════════
    // 邻居搜索（Floaty src/lib.rs NeighborLinkedList.find_neighbors）
    // ═════════════════════════════════════════════════════════

    static inline int HashCell(int gx, int gy)
    {
        // Floaty: ((gx * 92837111) ^ (gy * 689287499)) % HASH_SIZE
        // 用无符号回绕保证负坐标一致（Floaty 世界坐标恒正，本引擎大量使用负坐标）
        const unsigned int h = (unsigned int)gx * 92837111u ^ (unsigned int)gy * 689287499u;
        return (int)(h % (unsigned int)kHashBuckets);
    }

    void PhysicsLiquid::FindNeighbors(FLOAT_ gridSpacing)
    {
        const size_t N = mPos.size();
        mFirst.resize(N + 1); // 全部元素在下方两处写入
        if (N == 0)
        {
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
            const int h = HashCell(gx, gy);
            if (mHashMark[h] != mEpoch)
            {
                mHashMark[h] = mEpoch;
                mHashFirst[h] = -1;
            }
            mNext[i] = mHashFirst[h];
            mHashFirst[h] = (int)i;
        }

        // 第二遍：3×3 邻域收集 ≤64 个邻居（距离 < gridSpacing）
        const FLOAT_ h2 = gridSpacing * gridSpacing;
        size_t accum = 0;
        for (size_t i = 0; i < N; ++i)
        {
            mFirst[i] = (int)accum;
            const int gx = (int)floor(mPos[i].x * inv);
            const int gy = (int)floor(mPos[i].y * inv);
            int count = 0;
            for (int ix = gx - 1; ix <= gx + 1; ++ix)
            {
                for (int iy = gy - 1; iy <= gy + 1; ++iy)
                {
                    const int h = HashCell(ix, iy);
                    if (mHashMark[h] != mEpoch)
                    {
                        continue;
                    }
                    for (int id = mHashFirst[h]; id >= 0; id = mNext[id])
                    {
                        const FLOAT_ dx = mPos[id].x - mPos[i].x;
                        const FLOAT_ dy = mPos[id].y - mPos[i].y;
                        const FLOAT_ r2 = dx * dx + dy * dy;
                        if (r2 < h2 && (size_t)id != i && count < kMaxNeighbors)
                        {
                            mNeighbors[accum + count] = id;
                            ++count;
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
    // ═════════════════════════════════════════════════════════

    void PhysicsLiquid::MarkInside()
    {
        const size_t n = mFluidCount;
        mInside.assign(n, 0);
        for (const SolidXf &xf : mSolidXf)
        {
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
    // 子步阶段
    // ═════════════════════════════════════════════════════════

    // 半隐式欧拉（Floaty solver.ts simulate 积分段：v += g·dt; prevPos = pos; pos += v·dt）
    // 注：Floaty 只有重力项；此处把 AddForce 外力并入同一积分式（PBD 标准外力项）。
    void PhysicsLiquid::IntegrateFluid(FLOAT_ dt)
    {
        const size_t n = mFluidCount;
        const Vec2_ g = mWorld->GravityAcceleration;
        for (size_t i = 0; i < n; ++i)
        {
            PhysicsParticle *p = mParticles[i];
            mVel[i] += dt * (g + p->invMass * p->force);
            mPrevPos[i] = mPos[i];
            mPos[i] += mVel[i] * dt;
        }
    }

    // ── 数据并行分片（λ/Δp 求解按 i 独立读写、无写冲突 → 结果与串行逐位一致、确定性） ──
    static void ParallelRange(ThreadPool &pool, size_t count, const std::function<void(size_t, size_t)> &fn)
    {
        const unsigned int T = std::min(std::thread::hardware_concurrency(), 8u);
        if (T <= 1 || count < 1024) // 小规模直接串行（分发开销 > 收益）
        {
            fn(0, count);
            return;
        }
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
            fs.push_back(pool.enqueue([fn, b, e]() { fn(b, e); }));
        }
        for (auto &f : fs)
        {
            f.wait();
        }
    }

    // λ + Δp 求解（Floaty src/lib.rs solve_fluid_jacobi 逐式移植）
    void PhysicsLiquid::SolveFluidJacobi()
    {
        const size_t n = mFluidCount;
        const size_t N = mPos.size();
        const FLOAT_ h = param.kernelRadius;
        const FLOAT_ h2 = h * h;
        // Floaty lib.rs：
        //   poly6_scale = 4.0 / (pi * h2 * h2 * h2 * h2)   （2D poly6 核）
        //   spiky_scale = 10.0 / (pi * h2 * h2 * h)        （2D spiky 梯度）
        const FLOAT_ poly6Scale = FLOAT_(4.0) / (kPi * h2 * h2 * h2 * h2);
        const FLOAT_ spikyScale = FLOAT_(10.0) / (kPi * h2 * h2 * h);
        const FLOAT_ restInv = FLOAT_(1) / param.restDensity;
        const FLOAT_ selfRho = poly6Scale * h2 * h2 * h2; // 自贡献（Floaty: rho = poly6_scale * h2³）

        // —— λ 求解（Jacobi：λ_i = −C_i / (Σ|∇C|² + eps)，C<0 时 λ=0）——
        // 每 i 独立写 mLambda/mGrad/mDensity/mPair* → 数据并行
        ParallelRange(mWorld->mThreadPool, N, [&](size_t rangeBegin, size_t rangeEnd)
        {
        for (size_t i = rangeBegin; i < rangeEnd; ++i)
        {
            const bool insideI = (i < n) && (mInside[i] != 0);
            FLOAT_ rho = selfRho;
            Vec2_ gradSum{0, 0};
            FLOAT_ sumGrad2 = 0;

            if (!insideI)
            {
                const int first = mFirst[i];
                const int last = mFirst[i + 1];
                for (int k = first; k < last; ++k)
                {
                    const int j = mNeighbors[k];
                    // Floaty：刚体↔刚体相互作用忽略；在刚体内部的流体忽略
                    //（无效对写 mPairW[k] < 0 哨兵，Δp 遍直接跳过——两遍跳过集完全一致）
                    if (j < 0 || (i >= n && (size_t)j >= n) || ((size_t)j < n && mInside[j]))
                    {
                        mPairW[k] = -FLOAT_(1);
                        continue;
                    }

                    const FLOAT_ dx = mPos[i].x - mPos[j].x;
                    const FLOAT_ dy = mPos[i].y - mPos[j].y;
                    const FLOAT_ r2 = dx * dx + dy * dy;
                    if (r2 < h2 && r2 > FLOAT_(0))
                    {
                        rho += poly6Scale * (h2 - r2) * (h2 - r2) * (h2 - r2);
                        const FLOAT_ r = sqrt(r2);
                        const FLOAT_ w = h - r;
                        // 单位方向用"一次倒数 + 乘法"（旧式 dx/r, dy/r = 每对两次除法，
                        // 数十万对/秒的除法是最大单项指令开销；同公式换乘倒数）
                        const FLOAT_ invR = FLOAT_(1) / r;
                        const Vec2_ u = Vec2_{dx * invR, dy * invR};
                        // 配对缓存：单位方向 + 核距（Δp 遍复用，省掉第二次 dx/dy/r²/√r/核求值）
                        mPairDir[k] = u;
                        mPairW[k] = w;
                        // Floaty: cur_grad = d/r * (spiky_scale * w * w * -3) / restDensity
                        const FLOAT_ gscale = (spikyScale * w * w * -FLOAT_(3)) * restInv;
                        const Vec2_ curGrad = u * gscale;
                        sumGrad2 += curGrad.x * curGrad.x + curGrad.y * curGrad.y;
                        gradSum += curGrad;
                    }
                    else
                    {
                        mPairW[k] = -FLOAT_(1);
                    }
                }
            }

            // Floaty: sum_grad_2 += (grad_ix² + grad_iy²) * inv_m
            const FLOAT_ invM = (i >= n) ? SampleWeight(i) : FLOAT_(1);
            sumGrad2 += (gradSum.x * gradSum.x + gradSum.y * gradSum.y) * invM;
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
        // 每 i 独立写 mDelta（λ/grad 缓存本遍只读）→ 数据并行
        ParallelRange(mWorld->mThreadPool, N, [&](size_t rangeBegin, size_t rangeEnd)
        {
        for (size_t i = rangeBegin; i < rangeEnd; ++i)
        {
            const bool insideI = (i < n) && (mInside[i] != 0);
            Vec2_ delta{0, 0};
            const FLOAT_ invM = (i >= n) ? SampleWeight(i) : FLOAT_(1);

            if (i >= n)
            {
                // 刚体采样点（Floaty blob 分支）：只与流体作用
                const int first = mFirst[i];
                const int last = mFirst[i + 1];
                for (int k = first; k < last; ++k)
                {
                    const FLOAT_ w = mPairW[k];
                    if (w < FLOAT_(0))
                        continue;
                    const int j = mNeighbors[k];
                    delta += mPairDir[k] * ((spikyScale * w * w * -FLOAT_(3)) * restInv * (mLambda[i] + mLambda[j]));
                }
            }
            else if (!insideI)
            {
                // 流体粒子
                const int first = mFirst[i];
                const int last = mFirst[i + 1];
                for (int k = first; k < last; ++k)
                {
                    const FLOAT_ w = mPairW[k];
                    if (w < FLOAT_(0))
                        continue;
                    const int j = mNeighbors[k];
                    delta += mPairDir[k] * ((spikyScale * w * w * -FLOAT_(3)) * restInv * (mLambda[i] + mLambda[j]));

                    if ((size_t)j < n && param.surfaceTension)
                    {
                        // 表面张力（Akinci 内聚 + 曲率，Floaty lib.rs 流体↔流体分支）：
                        //   cohesion = d/r * (-0.01*0.01)
                        //   curvature = (grads_i - grads_j) * spiky_scale*w³ * 0.001*0.02
                        delta += mPairDir[k] * (-param.cohesion);
                        delta += (mGrad[i] - mGrad[j]) * (spikyScale * w * w * w * param.curvature);
                    }
                }
            }
            mDelta[i] = delta * invM;
        }
        });
    }

    // 边界钳制（Floaty solver.ts PBD.solveFluidBoundaries 的容器类比）：
    // 容器 = 地图格 + 静态刚体 + **动态刚体内部**，粒子一律沿 8 方向小步进找表面
    // 并二分精确贴面推出（推出量 = 实际穿透量；推出位移记入 mBoundaryDisp、
    // 不注入速度——穿入速度反而会被推出位移精确抵消，见 EnforceCFL 的 bVel 规则）。
    //
    // 动态刚体也参与投影是关键修复：Floaty 对"困在软体里的水"的处理是 inside 标记
    // + 放行自由流出（其软体是空心环，成立）；但实心刚体里"自由流出" = 幽灵带着
    // 入水速度**坠穿整块浮板**、从板底钻出砸进板下水面 → 溅起的浪把板弹起来
    // （水滴落在浮板上会"被排斥到板底下"的怪象）。几何挤出后水滴触板即被挤出，
    // 不穿透、不弹飞。
    void PhysicsLiquid::SolveMapBoundaries()
    {
        const size_t n = mFluidCount;
        MapFormwork *map = mWorld->GetMapFormwork();
        const FLOAT_ jitter = param.kernelRadius * FLOAT_(0.01);
        const FLOAT_ step = std::max(param.kernelRadius * FLOAT_(0.05), FLOAT_(0.02));
        const int maxSteps = 64; // 最远探测 ≈ 3.2 世界单位

        auto PointSolid = [&](const Vec2_ &p) -> bool
        {
            if (map != nullptr && map->FMGetCollide(p))
            {
                return true;
            }
            // 静态 + 动态刚体统一走变换缓存（AABB 快筛；动态刚体内部挤出 = 修复幽灵穿透）
            for (const SolidXf &xf : mSolidXf)
            {
                if (SolidContains(xf, p))
                {
                    return true;
                }
            }
            return false;
        };

        static const Vec2_ dirs[8] = {
            {0, 1}, {0, -1}, {1, 0}, {-1, 0},
            {FLOAT_(0.7071), FLOAT_(0.7071)}, {FLOAT_(0.7071), FLOAT_(-0.7071)},
            {FLOAT_(-0.7071), FLOAT_(0.7071)}, {FLOAT_(-0.7071), FLOAT_(-0.7071)}};

        // 8 方向搜索最近表面（limitSteps 内找到即回）。返回是否找到。
        // ⚠ limitSteps 必须小：CFL 限制每子步最多移动 maxDisp，近表面穿透（绝大多数
        // 粒子的常态——池底/池壁接触层每子步被重力压入 1e-4）用不到 3 步以外的搜索；
        // 旧版一律扫满 64 步，向墙内方向的空探测占了整个边界阶段 90% 的耗时。
        auto FindEscape = [&](const Vec2_ &p, int limitSteps, Vec2_ &outDir, FLOAT_ &outLo, FLOAT_ &outHi) -> bool
        {
            FLOAT_ bestDist = FLOAT_(1e30);
            bool found = false;
            for (int d = 0; d < 8; ++d)
            {
                for (int k = 1; k <= limitSteps; ++k)
                {
                    const Vec2_ cand = p + dirs[d] * (step * k);
                    if (!PointSolid(cand))
                    {
                        const FLOAT_ dist = step * k;
                        if (dist < bestDist)
                        {
                            bestDist = dist;
                            outDir = dirs[d];
                            outLo = step * (k - 1); // 固体侧端点距离
                            outHi = step * k;       // 空位侧端点距离
                            found = true;
                        }
                        break; // 该方向的表面
                    }
                }
            }
            return found;
        };

        const FLOAT_ maxDisp = param.cfl * param.kernelRadius;
        const int fastSteps = std::max(2, (int)(maxDisp / step) + 1); // ≈3：覆盖每子步实际可移动距离

        for (size_t i = 0; i < n; ++i)
        {
            mBoundaryDisp[i] = Vec2_{0, 0};
            Vec2_ &p = mPos[i];
            if (!PointSolid(p))
            {
                continue;
            }
            // 两段式搜索：近程快路径（常态接触）→ 深陷实体才走全距慢路径
            Vec2_ bestDir{0, 0};
            FLOAT_ bestLo = 0, bestHi = 0;
            if (!FindEscape(p, fastSteps, bestDir, bestLo, bestHi) &&
                !FindEscape(p, maxSteps, bestDir, bestLo, bestHi))
            {
                continue;
            }

            // 二分定位表面（[bestLo 固体, bestHi 空位]），推出量 ≈ 实际穿透量
            //（5 次迭代 = 步长 1/32 ≈ 1.5mm 精度，足够；旧版 8 次白算 3 轮）
            for (int iter = 0; iter < 5; ++iter)
            {
                const FLOAT_ mid = (bestLo + bestHi) * FLOAT_(0.5);
                if (PointSolid(p + bestDir * mid))
                    bestLo = mid;
                else
                    bestHi = mid;
            }
            Vec2_ target = p + bestDir * bestHi;
            // 大幅推出（深陷实体逃逸，bestHi = 表面距离）才抖动松解；接触级微修正不抖动
            if (bestHi > param.kernelRadius)
            {
                target += Vec2_{Random(-jitter, jitter), Random(-jitter, jitter)};
            }
            // 推出量受 CFL 位移上限：深陷实体（如初始就压在地形里的水）逐子步
            // 渐进逃逸。表面可以一次找到，但位移必须分帧摊销——整段应用会让
            // 逃逸粒子瞬间砸进邻近水体（局部密度 +150% 的冲击波）。
            Vec2_ move = target - p;
            const FLOAT_ maxDisp = param.cfl * param.kernelRadius;
            const FLOAT_ len2 = ModulusLength(move);
            if (len2 > maxDisp * maxDisp)
            {
                move *= maxDisp / sqrt(len2);
            }
            mBoundaryDisp[i] = move;
            p += move;
        }
    }

    // Δp 应用 + 刚体拟合投影（Floaty 的软体约束投影槽位 → 刚体最小二乘拟合）
    void PhysicsLiquid::ApplyDeltas()
    {
        const size_t n = mFluidCount;
        for (size_t i = 0; i < n; ++i)
        {
            mPos[i] += mDelta[i]; // 流体 w = 1（mDelta 已含 inv_m）
        }
        for (size_t s = 0; s < mSamples.size(); ++s)
        {
            mPos[n + s] += mDelta[n + s]; // 采样点：Δp·w（质量加权 PBD）
        }
        ProjectSolids();
    }

    void PhysicsLiquid::ProjectSolids()
    {
        const size_t n = mFluidCount;
        for (size_t bi = 0; bi < mSolids.size(); ++bi)
        {
            SolidTrack &st = mSolids[bi];
            PhysicsAngle *body = st.body;
            const int cnt = st.end - st.begin;
            if (body == nullptr || cnt <= 0 || body->invMass <= FLOAT_(0))
            {
                continue; // 静态刚体不动（w = 0，采样点也不需重锚定）
            }

            // 最小二乘刚体拟合：平移 = 平均位移；转动 = Σ(r×d)/Σr²
            //（cos/sin 每刚体一对；旧版逐采样点调 vec2angle 重复算三角函数）
            const Vec2_ cs = Vec2_{(FLOAT_)cos(body->angle), (FLOAT_)sin(body->angle)};
            Vec2_ t{0, 0};
            FLOAT_ rot = 0;
            FLOAT_ den = 0;
            for (int s = st.begin; s < st.end; ++s)
            {
                const Vec2_ d = mDelta[n + s];
                t += d;
                const Vec2_ r = RotateCS(mSamples[s].local, cs); // 采样点当前力臂
                rot += r.x * d.y - r.y * d.x;
                den += r.x * r.x + r.y * r.y;
            }
            t /= (FLOAT_)cnt;
            FLOAT_ dTheta = (den > FLOAT_(1e-6)) ? (rot / den) : FLOAT_(0);

            // 位移钳制（Floaty enforceCFL 的 blob 槽位：每子步 ≤ cfl·h）
            const FLOAT_ maxDisp = param.cfl * param.kernelRadius;
            const FLOAT_ len2 = ModulusLength(t);
            if (len2 > maxDisp * maxDisp)
            {
                t *= maxDisp / sqrt(len2);
            }
            if (dTheta > maxDisp)
                dTheta = maxDisp;
            if (dTheta < -maxDisp)
                dTheta = -maxDisp;

            body->pos += t;
            body->angle += dTheta;
            st.accumT += t;
            st.accumR += dTheta;

            // 本刚体采样点重锚定 + 变换缓存就地刷新
            //（替代旧的整表 SyncSamplePositions / 全量重建；静态体从不移动无需重锚定）
            const Vec2_ csNew = Vec2_{(FLOAT_)cos(body->angle), (FLOAT_)sin(body->angle)};
            for (int s = st.begin; s < st.end; ++s)
            {
                mPos[n + s] = body->pos + RotateCS(mSamples[s].local, csNew);
            }
            if (bi < mSolidXf.size())
            {
                mSolidXf[bi].pos = body->pos;
                mSolidXf[bi].cs = csNew;
            }
        }
    }

    // 位移 CFL 钳制 + 速度回代（Floaty solver.ts PBD.enforceCFL：v = Δx/dt）
    // 边界修正的处理（关键稳定性规则）：
    //   · 修正量 ≤ 来流运动量（正常接触，b ≈ −a 的法向分量）→ 全额计入 Δx：
    //     接触把法向速度自然消掉（否则重力速度在贴地时无限累积，几十帧后整池弹飞）；
    //   · 修正量 > 来流运动量（深陷实体逃逸）→ 最多只计入 |a|：位置照常逃逸，
    //     但不注入逃逸速度（否则推出修正经 v = Δx/dt 变成千倍假速度）。
    void PhysicsLiquid::EnforceCFL(FLOAT_ dt)
    {
        const size_t n = mFluidCount;
        const FLOAT_ maxDisp = param.cfl * param.kernelRadius * FLOAT_(0.9); // Floaty: 0.9 * maxVel
        const FLOAT_ invDt = FLOAT_(1) / dt;
        for (size_t i = 0; i < n; ++i)
        {
            const Vec2_ b = mBoundaryDisp[i];                       // 边界修正
            const Vec2_ a = (mPos[i] - b) - mPrevPos[i];            // 边界修正前的运动量
            const FLOAT_ aLen2 = ModulusLength(a);                  // 平方长度：常态（无边界修正）零开方
            const FLOAT_ bLen2 = ModulusLength(b);
            Vec2_ bVel = b;
            if (bLen2 > aLen2 && bLen2 > FLOAT_(0))
            {
                bVel = b * (FLOAT_)(sqrt(aLen2) / sqrt(bLen2)); // 逃逸修正不注入速度
            }
            Vec2_ d = a + bVel;
            const FLOAT_ len2 = ModulusLength(d);
            if (len2 > maxDisp * maxDisp)
            {
                d *= maxDisp / sqrt(len2);
            }
            mVel[i] = d * invDt;
            mPos[i] = mPrevPos[i] + d + (b - bVel); // 位置保留全部边界修正
        }
    }

    // XSPH 速度平滑黏性（Macklin《Position Based Fluids》论文的可选黏性项）：
    //   v_i += viscosity · Σ_j (v_j − v_i)·W(r_ij) / Σ_j W(r_ij)
    // 物理黏性：抹平速度高频噪声、让晃动自然衰减——但没有任何黏连力，
    // 因此是"水"的手感；史莱姆的胶感来自表面张力（内聚），两者是不同的东西。
    // 注意：每次 Update 只调用**一次**（帧级系数语义）。
    void PhysicsLiquid::ApplyXSPH()
    {
        const FLOAT_ c = param.viscosity;
        if (c <= FLOAT_(0))
        {
            return;
        }
        const size_t n = mFluidCount;
        const FLOAT_ h = param.kernelRadius;
        const FLOAT_ h2 = h * h;
        for (size_t i = 0; i < n; ++i)
        {
            if (mInside[i])
            {
                continue;
            }
            Vec2_ avg{0, 0};
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
                const FLOAT_ dx = mPos[i].x - mPos[j].x;
                const FLOAT_ dy = mPos[i].y - mPos[j].y;
                const FLOAT_ r2 = dx * dx + dy * dy;
                if (r2 < h2 && r2 > FLOAT_(0))
                {
                    const FLOAT_ w = (h2 - r2) * (h2 - r2) * (h2 - r2); // poly6 权重（归一化后无量纲）
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
    void PhysicsLiquid::CommitSolids(FLOAT_ time)
    {
        const FLOAT_ dtSub = time / (FLOAT_)param.substeps;
        const FLOAT_ maxSpeed = (dtSub > FLOAT_(0)) ? (param.cfl * param.kernelRadius / dtSub) : FLOAT_(0);
        for (SolidTrack &st : mSolids)
        {
            PhysicsAngle *body = st.body;
            if (body == nullptr || body->invMass <= FLOAT_(0))
            {
                st.accumT = Vec2_{0, 0};
                st.accumR = 0;
                continue;
            }
            Vec2_ dv = st.accumT / time;
            FLOAT_ dw = st.accumR / time;
            const FLOAT_ len2 = ModulusLength(dv);
            if (len2 > maxSpeed * maxSpeed)
            {
                dv *= maxSpeed / sqrt(len2);
            }
            if (dw > maxSpeed)
                dw = maxSpeed;
            if (dw < -maxSpeed)
                dw = -maxSpeed;
            body->speed += dv;
            body->angleSpeed += dw;
            body->StaticNum = 0; // 唤醒
            st.accumT = Vec2_{0, 0};
            st.accumR = 0;
        }
    }

    // 回写粒子（对应引擎 PhysicsPos 的 pos/speed/OldPos/StaticNum 记账）
    void PhysicsLiquid::CommitFluid()
    {
        const size_t n = mFluidCount;
        for (size_t i = 0; i < n; ++i)
        {
            PhysicsParticle *p = mParticles[i];
            p->pos = mPos[i];
            p->speed = mVel[i];
            p->force = {0, 0};
            if (p->OldPos == p->pos)
            {
                if (p->StaticNum < 200)
                    ++p->StaticNum;
            }
            else
            {
                p->StaticNum = 0;
            }
        }
    }

    // ═════════════════════════════════════════════════════════
    // 主循环（Floaty solver.ts PBD.simulate）
    // ═════════════════════════════════════════════════════════

    void PhysicsLiquid::Update(FLOAT_ time)
    {
        const size_t n = mParticles.size();
        if (n == 0 || time <= FLOAT_(0) || mWorld == nullptr)
        {
            return;
        }
        param.Sanitize();
        mFluidCount = n;

        // 1) 刚体布点（Floaty 的 blob 粒子）→ 组合位置 → 邻居搜索 → inside 标记
        BuildSolidSamples();
        SyncPositions();
        FindNeighbors(FLOAT_(1.3) * param.kernelRadius); // Floaty: gridSpacing = 1.3 * kernelRadius
        MarkInside();

        // 2) 子步循环（Floaty: dt = dtFrame / substeps）
        const int substeps = param.substeps;
        for (int s = 0; s < substeps; ++s)
        {
            const FLOAT_ dt = time / (FLOAT_)substeps;
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

    glm::vec4 PhysicsLiquid::ColorByDensity(FLOAT_ density, const Params &param)
    {
        // PBF 密度围绕 ρ0 波动（λ 只压不拉，压缩时 > ρ0）。
        // 配色按"水"调校：静息水体 = 饱和水蓝，稀疏水花 = 深蓝，压缩浪尖/泡沫 = 亮青白。
        //（旧映射把静息水体放在浅蓝白区间，看起来像史莱姆凝胶）
        const FLOAT_ low = param.restDensity * FLOAT_(0.7);
        const FLOAT_ high = param.restDensity * FLOAT_(1.45);
        const FLOAT_ span = high - low;
        FLOAT_ t = FLOAT_(0);
        if (span > FLOAT_(1e-6))
        {
            t = (density - low) / span;
        }
        t = std::clamp(t, FLOAT_(0), FLOAT_(1));
        // 深蓝(稀疏) → 饱和水蓝(静息) → 亮青白(压缩/泡沫)
        return glm::vec4(
            FLOAT_(0.02) + FLOAT_(0.73) * t,
            FLOAT_(0.15) + FLOAT_(0.77) * t,
            FLOAT_(0.65) + FLOAT_(0.35) * t,
            FLOAT_(0.90));
    }

}
