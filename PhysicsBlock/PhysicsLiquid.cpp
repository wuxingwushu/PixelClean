#include "PhysicsLiquid.hpp"
#include "BaseCalculate.hpp"
#include <algorithm>
#include <limits>
#include <cmath>

namespace PhysicsBlock
{

    PhysicsLiquid::PhysicsLiquid(PhysicsWorld *World, const Params &P) : mWorld(World), param(P)
    {
    }

    PhysicsLiquid::~PhysicsLiquid()
    {
        // 主动把液体粒子从物理世界摘除（Clear 内部逐个 RemoveObject）。
        // 这样无论是"世界析构先删液体"还是"用户手动 delete 液体"，
        // 都不会留下 IsLiquidParticle=true 却无人更新的孤儿粒子
        //（孤儿粒子跳过全部仲裁器，会穿地心无限下落）。
        Clear();
        if (mWorld != nullptr && mWorld->GetLiquid() == this)
        {
            mWorld->SetLiquid(nullptr);
        }
        mSearchV.clear();
        mClipPoly.clear();
        mReactionBodies.clear();
        mReactionJ.clear();
        mReactionT.clear();
        mOrder.clear();
        mHeightOf.clear();
        mEligible.clear();
        mExposed.clear();
        mClusterCount.clear();
        mBins.clear();
        mBinNum = 0;
        mIndexMap.clear();
    }

    PhysicsParticle *PhysicsLiquid::AddParticle(Vec2_ pos, FLOAT_ mass, FLOAT_ friction)
    {
        PhysicsParticle *p = new PhysicsParticle(pos, mass, friction);
        p->IsLiquidParticle = true; // 液体粒子跳过引擎仲裁器（固液交互由液体系统自理）
        mWorld->AddObject(p); // 注册到世界：网格搜索/重力/位置积分/渲染统计复用
        mParticles.push_back(p);
        mPrevPos.push_back(pos);
        mDensity.push_back(FLOAT_(0));
        mDensityNear.push_back(FLOAT_(0));
        mShift.push_back(Vec2_{0, 0});
        mPreProjPos.push_back(pos);
        mProjNormal.push_back(Vec2_{0, 0});
        mProjBodyVel.push_back(Vec2_{0, 0});
        mProjBodyIdx.push_back(-1);
        mParticleMass = mass; // 记录粒子质量（浮力/面密度估算用）
        mIndexMapDirty = true;
        return p;
    }

