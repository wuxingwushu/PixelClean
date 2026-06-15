# 方案 E 混合驱动移动 —— 完整执行规划

> 目标：把 `MazeMods` / `TankTrouble` / `UnlimitednessMapMods` 三个模组的玩家与 NPC 移动，从当前的"伪力累加速度"（`PFSpeed() += Δ`）重构为**3A 级混合驱动架构**。
>
> 已确认的规格（来自需求确认）：
> - **完整三态状态机切换**（Controlled / Ragdoll / Frozen）—— 3A 级
> - **统一抽取 `MovementComponent`**（玩家 + NPC 共用）
> - **引擎新增 API**：用户级 `ApplyImpulse` + `ComputeMoveForce` 辅助函数
> - **NPC AI 升级**：受击硬直 + 击退反应

---

## 〇、术语与现状速览（重构前的基线）

| 概念 | 当前实现 | 位置 |
|---|---|---|
| 平移驱动 | `PFSpeed() += dir * mMoveForce * FPStime`（伪力，实为速度增量） | `MazeMods.cpp:465`、`TankTrouble.cpp:433`、`UnlimitednessMapMods.cpp:270`、`NPC.cpp:238/271/274/402` |
| 旋转驱动 | `angle = m_angle` 直接赋值（瞬切，无角速度） | 各 GameMod GameLoop + `NPC.cpp:206/237/261/299/400` |
| 刹车 | 靠物理体 `friction = 1.0` + 全局阻尼 `Define_MinSpoilage = 0.9994` | `GamePlayer.cpp:85`、`BaseDefine.h:10` |
| 击退 | **完全不存在**（子弹只破像素，爆炸只破像素） | `GamePlayer.cpp:20-56 TankHitByBullet`、`Arms.cpp:201-248 Explode` |
| NPC 受伤 | 仅状态切换到 `C_Injury`（朝向玩家+计时），无物理冻结/击退 | `NPC.cpp:296-318` |
| 引擎冲量 API | **不存在**用户级 API；内部 `ApplyImpulse()` 散落在 4 个 Arbiter 子类 | `PhysicsBaseArbiter.cpp:118/289/453/598` |

---

## 一、目标架构总览

```
┌─────────────────────────────────────────────────────────────────┐
│                       游戏逻辑层 (GameMod / NPC AI)              │
│  MazeMods / TankTrouble / UnlimitednessMapMods   |   NPC FSM     │
│         ↓ 仅设置"意图"                          ↓ 仅设置"意图"    │
│   SetMoveInput(dir) / SetLookAngle(θ)       SetMoveTarget(pathPt)│
└──────────────────────────┬──────────────────────────────────────┘
                           │ 持有 (1:1)
┌──────────────────────────▼──────────────────────────────────────┐
│              MovementComponent（新建，玩家+NPC 共用）            │
│                                                                  │
│  ┌─────────────── MovementMode 状态机 ───────────────┐          │
│  │  Controlled  ←──────→  Ragdoll  ←──────→  Frozen   │          │
│  │  (走位:目标速度)    (被击飞:真物理)    (硬直:冻结)  │          │
│  └────────────────────────────────────────────────────┘          │
│                                                                  │
│  Update(dt):                                                     │
│    if Controlled: ComputeMoveForce → body.AddForce               │
│    if Ragdoll:   速度衰减判定 → 自动切回 Controlled               │
│    if Frozen:    清零目标速度(可衰减角速度)                       │
└──────────────────────────┬──────────────────────────────────────┘
                           │ 调用
┌──────────────────────────▼──────────────────────────────────────┐
│            物理引擎层 PhysicsBlock（新增 API）                    │
│  PhysicsFormwork::ApplyImpulse(impulse) / (impulse, worldPoint)  │
│  PhysicsFormwork::ApplyTorqueImpulse(torqueImpulse)              │
│  ComputeMoveForce(current, desired, mass, dt)  [工具函数]        │
│  PhysicsSpeed / PhysicsPos  (现有积分:半隐式 Euler)              │
└─────────────────────────────────────────────────────────────────┘
```

**核心思想**：上层只产生"意图"（要往哪走、朝哪看），`MovementComponent` 负责把意图翻译成合适的物理操作（施力 / 放手让物理接管 / 冻结），并管理三种驱动模式之间的切换。

---

## 二、文件改动清单（总览）

