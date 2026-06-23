# NPCGOBT 智能升级方案设计

> 基于 `c:\GitHub\PixelClean\Character\NPCGOBT.h` / `NPCGOBT.cpp` 现有实现的深度分析与多方案智能升级规划。

---

## 目录

- [一、现有系统深度分析](#一现有系统深度分析)
- [二、局限性诊断矩阵](#二局限性诊断矩阵)
- [三、方案一：感知战斗增强型（Perception-Combat）](#三方案一感知战斗增强型perception-combat)
- [四、方案二：战术协作型（Tactical-Coop）](#四方案二战术协作型tactical-coop)
- [五、方案三：学习适应型（Learning-Adaptive）](#五方案三学习适应型learning-adaptive)
- [六、方案四：综合演进型（Synthesis）](#六方案四综合演进型synthesis)
- [七、方案对比矩阵](#七方案对比矩阵)
- [八、实施优先级建议](#八实施优先级建议)
- [九、技术风险评估](#九技术风险评估)

---

## 一、现有系统深度分析

### 1.1 架构总览

NPCGOBT 采用 GOBT 三层架构，将原 NPC 的 boost::sml FSM（6 状态 + 15 转移）重构为目标导向行为树：

| 层级 | 组件 | 职责 |
|------|------|------|
| 战略层 | StrategicGoalNode + GoalManager + StrategicPlanner | 从目标池选最高优先级未满足目标，分解为子目标队列 |
| 战术层 | TacticalSubgoalNode + SubtreeReferenceNode + TacticalDecomposer | 按子目标类型从子树库实例化操作层子树，支持重试与失败上报 |
| 操作层 | OperationalActionNode + LayerBridgeNode | 执行原子动作，校验前提，应用效果，支持战略中断 |

### 1.2 目标体系

```
Survive (优先级 100)  ← 受伤时激活
  └─ 满足条件: injury_recovered == true
  └─ 分解: RecoverFromInjury (Recover)

FightEnemy (优先级 80)  ← 看到玩家时激活
  └─ 满足条件: player_visible == false
  └─ 动态分解:
       ├─ player_in_range == true  → AttackPlayer (Combat)
       └─ player_in_range == false → ChasePlayer (MoveTo)

PatrolArea (优先级 20)  ← 默认 fallback，永不满足
  └─ 满足条件: patrol_done == true (此键永不设置)
  └─ 分解: Standby → Patrol 循环
```

### 1.3 感知系统（GetSensoryMessages）

| 感知项 | 实现方式 | 标志位 |
|--------|----------|--------|
| 玩家距离 | `sqrt(dx² + dy²)`，存入 `mPlayerDistance` | — |
| 玩家相对角度 | `EdgeVecToCosAngleFloat(dx, dy)`，存入 `wanjiaAngle` | — |
| 视野锥 | 角度差 `|AngleCha| < 1.57 弧度`（±90°） | `SensoryMessages_VisualField` |
| 可见性 | 视野内 + `RadialCollisionDetection` 射线无遮挡 | `SensoryMessages_Visible` |
| 攻击范围 | `mPlayerDistance < AttackRange(90)` | `SensoryMessages_Range` |

**关键特性：**
- 视野锥固定 ±90°，朝向由 `mNPC->GetObjectCollision()->angle` 决定
- 射线检测使用 Bresenham 直线算法（`FMBresenhamDetection`）
- 可疑位置记忆：看到玩家时记录 `mSuspiciousPos`，追击时优先前往

### 1.4 移动寻路系统

#### 寻路（DoChase）
- **算法**：JPS（Jump Point Search），A* 变体，跳点搜索
- **范围**：以起点为中心的 `[-300, 300) × [-300, 300)` 正方形
- **异步执行**：通过 `TOOL::mThreadPool->enqueue` 投递到线程池
- **完成检测**：`GetPathfindingCompleted()` 轮询（注意：非 atomic，存在数据竞争风险）
- **重新寻路周期**：固定 1.5s（`mPathfindingCycle`）
- **路径点跟随**：`distToWaypoint < 9.0f` 视为到达，`pop_back()` 取下一个
- **距离截断**：目标超出范围时 `toTarget *= 0.5f` 循环截断

#### 巡逻（DoPatrol）
- 前进方向 `qianjinfang`，初始 `{1, 0}`
- 前方 2 像素处检测墙壁（`GetPixelWallNumber`）
- 遇墙随机选 4 方向之一（`rand() % 4`）
- 返回 Success 触发转待机

#### 待机（DoStandby）
- 转向前进方向
- 1.5s 后转巡逻

### 1.5 战斗系统（DoAttack）

| 行为 | 实现 |
|------|------|
| 朝向 | `SetLookAngle(wanjiaAngle)` 朝向玩家 |
| 距离保持 | 理想距离 50，容差 ±10；太近后退，太远靠近 |
| 射击冷却 | `mShootInterval = 0.8s` |
| 瞄准误差 | `±0.075 弧度`（`((rand()%100)/100.0 - 0.5) * 0.15`） |
| 子弹参数 | 速度 500，类型 0（手枪） |
| 射击位置 | `pos + vec2angle({9, 0}, angle)`（炮口偏移） |

### 1.6 受伤恢复（DoInjury）

- 首次进入重置计时器（`injuryEntered_` 标志）
- `MovementMode::Frozen` 冻结移动
- 仍可转向（`SetLookAngle(wanjiaAngle)`）
- 1.0s 后返回 Success，通过 Action 效果设置 `injury_recovered = true`

### 1.7 Event 主循环

```
SyncWorldState()          // 感官 → WorldState
  ↓
mExecutor->tick_once()    // GOBT 决策（替代 FSM process_event）
  ↓
Movement::Update(FPSTime) // 物理积分
  ↓
mNPC->UpData()            // 消费伤害队列
  ↓
setGamePlayerMatrix()     // 更新渲染矩阵
```

### 1.8 世界状态同步（SyncWorldState）

```
injured          = (mPixelQueue->GetNumber() > 0)
player_visible   = (flags & Visible)
player_in_range  = (flags & Range)
player_in_viewfield = (flags & VisualField)
injury_recovered = 新伤害时 false / 队列归零时 true / DoInjury 超时后 true
```

### 1.9 移动参数配置

| 参数 | NPC 值 | 玩家值（对比） |
|------|--------|----------------|
| MaxSpeed | 90.0 | 120.0 |
| TurnRate | 7.0 | 12.0 |
| RagdollMinTime | 0.4 | 0.3 |

### 1.10 可用武器系统（未充分利用）

| 武器 | 射击间隔 | 反弹 | 爆炸 | 爆炸半径 | 适用场景 |
|------|----------|------|------|----------|----------|
| Pistol（手枪） | 0.5s | 3 次 | 否 | — | 当前使用，通用 |
| Shrapnel（霰弹） | 1.0s | 0 | 否 | — | 近距离爆发 |
| MachineGun（机枪） | 0.1s | 0 | 否 | — | 压制 |
| Rocket（火箭） | 1.5s | 0 | 是 | 5.0 格 | 远距离/群伤 |

**当前 NPCGOBT 仅使用 Pistol（Type=0），未利用其他武器。**

---

## 二、局限性诊断矩阵

| 维度 | 现状 | 局限性 | 影响 |
|------|------|--------|------|
| **感知-视觉** | 视野锥 ±90° + 射线 | 无扫视行为、无视野扇区细分、无记忆衰减 | 玩家易绕背 |
| **感知-听觉** | 无 | 不感知枪声/脚步声/爆炸 | 玩家可无声接近 |
| **感知-记忆** | mSuspiciousPos 单点 | 无衰减、无多目标记忆、无威胁等级 | 行为可预测 |
| **感知-团队** | 无 | 不共享玩家位置信息 | 无法协作 |
| **移动-寻路** | JPS 异步 | 无避障（NPC 重叠）、无路径平滑、周期固定 1.5s | 移动僵硬 |
| **移动-追击** | 追当前位置 | 无预判、无包抄、无侧移 | 易被风筝 |
| **移动-巡逻** | 4 方向随机 | 无巡逻路线、无区域意识 | 行为无意义 |
| **战斗-武器** | 仅手枪 | 不切换霰弹/机枪/火箭 | 火力单一 |
| **战斗-瞄准** | 固定误差 | 不随距离调整、无预判射击 | 命中率低 |
| **战斗-走位** | 保持距离 50 | 无掩体利用、无闪避、无侧翼 | 易被击杀 |
| **战斗-自保** | 无 | 不评估血量、不逃跑、不找血包 | 送人头 |
| **决策-目标** | 3 静态目标 | 无动态优先级、无局势评估 | 响应迟钝 |
| **决策-团队** | 无 | 无角色分工、无通信、无阵型 | 各自为战 |
| **决策-学习** | 无 | 不记忆玩家模式、不调整难度 | 可被针对 |
| **线程安全** | bool 非原子 | JPS 跨线程访问 `mPathfindingCompleted` | 潜在崩溃 |

---

## 三、方案一：感知战斗增强型（Perception-Combat）

**核心思想：** 在不改变 GOBT 三层架构的前提下，增强感知系统精度与战斗决策多样性，让单个 NPC 更"像人"。

### 3.1 架构设计

```
┌─────────────────────────────────────────┐
│         增强感知层（SensoryLayer）        │
│  视觉扇区 + 听觉事件 + 记忆衰减 + 威胁评估 │
└────────────────┬────────────────────────┘
                 ↓ 写入 WorldState
┌─────────────────────────────────────────┐
│         GOBT 三层（保持不变）             │
│  战略层 → 战术层 → 操作层                 │
└────────────────┬────────────────────────┘
                 ↓ 扩展动作执行器
┌─────────────────────────────────────────┐
│      增强动作层（ActionLayer）            │
│  预判射击 + 武器切换 + 掩体利用 + 闪避    │
└─────────────────────────────────────────┘
```

### 3.2 核心组件

#### 3.2.1 视觉扇区系统（VisualSector）

将视野从单一锥形升级为多扇区，每个扇区独立检测：

```cpp
// NPCGOBT.h 新增
struct VisualSector {
    float centerAngle;      // 扇区中心角度（相对 NPC 朝向）
    float halfAngle;        // 半张角
    bool  hasTarget;        // 当前扇区是否看到玩家
    float lastSeenTime;     // 最后看到时间（用于衰减）
};

class NPCGOBT {
    // ...
    std::array<VisualSector, 5> mSectors = {{
        { 0.0f,  0.35f, false, 0.0f },  // 正前窄扇区（高精度）
        { 0.0f,  1.57f, false, 0.0f },  // 正前宽扇区（主视野）
        { 1.57f, 0.6f,  false, 0.0f },  // 左侧
        {-1.57f, 0.6f,  false, 0.0f },  // 右侧
        { 3.14f, 0.35f, false, 0.0f },  // 正后（狭窄，模拟余光）
    }};
    float mScanAngle = 0.0f;  // 扫视角度（待机时左右扫视）
};
```

**扫视行为：** 待机/巡逻时 `mScanAngle` 在 `[-0.5, +0.5]` 弧度间正弦摆动，扩大有效视野。

#### 3.2.2 听觉感知系统（AuditorySense）

利用现有 `Arms::ShootBullets` 和 `Explode` 事件，通过 EventBus 广播声音：

```cpp
// 新增声音事件类型
namespace NPCWS {
    inline const std::string kGunshotPos     = "gunshot_pos";      // 枪声位置
    inline const std::string kExplosionPos   = "explosion_pos";    // 爆炸位置
    inline const std::string kFootstepPos    = "footstep_pos";     // 脚步声位置
}

// NPCGOBT 构造时订阅全局声音事件
mEventBus->subscribe("SOUND_GUNSHOT", [this](const std::any& payload) {
    auto* pos = std::any_cast<glm::vec2>(&payload);
    if (pos) {
        float dist = glm::length(*pos - mNPC->GetObjectCollision()->pos);
        if (dist < mHearingRange) {  // 听觉范围 400
            mSuspicious = true;
            mSuspiciousPos = {(int)pos->x, (int)pos->y};
            mSuspiciousDecay = 8.0f;  // 8 秒后遗忘
        }
    }
});
```

#### 3.2.3 记忆衰减系统（MemoryDecay）

```cpp
struct TargetMemory {
    JPSVec2 lastKnownPos;
    float   confidence;     // 0.0~1.0，随时间衰减
    float   lastSeenTime;
    MovementMode predictedMode;  // 预测玩家移动模式
};

TargetMemory mTargetMemory;
float mSuspiciousDecay = 0.0f;  // 可疑度衰减计时器

// SyncWorldState 中更新
if (flags & SensoryMessages_Visible) {
    mTargetMemory.lastKnownPos = {(int)Global::GamePlayerX, (int)Global::GamePlayerY};
    mTargetMemory.confidence = 1.0f;
    mTargetMemory.lastSeenTime = mTime;
} else {
    mTargetMemory.confidence -= FPSTime / 8.0f;  // 8 秒线性衰减到 0
    mTargetMemory.confidence = std::max(0.0f, mTargetMemory.confidence);
}
```

#### 3.2.4 预判射击（PredictiveAim）

基于玩家速度（需从全局或历史位置推算）预测未来位置：

```cpp
gobot::Status NPCGOBT::DoAttack(gobot::Context& ctx) {
    // ... 现有朝向逻辑 ...

    // 预判射击：根据子弹飞行时间预测玩家位置
    glm::vec2 playerPos = {Global::GamePlayerX, Global::GamePlayerY};
    glm::vec2 npcPos = mNPC->GetObjectCollision()->pos;
    float bulletSpeed = 500.0f;
    float flightTime = mPlayerDistance / bulletSpeed;

    // 玩家速度估算（从历史位置差分）
    glm::vec2 playerVel = mPlayerVelocityEstimate;  // 需维护
    glm::vec2 predictedPos = playerPos + playerVel * flightTime;

    // 瞄准预判位置，误差随距离增大
    float aimAngle = EdgeVecToCosAngleFloat(predictedPos - npcPos);
    float dynamicError = 0.03f + (mPlayerDistance / 300.0f) * 0.12f;  // 近准远散
    aimAngle += ((rand() % 1000) / 1000.0f - 0.5f) * 2.0f * dynamicError;

    // 射击
    if (mShootCooldown <= 0.0f) {
        wArms->ShootBullets(shootPos.x, shootPos.y, aimAngle, bulletSpeed, mCurrentWeaponType);
    }
}
```

#### 3.2.5 动态武器选择（WeaponSelection）

根据距离和局势选择武器：

```cpp
int NPCGOBT::SelectWeapon(int flags, float distance) {
    if (distance > 200 && mHasRocket)  return 3;  // 远距离用火箭
    if (distance < 40)                  return 1;  // 近距离用霰弹
    if (mSuppressing)                   return 2;  // 压制时机枪
    return 0;                                       // 默认手枪
}
```

#### 3.2.6 掩体利用（CoverSystem）

利用现有 `RadialCollisionDetection` 检测周围墙壁作为掩体：

```cpp
struct CoverPoint {
    glm::vec2 pos;
    float safetyScore;  // 掩体安全度（越高越好）
};

std::vector<CoverPoint> NPCGOBT::FindNearbyCovers() {
    std::vector<CoverPoint> covers;
    glm::vec2 npcPos = mNPC->GetObjectCollision()->pos;
    // 8 方向射线探测墙壁
    for (int i = 0; i < 8; ++i) {
        float angle = i * (3.14159f / 4.0f);
        glm::vec2 dir = {cos(angle), sin(angle)};
        glm::vec2 probeEnd = npcPos + dir * 60.0f;
        auto hit = wPathfinding->RadialCollisionDetection(
            npcPos.x, npcPos.y, probeEnd.x, probeEnd.y);
        if (hit.Collision) {
            // 墙壁后方 10 像素处作为掩体点
            glm::vec2 coverPos = {hit.pos.x - dir.x * 10, hit.pos.y - dir.y * 10};
            // 评估：掩体到玩家连线是否被墙遮挡
            auto playerLine = wPathfinding->RadialCollisionDetection(
                coverPos.x, coverPos.y, Global::GamePlayerX, Global::GamePlayerY);
            float safety = playerLine.Collision ? 1.0f : 0.3f;
            covers.push_back({coverPos, safety});
        }
    }
    return covers;
}
```

### 3.3 新增目标与子目标

```
新增目标：
  SelfPreserve (优先级 90)  ← 血量低于阈值时激活
    └─ 满足条件: hp_ratio > 0.3
    └─ 分解: FindCover → HealWait

新增子目标类型：
  SubgoalType::TakeCover   // 寻找掩体
  SubgoalType::Suppress     // 火力压制
  SubgoalType::Flank        // 侧翼包抄（单 NPC 版）
```

### 3.4 实施步骤

1. **感知层升级**（独立模块，不影响现有逻辑）
   - 实现 `VisualSector` 扫视系统
   - 接入 `Arms::ShootBullets` 的声音事件广播
   - 实现 `TargetMemory` 衰减
2. **战斗层升级**
   - 实现预判射击（需维护玩家速度估算）
   - 实现动态武器选择
   - 实现掩体探测与利用
3. **GOBT 目标扩展**
   - 新增 `SelfPreserve` 目标
   - 注册 `TakeCover` 子树
   - 调整优先级体系
4. **调参与测试**
   - 听觉范围、衰减时间、武器切换阈值

### 3.5 伪代码：增强版 DoAttack

```cpp
Status DoAttack(Context& ctx) {
    int flags = GetSensoryMessages();
    if (!(flags & Range) || !(flags & Visible)) return Failure;

    // 1. 选择武器
    int weapon = SelectWeapon(flags, mPlayerDistance);
    float idealDist = (weapon == 1) ? 35.0f :    // 霰弹近
                      (weapon == 3) ? 180.0f :   // 火箭远
                      50.0f;                      // 手枪/机枪中

    // 2. 距离保持（根据武器调整）
    MaintainDistance(idealDist);

    // 3. 预判瞄准
    glm::vec2 aimPoint = PredictPlayerPosition(bulletSpeed[weapon]);
    float aimAngle = EdgeVecToCosAngleFloat(aimPoint - npcPos);
    aimAngle += RandomError(mPlayerDistance);

    // 4. 射击
    if (mShootCooldown <= 0) {
        wArms->ShootBullets(shootPos.x, shootPos.y, aimAngle, bulletSpeed[weapon], weapon);
        mShootCooldown = weaponInterval[weapon];
    }

    // 5. 血量低时触发自保
    if (GetHPRatio() < 0.3 && mTime - mLastDamageTime < 2.0f) {
        return Failure;  // 触发 SelfPreserve 目标
    }
    return Running;
}
```

---

## 四、方案二：战术协作型（Tactical-Coop）

**核心思想：** 引入 NPC 间通信机制，实现团队战术（包抄、压制、阵型），让多个 NPC 协同作战。

### 4.1 架构设计

```
┌──────────────────────────────────────────────┐
│        团队共享黑板（TeamSharedBlackboard）    │
│  玩家位置共识 + 威胁等级 + 角色分配 + 战术指令 │
└──────────┬───────────────────────────────────┘
           ↑ subscribe/publish
┌──────────┴───────────────────────────────────┐
│              NPCGOBT 实例 × N                 │
│  ┌─────────┐  ┌─────────┐  ┌─────────┐      │
│  │ 突击手  │  │ 狙击手  │  │ 支援手  │      │
│  │ Assaulter│  │ Sniper  │  │ Supporter│     │
│  └─────────┘  └─────────┘  └─────────┘      │
└──────────────────────────────────────────────┘
```

### 4.2 核心组件

#### 4.2.1 团队共享黑板（TeamSharedBlackboard）

```cpp
// 新增 TeamSharedBlackboard.hpp
class TeamSharedBlackboard {
public:
    // 玩家位置共识（多 NPC 观测融合）
    struct PlayerConsensus {
        glm::vec2 pos;
        glm::vec2 velocity;
        float confidence;
        float lastUpdate;
    };
    std::atomic<PlayerConsensus> mPlayerConsensus;

    // 角色分配
    enum class Role { Assaulter, Sniper, Supporter, Scout };
    std::unordered_map<evutil_socket_t, Role> mRoleAssignment;

    // 战术指令
    enum class TacticalOrder {
        None, Suppress, FlankLeft, FlankRight,
        Regroup, Retreat, PushForward
    };
    std::atomic<TacticalOrder> mCurrentOrder{TacticalOrder::None};

    // 威胁等级（团队共享）
    std::atomic<float> mTeamThreatLevel{0.0f};

    // 注册/注销 NPC
    void registerNPC(evutil_socket_t key, Role role);
    void unregisterNPC(evutil_socket_t key);
};
```

#### 4.2.2 角色分工

| 角色 | 武器偏好 | 行为特征 | 目标优先级调整 |
|------|----------|----------|----------------|
| Assaulter（突击手） | 霰弹/机枪 | 主动接近、包抄 | FightEnemy↑ |
| Sniper（狙击手） | 手枪（远距） | 保持距离、精准射击 | FightEnemy（远距）|
| Supporter（支援手） | 机枪 | 火力压制、掩护队友 | SelfPreserve↑ |
| Scout（侦察兵） | 手枪 | 巡逻扩大、信息上报 | PatrolArea↑ |

#### 4.2.3 包抄战术（Flanking）

```cpp
// 战术分解器新增策略
mDecomposer->register_strategy("TeamFight", [teamBB](const Goal&) {
    auto role = teamBB->getRole(myKey);
    auto order = teamBB->getCurrentOrder();

    if (order == TacticalOrder::FlankLeft) {
        return { std::make_shared<Subgoal>(
            SubgoalType::Custom, "FlankLeft",
            {{NPCWS::kPlayerInRange, true}}, 2) };
    }
    if (role == Role::Assaulter) {
        return { std::make_shared<Subgoal>(
            SubgoalType::Combat, "AssaultPlayer",
            {{NPCWS::kPlayerInRange, true}}, 3) };
    }
    if (role == Role::Sniper) {
        return { std::make_shared<Subgoal>(
            SubgoalType::Combat, "SnipePlayer",
            {{NPCWS::kPlayerVisible, false}}, 3) };
    }
    // ...
});
```

#### 4.2.4 火力压制（Suppression）

```cpp
Status DoSuppress(Context& ctx) {
    // 朝玩家方向持续射击（不追求命中，迫使玩家躲掩体）
    glm::vec2 playerPos = teamBB->getPlayerConsensus().pos;
    float suppressAngle = EdgeVecToCosAngleFloat(playerPos - npcPos);
    suppressAngle += ((rand()%100)/100.0f - 0.5f) * 0.3f;  // 大散布

    if (mShootCooldown <= 0) {
        wArms->ShootBullets(shootPos.x, shootPos.y, suppressAngle, 500, 2);  // 机枪
        mShootCooldown = 0.1f;
    }

    // 通知队友玩家被压制
    teamBB->setPlayerSuppressed(true);
    return Running;
}
```

#### 4.2.5 阵型保持（Formation）

```cpp
// V 字阵型、一字阵型等
glm::vec2 GetFormationPosition(Role role, int indexInFormation) {
    glm::vec2 leaderPos = teamBB->getLeaderPos();
    switch (mFormationType) {
        case FormationType::Wedge:  // V 字
            float offset = (indexInFormation - 1) * 30.0f;
            return leaderPos + glm::vec2{-offset, abs(offset) * 0.5f};
        // ...
    }
}
```

### 4.3 新增目标体系

```
TeamFight (优先级 85)  ← 团队作战模式
  └─ 分解根据角色:
       Assaulter → AssaultPlayer / FlankLeft / FlankRight
       Sniper    → SnipePlayer
       Supporter → SuppressEnemy / CoverAlly
       Scout     → ReportPlayerPos

ProtectAlly (优先级 75)  ← 队友受攻击时
  └─ 分解: MoveToAlly → CoverFire
```

### 4.4 通信协议

```cpp
// 事件类型
namespace TeamEvents {
    inline const std::string kPlayerSpotted  = "TEAM_PLAYER_SPOTTED";
    inline const std::string kAllyInjured    = "TEAM_ALLY_INJURED";
    inline const std::string kAllyDown       = "TEAM_ALLY_DOWN";
    inline const std::string kOrderChanged   = "TEAM_ORDER_CHANGED";
    inline const std::string kRequestBackup  = "TEAM_REQUEST_BACKUP";
}

// NPC 发现玩家时上报
mEventBus->publish(TeamEvents::kPlayerSpotted, PlayerSpottedInfo{
    .pos = {Global::GamePlayerX, Global::GamePlayerY},
    .reporterKey = mNPC->GetKey(),
    .timestamp = mTime
});
```

### 4.5 实施步骤

1. **基础设施**
   - 实现 `TeamSharedBlackboard`（线程安全）
   - 在 `Crowd` 类中持有团队黑板实例
   - NPCGOBT 构造时注入团队黑板
2. **角色系统**
   - 实现角色分配逻辑（基于 NPC 数量动态分配）
   - 各角色差异化配置（武器、距离、优先级）
3. **战术实现**
   - 包抄：计算侧翼路径点
   - 压制：持续散布射击
   - 阵型：相对位置计算
4. **通信集成**
   - 接入现有 EventBus（延迟派发模式）
   - 定义事件载荷结构

---

## 五、方案三：学习适应型（Learning-Adaptive）

**核心思想：** 让 NPC 记忆玩家行为模式，动态调整自身策略，实现"越打越聪明"的难度自适应。

### 5.1 架构设计

```
┌──────────────────────────────────────────────┐
│           玩家行为模型（PlayerModel）          │
│  移动模式 + 战斗偏好 + 走位习惯 + 弱点分析     │
└──────────┬───────────────────────────────────┘
           ↓ 提供策略建议
┌──────────┴───────────────────────────────────┐
│        自适应决策层（AdaptiveLayer）           │
│  难度调节 + 策略选择 + 反学习（防被针对）      │
└──────────┬───────────────────────────────────┘
           ↓
┌──────────┴───────────────────────────────────┐
│              GOBT 三层（动态配置）             │
└──────────────────────────────────────────────┘
```

### 5.2 核心组件

#### 5.2.1 玩家行为采集（PlayerBehaviorTracker）

```cpp
struct PlayerBehaviorSample {
    glm::vec2 pos;
    glm::vec2 velocity;
    float     hpRatio;
    int       weaponType;
    float     timestamp;
};

class PlayerBehaviorTracker {
    std::deque<PlayerBehaviorSample> mSamples;  // 滑动窗口
    static constexpr size_t kMaxSamples = 300;   // 5 秒 @ 60fps

public:
    void record(const PlayerBehaviorSample& sample);
    // 分析：玩家移动方向偏好
    float getMovementBiasAngle() const;  // 玩家习惯移动方向
    // 分析：玩家距离偏好
    float getPreferredDistance() const;
    // 分析：玩家走位频率（左右晃动）
    float getStrafeFrequency() const;
    // 分析：玩家攻击节奏
    float getAttackRhythm() const;
};
```

#### 5.2.2 玩家弱点分析（PlayerWeaknessAnalysis）

```cpp
struct PlayerWeakness {
    bool vulnerableToFlankLeft;    // 左侧防御弱
    bool vulnerableToFlankRight;   // 右侧防御弱
    bool vulnerableToRush;         // 怕近身
    bool vulnerableToSnipe;        // 怕远距
    float reactionTime;            // 反应时间估算
};

// 基于历史受击数据分析
PlayerWeakness analyzeWeakness(const std::vector<HitRecord>& hits) {
    PlayerWeakness w;
    // 统计玩家从哪个方向被击中最多
    int leftHits = countHitsFrom(hits, Side::Left);
    int rightHits = countHitsFrom(hits, Side::Right);
    w.vulnerableToFlankLeft = leftHits > rightHits * 1.5;
    w.vulnerableToFlankRight = rightHits > leftHits * 1.5;
    // ...
    return w;
}
```

#### 5.2.3 难度自适应（DifficultyAdjuster）

```cpp
class DifficultyAdjuster {
    float mPlayerSkillEstimate = 0.5f;  // 0~1
    int   mPlayerKills = 0;
    int   mNPCKills = 0;

public:
    void onPlayerKill() { mPlayerKills++; updateSkill(); }
    void onNPCKill()    { mNPCKills++;    updateSkill(); }

    // 动态调整 NPC 参数
    struct NPCParams {
        float aimError;
        float reactionDelay;
        float aggressionLevel;
        float shootInterval;
    };

    NPCParams getAdjustedParams() const {
        float skill = mPlayerSkillEstimate;
        return {
            .aimError       = lerp(0.15f, 0.03f, skill),  // 玩家强则 NPC 准
            .reactionDelay  = lerp(0.5f,  0.1f, skill),
            .aggressionLevel= lerp(0.3f,  0.9f, skill),
            .shootInterval  = lerp(1.2f,  0.5f, skill)
        };
    }
};
```

#### 5.2.4 反学习机制（AntiExploit）

防止玩家发现 NPC 固定模式后针对：

```cpp
// NPC 定期变换策略，避免被预测
class StrategyRotator {
    std::vector<StrategyType> mStrategyPool;
    float mLastRotationTime = 0;
    static constexpr float kRotationInterval = 30.0f;  // 30 秒换一次

public:
    StrategyType getCurrentStrategy(float time) {
        if (time - mLastRotationTime > kRotationInterval) {
            std::shuffle(mStrategyPool.begin(), mStrategyPool.end(), g);
            mLastRotationTime = time;
        }
        return mStrategyPool.front();
    }
};
```

### 5.3 持久化

```cpp
// 跨场次记忆（保存到文件）
struct NPCMemory {
    PlayerWeakness weakness;
    float playerSkillEstimate;
    int   totalEncounters;
    std::unordered_map<std::string, float> strategyEffectiveness;  // 策略胜率
};

// 保存/加载
void saveMemory(const std::string& path);
void loadMemory(const std::string& path);
```

### 5.4 实施步骤

1. **数据采集层**
   - 实现 `PlayerBehaviorTracker`，在 `SyncWorldState` 中采样
   - 记录受击/命中事件
2. **分析层**
   - 实现移动模式分析、弱点分析
   - 实现技能估算
3. **自适应层**
   - 参数动态调整
   - 策略选择
4. **持久化**
   - 内存/文件存储

---

## 六、方案四：综合演进型（Synthesis）

**核心思想：** 分阶段融合方案一、二、三，按"感知 → 战斗 → 协作 → 学习"的演进路线逐步实施。

### 6.1 演进路线图

```
阶段 1（感知战斗增强）          阶段 2（团队协作）           阶段 3（学习适应）
┌──────────────────┐      ┌──────────────────┐      ┌──────────────────┐
│ • 视觉扇区扫视   │      │ • 团队共享黑板   │      │ • 玩家行为建模   │
│ • 听觉感知       │  →   │ • 角色分工       │  →   │ • 弱点分析       │
│ • 记忆衰减       │      │ • 包抄/压制      │      │ • 难度自适应     │
│ • 预判射击       │      │ • 阵型保持       │      │ • 反学习         │
│ • 武器切换       │      │ • 通信协议       │      │ • 持久化记忆     │
│ • 掩体利用       │      │                  │      │                  │
└──────────────────┘      └──────────────────┘      └──────────────────┘
   独立 NPC 智能              团队战术智能              进化型智能
```

### 6.2 统一架构

```cpp
class NPCGOBT {
    // === 阶段 1：感知战斗 ===
    std::array<VisualSector, 5> mSectors;          // 视觉扇区
    TargetMemory mTargetMemory;                     // 记忆衰减
    PlayerBehaviorTracker mBehaviorTracker;         // 行为采集（阶段 3 复用）

    // === 阶段 2：团队协作 ===
    std::shared_ptr<TeamSharedBlackboard> mTeamBB;  // 团队黑板
    Role mRole;                                      // 角色身份

    // === 阶段 3：学习适应 ===
    DifficultyAdjuster mDifficulty;                  // 难度调节
    StrategyRotator mStrategyRotator;                // 策略轮换
    NPCMemory mLongTermMemory;                       // 长期记忆
};
```

### 6.3 目标体系演进

```
阶段 1 目标:
  Survive(100) > SelfPreserve(90) > FightEnemy(80) > PatrolArea(20)

阶段 2 目标:
  Survive(100) > ProtectAlly(95) > TeamFight(85) > SelfPreserve(90) > PatrolArea(20)

阶段 3 目标（动态优先级）:
  由 DifficultyAdjuster 根据玩家技能动态调整各目标优先级
```

### 6.4 实施优先级

| 优先级 | 模块 | 阶段 | 理由 |
|--------|------|------|------|
| P0 | 听觉感知 + 记忆衰减 | 1 | 低成本高收益，立即改善"背刺"问题 |
| P0 | 预判射击 | 1 | 显著提升战斗体验 |
| P1 | 武器切换 | 1 | 利用现有武器系统，代码量小 |
| P1 | 掩体利用 | 1 | 提升生存能力 |
| P2 | 团队共享黑板 | 2 | 协作基础 |
| P2 | 角色分工 | 2 | 战术多样性 |
| P3 | 包抄/压制 | 2 | 高级战术 |
| P4 | 玩家行为建模 | 3 | 需要数据积累 |
| P4 | 难度自适应 | 3 | 需要调参验证 |

---

## 七、方案对比矩阵

| 维度 | 方案一（感知战斗） | 方案二（战术协作） | 方案三（学习适应） | 方案四（综合演进） |
|------|:-:|:-:|:-:|:-:|
| **功能完整性** | ★★★★☆ | ★★★★★ | ★★★★☆ | ★★★★★ |
| **单 NPC 智能** | ★★★★★ | ★★★☆☆ | ★★★★☆ | ★★★★★ |
| **团队智能** | ★☆☆☆☆ | ★★★★★ | ★★☆☆☆ | ★★★★★ |
| **适应性** | ★★☆☆☆ | ★★☆☆☆ | ★★★★★ | ★★★★★ |
| **实施复杂度** | 中 | 高 | 高 | 最高（分阶段） |
| **代码改动量** | ~800 行 | ~1500 行 | ~1200 行 | ~3000 行（累计） |
| **性能开销** | 低（射线+扇区） | 中（通信+共识） | 中（采样+分析） | 高 |
| **可维护性** | ★★★★☆ | ★★★☆☆ | ★★★☆☆ | ★★★☆☆ |
| **调试便利性** | ★★★★☆ | ★★★☆☆ | ★★☆☆☆ | ★★☆☆☆ |
| **学习曲线** | 低 | 中 | 高 | 高 |
| **即时效果** | 显著 | 显著 | 渐进 | 渐进 |
| **依赖现有框架** | GOBT 不变 | GOBT + 新黑板 | GOBT + 采集器 | 全部 |
| **线程安全风险** | 低 | 高（共享状态） | 中 | 高 |
| **适用场景** | 单人 PVE | 多人 PVE/PVP | 长期游玩 | 商业级产品 |

---

## 八、实施优先级建议

### 8.1 推荐路线：方案四（分阶段实施）

**理由：** 方案一、二、三并非互斥，而是递进关系。方案四的演进路线允许每个阶段独立交付价值，降低整体风险。

### 8.2 阶段一（感知战斗增强）详细优先级

| 序号 | 任务 | 收益 | 成本 | 建议 |
|------|------|------|------|------|
| 1 | 听觉感知（枪声/爆炸） | 高 | 低 | **立即实施** |
| 2 | 记忆衰减（8 秒遗忘） | 高 | 低 | **立即实施** |
| 3 | 预判射击 | 高 | 中 | **立即实施** |
| 4 | 动态武器选择 | 中 | 低 | **立即实施** |
| 5 | 视觉扇区扫视 | 中 | 中 | 推荐 |
| 6 | 掩体利用 | 高 | 中 | 推荐 |
| 7 | 自保目标（SelfPreserve） | 中 | 中 | 推荐 |

### 8.3 阶段二（团队协作）触发条件

- 阶段一稳定运行且 NPC 单体智能达标
- 游戏模式支持多 NPC 同屏（≥3）

### 8.4 阶段三（学习适应）触发条件

- 阶段二完成
- 有玩家行为数据积累渠道
- 需要长期可玩性提升

---

## 九、技术风险评估

| 风险 | 概率 | 影响 | 缓解措施 |
|------|------|------|----------|
| JPS `mPathfindingCompleted` 非原子访问导致崩溃 | 中 | 高 | 改为 `std::atomic<bool>` |
| 团队共享黑板线程竞争 | 高 | 高 | 使用 `std::atomic` / 互斥锁，或无锁结构 |
| 听觉事件广播性能开销 | 中 | 中 | 限制广播频率（每 0.1s 一次），距离过滤 |
| 预判射击玩家速度估算不准 | 中 | 低 | 滑动窗口平均 + 卡尔曼滤波 |
| 难度自适应被玩家利用（故意送人头降低难度） | 中 | 中 | 设置难度下限 + 单向调节（只升不降） |
| 掩体探测射线开销 | 低 | 低 | 缓存结果，每 0.5s 探测一次 |
| 角色分配在 NPC 死亡后失衡 | 中 | 中 | 动态重新分配角色 |
| 持久化记忆文件损坏 | 低 | 中 | JSON + 校验和 + 默认值兜底 |

---

## 附录：现有代码关键路径索引

| 功能 | 文件 | 行号 |
|------|------|------|
| 感官系统 | `Character\NPCGOBT.cpp` | 261-295 |
| 世界状态同步 | `Character\NPCGOBT.cpp` | 232-255 |
| JPS 寻路调用 | `Character\NPCGOBT.cpp` | 402-405 |
| 攻击逻辑 | `Character\NPCGOBT.cpp` | 438-478 |
| 受伤恢复 | `Character\NPCGOBT.cpp` | 481-499 |
| Event 主循环 | `Character\NPCGOBT.cpp` | 512-531 |
| 目标注册 | `Character\NPCGOBT.cpp` | 67-91 |
| 分解策略 | `Character\NPCGOBT.cpp` | 93-144 |
| 子树注册 | `Character\NPCGOBT.cpp` | 146-208 |
| 武器系统 | `Arms\Arms.cpp` | 58-181 |
| 移动组件 | `Character\MovementComponent.cpp` | 83-156 |
| JPS 算法 | `Tool\JPS.h` | 555-675 |
| GOBT 框架 | `GOBT\gobot\` | 全目录 |

---

*文档结束。建议从阶段一的"听觉感知 + 记忆衰减 + 预判射击"三项开始实施，这三项改动量小、收益高、风险低。*