    void PhysicsLiquid::AddGrid(Vec2_ center, int numX, int numY, FLOAT_ spacing, FLOAT_ mass, FLOAT_ friction)
    {
        mSpacing = spacing; // 记录粒子间距（浮力估算兜底）
        for (int x = 0; x < numX; ++x)
        {
            for (int y = 0; y < numY; ++y)
            {
                Vec2_ pos = center + Vec2_(
                    (x - (numX - 1) * 0.5f) * spacing,
                    (y - (numY - 1) * 0.5f) * spacing);
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
                    mWorld->RemoveObject(p); // RemoveObject 内部会 delete，并从网格搜索中移除
                }
            }
        }
        else
        {
            // 正常路径下 mWorld 必非空（AddParticle 依赖它）；仅为防御性释放
            for (auto *p : pending)
            {
                delete p;
            }
        }
        mDensity.clear();
        mDensityNear.clear();
        mPrevPos.clear();
        mShift.clear();
        mPreProjPos.clear();
        mProjNormal.clear();
        mProjBodyVel.clear();
        mProjBodyIdx.clear();
        mNeighborIndex.clear();
        mNeighborOffset.clear();
        mBoundary.clear();
        mBoundaryOffset.clear();
        mIndexMap.clear();
        mIndexMapDirty = true;
        // 重置浮力估算基准，防止 Clear 后仅 AddParticle（未 AddGrid）时沿用旧值
        mParticleMass = 1.0f;
        mSpacing = 0.5f;
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
            // 尾部交换删除：与 mParticles 平行的所有缓冲同步维护。
            // 每个缓冲**各自独立**判定（不要把它们嵌进同一个 if：一旦某个缓冲长度
            // 不同步，后面几个就会被整段跳过，指针列表与平行数组当场错位）。
            const size_t last = mParticles.size() - 1;
            auto SwapBack = [&](auto &buf)
            {
                if (i < buf.size() && last < buf.size())
                {
                    buf[i] = buf[last];
                }
            };
            mParticles[i] = mParticles[last];
            SwapBack(mPrevPos);
            SwapBack(mDensity);
            SwapBack(mDensityNear);
            SwapBack(mShift);
            SwapBack(mPreProjPos);
            SwapBack(mProjNormal);
            SwapBack(mProjBodyVel);
            SwapBack(mProjBodyIdx);
            mParticles.pop_back();
            if (mPrevPos.size() > mParticles.size()) mPrevPos.pop_back();
            if (mDensity.size() > mParticles.size()) mDensity.pop_back();
            if (mDensityNear.size() > mParticles.size()) mDensityNear.pop_back();
            if (mShift.size() > mParticles.size()) mShift.pop_back();
            if (mPreProjPos.size() > mParticles.size()) mPreProjPos.pop_back();
            if (mProjNormal.size() > mParticles.size()) mProjNormal.pop_back();
            if (mProjBodyVel.size() > mParticles.size()) mProjBodyVel.pop_back();
            if (mProjBodyIdx.size() > mParticles.size()) mProjBodyIdx.pop_back();
            mIndexMapDirty = true;
            return;
        }
    }

    void PhysicsLiquid::BuildIndexMap()
    {
        const size_t n = mParticles.size();
        mIndexMap.clear();
        mIndexMap.reserve(n * 2);
        for (size_t i = 0; i < n; ++i)
        {
            mIndexMap[mParticles[i]] = (unsigned int)i;
        }
        mIndexMapDirty = false;
    }

    void PhysicsLiquid::RebuildNeighbors(FLOAT_ h)
    {
        const size_t n = mParticles.size();
        mNeighborOffset.assign(n + 1, 0);
        mNeighborIndex.clear();
        if (n == 0)
        {
            return;
        }

        // 指针 → 液体粒子索引（只在粒子增删后重建，避免每帧 n 次哈希插入）
        if (mIndexMapDirty)
        {
            BuildIndexMap();
        }

        const FLOAT_ h2 = h * h;

        for (size_t i = 0; i < n; ++i)
        {
            const Vec2_ pi = mParticles[i]->pos;
            // 复用世界网格搜索：矩形重载，盒半宽 = h（正好覆盖 h 半径的圆）。
            // 注意不要用 Get(pos,R) 重载：它内部把 R 再乘 2，会多查 4 倍格子。
            mWorld->mGridSearch.Get(pi - Vec2_{h, h}, pi + Vec2_{h, h}, mSearchV);
            for (auto *o : mSearchV)
            {
                if (o == nullptr || o->PFGetType() != PhysicsObjectEnum::particle)
                {
                    continue;
                }
                auto it = mIndexMap.find((PhysicsParticle *)o);
                if (it == mIndexMap.end())
                {
                    continue; // 非本液体系统的粒子不参与液体交互
                }
                const unsigned int j = it->second;
                if (j == i)
                {
                    continue;
                }
                // 距离过滤（网格查询有裕量，需精确判定）
                const Vec2_ d = o->PFGetPos() - pi;
                if (ModulusLength(d) >= h2)
                {
                    continue;
                }
                mNeighborIndex.push_back(j);
            }
            mNeighborOffset[i + 1] = (unsigned int)mNeighborIndex.size();
        }
    }

    void PhysicsLiquid::RebuildBoundarySamples(FLOAT_ h, FLOAT_ spacingEff)
    {
        const size_t n = mParticles.size();
        mBoundaryOffset.assign(n + 1, 0);
        mBoundary.clear();
        if (n == 0)
        {
            return;
        }
        MapFormwork *map = mWorld->GetMapFormwork();
        const auto &shapes = mWorld->PhysicsShapeS;
        const auto &circles = mWorld->PhysicsCircleS;
        const FLOAT_ h2 = h * h;
        // 每个实体格按流体间距子采样：只取格心会让薄板表面的密度贡献严重不足
        //（格心到表面 0.5~1.0，核值几乎为 0）。
        // ① sub 必须有上限：spacingEff = h·sqrt(0.336/ρ0)，h 小 / ρ0 大时 1/spacingEff
        //    可以到几百（h=0.05、ρ0=50 → 244），每格 sub² 次内层循环能把一帧拖成几秒。
        // ② 权重必须补偿采样密度：每单位面积有 sub² 个样本，而流体是 1/spacingEff² 个
        //    粒子，所以每点权重 = 1/(spacingEff²·sub²)。原来恒为 1，只有在
        //    1/spacingEff 恰好是整数时才正确，否则壁面密度补偿会偏 ±50%
        //   （ρ0=2 时少 33% → 水被吸进墙里；ρ0=0.8 时多 68% → 水被从墙面推开）。
        const int sub = std::clamp((int)std::lround(FLOAT_(1) / std::max(spacingEff, FLOAT_(1e-3))), 1, 8);
        const FLOAT_ subInv = FLOAT_(1) / (FLOAT_)sub;
        const FLOAT_ boundaryWeight =
            FLOAT_(1) / std::max(spacingEff * spacingEff * (FLOAT_)(sub * sub), FLOAT_(1e-6));

        for (size_t i = 0; i < n; ++i)
        {
            const Vec2_ pi = mParticles[i]->pos;

            // ── 地图：h 盒内的实体格 ──
            if (map != nullptr)
            {
                const glm::ivec2 c0 = ToInt(pi - Vec2_{h, h});
                const glm::ivec2 c1 = ToInt(pi + Vec2_{h, h});
                for (int cx = c0.x; cx <= c1.x; ++cx)
                {
                    for (int cy = c0.y; cy <= c1.y; ++cy)
                    {
                        const Vec2_ cc{(FLOAT_)cx + FLOAT_(0.5), (FLOAT_)cy + FLOAT_(0.5)};
                        if (!map->FMGetCollide(cc))
                        {
                            continue; // 空格子
                        }
                        for (int sx = 0; sx < sub; ++sx)
                        {
                            for (int sy = 0; sy < sub; ++sy)
                            {
                                const Vec2_ pc = cc + Vec2_{
                                    ((FLOAT_)sx + FLOAT_(0.5)) * subInv - FLOAT_(0.5),
                                    ((FLOAT_)sy + FLOAT_(0.5)) * subInv - FLOAT_(0.5)};
                                if (ModulusLength(pc - pi) < h2)
                                {
                                    mBoundary.push_back(BoundarySample{pc, boundaryWeight});
                                }
                            }
                        }
                    }
                }
            }

            // ── 形状：局部格坐标下的 h 盒 ──
            for (auto *s : shapes)
            {
                if (s == nullptr)
                {
                    continue;
                }
                const Vec2_ d = pi - s->pos;
                if (ModulusLength(d) > (s->radius + h) * (s->radius + h))
                {
                    continue; // 远距粗筛
                }
                const Vec2_ local = vec2angle(d, -s->angle) + s->CentreMass;
                const int x0 = std::max(0, (int)std::floor(local.x - h));
                const int x1 = std::min((int)s->width - 1, (int)std::floor(local.x + h));
                const int y0 = std::max(0, (int)std::floor(local.y - h));
                const int y1 = std::min((int)s->height - 1, (int)std::floor(local.y + h));
                for (int gx = x0; gx <= x1; ++gx)
                {
                    for (int gy = y0; gy <= y1; ++gy)
                    {
                        if (!s->at(gx, gy).Collision)
                        {
                            continue;
                        }
                        const Vec2_ lc{(FLOAT_)gx + FLOAT_(0.5), (FLOAT_)gy + FLOAT_(0.5)};
                        const Vec2_ cellW = vec2angle(lc - s->CentreMass, s->angle) + s->pos;
                        const Vec2_ ux = vec2angle(Vec2_{subInv, 0}, s->angle);
                        const Vec2_ uy = vec2angle(Vec2_{0, subInv}, s->angle);
                        for (int sx = 0; sx < sub; ++sx)
                        {
                            for (int sy = 0; sy < sub; ++sy)
                            {
                                const Vec2_ pw = cellW
                                                 + ux * (((FLOAT_)sx + FLOAT_(0.5)) - FLOAT_(0.5) * (FLOAT_)sub)
                                                 + uy * (((FLOAT_)sy + FLOAT_(0.5)) - FLOAT_(0.5) * (FLOAT_)sub);
                                if (ModulusLength(pw - pi) < h2)
                                {
                                    mBoundary.push_back(BoundarySample{pw, boundaryWeight});
                                }
                            }
                        }
                    }
                }
            }

            // ── 圆：圆内网格采样 ──
            for (auto *c : circles)
            {
                if (c == nullptr || c->radius <= FLOAT_(0))
                {
                    continue;
                }
                const Vec2_ d = pi - c->pos;
                if (ModulusLength(d) > (c->radius + h) * (c->radius + h))
                {
                    continue;
                }
                const FLOAT_ step = std::max(spacingEff, c->radius * FLOAT_(0.5));
                const FLOAT_ wSample = (step * step) / std::max(spacingEff * spacingEff, FLOAT_(1e-6));
                const int nSide = (int)std::ceil(c->radius / step);
                for (int gx = -nSide; gx <= nSide; ++gx)
                {
                    for (int gy = -nSide; gy <= nSide; ++gy)
                    {
                        const Vec2_ pw = c->pos + Vec2_{(FLOAT_)gx * step, (FLOAT_)gy * step};
                        if (Modulus(pw - c->pos) > c->radius)
                        {
                            continue;
                        }
                        if (ModulusLength(pw - pi) < h2)
                        {
                            mBoundary.push_back(BoundarySample{pw, wSample});
                        }
                    }
                }
            }

            mBoundaryOffset[i + 1] = (unsigned int)mBoundary.size();
        }
    }

    void PhysicsLiquid::Update(FLOAT_ time)
    {
        const size_t n = mParticles.size();
        if (mWorld == nullptr || n == 0 || time <= FLOAT_(0))
        {
            return;
        }

        // ─── 参数清洗 ────────────────────────────────────────────────
        // 参数由 ImGui 滑杆实时驱动（且不保证上下限），非法值会让求解器
        // 产生 NaN / 塌缩 / 反向注入能量，这里统一兜住。
        const FLOAT_ h = std::max(param.h, FLOAT_(1e-3));
        const FLOAT_ invH = FLOAT_(1.0) / h;
        const FLOAT_ dt2 = time * time;
        const int iterations = std::clamp(param.iterations, 1, 32);
        const FLOAT_ stiffness = std::max(param.stiffness, FLOAT_(0));
        const FLOAT_ stiffnessNear = std::max(param.stiffnessNear, FLOAT_(0));
        const FLOAT_ restDensity = std::max(param.restDensity, FLOAT_(0));
        const FLOAT_ tension = std::max(param.surfaceTension, FLOAT_(0));
        const FLOAT_ deficitCap = std::max(param.maxDensityDeficit, FLOAT_(0));
        const FLOAT_ viscosity = std::max(param.viscosity, FLOAT_(0));
        const FLOAT_ viscosityQuadratic = std::max(param.viscosityQuadratic, FLOAT_(0));
        const FLOAT_ maxSpeed = std::max(param.maxSpeed, FLOAT_(0));
        const FLOAT_ maxPairD = std::max(param.maxPairDisplacement, FLOAT_(0)) * h;
        // 地图投影的最大推出距离：压力单帧位移必须留在它之内，
        // 否则粒子被挤进实体内部后 NearestFree 找不到自由点 → 永久卡死。
        const FLOAT_ maxStepMap = std::max(param.mapEscapeRadius, FLOAT_(0)) * h;
        FLOAT_ frameShiftCap = std::max(param.maxFrameDisplacement, FLOAT_(0)) * h;
        frameShiftCap = std::min(frameShiftCap, maxStepMap * FLOAT_(0.9));
        // 下限兜底：滑杆被拖到 0 时不应让压力求解完全失效
        frameShiftCap = std::max(frameShiftCap, h * FLOAT_(0.02));
        const FLOAT_ iterShiftCap = frameShiftCap / (FLOAT_)iterations;

        if (mDensity.size() != n || mDensityNear.size() != n || mPrevPos.size() != n ||
            mShift.size() != n || mPreProjPos.size() != n || mProjNormal.size() != n ||
            mProjBodyVel.size() != n || mProjBodyIdx.size() != n)
        {
            const size_t oldPrev = mPrevPos.size();
            const size_t oldPre = mPreProjPos.size();
            mDensity.resize(n, FLOAT_(0));
            mDensityNear.resize(n, FLOAT_(0));
            mShift.resize(n, Vec2_{0, 0});
            mPrevPos.resize(n, Vec2_{0, 0});
            mPreProjPos.resize(n, Vec2_{0, 0});
            mProjNormal.resize(n, Vec2_{0, 0});
            mProjBodyVel.resize(n, Vec2_{0, 0});
            mProjBodyIdx.resize(n, -1);
            // 新条目的回代基准必须是粒子当前位置，绝不能用 {0,0}
            //（否则第一帧会得到一个巨大的假位移）
            for (size_t i = oldPrev; i < n; ++i)
            {
                mPrevPos[i] = mParticles[i]->pos;
            }
            for (size_t i = oldPre; i < n; ++i)
            {
                mPreProjPos[i] = mParticles[i]->pos;
            }
        }

        RebuildNeighbors(h);
        // 流体静息间距（由 h 与 ρ0 反解，标定 C≈0.336）→ 边界采样的子采样步长
        {
            FLOAT_ spacingEff = h * SQRT_(FLOAT_(0.336) / std::max(restDensity, FLOAT_(1e-6)));
            if (!(spacingEff > FLOAT_(1e-3)) || !std::isfinite(spacingEff))
            {
                spacingEff = std::max(mSpacing, FLOAT_(1e-3));
            }
            RebuildBoundarySamples(h, spacingEff);
        }

        // ─── 双密度松弛（Clavet 2005 风格）─────────────────────────
        for (int iter = 0; iter < iterations; ++iter)
        {
            // 1) 密度（当前位置快照）
            //    注意：**边界粒子也计入密度**（Akinci 2012 的做法）。若不记入，固体
            //    占据的空间在密度场里是"空洞"，水会不断往固体内挤、再被投影推回，
            //    形成"贴壁的水以 maxSpeed 弹射、永不收敛"的恶性循环。
            for (size_t i = 0; i < n; ++i)
            {
                const Vec2_ &pi = mParticles[i]->pos;
                FLOAT_ rho = 0;
                FLOAT_ rhoNear = 0;
                for (unsigned int k = mNeighborOffset[i]; k < mNeighborOffset[i + 1]; ++k)
                {
                    const unsigned int j = mNeighborIndex[k];
                    const Vec2_ d = mParticles[j]->pos - pi;
                    const FLOAT_ r = Modulus(d);
                    if (r >= h)
                    {
                        continue;
                    }
                    const FLOAT_ q = FLOAT_(1.0) - r * invH;
                    rho += q * q;
                    rhoNear += q * q * q;
                }
                for (unsigned int k = mBoundaryOffset[i]; k < mBoundaryOffset[i + 1]; ++k)
                {
                    const BoundarySample &b = mBoundary[k];
                    const FLOAT_ r = Modulus(b.pos - pi);
                    if (r >= h)
                    {
                        continue;
                    }
                    const FLOAT_ q = FLOAT_(1.0) - r * invH;
                    // 只补**压缩密度**：近场压力是恒正斥力项，若边界也参与近场，
                    // 壁面一侧的斥力会被放大且没有对称补偿 → 水被持续推向池内，
                    // 形成永不停止的环流。
                    rho += b.weight * q * q;
                }
                mDensity[i] = rho;
                mDensityNear[i] = rhoNear;
            }

            // 2) 位移推进（Jacobi：本迭代内所有粒子读同一份位置快照，
            //    位移先累加到 mShift，最后统一施加 → 结果与粒子索引顺序无关，
            //    消除 Gauss-Seidel 就地写入带来的方向性漂移/条纹）
            std::fill(mShift.begin(), mShift.end(), Vec2_{0, 0});
            for (size_t i = 0; i < n; ++i)
            {
                const FLOAT_ invMi = mParticles[i]->invMass;
                if (invMi == FLOAT_(0))
                {
                    continue;
                }
                // 压缩压力（≥0）
                const FLOAT_ P = stiffness * std::max(mDensity[i] - restDensity, FLOAT_(0));
                // 近场压力（恒为正，抗重叠）
                const FLOAT_ Pnear = stiffnessNear * mDensityNear[i];
                // 内聚力（表面张力）：欠密时吸引（≤0），限幅防塌缩
                const FLOAT_ deficit = std::min(mDensity[i] - restDensity, FLOAT_(0));
                const FLOAT_ Ptension = (tension > FLOAT_(0) && deficit < FLOAT_(0))
                                            ? stiffness * tension * std::max(deficit, -deficitCap)
                                            : FLOAT_(0);
                for (unsigned int k = mNeighborOffset[i]; k < mNeighborOffset[i + 1]; ++k)
                {
                    const unsigned int j = mNeighborIndex[k];
                    const Vec2_ d = mParticles[j]->pos - mParticles[i]->pos;
                    const FLOAT_ r = Modulus(d);
                    if (r < FLOAT_(1e-5) || r >= h)
                    {
                        continue;
                    }
                    const FLOAT_ invMj = mParticles[j]->invMass;
                    const FLOAT_ totalInv = invMi + invMj;
                    if (totalInv <= FLOAT_(0))
                    {
                        continue;
                    }
                    const FLOAT_ q = FLOAT_(1.0) - r * invH;
                    // 正=排斥，负=吸引（内聚项），双向限幅
                    FLOAT_ Dmag = dt2 * (P * q + (Pnear + Ptension) * q * q);
                    Dmag = std::clamp(Dmag, -maxPairD, maxPairD);
                    const Vec2_ D = d * (Dmag / r); // 从 i 指向 j 的位移向量
                    // 质量加权（等质量时各移动 D/2）
                    mShift[j] += D * (invMj / totalInv);
                    mShift[i] -= D * (invMi / totalInv);
                }
                // 边界粒子（固体/地图）：不可移动，只有流体粒子被推开。
                // 只取压力项（P/Pnear），不取内聚力（流体与固体之间没有"表面张力"）。
                for (unsigned int k = mBoundaryOffset[i]; k < mBoundaryOffset[i + 1]; ++k)
                {
                    const BoundarySample &b = mBoundary[k];
                    const Vec2_ d = b.pos - mParticles[i]->pos; // 从 i 指向边界采样点
                    const FLOAT_ r = Modulus(d);
                    if (r < FLOAT_(1e-5) || r >= h)
                    {
                        continue;
                    }
                    const FLOAT_ q = FLOAT_(1.0) - r * invH;
                    FLOAT_ Dmag = dt2 * (P * q + Pnear * q * q);
                    Dmag = std::clamp(Dmag, -maxPairD, maxPairD);
                    // 边界不动 → 流体承担全部位移，方向为"离开边界"
                    mShift[i] -= d * (Dmag / r);
                }
            }
            // 3) 统一施加（单粒子单迭代位移安全钳制）
            for (size_t i = 0; i < n; ++i)
            {
                const FLOAT_ shiftLen = Modulus(mShift[i]);
                if (shiftLen > iterShiftCap)
                {
                    mShift[i] *= (iterShiftCap / shiftLen);
                }
                mParticles[i]->pos += mShift[i];
            }
        }

        // ─── 固体/地图重合解算 ─────────────────────────────────────
        // PBF 压力对固体毫不知情，会把粒子推进方块内部/地面以下；
        // 深穿透 → 碰撞解算的 Baumgarte 偏压产生巨大反弹速度 → 模拟爆炸。
        // 这里把每个粒子钳制到固体与地图之外，穿透量被限制在帧内运动量级。
        ResolveSolidOverlap(time);

        // ─── 速度回代：v = Δx/dt（PBF 标准做法，含重力/碰撞/压力的综合效果）───
        // 关键（Ihmsen 2011 的粒子推出做法）：用**投影前**的位置回代，再只去掉
        // "撞进固体"的法向速度分量。若直接对投影后的位置回代，固定步长推出
        // （0.02 → 2 u/s）与深穿透推出（1~2 单位 → 钳到 maxSpeed）都会变成
        // 每帧注入的假速度 —— 这就是"贴壁/贴底的水一直沸腾、永不收敛"的根源。
        const FLOAT_ invDt = FLOAT_(1.0) / time;
        for (size_t i = 0; i < n; ++i)
        {
            Vec2_ v = (mPreProjPos[i] - mPrevPos[i]) * invDt;
            const FLOAT_ nLen = Modulus(mProjNormal[i]);
            if (nLen > FLOAT_(1e-9))
            {
                const Vec2_ n = mProjNormal[i] / nLen;
                const int bi = mProjBodyIdx[i];
                PhysicsAngle *body = (bi >= 0 && bi < (int)mReactionBodies.size())
                                         ? mReactionBodies[(size_t)bi] : nullptr;
                // 接触点 ≈ 投影前的位置；力臂从刚体质心算起
                const Vec2_ arm = mPreProjPos[i] - ((body != nullptr) ? body->pos : Vec2_{0, 0});
                // 刚体在接触点处的速度：**必须带上自转**，否则自转刚体给水的参考速度
                // 是错的（水会被按质心速度拖拽，形成假的"搅拌"泵）
                Vec2_ vBody = mProjBodyVel[i];
                if (body != nullptr && body->invMomentInertia > FLOAT_(0))
                {
                    vBody += Cross(body->angleSpeed, arm);
                }
                Vec2_ vRel = v - vBody;
                const FLOAT_ vn = Dot(vRel, n);
                if (vn < FLOAT_(0))
                {
                    // 水被推向"远离刚体"的一侧，获得动量 −m_p·n·vn；
                    // 反作用等大反向还给刚体（**速度型**：量级天然有界，不会像 Δx/dt
                    // 那样恒被限幅钳死）。旧实现只算力矩、还算错量级，线性反作用全丢。
                    if (body != nullptr && body->invMass > FLOAT_(0) && bi < (int)mReactionJ.size())
                    {
                        const Vec2_ J = n * (mParticles[i]->mass * vn); // vn < 0 → J 指向 −n
                        mReactionJ[(size_t)bi] += J;
                        mReactionT[(size_t)bi] += Cross(arm, J);
                    }
                    vRel -= n * vn; // 位置修正不产生"弹射"速度，只保留切向滑动
                }
                v = vBody + vRel;
            }
            if (maxSpeed <= FLOAT_(0))
            {
                v = Vec2_{0, 0};
            }
            else
            {
                const FLOAT_ len = Modulus(v);
                if (len > maxSpeed && len > FLOAT_(1e-9))
                {
                    v *= (maxSpeed / len);
                }
            }
            mParticles[i]->speed = v;
        }

        // ─── 接触反作用结算到刚体（速度型 + 单帧限幅）─────────────────────
        // 旧实现把投影位移当冲量（J = −m_p·Δx/dt）再乘 0.08 增益：
        //   · 量级比 maxTorqueImpulse 大几十倍 → 力矩**恒被钳死**，等价于"幅值恒定、
        //     方向逐帧翻转"的力矩 → 刚体在水里一直摇/自旋，永远静不下来；
        //   · 只作用于形状、且只算力矩 → 水的动量是白拿的（刚体撞水不减速）。
        // 现在：
        //   ① 用**相对速度**算动量交换（含刚体自转在接触点的速度）→ 量级与速度同阶，
        //      天然有界、不饱和，本质是一个阻尼项；
        //   ② 线性 + 角都给 → 水既能推着刚体走（水流推动漂浮物），也阻尼它的平移/自转；
        //   ③ 单帧 Δv/Δω 限幅 → 多接触点叠加也不会变成"撞墙式"急停。
        //   ④ 只对"水正在接近刚体"（vn < 0）的接触生效：静止平衡时反作用为 0，
        //      不会给静止的浮体注入噪声。
        {
            const FLOAT_ gainLin = std::max(param.reactionDragGain, FLOAT_(0));
            const FLOAT_ gainAng = std::max(param.reactionTorqueGain, FLOAT_(0));
            const FLOAT_ maxDv = std::max(param.maxReactionSpeed, FLOAT_(0));
            const FLOAT_ maxDw = std::max(param.maxTorqueImpulse, FLOAT_(0));
            for (size_t b = 0; b < mReactionBodies.size(); ++b)
            {
                PhysicsAngle *body = mReactionBodies[b];
                if (body == nullptr || body->invMass <= FLOAT_(0))
                {
                    continue; // 静态/运动学刚体不接受反作用
                }
                if (maxDv > FLOAT_(0) && gainLin > FLOAT_(0) &&
                    ModulusLength(mReactionJ[b]) > FLOAT_(0))
                {
                    Vec2_ dv = mReactionJ[b] * (gainLin * body->invMass);
                    const FLOAT_ len = Modulus(dv);
                    if (len > maxDv)
                    {
                        dv *= (maxDv / len);
                    }
                    body->speed += dv;
                    body->StaticNum = 0;
                }
                if (maxDw > FLOAT_(0) && gainAng > FLOAT_(0) &&
                    body->invMomentInertia > FLOAT_(0) && mReactionT[b] != FLOAT_(0))
                {
                    FLOAT_ dw = mReactionT[b] * (gainAng * body->invMomentInertia);
                    dw = std::clamp(dw, -maxDw, maxDw);
                    body->angleSpeed += dw;
                    body->StaticNum = 0;
                }
            }
            // 本帧结算完立即清空：ResolveSolidOverlap 每帧重建，
            // 清掉可避免它提前返回（n==0）时把上一帧的反作用重复施加一次
            mReactionBodies.clear();
            mReactionJ.clear();
            mReactionT.clear();
        }

        // ─── 粘滞（线性 + 二次）：成对消耗相对接近速度 ───────────────────
        // 注意：邻居表是双向的，每对 (i,j) 会被访问两次，等效粘滞是
        // viscosity 的两倍（与 Clavet 原文一致，标定时已包含该系数）。
        for (size_t i = 0; i < n; ++i)
        {
            for (unsigned int k = mNeighborOffset[i]; k < mNeighborOffset[i + 1]; ++k)
            {
                const unsigned int j = mNeighborIndex[k];
                const Vec2_ d = mParticles[j]->pos - mParticles[i]->pos;
                const FLOAT_ r = Modulus(d);
                if (r < FLOAT_(1e-5) || r >= h)
                {
                    continue;
                }
                const Vec2_ rhat = d / r;
                const FLOAT_ u = Dot(mParticles[i]->speed - mParticles[j]->speed, rhat);
                if (u <= FLOAT_(0))
                {
                    continue; // 正在远离，无粘滞耗散
                }
                const FLOAT_ q = FLOAT_(1.0) - r * invH;
                const FLOAT_ I = time * q * (viscosity * u + viscosityQuadratic * u * u);
                const Vec2_ impulse = rhat * (I * FLOAT_(0.5));
                mParticles[i]->speed -= impulse;
                mParticles[j]->speed += impulse;
            }
        }

        // ─── 可选速度平滑（XSPH 风格）：对邻居速度做核加权平均 ─────────────
        // 专治"贴壁/贴底的水一直抖"。默认 0 = 关闭（保持原有手感）；
        // 觉得水面静不下来时按 0.1 / 0.2 / 0.3 逐档加。
        const FLOAT_ smoothing = std::clamp(param.velocitySmoothing, FLOAT_(0), FLOAT_(1));
        if (smoothing > FLOAT_(0))
        {
            for (size_t i = 0; i < n; ++i)
            {
                mShift[i] = mParticles[i]->speed; // 速度快照（Jacobi 式，避免顺序相关）
            }
            for (size_t i = 0; i < n; ++i)
            {
                Vec2_ dv{0, 0};
                FLOAT_ wsum = FLOAT_(0);
                for (unsigned int k = mNeighborOffset[i]; k < mNeighborOffset[i + 1]; ++k)
                {
                    const unsigned int j = mNeighborIndex[k];
                    const FLOAT_ r = Modulus(mParticles[i]->pos - mParticles[j]->pos);
                    if (r >= h)
                    {
                        continue;
                    }
                    const FLOAT_ q = FLOAT_(1.0) - r * invH;
                    const FLOAT_ w = q * q;
                    // 必须读快照 mShift，不能读 mParticles[?]->speed：
                    // 后者会让先处理的粒子污染后处理粒子的邻域（Gauss-Seidel，结果与顺序相关）
                    dv += (mShift[j] - mShift[i]) * w;
                    wsum += w;
                }
                if (wsum > FLOAT_(1e-9))
                {
                    mParticles[i]->speed += dv * (smoothing / wsum);
                }
            }
        }

        // ─── 固体↔液体耦合：浮力 + 阻力 ─────────────────────────────
        ApplySolidCoupling(time);

        // ─── 记录本次修正后的位置（下一帧速度回代基准）──────────────────
        for (size_t i = 0; i < n; ++i)
        {
            mPrevPos[i] = mParticles[i]->pos;
        }
    }

    glm::vec4 PhysicsLiquid::ColorByDensity(FLOAT_ density, const Params &param)
    {
        const FLOAT_ low = param.restDensity * FLOAT_(0.7);
        const FLOAT_ high = param.restDensity * FLOAT_(3.0);
        const FLOAT_ span = high - low;
        FLOAT_ t = FLOAT_(0);
        if (span > FLOAT_(1e-6))
        {
            t = (density - low) / span;
        }
        t = std::clamp(t, FLOAT_(0), FLOAT_(1));
        // 深蓝(低压/稀疏) → 亮蓝白(高压/压缩)
        return glm::vec4(
            FLOAT_(0.06) + FLOAT_(0.80) * t,
            FLOAT_(0.40) + FLOAT_(0.55) * t,
            FLOAT_(0.90) + FLOAT_(0.10) * t,
            FLOAT_(0.90));
    }

    void PhysicsLiquid::ResolveSolidOverlap(FLOAT_ time)
    {
        const size_t n = mParticles.size();
        if (n == 0 || time <= FLOAT_(0))
        {
            return;
        }

        MapFormwork *map = mWorld->GetMapFormwork();
        const auto &shapes = mWorld->PhysicsShapeS;
        const auto &circles = mWorld->PhysicsCircleS;
        const auto &lines = mWorld->PhysicsLineS;

        const FLOAT_ h = std::max(param.h, FLOAT_(1e-3));
        const FLOAT_ maxStepMap = std::max(param.mapEscapeRadius, FLOAT_(0)) * h;
        const Vec2_ stepDirs[8] = {
            {0, 1}, {0, -1}, {1, 0}, {-1, 0},
            {FLOAT_(0.7071), FLOAT_(0.7071)}, {FLOAT_(0.7071), FLOAT_(-0.7071)},
            {FLOAT_(-0.7071), FLOAT_(0.7071)}, {FLOAT_(-0.7071), FLOAT_(-0.7071)}
        };

        // 粗扫 + 二分细化，求**精确穿透深度**的最近空闲点：
        //   1) 粗扫（≈12 步）先定位能逃逸的射线，得到"最后一个实心点/第一个空闲点"区间；
        //   2) 在区间内二分到 tol，得到几乎精确的边界距离。
        //   精确性对稳定性至关重要：固定步长（例如 0.02）会让静止粒子每次接触都被
        //   多推一个步长，位置抖动且速度回代被注入假速度。
        //   preferDir 非零时，优先只考虑"朝来路反方向"的射线，避免沿最近路径
        //   推穿薄壁（隧穿）；若该半平面内无解则回退到全部方向。
        auto NearestFree = [&](Vec2_ from, auto &&IsFree, FLOAT_ maxLen, Vec2_ preferDir) -> Vec2_
        {
            if (maxLen <= FLOAT_(1e-5))
            {
                return from;
            }
            const FLOAT_ coarseStep = std::max(FLOAT_(0.05), maxLen / FLOAT_(12.0));
            const FLOAT_ tol = FLOAT_(1e-4);
            const FLOAT_ preferLen = Modulus(preferDir);
            const Vec2_ prefer = (preferLen > FLOAT_(1e-6)) ? (preferDir / preferLen) : Vec2_{0, 0};
            const bool hasPrefer = (preferLen > FLOAT_(1e-6));

            Vec2_ best = from;
            FLOAT_ bestLen = FLOAT_MAX;
            // 两轮：0 = 只考虑顺来路的半平面（有解则采用），1 = 全部方向兜底
            for (int phase = 0; phase < 2; ++phase)
            {
                for (int di = 0; di < 8; ++di)
                {
                    if (hasPrefer && phase == 0 && Dot(stepDirs[di], prefer) <= FLOAT_(0.2))
                    {
                        continue; // 会朝来路继续深入的方向，第一轮排除
                    }
                    const Vec2_ dir = stepDirs[di];
                    FLOAT_ lo = 0;  // 已知实心（含起点）
                    FLOAT_ hi = -1; // 已知空闲
                    for (FLOAT_ len = coarseStep; len <= maxLen + FLOAT_(1e-4); len += coarseStep)
                    {
                        if (IsFree(from + dir * len))
                        {
                            hi = len;
                            break;
                        }
                        lo = len;
                    }
                    if (hi < 0)
                    {
                        continue; // 该方向射程内无空闲点
                    }
                    for (int it = 0; it < 24 && (hi - lo) > tol; ++it)
                    {
                        const FLOAT_ mid = (lo + hi) * FLOAT_(0.5);
                        if (IsFree(from + dir * mid))
                        {
                            hi = mid;
                        }
                        else
                        {
                            lo = mid;
                        }
                    }
                    if (hi < bestLen)
                    {
                        bestLen = hi;
                        best = from + dir * hi;
                    }
                }
                if (bestLen < FLOAT_MAX)
                {
                    break;
                }
            }
            return bestLen < FLOAT_MAX ? best : from;
        };

        // 接触反作用的刚体表（形状在前、圆在后）与其冲量缓冲：
        // 每个粒子在投影时记录"是谁推的"（mProjBodyIdx），速度回代时按相对速度
        // 把水获得的动量等大反向还给刚体（见 Update 的速度回代段）。
        mReactionBodies.clear();
        mReactionBodies.reserve(shapes.size() + circles.size());
        for (auto *s : shapes)
        {
            mReactionBodies.push_back(s);
        }
        for (auto *c : circles)
        {
            mReactionBodies.push_back(c);
        }
        mReactionJ.assign(mReactionBodies.size(), Vec2_{0, 0});
        mReactionT.assign(mReactionBodies.size(), FLOAT_(0));

        for (size_t i = 0; i < n; ++i)
        {
            PhysicsParticle *p = mParticles[i];
            Vec2_ pos = p->pos;
            // 逃逸偏好方向 = 本帧进入解算前的来路反向（把它推回出发的那一侧）
            const Vec2_ preferDir = mPrevPos[i] - pos;
            // 投影前的位置与接触信息：速度回代用"投影前位置"，接触法线用于碰撞响应
            mPreProjPos[i] = pos;
            mProjNormal[i] = Vec2_{0, 0};
            mProjBodyVel[i] = Vec2_{0, 0};
            mProjBodyIdx[i] = -1; // -1 = 地图/线/无（不产生接触反作用）

            // ── 单粒子投影：固体（圆/形状/线）与地图交替迭代至稳定 ──
            // 顺序很关键：若先推地图再推固体，固体可能把粒子重新压回地图格子里
            //（典型场景：方块/木板压在池底），表现为池底粒子跨帧抖动。
            for (int pass = 0; pass < 2; ++pass)
            {
                const Vec2_ before = pos;

                // 圆形固体：与形状/地图同样的"最近空闲点 + 来路偏好"推出。
                // 旧实现直接径向瞬移到 radius+0.01：粒子最坏会被移动 2R（一帧），
                // 而且**专挑最近的一侧**——从下方钻进去的粒子会被往下方推穿地板，
                // 再和地图投影来回对拉，是"贴着球的水疯狂抖动/被弹飞"的主因。
                for (size_t ci = 0; ci < circles.size(); ++ci)
                {
                    auto *c = circles[ci];
                    if (c == nullptr)
                    {
                        continue;
                    }
                    if (Modulus(pos - c->pos) >= c->radius)
                    {
                        continue;
                    }
                    const FLOAT_ maxStepCircle = FLOAT_(2) * c->radius + FLOAT_(0.01);
                    const Vec2_ newPos = NearestFree(
                        pos,
                        [&](const Vec2_ &cand) { return Modulus(cand - c->pos) >= c->radius; },
                        maxStepCircle,
                        preferDir);
                    if (newPos != pos)
                    {
                        mProjNormal[i] += (newPos - pos);
                        mProjBodyVel[i] = c->speed;
                        mProjBodyIdx[i] = (int)(shapes.size() + ci);
                    }
                    pos = newPos;
                }

                // 网格形状固体：逐格判定（无 DropCollision 的越界钳制！）
                // 注意：PhysicsShape::DropCollision 会把越界坐标钳制到边界格（边界修正），
                // 而实心形状的边界格恒为"碰撞" → 任何点都判定为在形状内部 →
                // 最近的"空闲点"搜索永远失败 → 粒子困在方块内部 → 仲裁器深穿透 bias 猛踢方块。
                // 因此这里必须用带越界检查的逐格测试（越界 = 空闲）——见 ShapeSolidAt。
                for (size_t si = 0; si < shapes.size(); ++si)
                {
                    auto *s = shapes[si];
                    if (s == nullptr)
                    {
                        continue;
                    }
                    const FLOAT_ maxStepShape = s->radius + FLOAT_(0.01);
                    const Vec2_ d = pos - s->pos;
                    if (Modulus(d) > s->radius + maxStepShape)
                    {
                        continue; // 远距粗筛
                    }
                    // 该形状本帧的旋转量缓存一次：ShapeSolidAt 在 NearestFree 里每个方向
                    // 要调用十几次，函数内部重算 cos/sin 会让这段成为主要热点
                    const FLOAT_ cosA = std::cos(s->angle);
                    const FLOAT_ sinA = std::sin(s->angle);
                    if (!ShapeSolidAt(s, pos, cosA, sinA))
                    {
                        continue;
                    }
                    const Vec2_ newPos = NearestFree(
                        pos,
                        [&](const Vec2_ &cand) { return !ShapeSolidAt(s, cand, cosA, sinA); },
                        maxStepShape,
                        preferDir);
                    if (newPos != pos)
                    {
                        mProjNormal[i] += (newPos - pos);
                        mProjBodyVel[i] = s->speed;
                        mProjBodyIdx[i] = (int)si; // 接触反作用在速度回代段统一结算
                    }
                    pos = newPos;
                }

                // 线（PhysicsLine）：零厚度线段的**扫掠**穿越检测。
                // 引擎的 LP 仲裁器同样只判"旧位置→新位置是否穿过线段"，
                // 这里保持同语义：一旦穿越，把粒子拉回穿越前所在的一侧。
                for (size_t li = 0; li < lines.size(); ++li)
                {
                    auto *l = lines[li];
                    if (l == nullptr || l->radius <= FLOAT_(0))
                    {
                        continue;
                    }
                    const Vec2_ ldir = vec2angle(Vec2_{l->radius, 0}, l->angle);
                    const Vec2_ a = l->pos + ldir;
                    const Vec2_ b = l->pos - ldir;
                    const Vec2_ ab = b - a;
                    const FLOAT_ abLen2 = ModulusLength(ab);
                    if (abLen2 <= FLOAT_(1e-12))
                    {
                        continue;
                    }
                    const FLOAT_ s0 = Cross(ab, mPrevPos[i] - a);
                    const FLOAT_ s1 = Cross(ab, pos - a);
                    if (s0 * s1 >= FLOAT_(0))
                    {
                        continue; // 未穿越（含同侧/贴线）
                    }
                    const FLOAT_ t = std::clamp(Dot(pos - a, ab) / abLen2, FLOAT_(0), FLOAT_(1));
                    if (t <= FLOAT_(0) || t >= FLOAT_(1))
                    {
                        continue; // 穿越点落在线段之外 → 不算与这条线相交
                    }
                    const Vec2_ closest = a + ab * t;
                    // 朝向"穿越前所在的一侧"的法线
                    const Vec2_ n = (s0 > FLOAT_(0)) ? Vec2_{-ab.y, ab.x} : Vec2_{ab.y, -ab.x};
                    const FLOAT_ nLen = Modulus(n);
                    if (nLen > FLOAT_(1e-9))
                    {
                        const Vec2_ newPos = closest + n * (FLOAT_(1e-3) / nLen);
                        mProjNormal[i] += (newPos - pos);
                        mProjBodyVel[i] = l->speed;
                        pos = newPos;
                    }
                }

                // ── 地图：把粒子钳制出碰撞格子（精确最近空闲点，保持切向滑动）──
                if (map != nullptr && map->FMGetCollide(pos))
                {
                    const Vec2_ newPos = NearestFree(
                        pos,
                        [&](const Vec2_ &cand) { return !map->FMGetCollide(cand); },
                        maxStepMap,
                        preferDir);
                    if (newPos != pos)
                    {
                        mProjNormal[i] += (newPos - pos);
                        mProjBodyVel[i] = Vec2_{0, 0}; // 地图是静态的
                    }
                    pos = newPos;
                }

                if (pos == before)
                {
                    break; // 已稳定，无需第二轮
                }
            }

            p->pos = pos;
        }
        // 接触反作用不在这里施加：位置投影不知道速度，任何"用 Δx/dt 当冲量"的写法
        // 都会比限幅大几十倍（恒被钳死 = 方向逐帧翻转的恒力矩）。真正的结算放在
        // Update 的速度回代段（见"接触反作用结算到刚体"）。
    }

    bool PhysicsLiquid::ShapeSolidAt(PhysicsShape *s, const Vec2_ &cand, FLOAT_ cosA, FLOAT_ sinA) const
    {
        // 等价于 vec2angle(cand - s->pos, -s->angle) + s->CentreMass，
        // 但 cos/sin 由调用方按形状缓存（见声明处注释）。
        // vec2angle(v, -a) = (v.x·cosA + v.y·sinA, -v.x·sinA + v.y·cosA)
        const Vec2_ d = cand - s->pos;
        const Vec2_ local{(d.x * cosA) + (d.y * sinA), -(d.x * sinA) + (d.y * cosA)};
        const glm::ivec2 g = ToInt(local + s->CentreMass);
        if (g.x < 0 || g.y < 0 || g.x >= (int)s->width || g.y >= (int)s->height)
        {
            return false; // 越界即空闲
        }
        return s->at(g.x, g.y).Collision;
    }

    bool PhysicsLiquid::QuerySurfaceLevel(Vec2_ pos, FLOAT_ &outLevel) const
    {
        // 调试/测试接口：返回该水平位置附近最高液体粒子的高度。
        // 浮力本身不用它（见 ApplySolidCoupling 的表面压强积分）。
        outLevel = 0;
        if (mWorld == nullptr || mBinNum <= 0)
        {
            return false;
        }
        const FLOAT_ g = Modulus(mWorld->GravityAcceleration);
        if (g < FLOAT_(1e-6))
        {
            return false;
        }
        const Vec2_ up = -mWorld->GravityAcceleration / g;
        const Vec2_ right = Vec2_{up.y, -up.x};
        const FLOAT_ level = SurfaceAt(Dot(pos, right));
        if (level <= mNoWater)
        {
            return false;
        }
        outLevel = level;
        return true;
    }

    bool PhysicsLiquid::InsideDynamicSolid(const Vec2_ &pos) const
    {
        if (mWorld == nullptr)
        {
            return false;
        }
        for (auto *c : mWorld->PhysicsCircleS)
        {
            if (c != nullptr && Modulus(pos - c->pos) < c->radius)
            {
                return true;
            }
        }
        for (auto *s : mWorld->PhysicsShapeS)
        {
            if (s == nullptr)
            {
                continue;
            }
            // 粗筛：形状的外接圆
            const Vec2_ d = pos - s->pos;
            if (ModulusLength(d) > (s->radius + FLOAT_(0.5)) * (s->radius + FLOAT_(0.5)))
            {
                continue;
            }
            if (ShapeSolidAt(s, pos, std::cos(s->angle), std::sin(s->angle)))
            {
                return true;
            }
        }
        return false;
    }

    FLOAT_ PhysicsLiquid::BinTopAt(int bin) const
    {
        if (bin < 0 || bin >= mBinNum)
        {
            return mNoWater;
        }
        const SurfaceBin &b = mBins[(size_t)bin];
        return b.valid ? b.top : mNoWater;
    }

    FLOAT_ PhysicsLiquid::SurfaceAt(FLOAT_ u) const
    {
        if (mBinNum <= 0 || !(mBinWidth > FLOAT_(0)))
        {
            return mNoWater;
        }
        const FLOAT_ f = (u - mBinOrigin) / mBinWidth;
        // 必须用 floor 而不是 (int)：u 略小于 mBinOrigin 时 f ∈ (-1,0)，
        // (int) 截断会得到 0 并通过下面的范围检查 → 水域**左侧**多出一条
        // 宽度 = 一个桶的"幽灵水面"（岸上/池壁外的固体被判成浸没），
        // 而且 frac 变负会去查错的邻居。右侧没这个问题，于是左右还不对称。
        const int bi = (int)std::floor(f);
        if (bi < 0 || bi >= mBinNum)
        {
            return mNoWater; // 该水平坐标超出水面范围 → 无水
        }
        const FLOAT_ here = BinTopAt(bi);
        if (here <= mNoWater)
        {
            return mNoWater; // 该水平区域没有实质水体
        }
        const FLOAT_ frac = f - (FLOAT_)bi; // 桶内位置 [0,1)
        if (frac < FLOAT_(0.5))
        {
            const FLOAT_ prev = BinTopAt(bi - 1);
            if (prev > mNoWater)
            {
                return prev + (here - prev) * (frac + FLOAT_(0.5));
            }
        }
        else
        {
            const FLOAT_ next = BinTopAt(bi + 1);
            if (next > mNoWater)
            {
                return here + (next - here) * (frac - FLOAT_(0.5));
            }
        }
        return here;
    }

    void PhysicsLiquid::ApplySolidCoupling(FLOAT_ time)
    {
        if (mWorld == nullptr || mParticles.empty())
        {
            return;
        }
        const FLOAT_ h = std::max(param.h, FLOAT_(1e-3));
        const FLOAT_ g = Modulus(mWorld->GravityAcceleration);
        if (g < FLOAT_(1e-6) || mParticleMass <= FLOAT_(0))
        {
            return;
        }
        const Vec2_ up = -mWorld->GravityAcceleration / g; // 浮力方向 = 反重力方向

        // 液体面密度：由 h 与 restDensity 反解静息间距，而不是沿用"添加时刻"的
        // mSpacing——否则运行时调整 h/restDensity 后，液体的真实静息间距已变，
        // 浮力基准却还停在旧值，浮沉会整体错位。
        // 标定：均匀排布下核密度 Σ(1−r/h)² ≈ C·(h/s)²，C≈0.336
        //      （h=1、s=0.5 时 Σ≈1.343，与默认 restDensity=1.35 自洽）。
        const FLOAT_ restDensity = std::max(param.restDensity, FLOAT_(1e-6));
        const FLOAT_ kernelC = FLOAT_(0.336);
        FLOAT_ spacingEff = h * SQRT_(kernelC / restDensity);
        if (!(spacingEff > FLOAT_(1e-3)) || !std::isfinite(spacingEff))
        {
            spacingEff = std::max(mSpacing, FLOAT_(1e-3)); // 兜底
        }
        const FLOAT_ rhoLiquid = mParticleMass / (spacingEff * spacingEff);

        // ── 局部水面高度场（沿水平轴分桶）────────────────────────────
        // 液面不再是"一个全局平面"：把液体粒子按垂直于重力的水平轴分桶，
        // 每桶记录该水平区间的水面高度。固体逐格/逐圆取自己所在桶的水面做水线
        // 裁剪 → 悬在池壁外、岸上、或跨越两个水池的部分各用各的水面，
        // 不会因为"全局水位"把干燥部分算成浸没。
        //
        // 只统计"实质水体"：先按高度分簇，剔除粒子数过少的簇（被固体带起来的
        // 小水花），否则一块木板可以踩着自己带起的水悬浮。
        // gapTh 改为随 h 缩放：旧的硬下限 1.0 在 h=0.25 时相当于 8 个粒子间距，
        // 分簇/水柱连续性判据在细网格下会失效（h=1 时与旧行为完全一致）。
        const FLOAT_ gapTh = std::max(h, spacingEff * FLOAT_(2.0));
        const size_t clusterMinCount = std::max<size_t>(2, mParticles.size() / 50);
        const Vec2_ right = Vec2_{up.y, -up.x}; // 水平轴（垂直于重力）
        const size_t n = mParticles.size();

        // 1) 高度缓存 + 高度降序索引
        mHeightOf.resize(n);
        mOrder.resize(n);
        // 全体粒子的水平范围（只作兜底；真正参与分桶的范围见步骤 3，只统计实质水体）
        FLOAT_ uMinAll = std::numeric_limits<FLOAT_>::max();
        FLOAT_ uMaxAll = -std::numeric_limits<FLOAT_>::max();
        for (size_t i = 0; i < n; ++i)
        {
            mHeightOf[i] = Dot(mParticles[i]->pos, up);
            mOrder[i] = (unsigned int)i;
            const FLOAT_ u = Dot(mParticles[i]->pos, right);
            if (u < uMinAll) uMinAll = u;
            if (u > uMaxAll) uMaxAll = u;
        }
        std::sort(mOrder.begin(), mOrder.end(),
                  [&](unsigned int a, unsigned int b) { return mHeightOf[a] > mHeightOf[b]; });

        // 2) 全局分簇（高度间隙 > gapTh 切开）→ 标记"实质水体"粒子。
        //    只有"够大"的簇参与水面高度场：主簇（粒子数最多）以及规模不小于
        //    主簇 1/6 的其他簇（真正的第二个水池）。被浮体抬起的水、飞溅水柱、
        //    小水洼里被带起的水花都远小于该门槛 → 不参与 → 不会把水面抬起来
        //    （否则浮体会踩着自己带起的水一路升空：实测正反馈）。
        mEligible.assign(n, (unsigned char)0);
        {
            mClusterCount.clear();
            size_t clusterStart = 0;
            size_t mainCount = 0;
            for (size_t i = 1; i <= n; ++i)
            {
                const bool cut = (i == n) || (mHeightOf[mOrder[i - 1]] - mHeightOf[mOrder[i]] > gapTh);
                if (!cut)
                {
                    continue;
                }
                const size_t cnt = i - clusterStart;
                mClusterCount.push_back(cnt);
                if (cnt > mainCount)
                {
                    mainCount = cnt;
                }
                clusterStart = i;
            }
            const size_t eligibleMin = std::max(clusterMinCount, mainCount / 6);
            clusterStart = 0;
            size_t ci = 0;
            for (size_t i = 1; i <= n; ++i)
            {
                const bool cut = (i == n) || (mHeightOf[mOrder[i - 1]] - mHeightOf[mOrder[i]] > gapTh);
                if (!cut)
                {
                    continue;
                }
                if (mClusterCount[ci] >= eligibleMin)
                {
                    for (size_t k = clusterStart; k < i; ++k)
                    {
                        mEligible[mOrder[k]] = 1;
                    }
                }
                ++ci;
                clusterStart = i;
            }
        }

        // 3) 桶划分：桶宽默认 ≈ max(h, 2×静息间距)（默认参数下 = 1 个地图格），
        //    也可用 param.surfaceBinWidth 显式指定；粒子铺得很开时限桶数，
        //    避免超大水面的内存/耗时爆炸。
        //    **水平范围只用"实质水体"粒子统计**（步骤 2 的 mEligible）：否则一颗飞出去
        //    的水花就能把跨度拉大 → 触发 2048 桶上限 → 整池水面分辨率被拖粗，
        //    而且桶原点会跟着水花乱跳（步骤 7 的时间低通会被搅乱）。
        FLOAT_ uMin = uMinAll;
        FLOAT_ uMax = uMaxAll;
        {
            FLOAT_ eMin = std::numeric_limits<FLOAT_>::max();
            FLOAT_ eMax = -std::numeric_limits<FLOAT_>::max();
            for (size_t i = 0; i < n; ++i)
            {
                if (mEligible[i] == 0)
                {
                    continue;
                }
                const FLOAT_ u = Dot(mParticles[i]->pos, right);
                if (u < eMin) eMin = u;
                if (u > eMax) eMax = u;
            }
            if (eMax >= eMin)
            {
                uMin = eMin;
                uMax = eMax;
            }
            // 没有合格水体时退回全体粒子的范围（此时没有任何桶会 valid，几何仍自洽）
        }
        const FLOAT_ binWanted = (param.surfaceBinWidth > FLOAT_(0))
                                      ? param.surfaceBinWidth
                                      : std::max(h, spacingEff * FLOAT_(2.0));
        mBinWidth = std::max(binWanted, FLOAT_(1e-3));
        const int kMaxBins = 2048;
        // 桶网格锚定到世界坐标的固定栅格：origin 只随 uMin 做"整桶"跳变。
        // 旧实现 mBinOrigin = uMin 会逐帧连续漂移，而步骤 7 的时间低通是按桶下标
        // 存的 → 每帧都在混用"不同水平位置"的历史水面。
        auto AnchorOrigin = [](FLOAT_ u, FLOAT_ w) { return std::floor(u / w) * w; };
        mBinOrigin = AnchorOrigin(uMin, mBinWidth);
        FLOAT_ uSpan = std::max(uMax - mBinOrigin, FLOAT_(0));
        if (uSpan > FLOAT_(0) && uSpan / mBinWidth > (FLOAT_)kMaxBins)
        {
            mBinWidth = uSpan / (FLOAT_)kMaxBins;
            mBinOrigin = AnchorOrigin(uMin, mBinWidth);
            uSpan = std::max(uMax - mBinOrigin, FLOAT_(0));
        }
        mBinNum = (uSpan > FLOAT_(0)) ? ((int)(uSpan / mBinWidth) + 1) : 1;
        if (mBinNum < 1)
        {
            mBinNum = 1;
        }
        mBins.clear();
        mBins.resize((size_t)mBinNum);

        // 4) 识别"自由水面"粒子。
        //    自由水面 = ① 正上方 probe 内没有水、也没有动态刚体（真正暴露在空气中）；
        //               ② **所在桶内**下方有连续 ≥minChain 层水（且有半层以上的竖直
        //                  跨度），且该桶水柱根部不在动态刚体上；
        //               ③ 其正下方一段竖直范围内没有动态刚体（见 ColumnClearOfSolids）。
        //    这三条把"被浮体压住的水"（上方是刚体）、"堆在浮体顶面的水"（水柱根在
        //    刚体上）、"浮体正下方的深水"（上方是水）全部排除；这些桶随后由邻居水面
        //    填充。否则浮体会踩着自己推到身上的水不断抬高水面，形成正反馈
        //   （实测：木板会一路升空，只能靠 maxRiseSpeed 压住）。
        const FLOAT_ probe = std::max(spacingEff * FLOAT_(1.5), gapTh * FLOAT_(0.5));
        // 候选水面有效性：从候选粒子**向下** candidateDepth 的竖直段内不得有动态刚体。
        // 这一条专门干掉"被投影推到刚体顶面上的那层水"：它上方是空气、下方也有水（自身
        // 就是水柱），但正下方 1~2 个粒子层处就是刚体 → 判为被刚体托着 → 不算自由水面。
        const FLOAT_ candidateDepth = std::max(h * FLOAT_(1.5), spacingEff * FLOAT_(3.0));
        const FLOAT_ columnSampleStep = std::max(spacingEff * FLOAT_(0.5), FLOAT_(0.05));
        const unsigned int minChain = 2;
        // "≥2 层连续水柱"的竖直跨度判据：chain 数的是**粒子数**，同一高度的两颗粒子
        // （一层水膜）也会让 chain=2，所以再要求水柱自身有至少半层的竖直跨度。
        const FLOAT_ minColumnDepth = std::max(spacingEff * FLOAT_(0.5), FLOAT_(1e-3));
        auto ColumnClearOfSolids = [&](const Vec2_ &p) -> bool
        {
            for (FLOAT_ d = columnSampleStep; d <= candidateDepth; d += columnSampleStep)
            {
                if (InsideDynamicSolid(p - up * d))
                {
                    return false;
                }
            }
            return true;
        };

        // 4a) 降序扫描：标记"暴露在空气中"的粒子（本桶内其正上方 probe 内没有水，
        //     且正上方 probe 内没有动态刚体）
        mExposed.assign(n, (unsigned char)0);
        for (size_t k = 0; k < n; ++k)
        {
            const unsigned int pi = mOrder[k];
            if (mEligible[pi] == 0)
            {
                continue;
            }
            const Vec2_ pos = mParticles[pi]->pos;
            const FLOAT_ v = mHeightOf[pi];
            const FLOAT_ u = Dot(pos, right);
            const int bi = (int)((u - mBinOrigin) / mBinWidth);
            if (bi < 0 || bi >= mBinNum)
            {
                continue;
            }
            SurfaceBin &bin = mBins[(size_t)bi];
            const bool waterAbove = bin.hasWater && ((bin.lastV - v) <= probe);
            bin.lastV = v;
            bin.hasWater = true;
            if (!waterAbove && !InsideDynamicSolid(pos + up * probe))
            {
                mExposed[pi] = 1;
            }
        }

        // 4b) 升序扫描（mOrder 逆序）：**按桶**统计水柱层数与根部，选出每桶的水面。
        //     关键：chain / lastVUp / chainBottom / chainOnSolid 必须是**每桶**状态
        //     （SurfaceBin 的字段，不是函数局部变量）。用全局变量时：
        //       - chain 会在整池范围内一路累加 → "≥2 层"永远成立，判据形同虚设；
        //       - chainOnSolid 只由"全池最低的那颗粒子"一次性决定，之后整条链继承它
        //         → 只要那颗粒子坐在某个动态刚体上（例如用 PhysicsShape 拼的水箱、
        //           水面坐落在浮体上），**所有桶都拿不到有效水面 → 全场浮力归零**。
        for (size_t idx = n; idx-- > 0;)
        {
            const unsigned int pi = mOrder[idx];
            if (mEligible[pi] == 0)
            {
                continue;
            }
            const Vec2_ pos = mParticles[pi]->pos;
            const FLOAT_ v = mHeightOf[pi];
            const FLOAT_ u = Dot(pos, right);
            const int bi = (int)((u - mBinOrigin) / mBinWidth);
            if (bi < 0 || bi >= mBinNum)
            {
                continue;
            }
            SurfaceBin &bin = mBins[(size_t)bi];
            bin.hasWater = true;
            if (bin.chain > 0 && (v - bin.lastVUp) <= gapTh)
            {
                ++bin.chain; // 与**该桶**下方粒子连续 → 水柱加高一层
            }
            else
            {
                bin.chain = 1;      // 该桶内的新水柱
                bin.chainBottom = v; // 柱子根部高度
                bin.chainOnSolid = InsideDynamicSolid(pos - up * probe);
            }
            bin.lastVUp = v;
            // 自由水面 = ① 该桶内下方有 ≥2 层连续水（用竖直跨度兜住"同高度两颗粒子
            //               只算一层"的情况，即真正的薄水膜不算水面）
            //            ② 该桶水柱根部不坐在动态刚体上
            //            ③ 自身暴露在空气中 ④ 正下方一段范围内没有动态刚体
            const FLOAT_ columnDepth = v - bin.chainBottom;
            if (bin.chain >= minChain && columnDepth >= minColumnDepth &&
                !bin.chainOnSolid && mExposed[pi] != 0 && ColumnClearOfSolids(pos))
            {
                if (!bin.valid || v > bin.top)
                {
                    bin.top = v;
                    bin.valid = true;
                    bin.topIdx = pi;
                }
            }
        }

        // 5) 填充：桶里有水但找不到自由水面（水面被浮体挡住/压住，例如木板正下方的水）
        //    → 借用左右最近一个有效桶的水面。**完全没水的桶不会被填充**
        //    （这正是"悬在池壁外/岸上的部分没有水面"的来源）。
        {
            int src = -1;
            for (int i = 0; i < mBinNum; ++i)
            {
                if (mBins[(size_t)i].valid)
                {
                    src = i;
                }
                else if (src >= 0 && mBins[(size_t)i].hasWater)
                {
                    mBins[(size_t)i].top = mBins[(size_t)src].top;
                    mBins[(size_t)i].valid = true;
                }
            }
            src = -1;
            for (int i = mBinNum - 1; i >= 0; --i)
            {
                if (mBins[(size_t)i].valid)
                {
                    src = i;
                }
                else if (src >= 0 && mBins[(size_t)i].hasWater)
                {
                    mBins[(size_t)i].top = mBins[(size_t)src].top;
                    mBins[(size_t)i].valid = true;
                }
            }
        }

        // 6) 坡度限制：水面不能出现"相邻桶之间超过 maxSlopeStep 的悬崖"。
        //    只对**相邻的有效桶**生效——被池壁隔开的另一个水池中间隔着无水桶，
        //    因此不受影响（多水池/水洼各自的高度仍然保留）。
        //    这一层是兜底：万一还有水被浮体局部抬高，最多也只能高出邻居这么多。
        //    上限从 max(0.5, 2·spacingEff)（默认 = 1 个桶宽 → 允许 45° 斜坡）收到
        //    max(一个静息间距, 1/4 桶宽)：浮体顶开的水被步骤 5 填充到左右邻居后，
        //    原来能连成一条 45° 的假斜坡 → 木板下面出现倾斜水线 → 浸没面积/形心
        //    跟着歪，产生非物理的侧倾力矩。
        {
            const FLOAT_ maxSlopeStep = std::max(spacingEff, mBinWidth * FLOAT_(0.25));
            for (int pass = 0; pass < 16; ++pass)
            {
                bool changed = false;
                for (int i = 0; i < mBinNum; ++i)
                {
                    SurfaceBin &bin = mBins[(size_t)i];
                    if (!bin.valid)
                    {
                        continue;
                    }
                    if (i > 0 && mBins[(size_t)(i - 1)].valid)
                    {
                        const FLOAT_ limit = mBins[(size_t)(i - 1)].top + maxSlopeStep;
                        if (bin.top > limit)
                        {
                            bin.top = limit;
                            changed = true;
                        }
                    }
                    if (i + 1 < mBinNum && mBins[(size_t)(i + 1)].valid)
                    {
                        const FLOAT_ limit = mBins[(size_t)(i + 1)].top + maxSlopeStep;
                        if (bin.top > limit)
                        {
                            bin.top = limit;
                            changed = true;
                        }
                    }
                }
                if (!changed)
                {
                    break;
                }
            }
        }

        // 7) 时间低通：水面高度逐帧抖动会让浮力跟着抖 → 浮体上下泵水 → 水静不下来。
        //    这里对每个桶的水面高度做一阶低通（top = 新·s + 旧·(1−s)），切断该正反馈。
        //    历史值必须按**世界坐标**对齐：桶网格已在步骤 3 锚定到固定栅格，
        //    mBinOrigin 只会整桶跳变 → 历史数组按整桶平移复用；
        //    分辨率或对齐方式变了就整段作废（mBinTopSmoothValid 置 0）。
        //    旧实现按裸下标混用历史值（origin 逐帧漂移），并且用 "top != 0" 当
        //    未初始化哨兵 → 水面高度恰好经过 0 时滤波会静默失效。
        {
            const FLOAT_ s = std::clamp(param.surfaceSmoothing, FLOAT_(0), FLOAT_(1));
            const bool sizeOk = ((int)mBinTopSmooth.size() == mBinNum) &&
                                ((int)mBinTopSmoothValid.size() == mBinNum);
            const bool widthOk = (mBinTopSmoothWidth > FLOAT_(0)) &&
                                 (std::fabs(mBinTopSmoothWidth - mBinWidth) <= mBinWidth * FLOAT_(1e-3));
            int binShift = 0;
            bool originOk = false;
            if (sizeOk && widthOk)
            {
                const FLOAT_ delta = (mBinOrigin - mBinTopSmoothOrigin) / mBinWidth;
                binShift = (int)std::llround(delta);
                originOk = std::fabs(delta - (FLOAT_)binShift) <= FLOAT_(1e-3);
            }
            const bool usable = (s > FLOAT_(0)) && (s < FLOAT_(1)) && sizeOk && widthOk && originOk;
            if (!usable)
            {
                // 重新开始积累历史（也覆盖"用户把 s 从 0 打开"的情况）
                mBinTopSmooth.assign((size_t)mBinNum, FLOAT_(0));
                mBinTopSmoothValid.assign((size_t)mBinNum, (unsigned char)0);
                binShift = 0;
            }
            else if (binShift != 0)
            {
                // 整桶平移：把历史值搬到它对应的新下标上。
                // 就地搬（方向按位移方向取）以避免每帧分配临时数组。
                if (binShift > 0)
                {
                    for (int i = 0; i < mBinNum; ++i)
                    {
                        const int src = i + binShift;
                        if (src < mBinNum)
                        {
                            mBinTopSmooth[(size_t)i] = mBinTopSmooth[(size_t)src];
                            mBinTopSmoothValid[(size_t)i] = mBinTopSmoothValid[(size_t)src];
                        }
                        else
                        {
                            mBinTopSmoothValid[(size_t)i] = 0;
                        }
                    }
                }
                else
                {
                    for (int i = mBinNum - 1; i >= 0; --i)
                    {
                        const int src = i + binShift;
                        if (src >= 0)
                        {
                            mBinTopSmooth[(size_t)i] = mBinTopSmooth[(size_t)src];
                            mBinTopSmoothValid[(size_t)i] = mBinTopSmoothValid[(size_t)src];
                        }
                        else
                        {
                            mBinTopSmoothValid[(size_t)i] = 0;
                        }
                    }
                }
            }
            if (s > FLOAT_(0) && s < FLOAT_(1))
            {
                for (int i = 0; i < mBinNum; ++i)
                {
                    SurfaceBin &bin = mBins[(size_t)i];
                    if (!bin.valid)
                    {
                        continue;
                    }
                    if (mBinTopSmoothValid[(size_t)i] != 0)
                    {
                        bin.top = bin.top * s + mBinTopSmooth[(size_t)i] * (FLOAT_(1) - s);
                    }
                    mBinTopSmooth[(size_t)i] = bin.top;
                    mBinTopSmoothValid[(size_t)i] = 1;
                }
            }
            mBinTopSmoothOrigin = mBinOrigin;
            mBinTopSmoothWidth = mBinWidth;
        }

        // 水面查询：相邻有效桶之间分段线性插值（水面是折线而非台阶），
        // 跨到无水桶立即中断（悬在池壁外/岸上的区域拿不到水面 → 不计浸没）。
        mGapTh = gapTh;
        mNoWater = -std::numeric_limits<FLOAT_>::max() * FLOAT_(0.5);
        const FLOAT_ noWater = mNoWater;
        // ── 远场水位 refLevel 与允许的局部偏差 maxLevelDev ──────────────────
        // 桶内水面 = 该桶里最高的那颗"暴露的合格水粒子"，这是一个**极值统计量**：
        // 一颗被撞飞的粒子，或者浮体自己排开的水在身旁堆起 1~3 个单位高的水墙，
        // 就能把整桶水面抬高一两个单位。而浮体的桶内水面本来就查不到（水在它底下，
        // 被判为"被刚体压住"→ 无效桶），只能借邻居桶的水位 —— 借到的正是被自己
        // 排开的水抬高过的水位。于是形成正反馈：
        //     浮体下沉 → 排开更多水 → 邻居水位抬高 → 借来的水面抬高 → 浮力变大
        //   → 浮体升空 → 水塌回去 → 水位回落 → 浮力归零 → 浮体自由落体 → 循环
        // 实测（headless 探针）：浮力/自重 在 0.0 ~ 2.7 之间逐帧来回跳，物体在水里
        // 永远静不下来（水也一直被搅动，动能不衰减）。
        // 解法：每个刚体用**远场水位**做基准 —— 取刚体水平位置 ±(R+3h) 窗口内所有
        // 有效桶水面的**中位数**（排开的水只占窗口的一小部分，抬不动中位数），
        // 再把逐格/逐圆的局部水面夹在 refLevel ± maxLevelDev 之内：
        //   · 保留"每个格/圆用自己的水面"的语义（跨池壁、岸上、两个水池各自算）；
        //   · 但把"自己抬起来的水"对自身浮力的贡献限制在合理范围内（相当于用
        //     未扰动的远场水线做水线裁剪，这也正是浮体稳性的标准近似）。
        FLOAT_ refLevel = noWater;
        FLOAT_ maxLevelDev = h;
        auto LocalSurfaceAt = [&](FLOAT_ u) -> FLOAT_
        {
            const FLOAT_ level = SurfaceAt(u);
            if (level <= noWater || refLevel <= noWater)
            {
                return level;
            }
            return std::clamp(level, refLevel - maxLevelDev, refLevel + maxLevelDev);
        };
        // 跨水陆边界时的水面：在 [u-span, u+span] 上取有水的采样点求平均
        auto LocalSurfaceSpan = [&](FLOAT_ u, FLOAT_ span) -> FLOAT_
        {
            FLOAT_ sum = FLOAT_(0);
            int cnt = 0;
            const FLOAT_ samples[3] = {u, u - span, u + span};
            for (int i = 0; i < 3; ++i)
            {
                const FLOAT_ level = LocalSurfaceAt(samples[i]);
                if (level > noWater)
                {
                    sum += level;
                    ++cnt;
                }
            }
            return (cnt > 0) ? (sum / (FLOAT_)cnt) : noWater;
        };
        // 远场水位：只用**刚体覆盖范围之外**的桶做中位数（见前面 refLevel 的说明）。
        // 为什么必须排除自己覆盖的桶：浮体所在的桶要么查不到自由水面（水在它底下/被它
        // 压着 → 无效桶），要么记的就是它自己排开的水堆起来的高度 —— 拿这些桶给自己
        // 定水位就是"自己给自己定吃水"，必然自激。窗口内没有有效桶 → 无水。
        auto BodyReferenceLevel = [&](FLOAT_ uCenter, FLOAT_ R) -> FLOAT_
        {
            const FLOAT_ halfWin = R + h * FLOAT_(3.0);
            const FLOAT_ inner = R + h; // 刚体水平覆盖范围（再让 1h 余量）
            int b0 = (int)std::floor((uCenter - halfWin - mBinOrigin) / mBinWidth);
            int b1 = (int)std::floor((uCenter + halfWin - mBinOrigin) / mBinWidth);
            b0 = std::max(b0, 0);
            b1 = std::min(b1, mBinNum - 1);
            mLevelBuf.clear();
            for (int b = b0; b <= b1; ++b)
            {
                const FLOAT_ bu = mBinOrigin + ((FLOAT_)b + FLOAT_(0.5)) * mBinWidth;
                if (std::fabs(bu - uCenter) <= inner)
                {
                    continue;
                }
                if (mBins[(size_t)b].valid)
                {
                    mLevelBuf.push_back(mBins[(size_t)b].top);
                }
            }
            if (mLevelBuf.empty())
            {
                // 水体比刚体大不了多少（窄水槽/小水洼）→ 退回整个窗口
                for (int b = b0; b <= b1; ++b)
                {
                    if (mBins[(size_t)b].valid)
                    {
                        mLevelBuf.push_back(mBins[(size_t)b].top);
                    }
                }
            }
            if (mLevelBuf.empty())
            {
                return noWater;
            }
            const size_t mid = mLevelBuf.size() / 2;
            std::nth_element(mLevelBuf.begin(), mLevelBuf.begin() + (ptrdiff_t)mid, mLevelBuf.end());
            return mLevelBuf[mid];
        };
        // 允许的局部水面偏差：小刚体只允许很小的起伏，大刚体（长木板）允许整体倾斜
        auto BodyLevelDeviation = [&](FLOAT_ R) -> FLOAT_
        {
            return std::max(h * FLOAT_(0.5), R * FLOAT_(0.5));
        };

        // 水体接触判定 + 湿粒子列表：R+4h 内的**本系统**液体粒子下标收集到 mWetIdx，
        // 其中落在 [0.8R, R+4h] 环带内的 ≥2 个才算"接触"（用来区分"完全没碰到水"与
        // "碰到水但没浸没（只加阻尼）"）。
        // 这份列表同时供浮力反作用**局部**分摊使用（见文件末尾），因此不能像旧实现
        // 那样"数到 2 个就提前返回"，也不能因为 mIndexMapDirty 就把过滤整个跳过。
        auto CollectWaterContact = [&](const Vec2_ &c, FLOAT_ R) -> bool
        {
            const FLOAT_ inner2 = (R * FLOAT_(0.8)) * (R * FLOAT_(0.8));
            const FLOAT_ outer = R + h * FLOAT_(4.0);
            const FLOAT_ outer2 = outer * outer;
            mWetIdx.clear();
            mWorld->mGridSearch.Get(c - Vec2_{outer, outer}, c + Vec2_{outer, outer}, mSearchV);
            unsigned int count = 0;
            for (auto *o : mSearchV)
            {
                if (o == nullptr || o->PFGetType() != PhysicsObjectEnum::particle)
                {
                    continue;
                }
                PhysicsParticle *lp = (PhysicsParticle *)o;
                if (!lp->IsLiquidParticle)
                {
                    continue; // 只统计液体（普通刚体粒子不是水）
                }
                auto it = mIndexMap.find(lp);
                if (it == mIndexMap.end())
                {
                    continue; // 属于另一个液体实例
                }
                const FLOAT_ d2 = ModulusLength(lp->pos - c);
                if (d2 > outer2)
                {
                    continue;
                }
                mWetIdx.push_back(it->second);
                if (d2 >= inner2)
                {
                    ++count;
                }
            }
            return count >= 2;
        };

        // 把某固体的浮力反作用按"湿粒子质量占比"分摊到**局部**水体：
        // 每个固体的 F 被完整分给与它接触的粒子（占比之和 = 1），所以
        // Σ_j m_j·Δv_j = −Σ F·dt 依然精确成立（动量守恒），但不再撒到全池
        //（旧实现按全部粒子均摊：远处另一个水池会被无端加速，而真正被压的水
        //  反而几乎没反应，粒子少时更是会给每颗粒子注入上百 u/s 的假速度）。
        auto DistributeReactionForce = [&](const Vec2_ &F)
        {
            if (mWetIdx.empty() || ModulusLength(F) <= FLOAT_(1e-9))
            {
                return;
            }
            FLOAT_ sumM = FLOAT_(0);
            for (unsigned int idx : mWetIdx)
            {
                sumM += mParticles[idx]->mass;
            }
            if (!(sumM > FLOAT_(0)))
            {
                return;
            }
            const FLOAT_ invSum = FLOAT_(1) / sumM;
            for (unsigned int idx : mWetIdx)
            {
                PhysicsParticle *p = mParticles[idx];
                mReactionForce[idx] += F * (p->mass * invSum);
            }
        };

        // 通用：上浮速度上限（防"活塞效应"把整池水抬离水域）+ 液体阻力/角阻尼
        const FLOAT_ maxRiseSpeed = std::max(param.maxRiseSpeed, FLOAT_(0));
        const FLOAT_ solidDrag = std::max(param.solidDrag, FLOAT_(0));
        const FLOAT_ angularFactor = std::max(param.angularDampingFactor, FLOAT_(0));
        const FLOAT_ maxAngular = std::max(param.maxAngularSpeed, FLOAT_(0));
        auto ApplyCommon = [&](PhysicsAngle *solid, FLOAT_ frac)
        {
            if (solid == nullptr)
            {
                return;
            }
            const FLOAT_ upSpeed = Dot(solid->speed, up);
            if (upSpeed > maxRiseSpeed)
            {
                solid->speed -= up * (upSpeed - maxRiseSpeed);
                solid->StaticNum = 0;
            }
            const FLOAT_ dragRate = solidDrag * std::clamp(frac, FLOAT_(0), FLOAT_(1));
            if (dragRate > FLOAT_(0))
            {
                // 连续指数衰减（与帧率无关；线性近似在大 dt/大阻尼下会直接清零速度）
                solid->speed *= std::exp(-dragRate * time);
                solid->angleSpeed *= std::exp(-dragRate * angularFactor * time);
                solid->StaticNum = 0; // 湿物体不参与引擎的静止休眠
            }
            if (maxAngular > FLOAT_(0))
            {
                const FLOAT_ aspd = std::fabs(solid->angleSpeed);
                if (aspd > maxAngular)
                {
                    solid->angleSpeed *= (maxAngular / aspd);
                }
            }
            else
            {
                solid->angleSpeed = FLOAT_(0);
            }
        };

        const FLOAT_ buoyancyGain = std::max(param.buoyancy, FLOAT_(0));
        const FLOAT_ contactDamping = std::max(param.contactDamping, FLOAT_(0));

        // ── 圆：弓形面积浸没比例 + 局部接触水面 ─────────────────────────
        // 浮力反作用缓冲：本帧按"接触粒子质量占比"分摊到局部水体（见文件末尾）
        mReactionForce.assign(n, Vec2_{0, 0});
        // 圆被水平面截出的弓形面积占比（d = 浸没深度，0..2R）。
        // 旧实现用线性 subDepth/(2R)：浅浸没时严重高估（d=0.25R 时 0.125 vs 真值
        // 0.072，约 1.7 倍）→ 球浮得太高、出水时弹跳；接近全浸没时又偏低
        // （d=1.75R 时 0.875 vs 0.928）。
        auto CircleSubmergedFraction = [](FLOAT_ d, FLOAT_ R) -> FLOAT_
        {
            if (d <= FLOAT_(0))
            {
                return FLOAT_(0);
            }
            if (d >= FLOAT_(2) * R)
            {
                return FLOAT_(1);
            }
            const FLOAT_ c = R - d; // 圆心到水面的有向距离
            const FLOAT_ ratio = std::clamp(c / R, FLOAT_(-1), FLOAT_(1));
            const FLOAT_ halfChord = SQRT_(std::max(FLOAT_(0), FLOAT_(2) * R * d - d * d));
            const FLOAT_ segArea = R * R * std::acos(ratio) - c * halfChord;
            return std::clamp(segArea / (FLOAT_(M_PI) * R * R), FLOAT_(0), FLOAT_(1));
        };
        for (auto *c : mWorld->PhysicsCircleS)
        {
            if (c == nullptr || c->invMass == FLOAT_(0))
            {
                continue;
            }
            const FLOAT_ R = c->radius;
            if (R <= FLOAT_(0))
            {
                continue;
            }
            if (!CollectWaterContact(c->pos, R))
            {
                continue; // 周围没有水体接触
            }
            // 局部水面：圆跨越水陆边界（例如半悬在池壁上方）时，
            // 取圆心与左右两侧采样点中有水者的平均 → 按实际浸没的那半边算浮力。
            // 基准（refLevel）与偏差上限按本刚体先算好，LocalSurfaceAt 会用它们夹住
            // 被"自己排开的水"抬高的局部水面（见前面 refLevel 的说明）。
            refLevel = BodyReferenceLevel(Dot(c->pos, right), R);
            maxLevelDev = BodyLevelDeviation(R);
            const FLOAT_ surfaceLevel = LocalSurfaceSpan(Dot(c->pos, right), R);
            if (surfaceLevel <= noWater)
            {
                ApplyCommon(c, contactDamping); // 碰到水但没有水面（只在零星水花里）
                continue;
            }
            const FLOAT_ bottomLevel = Dot(c->pos, up) - R;
            const FLOAT_ subDepth = surfaceLevel - bottomLevel;
            const FLOAT_ frac = CircleSubmergedFraction(subDepth, R);
            if (frac <= FLOAT_(0.01))
            {
                // 未浸没但仍与水接触 → 先施加液体阻力/角阻尼（防带角速度穿过水面摇摆）
                ApplyCommon(c, contactDamping);
                continue;
            }
            const FLOAT_ rhoBody = c->mass / ((FLOAT_)M_PI * R * R);
            if (rhoBody <= FLOAT_(0) || !std::isfinite(rhoBody))
            {
                continue;
            }
            const FLOAT_ accel = g * (rhoLiquid / rhoBody) * frac * buoyancyGain;
            const FLOAT_ force = accel * c->mass; // F = m·a
            c->speed += up * (force * c->invMass * time);
            c->StaticNum = 0;
            DistributeReactionForce(up * force);
            ApplyCommon(c, frac);
        }

        // ── 网格形状：逐格水线裁剪（含扶正扭矩）────────────────────
        // 把每个实体格（1×1 局部方格）与水线半平面求交，累加面积与一阶矩：
        // 对非凸/稀疏形状也精确（旧的"外接矩形裁剪"会把 L 形/带孔形状的
        // 浸没面积算成整个包围盒 → 浮力虚高、扶正方向出错）。
        for (auto *s : mWorld->PhysicsShapeS)
        {
            if (s == nullptr || s->invMass == FLOAT_(0))
            {
                continue;
            }
            // 实心面积（格子数 = 世界面积，格子 1×1）与密度
            unsigned int cells = 0;
            for (unsigned int x = 0; x < s->width; ++x)
            {
                for (unsigned int y = 0; y < s->height; ++y)
                {
                    if (s->at(x, y).Entity)
                    {
                        ++cells;
                    }
                }
            }
            if (cells == 0)
            {
                continue;
            }
            const FLOAT_ rhoBody = s->mass / (FLOAT_)cells;
            if (rhoBody <= FLOAT_(0) || !std::isfinite(rhoBody))
            {
                continue;
            }
            // 水体接触判定：完全没碰到水就直接跳过（碰到水但没浸没 → 只加阻尼）
            if (!CollectWaterContact(s->pos, s->radius))
            {
                continue; // 周围没有水体接触
            }

            const FLOAT_ cosA = std::cos(s->angle);
            const FLOAT_ sinA = std::sin(s->angle);
            refLevel = BodyReferenceLevel(Dot(s->pos, right), s->radius);
            maxLevelDev = BodyLevelDeviation(s->radius);
            auto CellToWorld = [&](FLOAT_ lx, FLOAT_ ly) -> Vec2_
            {
                const FLOAT_ ox = lx - s->CentreMass.x;
                const FLOAT_ oy = ly - s->CentreMass.y;
                return Vec2_{ox * cosA - oy * sinA, ox * sinA + oy * cosA} + s->pos;
            };

            FLOAT_ subArea = 0;
            Vec2_ subMoment{0, 0};
            for (unsigned int x = 0; x < s->width; ++x)
            {
                for (unsigned int y = 0; y < s->height; ++y)
                {
                    if (!s->at(x, y).Entity)
                    {
                        continue;
                    }
                    // 该格自己的水面高度：按格心的水平坐标查局部水面高度场。
                    // 悬在池壁外/岸上的格子落在"无水桶"里 → 直接跳过，
                    // 不再被全局水位误判为浸没。
                    const Vec2_ cellCentre = CellToWorld((FLOAT_)x + FLOAT_(0.5), (FLOAT_)y + FLOAT_(0.5));
                    const FLOAT_ cellSurface = LocalSurfaceAt(Dot(cellCentre, right));
                    if (cellSurface <= noWater)
                    {
                        continue; // 该格所在水平区域没有实质水体
                    }
                    const Vec2_ quad[4] = {
                        CellToWorld((FLOAT_)x, (FLOAT_)y),
                        CellToWorld((FLOAT_)(x + 1), (FLOAT_)y),
                        CellToWorld((FLOAT_)(x + 1), (FLOAT_)(y + 1)),
                        CellToWorld((FLOAT_)x, (FLOAT_)(y + 1))
                    };
                    // Sutherland–Hodgman：保留 dot(p, up) <= 该格局部水面 的半边
                    mClipPoly.clear();
                    for (int k = 0; k < 4; ++k)
                    {
                        const Vec2_ &a = quad[k];
                        const Vec2_ &b = quad[(k + 1) & 3];
                        const FLOAT_ ha = Dot(a, up) - cellSurface;
                        const FLOAT_ hb = Dot(b, up) - cellSurface;
                        const bool aIn = (ha <= FLOAT_(0));
                        const bool bIn = (hb <= FLOAT_(0));
                        if (aIn)
                        {
                            mClipPoly.push_back(a);
                        }
                        if (aIn != bIn)
                        {
                            const FLOAT_ denom = (ha - hb);
                            if (std::fabs(denom) > FLOAT_(1e-12))
                            {
                                const FLOAT_ t = ha / denom;
                                mClipPoly.push_back(a + (b - a) * t);
                            }
                        }
                    }
                    if (mClipPoly.size() < 3)
                    {
                        continue; // 该格完全在水面之上
                    }
                    // 鞋带公式：面积 + 一阶矩（形心 = 一阶矩/面积）
                    FLOAT_ area2 = 0;
                    Vec2_ moment{0, 0};
                    const size_t m = mClipPoly.size();
                    for (size_t k = 0; k < m; ++k)
                    {
                        const Vec2_ &p0 = mClipPoly[k];
                        const Vec2_ &p1 = mClipPoly[(k + 1) % m];
                        const FLOAT_ cr = p0.x * p1.y - p1.x * p0.y;
                        area2 += cr;
                        moment += (p0 + p1) * cr;
                    }
                    const FLOAT_ area = std::fabs(area2) * FLOAT_(0.5);
                    if (area <= FLOAT_(1e-6) || std::fabs(area2) <= FLOAT_(1e-12))
                    {
                        continue;
                    }
                    subArea += area;
                    subMoment += (moment / (FLOAT_(3.0) * area2)) * area;
                }
            }
            if (subArea <= FLOAT_(1e-4))
            {
                // 未浸没但仍与水接触 → 仅施加液体阻力/角阻尼
                ApplyCommon(s, contactDamping);
                continue;
            }
            const Vec2_ centroid = subMoment / subArea; // 浸没形心

            // 阿基米德力作用于浸没形心：线速度 + 角速度（扶正扭矩）
            const Vec2_ F = up * (rhoLiquid * g * subArea * buoyancyGain);
            s->speed += F * (s->invMass * time);
            s->StaticNum = 0;
            const Vec2_ r = centroid - s->pos; // 力臂：质心 → 浸没形心
            const FLOAT_ torque = Cross(r, F);
            if (s->invMomentInertia > FLOAT_(0))
            {
                FLOAT_ dOmega = torque * s->invMomentInertia * time;
                const FLOAT_ maxDOmega = std::max(param.maxTorqueImpulse, FLOAT_(0));
                dOmega = std::clamp(dOmega, -maxDOmega, maxDOmega);
                s->angleSpeed += dOmega;
            }
            DistributeReactionForce(F);

            ApplyCommon(s, subArea / (FLOAT_)cells);
        }

        // ── 浮力反作用施加到液体（局部环带 + 单粒子限幅）───────────────
        // 关键：浮力必须作为"水↔固体"的内力——固体受到 F 向上的同时，液体必须
        // 受到等大反向的 −F。否则浮力变成外部上推力，会把"水 + 全部浮体"整体
        // 托离水池（数据实测：整池在无接触时一起升空）。
        // 与旧实现的区别：
        //   ① 只在**与该固体接触的粒子**上按质量占比分摊（DistributeReactionForce），
        //      远处另一个水池不会被无端加速，被压住的那部分水才有反应；
        //   ② 单粒子单帧速度增量另受 param.maxReactionSpeed 限制：旧写法（全池均摊、
        //      不限幅）在"20 颗粒子的水桶 + 大浮体"下能一帧给出 >100 u/s 的假速度，
        //      下一帧粒子先按这个速度位移再被 maxSpeed 钳制 → 直接穿过薄地板。
        {
            const FLOAT_ maxReactionDv = std::max(param.maxReactionSpeed, FLOAT_(0));
            for (size_t i = 0; i < n; ++i)
            {
                if (ModulusLength(mReactionForce[i]) <= FLOAT_(1e-12))
                {
                    continue;
                }
                PhysicsParticle *p = mParticles[i];
                if (p == nullptr || !(p->mass > FLOAT_(0)))
                {
                    continue;
                }
                Vec2_ dv = mReactionForce[i] * (-time / p->mass);
                if (maxReactionDv > FLOAT_(0))
                {
                    const FLOAT_ len = Modulus(dv);
                    if (len > maxReactionDv)
                    {
                        dv *= (maxReactionDv / len);
                    }
                }
                else
                {
                    dv = Vec2_{0, 0}; // maxReactionSpeed = 0：完全抑制反作用（仅调试用）
                }
                p->speed += dv;
            }
        }
    }

}