| 层 | 文件 | 操作 | 说明 |
|---|---|---|---|
| 引擎 | `PhysicsBlock/PhysicsFormwork.hpp` | **改** | 新增 3 个虚函数：`ApplyImpulse`×2、`ApplyTorqueImpulse` |
| 引擎 | `PhysicsBlock/PhysicsParticle.hpp/.cpp` | **改** | 实现质心冲量（角冲量 no-op） |
| 引擎 | `PhysicsBlock/PhysicsAngle.hpp/.cpp` | **改** | 实现带受力点冲量 + 角冲量 |
| 引擎 | `PhysicsBlock/PhysicsKinematic.cpp` | **改** | 切换 kinematic 时唤醒（`StaticNum=0`） |
| 引擎 | `PhysicsBlock/MovementUtil.hpp` | **新建** | `ComputeMoveForce` 等工具函数 |
| 引擎 | `PhysicsBlock/PhysicsWorld.hpp/.cpp` | **改** | 暴露 `ApplyImpulse` 转发 + 唤醒辅助 |
| 组件 | `Character/MovementComponent.h/.cpp` | **新建** | 三态状态机 + Update 逻辑 |
| 组件 | `Character/CharacterMovement.h`（可选） | **新建** | 玩家/NPC 共用的配置参数集 |
| 角色 | `Character/GamePlayer.h/.cpp` | **改** | 持有 `MovementComponent`；接入击退 |
| 角色 | `Character/NPC.h/.cpp` | **改** | 移除 `mMoveForce` 叠加；接入 `MovementComponent`；新增硬直 |
| 武器 | `Arms/Arms.cpp` | **改** | `Explode` 中施加爆炸击退 |
| 武器 | `Character/GamePlayer.cpp` | **改** | `TankHitByBullet` 中施加子弹击退 |
| GameMod | `GameMods/MazeMods/MazeMods.cpp` | **改** | `KeyDown`/`GameLoop` 改用 `MovementComponent` |
| GameMod | `GameMods/TankTrouble/TankTrouble.cpp` | **改** | 同上 |
| GameMod | `GameMods/UnlimitednessMapMods/UnlimitednessMapMods.cpp` | **改** | 同上 |
| 文档 | `docs/玩家移动驱动方式分析.md` | 已有 | 背景，无需改 |

---

## 三、分阶段执行步骤

> 分 5 个阶段，**每个阶段都可独立编译运行并通过测试**才进入下一阶段。每个阶段末尾都给出"验收标准"。

---

### 阶段 1：引擎层 API 扩展（基础设施）

**目标**：让物理引擎具备"用户施加冲量"和"反推施力"的能力。此阶段**不改动任何游戏逻辑**，纯增量。

#### 步骤 1.1 在 `PhysicsFormwork.hpp` 声明冲量虚函数

在基类（`PhysicsBlock/PhysicsFormwork.hpp`，现有纯虚 `PFSpeed/PFAngleSpeed` 附近）新增：

```cpp
// 用户级冲量 API（阶段1新增）
virtual void ApplyImpulse(const Vec2_& impulse) {};                                   // 质心冲量 Δv = invMass * impulse
virtual void ApplyImpulse(const Vec2_& impulse, const Vec2_& worldPoint) {};           // 带受力点（产生角速度）
virtual void ApplyTorqueImpulse(FLOAT_ torqueImpulse) {};                              // 纯角冲量
```

> 用带默认空实现（非纯虚），避免破坏 `PhysicsKinematic`/地图类等不旋转的派生。

#### 步骤 1.2 `PhysicsParticle` 实现质心冲量

`PhysicsParticle.hpp` 声明 `override`，`PhysicsParticle.cpp` 实现：

```cpp
void PhysicsParticle::ApplyImpulse(const Vec2_& impulse) {
    if (invMass == 0) return;        // 静态/kinematic 不响应
    speed += invMass * impulse;
    StaticNum = 0;                    // 唤醒（见步骤1.5）
}
void PhysicsParticle::ApplyTorqueImpulse(FLOAT_) { /* no-op：平动体无旋转 */ }
// 带受力点版本：平动体也当作质心处理（不产生自旋）
void PhysicsParticle::ApplyImpulse(const Vec2_& impulse, const Vec2_&) {
    ApplyImpulse(impulse);
}
```

#### 步骤 1.3 `PhysicsAngle` 实现带受力点冲量

复用 Arbiter 内部现成公式（`PhysicsBaseArbiter.cpp:140-149`）。`PhysicsAngle.cpp`：

```cpp
void PhysicsAngle::ApplyImpulse(const Vec2_& impulse) {
    if (invMass == 0) return;
    speed += invMass * impulse;
    StaticNum = 0;
}
void PhysicsAngle::ApplyImpulse(const Vec2_& impulse, const Vec2_& worldPoint) {
    if (invMass == 0) return;
    speed += invMass * impulse;
    Vec2_ r = worldPoint - pos;
    angleSpeed += invMomentInertia * (r.x * impulse.y - r.y * impulse.x);  // 二维叉积
    StaticNum = 0;
}
void PhysicsAngle::ApplyTorqueImpulse(FLOAT_ torqueImpulse) {
    if (invMomentInertia == 0) return;
    angleSpeed += invMomentInertia * torqueImpulse;
    StaticNum = 0;
}
```

> `Cross(r, impulse) = r.x*impulse.y - r.y*impulse.x`，定义在 `BaseCalculate.hpp:433`，可直接调用。

#### 步骤 1.4 新建 `MovementUtil.hpp`（目标速度反推工具）

`PhysicsBlock/MovementUtil.hpp`：

```cpp
#pragma once
#include "BaseStruct.hpp"
#include "BaseDefine.h"

namespace PhysicsBlock {
    // 给定当前速度、目标速度、质量、dt，反推本帧应施加的力（用于 Controlled 模式）。
    // 原理来自 PhysicsParticle::PhysicsSpeed：speed_n+1 = damp * speed_n + dt*(g + invMass*force)
    // 忽略重力（重力由 World 统一施加），并补偿阻尼。
    inline Vec2_ ComputeMoveForce(const Vec2_& currentSpeed, const Vec2_& desiredSpeed,
                                  FLOAT_ mass, FLOAT_ dt) {
        if (mass <= 0) return {0, 0};
#if Define_MinSpoilageBool
        FLOAT_ damp = Define_MinSpoilage;
#else
        FLOAT_ damp = 1.0f;
#endif
        // 反推：desired = damp*current + dt*force/mass  =>  force = mass*(desired - damp*current)/dt
        Vec2_ compensated{ desiredSpeed.x - damp * currentSpeed.x,
                           desiredSpeed.y - damp * currentSpeed.y };
        return { compensated.x * mass / dt, compensated.y * mass / dt };
    }

    // 指数平滑系数（帧率无关）：lerp 系数 k = 1 - exp(-rate*dt)
    inline float SmoothK(float rate, float dt) {
        return 1.0f - std::exp(-rate * dt);
    }
}
```

