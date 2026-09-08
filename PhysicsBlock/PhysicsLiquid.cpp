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
        mHeightBuf.clear();
        mTorqueBuf.clear();
        mClusterMin.clear();
        mClusterMax.clear();
        mClusterTop.clear();
        mClusterCount.clear();
        mRingCount.clear();
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
        mNeighborIndex.clear();
        mNeighborOffset.clear();
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
            }
            mParticles.pop_back();
            if (mPrevPos.size() > mParticles.size()) mPrevPos.pop_back();
            if (mDensity.size() > mParticles.size()) mDensity.pop_back();
            if (mDensityNear.size() > mParticles.size()) mDensityNear.pop_back();
            if (mShift.size() > mParticles.size()) mShift.pop_back();
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

        // ─── 双密度松弛（Clavet 2005 风格）─────────────────────────
        for (int iter = 0; iter < iterations; ++iter)
        {
            // 1) 密度（当前位置快照）
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
        const FLOAT_ invDt = FLOAT_(1.0) / time;
        for (size_t i = 0; i < n; ++i)
        {
            Vec2_ v = (mParticles[i]->pos - mPrevPos[i]) * invDt;
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
        const FLOAT_ stepBase = FLOAT_(0.02f);
        const FLOAT_ maxStepMap = std::max(param.mapEscapeRadius, FLOAT_(0)) * h;
        const Vec2_ stepDirs[8] = {
            {0, 1}, {0, -1}, {1, 0}, {-1, 0},
            {FLOAT_(0.7071), FLOAT_(0.7071)}, {FLOAT_(0.7071), FLOAT_(-0.7071)},
            {FLOAT_(-0.7071), FLOAT_(0.7071)}, {FLOAT_(-0.7071), FLOAT_(-0.7071)}
        };

        // 粗到细最近空闲点搜索：
        //   粗扫（≈12 步）先定位能逃逸的射线，再在该段内细扫（stepBase）取最近点。
        //   相比"逐 0.02 步扫满 maxLen"，同样覆盖全部射程但步数恒定，
        //   且深穿透时不会再因为射程不够而静默失败。
        //   preferDir 非零时，优先只考虑"朝来路反方向"的射线，避免沿最近路径
        //   推穿薄壁（隧穿）；若该半平面内无解则回退到全部方向。
        auto NearestFree = [&](Vec2_ from, auto &&IsFree, FLOAT_ maxLen, Vec2_ preferDir) -> Vec2_
        {
            if (maxLen <= stepBase)
            {
                return from;
            }
            const FLOAT_ coarseStep = std::max(stepBase, maxLen / FLOAT_(12.0));
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
                    FLOAT_ lastSolid = 0;
                    for (FLOAT_ len = coarseStep; len <= maxLen + FLOAT_(1e-4); len += coarseStep)
                    {
                        if (IsFree(from + stepDirs[di] * len))
                        {
                            // 首个自由点落在 (lastSolid, len]，在其中细扫取最近
                            bool foundFine = false;
                            for (FLOAT_ fine = lastSolid + stepBase; fine <= len; fine += stepBase)
                            {
                                if (IsFree(from + stepDirs[di] * fine))
                                {
                                    if (fine < bestLen)
                                    {
                                        bestLen = fine;
                                        best = from + stepDirs[di] * fine;
                                    }
                                    foundFine = true;
                                    break;
                                }
                            }
                            if (!foundFine && len < bestLen)
                            {
                                // 细扫因浮点累加错过区间端点：退化为粗扫点
                                bestLen = len;
                                best = from + stepDirs[di] * len;
                            }
                            break;
                        }
                        lastSolid = len;
                    }
                }
                if (bestLen < FLOAT_MAX)
                {
                    break; // 第一轮已找到解，无需全方向兜底
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
                // 因此这里必须用带越界检查的逐格测试（越界 = 空闲）。
                auto ShapeSolidAt = [&](PhysicsShape *s, const Vec2_ &cand) -> bool
                {
                    const Vec2_ local = vec2angle(cand - s->pos, -s->angle);
                    const glm::ivec2 g = ToInt(local + s->CentreMass);
                    if (g.x < 0 || g.y < 0 || g.x >= (int)s->width || g.y >= (int)s->height)
                    {
                        return false; // 越界即空闲
                    }
                    return s->at(g.x, g.y).Collision;
                };
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
                    if (pass == 0 && s->invMass != FLOAT_(0) && s->mass > FLOAT_(0) && newPos != pos)
                    {
                        // 接触点力矩：作用点 ≈ 粒子位置，力臂 = 接触点 − 质心。
                        // 反作用冲量 J = −m_p·Δx/dt（PBD 语义：粒子的速度变化即 Δx/dt），
                        // Δω = invI·(r × J)。reactionTorqueGain 是对 PBD 位置修正
                        // 当作速度冲量时 1/dt 放大的人为补偿（1.0 = 物理正确）。
                        const Vec2_ impulse = (newPos - pos) * (-mParticleMass * invDt);
                        const Vec2_ arm = pos - s->pos;
                        mTorqueBuf[si] += Cross(arm, impulse);
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
                        pos = closest + n * (FLOAT_(0.01) / nLen);
                    }
                }

                // ── 地图：把粒子钳制出碰撞格子（细粒度最近空闲点，保持切向滑动）──
                if (map != nullptr && map->FMGetCollide(pos))
                {
                    pos = NearestFree(
                        pos,
                        [&](const Vec2_ &cand) { return !map->FMGetCollide(cand); },
                        maxStepMap,
                        preferDir);
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

        // ── 液面：按高度分段聚类 ────────────────────────────────────
        // 全局主簇（粒子数最多的连续高度区）作为"整池液面"的兜底参考，
        // 同时保留每个簇的范围，供固体按"它实际接触到的水"选择液面。
        // 这样既修掉"多池/水洼按主池算"的错误，又保留原有保护：
        // 固体把一小簇水带上天时，该簇太小 → 回落到主池液面 → 浮力≈0 → 落回水中。
        const FLOAT_ gapTh = std::max(FLOAT_(1.0), spacingEff * FLOAT_(2.0));
        mHeightBuf.clear();
        mHeightBuf.reserve(mParticles.size());
        for (auto *p : mParticles)
        {
            if (p != nullptr)
            {
                mHeightBuf.push_back(Dot(p->pos, up));
            }
        }
        mClusterMin.clear();
        mClusterMax.clear();
        mClusterTop.clear();
        mClusterCount.clear();
        FLOAT_ mainSurfaceLevel = -std::numeric_limits<FLOAT_>::max();
        if (!mHeightBuf.empty())
        {
            std::sort(mHeightBuf.begin(), mHeightBuf.end());
            size_t clusterStart = 0;
            for (size_t i = 1; i <= mHeightBuf.size(); ++i)
            {
                const bool cut = (i == mHeightBuf.size()) ||
                                 (mHeightBuf[i] - mHeightBuf[i - 1] > gapTh);
                if (cut)
                {
                    mClusterMin.push_back(mHeightBuf[clusterStart]);
                    mClusterMax.push_back(mHeightBuf[i - 1]);
                    mClusterTop.push_back(mHeightBuf[i - 1]);
                    mClusterCount.push_back(i - clusterStart);
                    clusterStart = i;
                }
            }
            size_t mainIdx = 0;
            for (size_t ci = 1; ci < mClusterCount.size(); ++ci)
            {
                if (mClusterCount[ci] > mClusterCount[mainIdx])
                {
                    mainIdx = ci;
                }
            }
            mainSurfaceLevel = mClusterTop[mainIdx];
        }
        // "够格当作液面"的最小簇规模：防止固体自带的水花被当成液面
        const size_t clusterMinCount = std::max<size_t>(2, mParticles.size() / 50);
        mRingCount.assign(mClusterCount.size(), 0);

        auto ClusterOf = [&](FLOAT_ level) -> int
        {
            for (size_t ci = 0; ci < mClusterCount.size(); ++ci)
            {
                if (level >= mClusterMin[ci] - FLOAT_(1e-4) && level <= mClusterMax[ci] + FLOAT_(1e-4))
                {
                    return (int)ci;
                }
            }
            return -1;
        };

        // 固体接触到的液面：环带 [0.8R, R+4h] 内液体粒子最多的簇的顶面；
        // 该簇太小时回落到全局主簇液面。环带内没有液体粒子 → 无水体接触。
        auto LocalSurface = [&](const Vec2_ &c, FLOAT_ R) -> FLOAT_
        {
            if (mClusterCount.empty())
            {
                return -std::numeric_limits<FLOAT_>::max();
            }
            const FLOAT_ inner = R * FLOAT_(0.8);
            const FLOAT_ outer = R + h * FLOAT_(4.0);
            std::fill(mRingCount.begin(), mRingCount.end(), (size_t)0);
            mWorld->mGridSearch.Get(c - Vec2_{outer, outer}, c + Vec2_{outer, outer}, mSearchV);
            unsigned int total = 0;
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
                const Vec2_ &pp = lp->pos;
                const FLOAT_ d = Modulus(pp - c);
                if (d < inner || d > outer)
                {
                    continue;
                }
                ++total;
                const int ci = ClusterOf(Dot(pp, up));
                if (ci >= 0 && (size_t)ci < mRingCount.size())
                {
                    ++mRingCount[(size_t)ci];
                }
            }
            if (total < 2)
            {
                return -std::numeric_limits<FLOAT_>::max(); // 环带内没有水体接触
            }
            size_t bestIdx = 0;
            for (size_t ci = 1; ci < mRingCount.size(); ++ci)
            {
                if (mRingCount[ci] > mRingCount[bestIdx])
                {
                    bestIdx = ci;
                }
            }
            if (mRingCount[bestIdx] >= clusterMinCount)
            {
                return mClusterTop[bestIdx]; // 固体接触到的水体自成规模 → 用它的液面
            }
            return mainSurfaceLevel; // 接触的只是一小簇（多为被带起来的水花）→ 回落到主池
        };
        const FLOAT_ noWater = -std::numeric_limits<FLOAT_>::max() * FLOAT_(0.5);

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

        // ── 圆：外接圆盘 + 局部接触水面浸没比例（保持现有模型）──────────────
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
            const FLOAT_ surfaceLevel = LocalSurface(c->pos, R);
            if (surfaceLevel <= noWater)
            {
                continue; // 周围没有水体接触
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
            // 该固体所在位置的局部接触水面
            const FLOAT_ surfaceLevel = LocalSurface(s->pos, s->radius);
            if (surfaceLevel <= noWater)
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
                    const Vec2_ quad[4] = {
                        CellToWorld((FLOAT_)x, (FLOAT_)y),
                        CellToWorld((FLOAT_)(x + 1), (FLOAT_)y),
                        CellToWorld((FLOAT_)(x + 1), (FLOAT_)(y + 1)),
                        CellToWorld((FLOAT_)x, (FLOAT_)(y + 1))
                    };
                    // Sutherland–Hodgman：保留 dot(p, up) <= surfaceLevel 的半边
                    mClipPoly.clear();
                    for (int k = 0; k < 4; ++k)
                    {
                        const Vec2_ &a = quad[k];
                        const Vec2_ &b = quad[(k + 1) & 3];
                        const FLOAT_ ha = Dot(a, up) - surfaceLevel;
                        const FLOAT_ hb = Dot(b, up) - surfaceLevel;
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
