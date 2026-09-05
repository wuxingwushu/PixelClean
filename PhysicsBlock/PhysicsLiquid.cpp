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

        // 投影反作用（动量守恒：粒子被推开的 Δp ⇔ 固体受到的 Δv = −Δp/dt·(m_p/m_s)）
        // 只对动态固体生效；每帧每固体总反作用钳制。
        // 注意：
        // 1) 反作用不能太强——静压压缩也会让粒子每帧被投影出去（不全是固体的运动
        //    造成的），过强的反作用会压过浮力，使轻物体沉在水里；
        // 2) 小圆形（半径 < h）自身就能"挖"出包围空腔，周围粒子稀疏时浮力本就不够，
        //    此时反作用只会雪上加霜（上方水柱压着圆圈 → 反作用向下 → 永久潜艇），
        //    因此圆的反作用直接关闭，只保留方块/大形状的制动反作用。
        const FLOAT_ invDt = FLOAT_(1.0) / time;
        const FLOAT_ maxReactionShapes = 0.6f;
        const FLOAT_ maxReactionCircles = 0.0f;
        std::vector<Vec2_> reactionShapes(shapes.size(), Vec2_{0, 0});
        std::vector<Vec2_> reactionCircles(circles.size(), Vec2_{0, 0});

        for (size_t i = 0; i < n; ++i)
        {
            PhysicsParticle *p = mParticles[i];
            Vec2_ pos = p->pos;

            // ── 地图：把粒子钳制出碰撞格子（细粒度最近空闲点，保持切向滑动）──
            if (map != nullptr && map->FMGetCollide(pos))
            {
                pos = NearestFree(pos, [&](const Vec2_ &cand) { return !map->FMGetCollide(cand); }, maxStepMap);
            }

            // ── 圆形固体：径向推出到圆外 + 反作用 ──
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
                Vec2_ newPos;
                if (r < FLOAT_(1e-4))
                {
                    newPos = c->pos + Vec2_{FLOAT_(0.01), c->radius};
                }
                else
                {
                    newPos = c->pos + d * ((c->radius + FLOAT_(0.01)) / r);
                }
                if (c->invMass != FLOAT_(0) && c->mass > FLOAT_(0))
                {
                    reactionCircles[ci] += (newPos - pos) * (-invDt * (mParticleMass / c->mass));
                }
                pos = newPos;
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
                    reactionShapes[si] += (newPos - pos) * (-invDt * (mParticleMass / s->mass));
                }
                pos = newPos;
            }

            p->pos = pos;
        }

        // 施加钳制后的反作用速度
        for (size_t si = 0; si < shapes.size(); ++si)
        {
            if (shapes[si] == nullptr)
            {
                continue;
            }
            Vec2_ &v = reactionShapes[si];
            const FLOAT_ len = Modulus(v);
            if (len > maxReactionShapes)
            {
                v *= (maxReactionShapes / len);
            }
            shapes[si]->speed += v;
        }
        for (size_t ci = 0; ci < circles.size(); ++ci)
        {
            if (circles[ci] == nullptr)
            {
                continue;
            }
            Vec2_ &v = reactionCircles[ci];
            const FLOAT_ len = Modulus(v);
            if (len > maxReactionCircles)
            {
                v *= (maxReactionCircles / len);
            }
            circles[ci]->speed += v;
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

        // 全局液面（整池水位）：所有液体粒子沿 up 方向的最大投影 + 池底约束。
        // 用全局水位而非"固体上方的局部水柱"：局部测量会被固体挤开的空腔骗过
        // （轻物沉到水里却永远"量"不到液面，从而浮不起来）；整池水位只有一个值，
        // 对池内任何固体都是一致的真值，固体浸没比例 = (液面 − 固体底部)/直径。
        FLOAT_ surfaceLevel = -std::numeric_limits<FLOAT_>::max();
        for (auto *p : mParticles)
        {
            if (p == nullptr)
            {
                continue;
            }
            const FLOAT_ h = Dot(p->pos, up);
            if (h > surfaceLevel)
            {
                surfaceLevel = h;
            }
        }
        if (surfaceLevel <= -std::numeric_limits<FLOAT_>::max() * FLOAT_(0.5))
        {
            return; // 没有液体
        }

        auto ApplyTo = [&](PhysicsParticle *solid, PhysicsAngle *angle)
        {
            if (solid == nullptr || solid->invMass == FLOAT_(0))
            {
                return; // 静态/运动学固体不受浮力（碰撞依然阻挡液体）
            }
            const FLOAT_ R = solid->PFGetCollisionR();
            if (R <= FLOAT_(0))
            {
                return;
            }
            const Vec2_ c = solid->PFGetPos();

            // 浸没比例 = (全局液面 − 固体底部) / 直径（0..1）
            const FLOAT_ bottomLevel = Dot(c, up) - R;
            const FLOAT_ subDepth = surfaceLevel - bottomLevel;
            const FLOAT_ frac = std::clamp(subDepth / (FLOAT_(2.0) * R), FLOAT_(0), FLOAT_(1));
            if (frac <= FLOAT_(0.01))
            {
                return;
            }

            // 阿基米德浮力：Δv = g·(ρ_液/ρ_体)·浸没比例·dt·浮力倍率
            const FLOAT_ rhoBody = solid->PFGetMass() / ((FLOAT_)M_PI * R * R);
            if (rhoBody <= FLOAT_(0))
            {
                return;
            }
            const FLOAT_ accel = g * (rhoLiquid / rhoBody) * frac * param.buoyancy;
            solid->speed += up * (accel * time);

            // 上浮速度上限：浮力只用于"撑住+缓慢抬起"，不产生 "活塞效应"
            // （方块快速上浮时投影会把液体一起抬升，液面永远在方块上方 → 浮力不消失 →
            // 正反馈把整池水带离水域）。限制上浮速度后，水因重力回落而与固体分离，
            // 浸没比例随之下降，浮力收敛到平衡点。
            const FLOAT_ upSpeed = Dot(solid->speed, up);
            if (upSpeed > param.maxRiseSpeed)
            {
                solid->speed -= up * (upSpeed - param.maxRiseSpeed);
            }

            // 液体阻力：浸没越深衰减越快；角速度被液体强阻尼（5×，抑制方块缓慢旋转漂移）
            const FLOAT_ dragRate = param.solidDrag * frac;
            solid->speed *= std::max(FLOAT_(0.0), FLOAT_(1.0) - dragRate * time);
            if (angle != nullptr)
            {
                const FLOAT_ angularRate = dragRate * FLOAT_(5.0);
                angle->angleSpeed *= std::max(FLOAT_(0.0), FLOAT_(1.0) - angularRate * time);
            }
        };

        for (auto *s : mWorld->PhysicsShapeS)
        {
            ApplyTo(s, s);
        }
        for (auto *c : mWorld->PhysicsCircleS)
        {
            ApplyTo(c, c);
        }
    }

}