> **关键**：`ComputeMoveForce` 走 `AddForce` 路径，因此**自动尊重 `invMass` 和 kinematic 状态**——当物体处于 kinematic/Ragdoll 时 `AddForce` 因 `invMass==0` 静默失效，符合预期。

#### 步骤 1.5 唤醒机制（防止冲量被休眠吞掉）

引擎多处用 `StaticNum > 10` 跳过碰撞检测（`PhysicsWorld.cpp:351/418/475/530`）。施加冲量/力后必须唤醒。

- 已在步骤 1.2/1.3 的 `ApplyImpulse` 里 `StaticNum = 0`。
- **额外**：在 `PhysicsParticle::AddForce`（`PhysicsParticle.cpp:51`）里也加 `StaticNum = 0;`，确保 `ComputeMoveForce`→`AddForce` 也能唤醒。

#### 步骤 1.6 `PhysicsWorld` 转发（可选但推荐，便于线程安全收敛）

`PhysicsWorld.hpp`：

```cpp
// 通过基类指针施加冲量（转发到对象自身的 ApplyImpulse）
void ApplyImpulse(PhysicsFormwork* obj, const Vec2_& impulse);
void ApplyImpulse(PhysicsFormwork* obj, const Vec2_& impulse, const Vec2_& worldPoint);
```

`PhysicsWorld.cpp` 直接转发 `obj->ApplyImpulse(...)`。**好处**：未来若引擎改多线程，可在这里加锁。

#### ✅ 阶段 1 验收标准
- [ ] 引擎库可编译通过，无新增警告。
- [ ] 写一个最小测试：创建一个 `PhysicsShape`，调 `ApplyImpulse({100,0})`，验证 `speed.x > 0` 且位置在若干帧后向 +x 移动。
- [ ] 调 `ComputeMoveForce({0,0}, {50,0}, mass=256, dt=0.016)` 返回正数力，施加后物体能逼近 50 的速度。
- [ ] **现有游戏行为完全不变**（因为还没接入游戏逻辑）。

---

### 阶段 2：构建 `MovementComponent`（核心组件）

**目标**：新建独立的移动组件，实现三态状态机，**先用单元测试验证**，暂不接入 GameMod/NPC。

#### 步骤 2.1 定义三态与配置

`Character/MovementComponent.h`：

```cpp
#pragma once
#include "../PhysicsBlock/PhysicsFormwork.hpp"
#include "../PhysicsBlock/MovementUtil.hpp"

namespace GAME {

// 驱动模式
enum class MovementMode {
    Controlled,   // 走位：用 AddForce 追踪目标速度（手感精确）
    Ragdoll,      // 被击飞：完全交给物理（速度自然衰减后切回 Controlled）
    Frozen        // 硬直：冻结移动（可保留物理碰撞，但清零目标速度）
};

// 配置参数（玩家/NPC 各持一份，可微调）
struct MovementConfig {
    float MaxSpeed       = 120.0f;  // 目标速度上限（像素/秒）
    float Acceleration   = 10.0f;   // 速度趋近率（越大越灵敏，松键停得越快）
    float TurnRate       = 12.0f;   // 朝向趋近率（消除 angle 瞬切）
    float RagdollRecoverSpeed = 20.0f; // Ragdoll 速度低于此值切回 Controlled
    float RagdollMinTime  = 0.3f;   // Ragdoll 最短持续时间（防抖）
};

class MovementComponent {
public:
    explicit MovementComponent(PhysicsBlock::PhysicsFormwork* body);

    // ===== 上层"意图"接口（每帧调用）=====
    void SetMoveInput(const Vec2_& worldDir);   // 期望移动方向（已归一化；零向量=停）
    void SetLookAngle(float targetAngle);        // 期望朝向（弧度）
    void ClearIntent();                          // 清空意图（用于 Frozen）

    // ===== 模式控制 =====
    void SetMode(MovementMode mode);
    MovementMode GetMode() const { return mMode; }

    // ===== 每帧更新（在 PhysicsEmulator 之前调用）=====
    void Update(float dt);

    // ===== 配置 =====
    MovementConfig& Config() { return mConfig; }

private:
    void UpdateControlled(float dt);
    void UpdateRagdoll(float dt);
    void UpdateFrozen(float dt);

    PhysicsBlock::PhysicsFormwork* mBody;
    MovementConfig  mConfig;
    MovementMode    mMode = MovementMode::Controlled;
    Vec2_           mDesiredSpeed = {0, 0};   // 由 SetMoveInput 计算
    float           mDesiredAngle = 0.0f;
    bool            mHasLookTarget = false;
    float           mRagdollTimer  = 0.0f;    // Ragdoll 持续计时
};

} // namespace GAME
```

#### 步骤 2.2 实现 Update（三个模式）

