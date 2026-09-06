#include "PhysicsLiquid.hpp"
#include "BaseCalculate.hpp"
#include <unordered_map>
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
        // 液体粒子归物理世界所有（添加到世界后由世界析构/删除），
        // 本类只保存指针；若 Clear() 未调用，粒子随世界一起销毁。
        mParticles.clear();
        mDensity.clear();
        mDensityNear.clear();
        mPrevPos.clear();
        mNeighborIndex.clear();
        mNeighborOffset.clear();
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
        mParticleMass = mass; // 记录粒子质量（浮力/面密度估算用）
        return p;
    }

    void PhysicsLiquid::AddGrid(Vec2_ center, int numX, int numY, FLOAT_ spacing, FLOAT_ mass, FLOAT_ friction)
    {
        mSpacing = spacing; // 记录粒子间距（浮力估算用）
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
        for (auto *p : mParticles)
        {
            if (p != nullptr)
            {
                mWorld->RemoveObject(p); // RemoveObject 内部会 delete，并从网格搜索中移除
            }
        }
        mParticles.clear();
        mDensity.clear();
        mDensityNear.clear();
        mPrevPos.clear();
        mNeighborIndex.clear();
        mNeighborOffset.clear();
        // 重置浮力估算基准，防止 Clear 后仅 AddParticle（未 AddGrid）时沿用旧值
        mParticleMass = 1.0f;
        mSpacing = 0.5f;
    }

    void PhysicsLiquid::RebuildNeighbors()
    {
        const size_t n = mParticles.size();
        mNeighborOffset.assign(n + 1, 0);
        mNeighborIndex.clear();
        if (n == 0)
        {
            return;
        }

        // 指针 → 液体粒子索引。演示规模（数百~数千粒子）下每帧重建哈希表足够；
        // 需要更高性能时可换为稳定句柄或按粒子 id 分配。
        std::unordered_map<const PhysicsParticle *, unsigned int> indexMap;
        indexMap.reserve(n);
        for (size_t i = 0; i < n; ++i)
        {
            indexMap[mParticles[i]] = (unsigned int)i;
        }

        const FLOAT_ h = param.h;
        const FLOAT_ h2 = h * h;

        for (size_t i = 0; i < n; ++i)
        {
            const Vec2_ pi = mParticles[i]->pos;
            // 复用世界网格搜索：返回查询框内的候选对象（可能包含多类型/距离外的，下面过滤）
            mWorld->mGridSearch.Get(pi, h, mSearchV);
            for (auto *o : mSearchV)
            {
                if (o == nullptr || o->PFGetType() != PhysicsObjectEnum::particle)
                {
                    continue;
                }
                auto it = indexMap.find((PhysicsParticle *)o);
                if (it == indexMap.end())
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

        const FLOAT_ h = param.h;
        const FLOAT_ invH = FLOAT_(1.0) / h;
        const FLOAT_ dt2 = time * time;
        const FLOAT_ maxPairD = param.maxPairDisplacement * h;

        if (mDensity.size() != n)
        {
            mDensity.resize(n, FLOAT_(0));
            mDensityNear.resize(n, FLOAT_(0));
            mPrevPos.resize(n, Vec2_{0, 0});
        }

        RebuildNeighbors();

        // ─── 双密度松弛（Clavet 2005 风格）─────────────────────────
        for (int iter = 0; iter < param.iterations; ++iter)
        {
            // 1) 密度（当前位置）
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

            // 2) 位移推进（成对：i 用自身的压力推 j，自身累加反作用位移）
            for (size_t i = 0; i < n; ++i)
            {
                const FLOAT_ invMi = mParticles[i]->invMass;
                if (invMi == FLOAT_(0))
                {
                    continue;
                }
                const FLOAT_ P = param.stiffness * std::max(mDensity[i] - param.restDensity, FLOAT_(0));
                const FLOAT_ Pnear = param.stiffnessNear * mDensityNear[i];
                Vec2_ shift{0, 0};
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
                    FLOAT_ Dmag = dt2 * (P * q + Pnear * q * q);
                    if (Dmag > maxPairD)
                    {
                        Dmag = maxPairD;
                    }
                    const Vec2_ D = d * (Dmag / r); // 从 i 指向 j 的位移向量
                    // 质量加权（等质量时各移动 D/2）
                    const Vec2_ moveJ = D * (invMj / totalInv);
                    mParticles[j]->pos += moveJ;
                    shift -= D * (invMi / totalInv);
                }
                // 单粒子单次迭代总位移安全钳制
                const FLOAT_ shiftLen = Modulus(shift);
                if (shiftLen > h * FLOAT_(0.5))
                {
                    shift *= (h * FLOAT_(0.5) / shiftLen);
                }
                mParticles[i]->pos += shift;
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
            const FLOAT_ len = Modulus(v);
            if (len > param.maxSpeed)
            {
                v *= (param.maxSpeed / len);
            }
            mParticles[i]->speed = v;
        }

        // ─── 粘滞（线性 + 二次）：成对消耗相对接近速度 ───────────────────
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
                const FLOAT_ I = time * q * (param.viscosity * u + param.viscosityQuadratic * u * u);
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
        FLOAT_ t = (density - low) / (high - low);
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

        // 亚像素级投影：步长 0.02。地图上限 0.55h（单帧 PBF 最大挤入量 0.5h，每帧必能完全推出）；
        // 形状上限覆盖整个刚体半径（半格 1.5 + 余量），否则被挤进方块中心 0.6+ 的粒子
        // 在 0.55 内 8 方向全是实心 → 永久卡死 → 引擎仲裁器以深穿透 bias 反复猛踢方块，
        // 这正是"方块 弹跳/炸飞"的确定性根源。
        // 0.02 步长把逐帧修正量压到真实穿透深度量级（0.005~0.05），
        // 回代速度只有 1~5 u/s 的微小震荡，且与压力/粘滞循环收敛成平静水面。
        const FLOAT_ stepBase = 0.02f;
        const FLOAT_ maxStepMap = param.h * FLOAT_(0.55);
        const Vec2_ stepDirs[8] = {
            {0, 1}, {0, -1}, {1, 0}, {-1, 0},
            {FLOAT_(0.7071), FLOAT_(0.7071)}, {FLOAT_(0.7071), FLOAT_(-0.7071)},
            {FLOAT_(-0.7071), FLOAT_(0.7071)}, {FLOAT_(-0.7071), FLOAT_(-0.7071)}
        };
        // 沿 8 个方向做细粒度最近空闲点搜索（取几何上最短位移）
        auto NearestFree = [&](Vec2_ from, auto&& IsFree, FLOAT_ maxLen) -> Vec2_
        {
            Vec2_ best = from;
            FLOAT_ bestLen = FLOAT_MAX;
            for (int di = 0; di < 8; ++di)
            {
                for (FLOAT_ len = stepBase; len <= maxLen + FLOAT_(1e-4); len += stepBase)
                {
                    const Vec2_ cand = from + stepDirs[di] * len;
                    if (IsFree(cand))
                    {
                        if (len < bestLen)
                        {
                            best = cand;
                            bestLen = len;
                        }
                        break;
                    }
                }
            }
            return bestLen < FLOAT_MAX ? best : from;
        };

        // 投影反作用"力矩"（接触点效应：水滴顶在木板右端 → τ = r×Δv → 木板旋转、
        // 水滴滑脱、分离坠落）；线反作用已关闭（=0）——任何"单滴水"的支撑反力都
        // 不可能托起整块木板，支撑/制动交给浮力与阻力。
        const FLOAT_ invDt = FLOAT_(1.0) / time;
        std::vector<FLOAT_> reactionTorqueShapes(shapes.size(), FLOAT_(0)); // 接触点力矩累积（τ = r × Δv）

        for (size_t i = 0; i < n; ++i)
        {
            PhysicsParticle *p = mParticles[i];
            Vec2_ pos = p->pos;

            // ── 地图：把粒子钳制出碰撞格子（细粒度最近空闲点，保持切向滑动）──
            if (map != nullptr && map->FMGetCollide(pos))
            {
                pos = NearestFree(pos, [&](const Vec2_ &cand) { return !map->FMGetCollide(cand); }, maxStepMap);
            }

            // ── 圆形固体：径向推出到圆外（无反作用：小圆会挖出包围空腔，易成"潜艇"）──
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

            // ── 网格形状固体：逐格判定（无 DropCollision 的越界钳制！）──
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
                    maxStepShape);
                if (s->invMass != FLOAT_(0) && s->mass > FLOAT_(0))
                {
                    // 接触点力矩：作用点 ≈ 粒子位置，力臂 = 接触点 − 质心。
                    // 反作用速度增量 reactV = −Δp/dt·(m_p/m_s)（方向为推开方向之反）。
                    const Vec2_ reactV = (newPos - pos) * (-invDt * (mParticleMass / s->mass));
                    const Vec2_ arm = pos - s->pos;
                    reactionTorqueShapes[si] += Cross(arm, reactV);
                }
                pos = newPos;
            }

            p->pos = pos;
        }

        // 施加钳制后的接触点反作用力矩
        for (size_t si = 0; si < shapes.size(); ++si)
        {
            PhysicsShape *s = shapes[si];
            if (s == nullptr)
            {
                continue;
            }
            // 单点支撑（如右端一滴水）会使 τ>0 → 木板逆时针旋转，水滴被挤出滑脱，
            // 木板与水滴分离并在重力下倒回液体——不会出现"单滴托起整块木板"。
            if (s->invMomentInertia > FLOAT_(0) && reactionTorqueShapes[si] != FLOAT_(0))
            {
                FLOAT_ dOmega = reactionTorqueShapes[si] * s->mass * s->invMomentInertia
                              * time * param.reactionTorqueGain;
                const FLOAT_ maxDOmega = param.maxTorqueImpulse; // 单帧角冲量上限（防止数值过冲）
                if (dOmega > maxDOmega)
                {
                    dOmega = maxDOmega;
                }
                else if (dOmega < -maxDOmega)
                {
                    dOmega = -maxDOmega;
                }
                s->angleSpeed += dOmega;
            }
        }
    }

    void PhysicsLiquid::ApplySolidCoupling(FLOAT_ time)
    {
        if (mSpacing <= FLOAT_(1e-6) || mParticleMass <= FLOAT_(0))
        {
            return;
        }
        const FLOAT_ g = Modulus(mWorld->GravityAcceleration);
        if (g < FLOAT_(1e-6))
        {
            return;
        }
        const Vec2_ up = -mWorld->GravityAcceleration / g; // 浮力方向 = 反重力方向
        const FLOAT_ areaPerParticle = mSpacing * mSpacing;
        const FLOAT_ rhoLiquid = mParticleMass / areaPerParticle; // 液体面密度（2D 中"密度"）

        // 液面锚定＝全局主水簇（粒子数最多的连续水体重心高度区）的顶面；
        // 接触判定＝固体环带内是否有水（"需要和水有接触就是水面"）。
        // 全局主水簇隔离了"固体自身携带的水花"：木板上升时投影会带起一小簇水，
        // 它永远比整池水少一个量级 → 即使跟着木板漂到 8 个单位外，液面依然
        // 是主水池的顶面 → 木板离池面越远浮力越小 → 落回水中，不会互相悬浮。
        // 分离的独立水洼（小簇且远离主池）会自成簇——若它成为该固体环带内
        // 唯一接触的水，则仍按主池液面处理（Demo 场景单水池，稳定性优先）。
        const FLOAT_ gapTh = std::max(FLOAT_(1.0), mSpacing * FLOAT_(2.0));
        FLOAT_ mainSurfaceLevel = -std::numeric_limits<FLOAT_>::max();
        {
            mHeightBuf.clear();
            for (auto *p : mParticles)
            {
                if (p != nullptr)
                {
                    mHeightBuf.push_back(Dot(p->pos, up));
                }
            }
            if (!mHeightBuf.empty())
            {
                std::sort(mHeightBuf.begin(), mHeightBuf.end());
                FLOAT_ bestTop = mHeightBuf.back();
                size_t bestCount = 0;
                size_t clusterStart = 0;
                for (size_t i = 1; i <= mHeightBuf.size(); ++i)
                {
                    const bool cut = (i == mHeightBuf.size()) ||
                                     (mHeightBuf[i] - mHeightBuf[i - 1] > gapTh);
                    if (cut)
                    {
                        const size_t cnt = i - clusterStart;
                        if (cnt > bestCount)
                        {
                            bestCount = cnt;
                            bestTop = mHeightBuf[i - 1];
                        }
                        clusterStart = i;
                    }
                }
                mainSurfaceLevel = bestTop;
            }
        }
        auto LocalSurface = [&](const Vec2_ &c, FLOAT_ R) -> FLOAT_
        {
            // 接触判定：环带 [0.8R, R+4h] 内至少 2 个水粒子
            const FLOAT_ inner = R * FLOAT_(0.8);
            const FLOAT_ outer = R + param.h * FLOAT_(4.0);
            mWorld->mGridSearch.Get(c, outer, mSearchV);
            unsigned int count = 0;
            for (auto *o : mSearchV)
            {
                if (o == nullptr || o->PFGetType() != PhysicsObjectEnum::particle)
                {
                    continue;
                }
                const FLOAT_ d = Modulus(o->PFGetPos() - c);
                if (d >= inner && d <= outer)
                {
                    ++count;
                    if (count >= 2)
                    {
                        break;
                    }
                }
            }
            if (count < 2)
            {
                return -std::numeric_limits<FLOAT_>::max(); // 环带内没有水体接触
            }
            return mainSurfaceLevel;
        };
        const FLOAT_ noWater = -std::numeric_limits<FLOAT_>::max() * FLOAT_(0.5);

        // 通用：上浮速度上限（防"活塞效应"把整池水抬离水域）+ 液体阻力/角阻尼
        auto ApplyCommon = [&](PhysicsParticle *solid, PhysicsAngle *angle, FLOAT_ frac)
        {
            const FLOAT_ upSpeed = Dot(solid->speed, up);
            if (upSpeed > param.maxRiseSpeed)
            {
                solid->speed -= up * (upSpeed - param.maxRiseSpeed);
            }
            const FLOAT_ dragRate = param.solidDrag * frac;
            solid->speed *= std::max(FLOAT_(0.0), FLOAT_(1.0) - dragRate * time);
            if (angle != nullptr)
            {
                const FLOAT_ angularRate = dragRate * param.angularDampingFactor;
                angle->angleSpeed *= std::max(FLOAT_(0.0), FLOAT_(1.0) - angularRate * time);
                // 角速度绝对上限：防带角速度穿过水面时无阻尼摇摆自激
                const FLOAT_ maxAngular = param.maxAngularSpeed;
                const FLOAT_ aspd = std::fabs(angle->angleSpeed);
                if (aspd > maxAngular)
                {
                    angle->angleSpeed *= (maxAngular / aspd);
                }
            }
        };

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
                ApplyCommon(c, c, param.contactDamping);
                continue;
            }
            const FLOAT_ rhoBody = c->mass / ((FLOAT_)M_PI * R * R);
            if (rhoBody <= FLOAT_(0))
            {
                continue;
            }
            const FLOAT_ accel = g * (rhoLiquid / rhoBody) * frac * param.buoyancy;
            const FLOAT_ force = accel * c->mass; // F = m·a
            buoyancyTotal += up * force;
            c->speed += up * (accel * time);
            ApplyCommon(c, c, frac);
        }

        // ── 网格形状：真实矩形水线裁剪（含扶正扭矩）────────────────────
        // 把形状的旋转矩形与水线做半边裁剪得到"浸没多边形"，浮力
        // F = ρ液·g·V排 作用在浸没形心（质心之外 → 稳心效应 → 扶正扭矩）：
        // 长方形木板倾斜时浸没形心偏向深水侧，扭矩把板子转回水平躺平。
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
            if (rhoBody <= FLOAT_(0))
            {
                continue;
            }
            // 该固体所在位置的局部接触水面（环带水体顶面）
            const FLOAT_ surfaceLevel = LocalSurface(s->pos, s->radius);
            if (surfaceLevel <= noWater)
            {
                continue; // 周围没有水体接触
            }

            // 矩形的 4 个角（相对质心的局部坐标 → 旋转 → 世界坐标）
            const Vec2_ localCorners[4] = {
                {-s->CentreMass.x, -s->CentreMass.y},
                {s->width - s->CentreMass.x, -s->CentreMass.y},
                {s->width - s->CentreMass.x, s->height - s->CentreMass.y},
                {-s->CentreMass.x, s->height - s->CentreMass.y}
            };
            Vec2_ corners[4];
            for (int k = 0; k < 4; ++k)
            {
                corners[k] = vec2angle(localCorners[k], s->angle) + s->pos;
            }

            // 水线裁剪：保留 dot(p, up) <= surfaceLevel 的半边（Sutherland–Hodgman）
            mClipPoly.clear();
            for (int k = 0; k < 4; ++k)
            {
                const Vec2_ &a = corners[k];
                const Vec2_ &b = corners[(k + 1) & 3];
                const FLOAT_ ha = Dot(a, up);
                const FLOAT_ hb = Dot(b, up);
                const bool aIn = (ha <= surfaceLevel);
                const bool bIn = (hb <= surfaceLevel);
                if (aIn)
                {
                    mClipPoly.push_back(a);
                }
                if (aIn != bIn)
                {
                    const FLOAT_ t = (surfaceLevel - ha) / (hb - ha);
                    mClipPoly.push_back(a + (b - a) * t);
                }
            }
            if (mClipPoly.size() < 3)
            {
                // 未浸没但仍与水接触 → 仅施加液体阻力/角阻尼（防止带角速度穿过水面摇摆）
                ApplyCommon(s, s, param.contactDamping);
                continue; // 未浸没
            }

            // 浸没多边形：有符号面积（鞋带公式）与形心
            FLOAT_ area2 = 0; // 2×面积（带符号）
            Vec2_ centroid{0, 0};
            const size_t m = mClipPoly.size();
            for (size_t k = 0; k < m; ++k)
            {
                const Vec2_ &p0 = mClipPoly[k];
                const Vec2_ &p1 = mClipPoly[(k + 1) % m];
                const FLOAT_ cross = p0.x * p1.y - p1.x * p0.y;
                area2 += cross;
                centroid += (p0 + p1) * cross;
            }
            const FLOAT_ area = std::fabs(area2) * FLOAT_(0.5);
            if (area <= FLOAT_(1e-4))
            {
                ApplyCommon(s, s, param.contactDamping);
                continue;
            }
            centroid /= (FLOAT_(3.0) * area2); // 形心公式（面积为带符号值）

            // 阿基米德力作用于浸没形心：线速度 + 角速度（扶正扭矩）
            const Vec2_ F = up * (rhoLiquid * g * area * param.buoyancy);
            buoyancyTotal += F;
            s->speed += F * (s->invMass * time);
            const Vec2_ r = centroid - s->pos; // 力臂：质心 → 浸没形心
            const FLOAT_ torque = Cross(r, F);
            if (s->invMomentInertia > FLOAT_(0))
            {
                FLOAT_ dOmega = torque * s->invMomentInertia * time;
                const FLOAT_ maxDOmega = param.maxTorqueImpulse; // 单帧角冲量上限（防止数值过冲）
                if (dOmega > maxDOmega)
                {
                    dOmega = maxDOmega;
                }
                else if (dOmega < -maxDOmega)
                {
                    dOmega = -maxDOmega;
                }
                s->angleSpeed += dOmega;
            }

            ApplyCommon(s, s, std::clamp(area / (FLOAT_)cells, FLOAT_(0), FLOAT_(1)));
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
