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
        mTorqueBuf.clear();
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
            // 尾部交换删除：与 mParticles 平行的所有缓冲同步维护
            const size_t last = mParticles.size() - 1;
            mParticles[i] = mParticles[last];
            if (i < mPrevPos.size() && last < mPrevPos.size())
            {
                mPrevPos[i] = mPrevPos[last];
            }
            if (i < mDensity.size() && last < mDensity.size())
            {
                mDensity[i] = mDensity[last];
            }
            if (i < mDensityNear.size() && last < mDensityNear.size())
            {
                mDensityNear[i] = mDensityNear[last];
            }
            if (i < mShift.size() && last < mShift.size())
            {
                mShift[i] = mShift[last];
            if (i < mPreProjPos.size() && last < mPreProjPos.size()) mPreProjPos[i] = mPreProjPos[last];
            if (i < mProjNormal.size() && last < mProjNormal.size()) mProjNormal[i] = mProjNormal[last];
            if (i < mProjBodyVel.size() && last < mProjBodyVel.size()) mProjBodyVel[i] = mProjBodyVel[last];
            }
            mParticles.pop_back();
            if (mPrevPos.size() > mParticles.size()) mPrevPos.pop_back();
            if (mDensity.size() > mParticles.size()) mDensity.pop_back();
            if (mDensityNear.size() > mParticles.size()) mDensityNear.pop_back();
            if (mShift.size() > mParticles.size()) mShift.pop_back();
            if (mPreProjPos.size() > mParticles.size()) mPreProjPos.pop_back();
            if (mProjNormal.size() > mParticles.size()) mProjNormal.pop_back();
            if (mProjBodyVel.size() > mParticles.size()) mProjBodyVel.pop_back();
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
        // 每个实体格按流体间距子采样（每点权重 1）：只取格心会让薄板表面的密度
        // 贡献严重不足（格心到表面 0.5~1.0，核值几乎为 0）。
        const int sub = std::max(1, (int)std::lround(FLOAT_(1) / std::max(spacingEff, FLOAT_(1e-3))));
        const FLOAT_ subInv = FLOAT_(1) / (FLOAT_)sub;

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
                                    mBoundary.push_back(BoundarySample{pc, FLOAT_(1)});
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
                                    mBoundary.push_back(BoundarySample{pw, FLOAT_(1)});
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

        if (mDensity.size() != n || mDensityNear.size() != n || mPrevPos.size() != n || mShift.size() != n)
        {
            const size_t oldPrev = mPrevPos.size();
            mDensity.resize(n, FLOAT_(0));
            mDensityNear.resize(n, FLOAT_(0));
            mShift.resize(n, Vec2_{0, 0});
            mPrevPos.resize(n, Vec2_{0, 0});
            // 新条目的回代基准必须是粒子当前位置，绝不能用 {0,0}
            //（否则第一帧会得到一个巨大的假位移）
            for (size_t i = oldPrev; i < n; ++i)
            {
                mPrevPos[i] = mParticles[i]->pos;
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
                const Vec2_ vBody = mProjBodyVel[i];
                Vec2_ vRel = v - vBody;
                const FLOAT_ vn = Dot(vRel, n);
                if (vn < FLOAT_(0))
                {
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
                mShift[i] = mParticles[i]->speed; // 暂存新速度（Jacobi 式，避免顺序相关）
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
                    dv += (mParticles[j]->speed - mParticles[i]->speed) * w;
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

        // 接触点反作用力矩冲量累积（τ 的冲量形式：r × (m_p·Δx/dt)）
        mTorqueBuf.assign(shapes.size(), FLOAT_(0));
        const FLOAT_ invDt = FLOAT_(1.0) / time;

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

            // ── 单粒子投影：固体（圆/形状/线）与地图交替迭代至稳定 ──
            // 顺序很关键：若先推地图再推固体，固体可能把粒子重新压回地图格子里
            //（典型场景：方块/木板压在池底），表现为池底粒子跨帧抖动。
            // 这里最多迭代 2 轮，第二轮不再累加力矩（避免重复计力）。
            for (int pass = 0; pass < 2; ++pass)
            {
                const Vec2_ before = pos;

                // 圆形固体：径向推出到圆外（无反作用：小圆会挖出包围空腔，易成"潜艇"）
                for (size_t ci = 0; ci < circles.size(); ++ci)
                {
                    auto *c = circles[ci];
                    if (c == nullptr)
                    {
                        continue;
                    }
                    const Vec2_ d = pos - c->pos;
                    const FLOAT_ r = Modulus(d);
                    if (r >= c->radius)
                    {
                        continue;
                    }
                    if (r < FLOAT_(1e-4))
                    {
                        pos = c->pos + Vec2_{FLOAT_(0.01), c->radius};
                    }
                    else
                    {
                        pos = c->pos + d * ((c->radius + FLOAT_(0.01)) / r);
                    }
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
                    if (!ShapeSolidAt(s, pos))
                    {
                        continue;
                    }
                    const Vec2_ newPos = NearestFree(
                        pos,
                        [&](const Vec2_ &cand) { return !ShapeSolidAt(s, cand); },
                        maxStepShape,
                        preferDir);
                    if (newPos != pos)
                    {
                        mProjNormal[i] += (newPos - pos);
                        mProjBodyVel[i] = s->speed;
                        if (pass == 0 && s->invMass != FLOAT_(0) && s->mass > FLOAT_(0))
                        {
                            // 接触点力矩：作用点 ≈ 粒子位置，力臂 = 接触点 − 质心。
                            // 反作用冲量 J = −m_p·Δx/dt，Δω = invI·(r × J)。
                            const Vec2_ impulse = (newPos - pos) * (-mParticleMass * invDt);
                            const Vec2_ arm = pos - s->pos;
                            mTorqueBuf[si] += Cross(arm, impulse);
                        }
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

        // 施加钳制后的接触点反作用力矩
        const FLOAT_ gain = std::max(param.reactionTorqueGain, FLOAT_(0));
        const FLOAT_ maxDOmega = std::max(param.maxTorqueImpulse, FLOAT_(0));
        for (size_t si = 0; si < shapes.size(); ++si)
        {
            PhysicsShape *s = shapes[si];
            if (s == nullptr)
            {
                continue;
            }
            if (s->invMomentInertia > FLOAT_(0) && mTorqueBuf[si] != FLOAT_(0) && gain > FLOAT_(0))
            {
                FLOAT_ dOmega = mTorqueBuf[si] * s->invMomentInertia * gain;
                dOmega = std::clamp(dOmega, -maxDOmega, maxDOmega);
                s->angleSpeed += dOmega;
                s->StaticNum = 0; // 唤醒：液体在推动它，不允许进入静止休眠
            }
        }
    }

    bool PhysicsLiquid::ShapeSolidAt(PhysicsShape *s, const Vec2_ &cand) const
    {
        const Vec2_ local = vec2angle(cand - s->pos, -s->angle);
        const glm::ivec2 g = ToInt(local + s->CentreMass);
        if (g.x < 0 || g.y < 0 || g.x >= (int)s->width || g.y >= (int)s->height)
        {
            return false; // 越界即空闲
        }
        return s->at(g.x, g.y).Collision;
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
            if (ShapeSolidAt(s, pos))
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
        const int bi = (int)f; // f >= 0 时向下取整
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

    bool PhysicsLiquid::QuerySurfaceLevel(Vec2_ pos, FLOAT_ &outLevel) const
    {
        outLevel = mNoWater;
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
        const FLOAT_ gapTh = std::max(FLOAT_(1.0), spacingEff * FLOAT_(2.0));
        const size_t clusterMinCount = std::max<size_t>(2, mParticles.size() / 50);
        const Vec2_ right = Vec2_{up.y, -up.x}; // 水平轴（垂直于重力）
        const size_t n = mParticles.size();

        // 1) 高度缓存 + 高度降序索引
        mHeightOf.resize(n);
        mOrder.resize(n);
        FLOAT_ uMin = std::numeric_limits<FLOAT_>::max();
        FLOAT_ uMax = -std::numeric_limits<FLOAT_>::max();
        for (size_t i = 0; i < n; ++i)
        {
            mHeightOf[i] = Dot(mParticles[i]->pos, up);
            mOrder[i] = (unsigned int)i;
            const FLOAT_ u = Dot(mParticles[i]->pos, right);
            if (u < uMin) uMin = u;
            if (u > uMax) uMax = u;
        }
        std::sort(mOrder.begin(), mOrder.end(),
                  [&](unsigned int a, unsigned int b) { return mHeightOf[a] > mHeightOf[b]; });

        // 2) 桶划分：桶宽默认 ≈ max(h, 2×静息间距)（默认参数下 = 1 个地图格），
        //    也可用 param.surfaceBinWidth 显式指定；粒子铺得很开时限桶数，
        //    避免超大水面的内存/耗时爆炸。
        const FLOAT_ binWanted = (param.surfaceBinWidth > FLOAT_(0))
                                      ? param.surfaceBinWidth
                                      : std::max(h, spacingEff * FLOAT_(2.0));
        mBinWidth = std::max(binWanted, FLOAT_(1e-3));
        const FLOAT_ uSpan = std::max(uMax - uMin, FLOAT_(0));
        const int kMaxBins = 2048;
        if (uSpan > FLOAT_(0) && uSpan / mBinWidth > (FLOAT_)kMaxBins)
        {
            mBinWidth = uSpan / (FLOAT_)kMaxBins;
        }
        mBinOrigin = uMin;
        mBinNum = (uSpan > FLOAT_(0)) ? ((int)(uSpan / mBinWidth) + 1) : 1;
        if (mBinNum < 1)
        {
            mBinNum = 1;
        }
        mBins.clear();
        mBins.resize((size_t)mBinNum);

        // 3) 全局分簇（高度间隙 > gapTh 切开）→ 标记"实质水体"粒子。
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

        // 4) 识别"自由水面"粒子。
        //    自由水面 = ① 正上方 probe 内没有水、也没有动态刚体（真正暴露在空气中）；
        //               ② 下方有连续 ≥minChain 层水，且水柱根部不在动态刚体上；
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

        // 4b) 升序扫描（mOrder 逆序）：统计水柱层数与根部，选出每桶的水面
        {
            FLOAT_ lastVUp = -std::numeric_limits<FLOAT_>::max();
            unsigned int chain = 0;
            bool chainOnSolid = false;
            for (size_t idx = n; idx-- > 0;)
            {
                const unsigned int pi = mOrder[idx];
                if (mEligible[pi] == 0)
                {
                    continue;
                }
                const Vec2_ pos = mParticles[pi]->pos;
                const FLOAT_ v = mHeightOf[pi];
                if (chain > 0 && (v - lastVUp) <= gapTh)
                {
                    ++chain; // 与下方粒子连续 → 水柱加高一层
                }
                else
                {
                    chain = 1; // 新水柱：记录它的底部是否坐在动态刚体上
                    chainOnSolid = InsideDynamicSolid(pos - up * probe);
                }
                lastVUp = v;
                if (chain >= minChain && !chainOnSolid && mExposed[pi] != 0 &&
                    ColumnClearOfSolids(pos))
                {
                    const FLOAT_ u = Dot(pos, right);
                    const int bi = (int)((u - mBinOrigin) / mBinWidth);
                    if (bi >= 0 && bi < mBinNum)
                    {
                        SurfaceBin &bin = mBins[(size_t)bi];
                        bin.hasWater = true;
                        if (!bin.valid || v > bin.top)
                        {
                            bin.top = v;
                            bin.valid = true;
                            bin.topIdx = pi;
                        }
                    }
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
        {
            const FLOAT_ maxSlopeStep = std::max(FLOAT_(0.5), spacingEff * FLOAT_(2.0));
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
        //    桶数变化（水面横向范围变化）时重新初始化，避免用到错位的旧值。
        {
            const FLOAT_ s = std::clamp(param.surfaceSmoothing, FLOAT_(0), FLOAT_(1));
            if (s < FLOAT_(1) && s > FLOAT_(0))
            {
                if ((int)mBinTopSmooth.size() != mBinNum)
                {
                    mBinTopSmooth.assign((size_t)mBinNum, FLOAT_(0));
                }
                for (int i = 0; i < mBinNum; ++i)
                {
                    SurfaceBin &bin = mBins[(size_t)i];
                    if (!bin.valid)
                    {
                        continue;
                    }
                    if (mBinTopSmooth[(size_t)i] != FLOAT_(0))
                    {
                        bin.top = bin.top * s + mBinTopSmooth[(size_t)i] * (FLOAT_(1) - s);
                    }
                    mBinTopSmooth[(size_t)i] = bin.top;
                }
            }
            else if ((int)mBinTopSmooth.size() != mBinNum)
            {
                mBinTopSmooth.assign((size_t)mBinNum, FLOAT_(0));
            }
        }

        // 水面查询：相邻有效桶之间分段线性插值（水面是折线而非台阶），
        // 跨到无水桶立即中断（悬在池壁外/岸上的区域拿不到水面 → 不计浸没）。
        mGapTh = gapTh;
        mNoWater = -std::numeric_limits<FLOAT_>::max() * FLOAT_(0.5);
        const FLOAT_ noWater = mNoWater;
        auto LocalSurfaceAt = [&](FLOAT_ u) -> FLOAT_ { return SurfaceAt(u); };
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

        // 水体接触判定：环带 [0.8R, R+4h] 内是否有 ≥2 个液体粒子。
        // 只用于区分"完全没碰到水"与"碰到水但没浸没（只加阻尼）"。
        auto HasWaterContact = [&](const Vec2_ &c, FLOAT_ R) -> bool
        {
            const FLOAT_ inner = R * FLOAT_(0.8);
            const FLOAT_ outer = R + h * FLOAT_(4.0);
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
                    continue; // 只统计本系统的液体（普通刚体粒子不是水）
                }
                if (!mIndexMapDirty && mIndexMap.find(lp) == mIndexMap.end())
                {
                    continue; // 属于另一个液体实例
                }
                const FLOAT_ d = Modulus(lp->pos - c);
                if (d < inner || d > outer)
                {
                    continue;
                }
                if (++count >= 2)
                {
                    return true;
                }
            }
            return false;
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

        // ── 圆：外接圆盘 + 局部接触水面浸没比例 ─────────────────────────
        Vec2_ buoyancyTotal{0, 0}; // 浮力等大反向作用到液体（动量守恒）
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
            if (!HasWaterContact(c->pos, R))
            {
                continue; // 周围没有水体接触
            }
            // 局部水面：圆跨越水陆边界（例如半悬在池壁上方）时，
            // 取圆心与左右两侧采样点中有水者的平均 → 按实际浸没的那半边算浮力
            const FLOAT_ surfaceLevel = LocalSurfaceSpan(Dot(c->pos, right), R);
            if (surfaceLevel <= noWater)
            {
                ApplyCommon(c, contactDamping); // 碰到水但没有水面（只在零星水花里）
                continue;
            }
            const FLOAT_ bottomLevel = Dot(c->pos, up) - R;
            const FLOAT_ subDepth = surfaceLevel - bottomLevel;
            const FLOAT_ frac = std::clamp(subDepth / (FLOAT_(2.0) * R), FLOAT_(0), FLOAT_(1));
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
            buoyancyTotal += up * force;
            c->speed += up * (accel * time);
            c->StaticNum = 0;
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
            if (!HasWaterContact(s->pos, s->radius))
            {
                continue; // 周围没有水体接触
            }

            const FLOAT_ cosA = std::cos(s->angle);
            const FLOAT_ sinA = std::sin(s->angle);
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
            buoyancyTotal += F;
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

            ApplyCommon(s, subArea / (FLOAT_)cells);
        }

        // ── 浮力反作用施加到液体（动量守恒）───────────────────────────
        // 关键：浮力必须作为"水↔固体"的内力——固体受到 F 向上的同时，液体必须
        // 受到等大反向的 −F。否则浮力变成外部上推力，会把"水 + 全部浮体"整体
        // 托离水池（数据实测：整池在无接触时一起升空）。等大反向均分给所有
        // 液体粒子，总动量守恒由构造保证。
        if (ModulusLength(buoyancyTotal) > FLOAT_(1e-6) && !mParticles.empty())
        {
            const Vec2_ dv = buoyancyTotal * (-time / (mParticleMass * (FLOAT_)mParticles.size()));
            for (auto *p : mParticles)
            {
                if (p != nullptr)
                {
                    p->speed += dv;
                }
            }
        }
    }

}