`Character/MovementComponent.cpp` 关键逻辑：

```cpp
void MovementComponent::SetMoveInput(const Vec2_& worldDir) {
    // worldDir 归一化后 × MaxSpeed = 目标速度
    float len = std::sqrt(worldDir.x*worldDir.x + worldDir.y*worldDir.y);
    if (len < 1e-4f) { mDesiredSpeed = {0, 0}; return; }
    mDesiredSpeed = { worldDir.x / len * mConfig.MaxSpeed,
                      worldDir.y / len * mConfig.MaxSpeed };
}

void MovementComponent::Update(float dt) {
    switch (mMode) {
        case MovementMode::Controlled: UpdateControlled(dt); break;
        case MovementMode::Ragdoll:    UpdateRagdoll(dt);    break;
        case MovementMode::Frozen:     UpdateFrozen(dt);     break;
    }
}

void MovementComponent::UpdateControlled(float dt) {
    // —— 平移：反推施力（自动尊重 mass/kinematic，且有阻尼补偿）——
    FLOAT_ mass = mBody->PFGetMass();
    if (mass > 0 && mass < FLOAT_MAX) {
        Vec2_ f = PhysicsBlock::ComputeMoveForce(
            mBody->PFSpeed(), mDesiredSpeed, mass, dt);
        mBody->AddForce(f);          // 注意：需在 PhysicsFormwork 加纯虚 AddForce，见下文"注意"
    }
    // —— 朝向：指数平滑（消除瞬切）——
    if (mHasLookTarget) {
        // 处理角度环绕（取最短弧）
        float k = PhysicsBlock::SmoothK(mConfig.TurnRate, dt);
        // ... lerpAngle 实现（见步骤2.3）...
    }
}
```

> **注意 —— `AddForce` 的基类缺口**：
> `PhysicsFormwork` 基类**没有** `AddForce` 纯虚声明，且 `PhysicsAngle::AddForce` 是**非虚**重载（阶段0探索已确认）。这会导致 `MovementComponent` 通过 `PhysicsFormwork*` 调用 `AddForce` 编译失败。
>
> **解决（属于阶段1的补丁，建议合并到阶段1）**：在 `PhysicsFormwork.hpp` 加 `virtual void AddForce(const Vec2_&) {}; virtual void AddForce(const Vec2_&, const Vec2_&) {};`，并给 `PhysicsParticle`/`PhysicsAngle` 的现有 `AddForce` 加 `override`。

```cpp
void MovementComponent::UpdateRagdoll(float dt) {
    // 不施加任何力，让物理自然演化（被击飞、被重力影响、被碰撞弹开）
    mRagdollTimer += dt;
    if (mRagdollTimer < mConfig.RagdollMinTime) return;
    // 速度衰减到阈值以下 → 切回 Controlled
    Vec2_ v = mBody->PFSpeed();
    if (std::sqrt(v.x*v.x + v.y*v.y) < mConfig.RagdollRecoverSpeed) {
        mRagdollTimer = 0;
        SetMode(MovementMode::Controlled);
    }
}

void MovementComponent::UpdateFrozen(float dt) {
    // 硬直：目标速度归零（让摩擦/阻尼把它停下来），不主动施力
    mDesiredSpeed = {0, 0};
    // 可选：保留角速度让角色"被打转"，但平移停下。按需求决定。
}
```

#### 步骤 2.3 角度环绕处理（工具函数）

放在 `MovementComponent.cpp` 内部：

```cpp
static float LerpAngle(float current, float target, float k) {
    float diff = std::fmod(target - current + 3.14159265f*3, 2*3.14159265f) - 3.14159265f;
    return current + diff * k;
}
```

#### ✅ 阶段 2 验收标准
- [ ] `MovementComponent` 可独立编译。
- [ ] 单元测试：Controlled 模式下，`SetMoveInput({1,0})`，若干帧后 `speed` 逼近 `MaxSpeed` 且有上限不会发散。
- [ ] 单元测试：松开（`SetMoveInput({0,0})`）后，速度在 ~0.3 秒内衰减到接近 0。
- [ ] 单元测试：切到 Ragdoll，施加一个冲量，物体飞出；速度降到阈值后自动切回 Controlled。
- [ ] 单元测试：Frozen 模式下，即使调用 `SetMoveInput`，物体也不会主动移动。

---

### 阶段 3：接入玩家（三个 GameMod）

**目标**：把三个 GameMod 的玩家移动从 `PFSpeed() += PlayerForce` 切换到 `MovementComponent`。**每改完一个模组就跑一遍游戏验证**，再改下一个。

#### 步骤 3.1 `GamePlayer` 持有 `MovementComponent`

`Character/GamePlayer.h`：
- 新增成员 `MovementComponent* mMovement = nullptr;`
- 新增访问器 `MovementComponent* GetMovement() { return mMovement; }`

`Character/GamePlayer.cpp` 构造函数末尾（`mSquarePhysics->AddObject` 之后）：
```cpp
mMovement = new MovementComponent(mObjectCollision);
```
析构函数：`delete mMovement;`

#### 步骤 3.2 改造玩家输入：从"累加速度增量"改为"累加方向投票"

> ✅ **已确认的前提**：`KeyDown` 在按键**按住期间每帧都触发一次**（每个被按住的键每帧派发一次 KeyDown 事件）。

**当前机制**（`MazeMods.cpp:292-308`）：每帧 `PlayerForce.x += 100*FPStime`（累加速度增量），GameLoop 里 `PFSpeed += PlayerForce` 后 `PlayerForce = {0,0}`（每帧清零）。本质是"每帧累加、每帧消费清零"的循环。

**新机制**（语义对齐，只是把"速度增量"换成"方向计数"）：用 `mMoveInput` 替代 `PlayerForce`，每帧累加方向投票、GameLoop 消费后清零。**完全不依赖 KeyUp**。

GameMod 加一个成员：
```cpp
// MazeMods.h（及另外两个 GameMod 的 .h）私有成员
glm::vec2 mMoveInput = {0, 0};   // 本帧移动方向投票（KeyDown 累加，GameLoop 清零）
```

`KeyDown` 改为（以 MazeMods 为例，三个 GameMod 相同模式）：
```cpp
void MazeMods::KeyDown(GameKeyEnum moveDirection) {
    switch (moveDirection) {
    case GameKeyEnum::MOVE_LEFT:  mMoveInput.x -= 1; break;   // 每帧触发一次 → 本帧 -1
    case GameKeyEnum::MOVE_RIGHT: mMoveInput.x += 1; break;
    case GameKeyEnum::MOVE_FRONT: mMoveInput.y += 1; break;
    case GameKeyEnum::MOVE_BACK:  mMoveInput.y -= 1; break;
    // ESC / Key_1 / Key_2 等非移动键保持原样
    ...
    }
}
```

**为什么不需要 KeyUp —— 四种情形验证**：

| 情形 | 每帧发生的事 | `mMoveInput`（GameLoop 读取时） | 结果 |
|---|---|---|---|
| 按住 LEFT | 每帧 KeyDown(MOVE_LEFT) 触发一次 | `{-1, 0}` | 持续左移 ✓ |
| 松开 LEFT | 该帧不再触发 KeyDown | `{0, 0}`（上帧清零后保持） | 停止施力，靠阻尼减速 ✓ |
| 同时按 LEFT+RIGHT | 同帧两个 KeyDown 都触发，`-1+1` | `{0, 0}` | 互相抵消，不动 ✓ |
| 同时按 LEFT+FRONT | 两个 KeyDown 都触发 | `{-1, 1}` | 归一化为对角线，速度=MaxSpeed（双键不加速）✓ |

> 💡 **关键**：这个"每帧累加、每帧清零"的模式与原 `PlayerForce` 机制**完全同构**，唯一区别是数值从"速度增量（像素/秒² × dt）"变成了"方向计数（-1/0/1）"，帧时间补偿 `× FPStime` 不再写在这里，而是移到 `MovementComponent::Update` 内部的 `ComputeMoveForce` 里统一处理。所以**语义更干净，行为完全可预测**。
>
> 即使某帧引擎对同一个键多次派发 KeyDown（如 `mMoveInput.x = -2`），`SetMoveInput` 内部的归一化也会把 `{-2,0}` 规整为 `{-1,0}`，方向不受影响 —— 方案对异常派发是鲁棒的。

#### 步骤 3.3 改造 GameLoop

`MazeMods.cpp:464-468`（替换原来的 `PFSpeed() += PlayerForce`）：
```cpp
// 把意图喂给 MovementComponent
mGamePlayer->GetMovement()->SetMoveInput(Vec2_{ mMoveInput.x, mMoveInput.y });
mGamePlayer->GetMovement()->SetLookAngle(m_angle);   // 鼠标朝向（平滑跟随，消除瞬切）
mGamePlayer->GetMovement()->Update(TOOL::FPStime);   // 在物理积分前更新
// mMoveInput 在帧末清零（若用轮询模式则不需要）
mMoveInput = {0, 0};

TOOL::mTimer->StartTiming(u8"物理模拟 ", true);
mSquarePhysics->PhysicsEmulator(TOOL::FPStime);
TOOL::mTimer->StartEnd();
```

对 `TankTrouble.cpp:432-436`、`UnlimitednessMapMods.cpp:269-271` 做同样改造。**删除 `PlayerForce` 变量**。

#### 步骤 3.4 相机跟随保持不变
`mCamera->setCameraPos(mGamePlayer->GetObjectCollision()->pos)` 仍从物理体读位置，无需改。

#### ✅ 阶段 3 验收标准
- [ ] 三个模组玩家都能正常移动，**手感改进**：松键即停、有平滑加速、朝向跟随鼠标不瞬切。
- [ ] 长按方向键**速度不再无限增长**（被 `MaxSpeed` 限制）。
- [ ] 高低帧率下最大速度一致（在 30/60/120 FPS 下分别测试）。
- [ ] 与墙壁碰撞正常（不会因速度上限改变而穿墙 —— 实际上速度上限让穿墙**更不可能**）。
- [ ] 鼠标拖拽场景物体（`AddForce` 那条路径）依然正常。

---

### 阶段 4：接入 NPC + 击退系统（战斗手感）

**目标**：把 NPC 的 4 处 `PFSpeed() +=` 改用 `MovementComponent`，并实现子弹击退、爆炸击飞、NPC 硬直。

#### 步骤 4.1 NPC 持有 `MovementComponent`（复用 GamePlayer 的）

NPC 本身就持有一个 `GamePlayer* mNPC`（`NPC.h:80`），而阶段 3 已经在 `GamePlayer` 里挂了 `MovementComponent`。**直接复用**：`mNPC->GetMovement()`。

#### 步骤 4.2 改造 NPC 各状态的移动调用

| 状态 | 原代码 | 新代码 |
|---|---|---|
| Patrol `NPC.cpp:235-238` | `angle=YAngle; PFSpeed() += qianjinfang*...` | `GetMovement()->SetLookAngle(YAngle); GetMovement()->SetMoveInput(qianjinfang);` |
| Attack 后退 `NPC.cpp:271` | `PFSpeed() += -moveDir*...` | `SetMoveInput(Vec2_{-moveDir.x, -moveDir.y})` |
| Attack 靠近 `NPC.cpp:274` | `PFSpeed() += moveDir*...` | `SetMoveInput(Vec2_{moveDir.x, moveDir.y})` |
| GoToLocation `NPC.cpp:402` | `PFSpeed() += dir*...` | `SetMoveInput(Vec2_{dir.x, dir.y})` |

**NPC 的 `Update` 调用时机**：放在 `NPC::Event`（`NPC.cpp:410-420`）的 FSM 处理之后、`mNPC->UpData()` 之前：
```cpp
void NPC::Event(int Frame, float time) {
    mTime += time;
    FPSTime = time;
    NPCFSM->Get()->process_event(S_Event{});   // FSM 内部调 SetMoveInput/SetLookAngle
    mNPC->GetMovement()->Update(FPSTime);       // ★新增：统一更新移动
    mNPC->UpData();
    mNPC->setGamePlayerMatrix(time, Frame);
}
```

> 这样 `SetMoveInput` 是"意图声明"（无副作用），`Update` 集中施力，干净分层。

#### 步骤 4.3 NPC 硬直（Frozen 模式）

改造 `NPC::Injury`（`NPC.cpp:296-318`）：
```cpp
bool NPC::Injury() {
    // 进入硬直：冻结移动
    mNPC->GetMovement()->SetMode(MovementMode::Frozen);

    int flags = GetSensoryMessages();
    mNPC->GetMovement()->SetLookAngle(wanjiaAngle);  // 仍可转向（朝向玩家）

    if (flags & SensoryMessages_Visible) {
        if (flags & SensoryMessages_Range) {
            mNPC->GetMovement()->SetMode(MovementMode::Controlled);  // 解冻
            NPCFSM->Get()->process_event(S_Attack{});
        } else {
            mNPC->GetMovement()->SetMode(MovementMode::Controlled);
            mTime = mPathfindingCycle + 1.0f;
            NPCFSM->Get()->process_event(S_Chasing{});
        }
        return true;
    }
    if (mTime > 1.0f) {  // 硬直 1 秒
        mNPC->GetMovement()->SetMode(MovementMode::Controlled);  // 解冻
        mTime = mPathfindingCycle + 1.0f;
        NPCFSM->Get()->process_event(S_Chasing{});
    }
    return true;
}
```

> 注意：切状态时要记得把模式切回 Controlled（在 Standby/Patrol/Attack/Chasing 的入口处，或更稳妥地在 FSM 的 transition action 里统一 `SetMode(Controlled)`）。**建议**：在每个状态函数开头根据状态设定模式，避免遗漏。

#### 步骤 4.4 子弹击退（`TankHitByBullet`）

`Character/GamePlayer.cpp:20-56`，在入队像素伤害之后、销毁子弹之前，新增击退：

```cpp
// === 新增：子弹击退 ===
if (self->mMovement) {
    Vec2_ bulletSpeed = const_cast<PhysicsBlock::PhysicsFormwork*>(otherObj)->PFSpeed();
    float bulletSpdLen = std::sqrt(bulletSpeed.x*bulletSpeed.x + bulletSpeed.y*bulletSpeed.y);
    if (bulletSpdLen > 1e-3f) {
        Vec2_ dir = { bulletSpeed.x / bulletSpdLen, bulletSpeed.y / bulletSpdLen };
        // 冲量大小 = 子弹动量 * 击退系数（可调）
        const float knockbackScale = 8.0f;
        // 受力点 = 接触点（产生自旋，更真实）
        glm::vec2 hp = arbiter->contacts[0].position;
        self->mObjectCollision->ApplyImpulse(
            Vec2_{ dir.x * knockbackScale, dir.y * knockbackScale },
            Vec2_{ hp.x, hp.y });
        // 切到 Ragdoll（被击飞，期间不响应玩家/AI 输入）
        self->mMovement->SetMode(MovementMode::Ragdoll);
    }
}
```

> **联机注意**：击退会改变速度/位置，若处于 `Global::MultiplePeopleMode`，需要考虑是否在服务器权威下同步。**建议先在单机验证，联机同步作为后续优化项**（见阶段5）。

#### 步骤 4.5 爆炸击飞（`Arms::Explode`）

`Arms/Arms.cpp:201-248` 的 `damageTank` lambda 里，在 `mPixelQueue->add` 之后新增：

```cpp
// === 新增：爆炸击飞 ===
auto damageTank = [&](GamePlayer* tank) {
    if (tank == nullptr || tank->GetDeathInBattle()) return;
    glm::vec2 tpos = tank->GetObjectCollision()->pos;
    float dist = std::sqrt((tpos.x-pos.x)*(tpos.x-pos.x) + (tpos.y-pos.y)*(tpos.y-pos.y));
    if (dist > radius) return;
    // ... 原有像素破坏逻辑保持不变 ...

    // 爆炸冲量：方向 = 从爆炸中心指向坦克，大小随距离衰减
    glm::vec2 kbDir = (dist > 1e-3f)
        ? glm::vec2((tpos.x-pos.x)/dist, (tpos.y-pos.y)/dist)
        : glm::vec2(1, 0);
    float falloff = 1.0f - dist / radius;       // 0~1
    const float explosionImpulse = 40.0f * falloff;
    tank->GetObjectCollision()->ApplyImpulse(
        Vec2_{ kbDir.x * explosionImpulse, kbDir.y * explosionImpulse },
        Vec2_{ pos.x, pos.y });                  // 受力点=爆炸中心（向外推）
    if (tank->GetMovement()) {
        tank->GetMovement()->SetMode(MovementMode::Ragdoll);
        // 爆炸击飞时间应更长，调大 RagdollMinTime（可临时改 config）
    }
};
```

#### ✅ 阶段 4 验收标准
- [ ] NPC 巡逻/追击/攻击走位手感与玩家一致（平滑、不滑冰、不冲过路径点）。
- [ ] 玩家被子弹击中：**有明显击退位移 + 轻微自旋**，击退期间玩家输入无效（Ragdoll），速度降下来后恢复操控。
- [ ] 火箭弹爆炸：范围内所有坦克被向外推飞，近处推得远、远处推得近。
- [ ] NPC 被击中：进入硬直（Frozen）1 秒不动，期间朝向玩家，1 秒后恢复追击/攻击。
- [ ] 多次被击中不会卡死在 Ragdoll/Frozen（自动恢复机制有效）。

---

### 阶段 5：联机同步、参数调优、清理收尾

#### 步骤 5.1 联机同步（击退是新增的状态变化）

**现状**：`MazeMods` 有联机（`Global::MultiplePeopleMode`），同步的是位置/角度（`MazeReplicationComponents.h` 的 `PlayerPositionComponent`）。

**问题**：击退/模式切换是客户端本地行为，不同步会导致各端看到的位置/状态不一致。

**方案**（按复杂度递增，择一）：
1. **最简（推荐先做）**：击退逻辑只在**服务器**执行，客户端收到服务器同步的位置即可看到击退效果。客户端不自己算击退。
2. **进阶**：新增 `MovementStateComponent` 同步模式（Controlled/Ragdoll/Frozen）+ 冲量事件（`KnockbackEvent`），用事件系统广播。

> 由于击退时间短（<1秒），方案1（服务器权威位置同步）通常足够。

#### 步骤 5.2 参数调优表

集中调参位置：`GamePlayer` 构造时设置 `MovementConfig`（玩家用一套参数，NPC 可在 `Crowd::AddNPC` 时覆盖另一套）。

| 参数 | 玩家建议值 | NPC 建议值 | 说明 |
|---|---|---|---|
| `MaxSpeed` | 120 | 80~100 | NPC 略慢于玩家 |
| `Acceleration` | 10 | 8 | 越大越灵敏 |
| `TurnRate` | 12 | 6~8 | NPC 转向更慢更"思考" |
| `RagdollRecoverSpeed` | 20 | 25 | |
| `RagdollMinTime` | 0.3 | 0.4 | |
| `knockbackScale`（子弹） | 8 | 8 | 在 `TankHitByBullet` 内 |
| `explosionImpulse`（爆炸） | 40 | 40 | 在 `Arms::Explode` 内 |

**调优顺序**：先固定玩家参数玩出"爽快感"，再调 NPC 让 AI 不至于太弱/太强。

#### 步骤 5.3 清理命名陷阱（可选但强烈推荐）

把残留的误导命名改正，避免后人踩坑：
- `NPC::mMoveForce`（`NPC.h:99`）→ 删除（已不用）或重命名为历史注释。
- 各 GameMod 的 `PlayerForce` → 已在阶段3删除。

#### 步骤 5.4 文档更新
- 在 `docs/玩家移动驱动方式分析.md` 末尾追加"重构完成说明"，指向本规划文档与实际改动。
- 更新 `MovementConfig` 的注释，说明每个参数的手感影响。

#### ✅ 阶段 5 验收标准
- [ ] 联机模式下击退表现一致（服务器权威）。
- [ ] 玩家/ NPC 参数已调优，手感经过实际试玩确认。
- [ ] 无残留的 `PlayerForce`/`mMoveForce` 误导命名。
- [ ] 文档已更新。

---

## 四、风险与对策

| ID | 风险 | 影响 | 对策 |
|---|---|---|---|
| R1 | `AddForce` 非虚 + 基类无声明 → `MovementComponent` 编译失败 | 阶段2卡住 | 阶段1补：基类加 `virtual AddForce`，子类加 `override` |
| ~~R2~~ | ~~`KeyDown` 在按住时持续触发~~ | ~~已解决~~ | ✅ **已确认 KeyDown 每帧触发**；采用"方向投票 + 每帧清零"机制，与原 `PlayerForce` 同构，**无需 KeyUp**；`SetMoveInput` 内部归一化保证对异常多次派发鲁棒（见步骤 3.2 四情形表） |
| R3 | `friction=1.0` 强阻尼 + 新的 `ComputeMoveForce` 双重刹车 → 移动过黏 | 玩家加速上不去 | 调试时若发现达不到 `MaxSpeed`，把 `GamePlayer.cpp:85` 的 `friction` 降到 0.2~0.3，或确认阻尼补偿公式正确 |
| R4 | Ragdoll 切回 Controlled 的瞬间，残留速度被 `ComputeMoveForce` 瞬间吃掉 → "急刹车"突兀 | 手感生硬 | 切回时把 `mDesiredSpeed` 设为当前速度方向，让 `ComputeMoveForce` 平滑过渡而非强制归零 |
| R5 | NPC FSM 切状态时忘记切回 `Controlled` → NPC 卡在 Ragdoll/Frozen 不动 | NPC 瘫痪 | 在 FSM 的 transition action 集中设模式；或每个状态函数开头断言模式正确 |
| R6 | 联机击退不同步 → 各端位置漂移 | 多人不同步 | 阶段5方案1：服务器权威；短期可接受单机验证 |
| R7 | `ApplyImpulse` 在 kinematic 体（`invMass==0`）上静默失效 → 玩家拖拽物体时击退无效 | 边缘 bug | 符合预期（kinematic 不应被冲量推动）；文档说明即可 |
| R8 | 子弹击退冲量 + 子弹本来的碰撞响应 → 双重作用，击退过强 | 击退过猛 | 先单独测击退强度，必要时降低 `knockbackScale` 或在 `ApplyImpulse` 前 `speed={0,0}` 清零碰撞响应速度 |

---

## 五、实施时间线（建议）

| 阶段 | 工作量 | 依赖 | 产出 |
|---|---|---|---|
| 阶段1 引擎API | 中（~半天） | 无 | 引擎可施加冲量/反推力 |
| 阶段2 组件 | 中（~半天） | 阶段1 | 可独立测试的 MovementComponent |
| 阶段3 玩家接入 | 小（~2小时，三个模组逐一） | 阶段2 | 玩家手感改善（可交付一次） |
| 阶段4 NPC+击退 | 中（~半天） | 阶段3 | 战斗手感完整 |
| 阶段5 联机/调优/清理 | 中（~半天） | 阶段4 | 最终交付 |

**建议**：每阶段完成后 commit 一次，便于回滚。阶段3完成后即可合入 main（玩家手感是纯改进），阶段4、5 视测试情况分批合入。

---

## 六、回滚策略

- **阶段1回滚**：删除新增的虚函数和 `MovementUtil.hpp` 即可，对游戏零影响。
- **阶段2回滚**：删除 `MovementComponent`，零影响。
- **阶段3/4回滚**：每个 GameMod 改动是独立的 git commit，可单独 revert。最坏情况 `git revert <commit>` 回到 `PFSpeed() += PlayerForce`。
- **整体回滚**：因为改动分布在多个 commit，最稳妥是保留一个 `main` 之前的 tag（如 `pre-movement-refactor`），需要时 `git reset --hard`。

---

## 七、关键代码位置索引（实施时快速定位）

| 改动点 | 文件 | 行号（重构前） |
|---|---|---|
| 玩家平移驱动 | `GameMods/MazeMods/MazeMods.cpp` | 465 |
| 玩家平移驱动 | `GameMods/TankTrouble/TankTrouble.cpp` | 433 |
| 玩家平移驱动 | `GameMods/UnlimitednessMapMods/UnlimitednessMapMods.cpp` | 270 |
| 玩家按键处理 | 三个 GameMod 的 `KeyDown` | MazeMods:292 / TT:205 / UM:162 |
| NPC Patrol 移动 | `Character/NPC.cpp` | 238 |
| NPC Attack 后退/靠近 | `Character/NPC.cpp` | 271, 274 |
| NPC GoToLocation 移动 | `Character/NPC.cpp` | 402 |
| NPC Injury 状态 | `Character/NPC.cpp` | 296-318 |
| NPC 每帧入口 | `Character/NPC.cpp` | 410-420 |
| GamePlayer 物理体创建 | `Character/GamePlayer.cpp` | 69-88 |
| GamePlayer 渲染同步 | `Character/GamePlayer.cpp` | 179-197 |
| 子弹命中回调 | `Character/GamePlayer.cpp` | 20-56 |
| 爆炸伤害 | `Arms/Arms.cpp` | 201-248 |
| 物理体强阻尼 | `Character/GamePlayer.cpp` | 85 (`friction=1.0`) |
| 引擎 AddForce（非虚） | `PhysicsBlock/PhysicsAngle.hpp/.cpp` | hpp:114/121, cpp:49/65 |
| 引擎速度积分 | `PhysicsBlock/PhysicsParticle.cpp` | 68-77 |
| 引擎位置积分 | `PhysicsBlock/PhysicsParticle.cpp` | 90-117 |
| 全局阻尼常数 | `PhysicsBlock/BaseDefine.h` | 10 |
| 碰撞内部冲量公式（可复用） | `PhysicsBlock/PhysicsBaseArbiter.cpp` | 140-149 |
| 二维叉积 | `PhysicsBlock/BaseCalculate.hpp` | 433 |

---

## 八、一句话总结

**五阶段、从底层到上层、每阶段可独立交付**：先给引擎加冲量/施力反推 API（阶段1），再做三态 `MovementComponent`（阶段2），然后玩家三个模组逐一切换（阶段3），最后 NPC 接入 + 击退/爆炸/硬直战斗手感（阶段4）和联机调优清理（阶段5）。每阶段都有明确验收标准和回滚策略，可安全推进。
