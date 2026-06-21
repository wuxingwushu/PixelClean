# 行为树（BT）与 GOAP 结合方案规划

> 面向项目 `PixelClean/Character/NPC.cpp` 的 NPC AI 架构升级
> 文档版本：v1.0　|　编写日期：2026-06-20

---

## 目录

- [1. 结论速览](#1-结论速览)
- [2. BT 与 GOAP 能否结合](#2-bt-与-goap-能否结合)
  - [2.1 可行性结论](#21-可行性结论)
  - [2.2 两者职责互补性](#22-两者职责互补性)
  - [2.3 三种结合模式](#23-三种结合模式)
- [3. 轻量化库选型](#3-轻量化库选型)
  - [3.1 候选库总览](#31-候选库总览)
  - [3.2 行为树库对比](#32-行为树库对比)
  - [3.3 GOAP 库对比](#33-goap-库对比)
  - [3.4 推荐选型](#34-推荐选型)
- [4. 结合架构设计](#4-结合架构设计)
  - [4.1 分层职责](#41-分层职责)
  - [4.2 世界状态建模](#42-世界状态建模)
  - [4.3 GOAP 动作集](#43-goap-动作集)
  - [4.4 行为树结构](#44-行为树结构)
  - [4.5 Planner 节点工作流](#45-planner-节点工作流)
- [5. 集成到本项目的详细规划](#5-集成到本项目的详细规划)
  - [5.1 阶段划分总览](#51-阶段划分总览)
  - [5.2 阶段 0：引入依赖](#52-阶段-0引入依赖)
  - [5.3 阶段 1：黑板与世界状态](#53-阶段-1黑板与世界状态)
  - [5.4 阶段 2：GOAP 动作实现](#54-阶段-2goap-动作实现)
  - [5.5 阶段 3：BT 节点与组装](#55-阶段-3bt-节点与组装)
  - [5.6 阶段 4：替换 FSM 主循环](#56-阶段-4替换-fsm-主循环)
  - [5.7 阶段 5：调参与个性化](#57-阶段-5调参与个性化)
- [6. 风险与对策](#6-风险与对策)
- [7. 落地建议](#7-落地建议)
- [参考资料](#参考资料)

---

## 1. 结论速览

| 问题 | 结论 |
|------|------|
| BT 与 GOAP 能否结合？ | **能**。学术界已有 GOBT（Goal-Oriented Behavior Tree）框架，工业界亦有成熟实践。 |
| 怎么结合？ | BT 负责高层控制流（优先级/回退/条件），GOAP 作为 BT 内的"规划节点"负责动态生成多步动作序列，BT 执行该序列。 |
| 用什么库？ | 首选 **aitoolkit**（header-only，一个库同时提供 BT + GOAP + Utility + FSM，MIT，C++20）。备选 behavior-tree-lite + goap-cpp。 |
| 开发成本？ | 引入库 + 搭骨架约 2~3 天；完整迁移现有 6 状态逻辑约 1~2 周（可灰度并存）。 |

---

## 2. BT 与 GOAP 能否结合

### 2.1 可行性结论

**完全可以，且是业界推荐的混合架构。** 学术上，2023 年 JMIS 期刊论文《GOBT: A Synergistic Approach to Game AI Using Goal-Oriented and Utility-Based Planning in Behavior Trees》明确提出 **GOBT 框架**，将 BT 的逻辑结构能力 + GOAP 的动态规划能力 + Utility 的效用选择能力三者融合，通过在 BT 中引入"规划器节点"实现。工业上，Unreal Engine 的 BT 黑板 + 任务节点本身就常被用来包装 GOAP 规划器。

### 2.2 两者职责互补性

BT 与 GOAP 各有所长，恰好互补：

| 维度 | 行为树 BT | GOAP |
|------|-----------|------|
| 核心能力 | **控制流编排**（优先级、回退、并发、条件分支） | **动态规划**（从当前状态到目标状态的最小代价动作序列） |
| 决策方式 | 每帧自顶向下重评估，反应快 | 离散规划，生成序列后执行，"想清楚再做" |
| 适合场景 | "现在该做什么"（战斗/逃跑/巡逻的优先级） | "怎么做到"（消灭玩家需要：接近→掩体→射击→换弹） |
| 短板 | 静态结构，难表达多步规划；条件顺序耦合 | 不擅长优先级仲裁；规划开销随动作集增长 |

> **互补关系**：BT 解决"做什么"（What），GOAP 解决"怎么做"（How）。BT 把"消灭玩家"这个目标交给 GOAP，GOAP 规划出动作序列交回 BT 执行。

### 2.3 三种结合模式

**模式 A：GOAP 作为 BT 的规划节点（推荐）**

BT 的某个 Action 节点内部持有 GOAP goal，首次 tick 时规划，后续 tick 执行序列。BT 负责更高层的优先级仲裁。

```
BT Root (Selector 优先级)
├── [血量低] → Flee (GOAP goal: 到达安全点)
├── [可见玩家] → Combat (GOAP goal: 玩家死亡)
└── [无可疑] → Patrol (GOAP goal: 巡逻完成)
```

**模式 B：BT 执行 GOAP 生成的 plan**

GOAP 规划器在外层生成完整动作序列，BT 仅作为"plan 执行器"，按序列依次执行动作，失败则回退触发重规划。

**模式 C：分层——BT 选目标，GOAP 选动作**

BT 用 Utility/条件选出当前最高优先级目标（如"生存">"战斗">"巡逻"），把目标传给 GOAP，GOAP 规划动作序列执行。

> 本项目推荐 **模式 A**：最贴合现有 FSM 的"状态→行为"心智模型，迁移成本最低，且 BT 的优先级仲裁能直接替代当前散落在各状态函数里的 `process_event` 转移逻辑。

---

## 3. 轻量化库选型

### 3.1 候选库总览

本项目约束：C++（CMake + boost::sml + Vulkan）、需控制依赖与二进制体积、希望减少开发成本。因此优先 **header-only / 零依赖 / 小体量** 的库。

| 库 | 类型 | 语言标准 | 依赖 | 协议 | 提供 |
|----|------|----------|------|------|------|
| **aitoolkit** | header-only | C++20 | 无 | MIT | **FSM + BT + Utility + GOAP** |
| behavior-tree-lite | header-only | C++23 | 无 | — | BT |
| bt.cc | 双文件 | C++20 | 无 | — | BT |
| BrainTree | 单头文件 | C++11 | 无 | MIT | BT |
| Beehive | header-only | C++17 | 无 | — | BT |
| BehaviorTree.CPP | 共享库 | C++14 | Boost/tinyxml2/cppzmq | MIT | BT（+ Groot2 可视化） |
| goap-cpp | CMake | C++ | 无 | — | GOAP |
| lcthw goap.hpp | 单头文件 | C++ | nlohmann/json | — | GOAP（88 行） |

### 3.2 行为树库对比

| 库 | 体积/开销 | 优点 | 缺点 |
|----|-----------|------|------|
| **BehaviorTree.CPP** | ~500KB lib，运行时多态 15~30ns | 最成熟、功能全、Groot2 可视化编辑器、社区大 | 依赖 Boost 等、非 header-only、引入重 |
| **behavior-tree-lite** | ~0.1ns，零分配 | 编译期组合、`&&\|\|!` DSL 极简洁、零依赖 | 需 C++23、树结构编译期固定（不便运行时配置） |
| **bt.cc** | 数据/行为分离，TreeBlob | 多实体共享一棵树、适合大量 NPC | API 风格独特（`_()` 缩进）有学习成本 |
| **BrainTree** | 单头文件 | 接入最简单、API 直观 | 维护活跃度一般 |
| **Beehive** | <500ns | 模板元编程、零开销 | 过度依赖 TMP，可读性/调试性差 |

### 3.3 GOAP 库对比

| 库 | 优点 | 缺点 |
|----|------|------|
| **aitoolkit GOAP** | 与同库 BT 无缝协作、header-only、API 简洁 | 较新，文档偏少 |
| **goap-cpp** | 忠实实现 Orkin 2006 F.E.A.R. 论文、CMake、示例清晰 | 非 header-only、需单独集成 |
| **lcthw goap.hpp** | 88 行极简、bitset 状态 | 依赖 json、偏教学 |

### 3.4 推荐选型

**首选：[aitoolkit](https://github.com/linkdd/aitoolkit)**

理由：
1. **一个库同时提供 BT + GOAP**，天然支持两者结合，无需拼接两个异构库；
2. **header-only + 零依赖**，只需把 `include` 加入 CMake，零集成成本；
3. **MIT 协议**，商用友好；
4. **C++20** 兼容（项目已用 boost::sml 等现代特性，标准无障碍）；
5. 还附带 FSM/Utility，未来扩展留有余地。

**备选：behavior-tree-lite（BT）+ goap-cpp（GOAP）**
- 若希望 BT 极致轻量且能接受 C++23，BT 用 behavior-tree-lite；GOAP 用 goap-cpp。
- 代价：两个库 API 风格不一，需自写适配层。

> 下文规划以 **aitoolkit** 为基础。若选备选，架构与阶段划分同样适用，仅替换 API 调用。

---

## 4. 结合架构设计

### 4.1 分层职责

```
┌─────────────────────────────────────────────┐
│  感知层  Perception（复用现有 GetSensoryMessages）  │
│  输出：WorldState flags + 黑板数据              │
├─────────────────────────────────────────────┤
│  决策层  Behavior Tree（高层控制流 / 优先级仲裁）   │
│  每帧 tick：按优先级选分支，条件不满足则回退        │
├─────────────────────────────────────────────┤
│  规划层  GOAP Planner（动态多步规划）              │
│  BT 的 Planner 节点触发：goal → 动作序列          │
├─────────────────────────────────────────────┤
│  执行层  MovementComponent / Arms / JPS（复用现有） │
└─────────────────────────────────────────────┘
```

- **BT** 取代当前散落在 `Standby()/Patrol()/...` 内的 `process_event` 转移逻辑，恢复声明式、可推理的决策流。
- **GOAP** 取代当前硬编码的"太近后退/太远靠近"等命令式行为，让 NPC 自主规划"怎么消灭玩家"。
- **执行层完全复用** 现有 `MovementComponent`、`Arms`、`JPS`，不重写物理与寻路。

### 4.2 世界状态建模

GOAP 需要一个可比较的"世界状态"。本项目用 `bitset` 或字段枚举表示布尔事实：

```cpp
enum class WS : size_t {
    PlayerVisible,      // 玩家可见
    PlayerInRange,      // 玩家在攻击范围
    HasAmmo,            // 有弹药
    LowHealth,          // 血量低
    HasSuspiciousPos,   // 有可疑位置
    AtCover,            // 在掩体后
    PlayerDead,         // 玩家死亡
    AtSafeSpot,         // 到达安全点
    AtWaypoint,         // 到达巡逻点
    // ... 按需扩展
};
using WorldState = std::bitset<32>;
```

每帧由感知层与执行层回写：`PlayerVisible` 来自 `GetSensoryMessages()`，`HasAmmo` 来自 `Arms`，`LowHealth` 来自 `GamePlayer` 血量等。

### 4.3 GOAP 动作集

每个动作定义 *前置条件 / 后效 / 代价 / execute()*，execute 调用现有执行层：

| 动作 | 前置条件 | 后效 | 代价 | execute 调用 |
|------|----------|------|------|--------------|
| `MoveToPlayer` | PlayerVisible | PlayerInRange | 3 | JPS 寻路 + MovementComponent |
| `MoveToCover` | (有掩体) | AtCover | 2 | JPS 到最近掩体 |
| `Shoot` | PlayerInRange ∧ HasAmmo | PlayerHurt | 1 | Arms::ShootBullets |
| `Reload` | ¬HasAmmo | HasAmmo | 2 | Arms 换弹 |
| `Flee` | LowHealth | AtSafeSpot | 4 | JPS 远离玩家 |
| `Investigate` | HasSuspiciousPos | AtWaypoint | 3 | JPS 到可疑位置 |
| `PatrolWaypoint` | (无目标) | AtWaypoint | 2 | 沿巡逻路点 |

**目标（Goal）示例**：
- `GoalKillPlayer`：`distance_to = (PlayerDead ? 0 : 1)`
- `GoalSurvive`：`distance_to = (AtSafeSpot ? 0 : 1)`
- `GoalPatrol`：`distance_to = (AtWaypoint ? 0 : 1)`

### 4.4 行为树结构

用 BT 的 Selector（优先级）+ Sequence（条件→动作）组织，动作节点包装 GOAP goal：

```
Root [Selector 优先级从高到低]
├── [Sequence] 生存
│   ├── Condition: LowHealth
│   └── PlannerAction(goal=Survive)        // Flee
├── [Sequence] 受伤反应
│   ├── Condition: Hurt ∧ ¬PlayerVisible
│   └── Action: InjuryStagger              // 硬直+转向（无需规划）
├── [Sequence] 战斗
│   ├── Condition: PlayerVisible
│   └── [Selector]
│       ├── [Sequence] 近距交战
│       │   ├── Condition: PlayerInRange
│       │   └── PlannerAction(goal=KillPlayer)  // 接近/掩体/射击/换弹
│       └── PlannerAction(goal=ApproachPlayer)  // MoveToPlayer
├── [Sequence] 调查最后目击
│   ├── Condition: HasSuspiciousPos
│   └── PlannerAction(goal=Investigate)
└── Action: Patrol                            // PlannerAction(goal=Patrol)
```

> 这棵树直接替代了当前 6 个状态 + 散落转移的逻辑，且优先级一目了然：生存 > 受伤 > 战斗 > 调查 > 巡逻。

### 4.5 Planner 节点工作流

`PlannerAction` 是 BT 的 Action 节点，内部持有 GOAP goal 与缓存 plan：

```
tick():
  if plan 为空 or 世界状态发生显著变化:
      plan = planner.plan(currentState, goal, actions)
      if plan 为空: return Failure        // 规划失败，让 BT 回退到下一分支
      stepIndex = 0
  if stepIndex < plan.size:
      status = plan[stepIndex].execute()
      if status == Running: return Running
      if status == Failure:
          plan.clear()                     // 失败 → 下帧重规划
          return Running
      stepIndex++                          // 成功 → 推进下一步
      return Running
  return Success                           // plan 完成
```

关键点：
- **懒规划 + 缓存**：只在首次或世界状态显著变化时规划，避免每帧 A\* 搜索。
- **失败重规划**：动作失败清空 plan，下帧重新规划，自然适应动态环境。
- **Running 语义**：规划/执行中返回 Running，让 BT 持续占据该分支，避免抖动。

---

## 5. 集成到本项目的详细规划

### 5.1 阶段划分总览

| 阶段 | 目标 | 产出 | 风险 |
|------|------|------|------|
| 0 | 引入依赖 | aitoolkit 接入 CMake，编译通过 | 低 |
| 1 | 黑板与世界状态 | `NPCBlackboard` + `WorldState` + 感知回写 | 低 |
| 2 | GOAP 动作实现 | 7 个 Action 类，execute 复用现有执行层 | 中 |
| 3 | BT 节点与组装 | 条件节点 + PlannerAction + 树组装 | 中 |
| 4 | 替换 FSM 主循环 | `NPC::Event` 用 `bt.tick()` 替换 `process_event` | 高（灰度并存） |
| 5 | 调参与个性化 | 反应延迟/噪声/参数随机化 | 低 |

### 5.2 阶段 0：引入依赖

```cmake
# CMakeLists.txt（顶层或 Character/CMakeLists.txt）
include(FetchContent)
FetchContent_Declare(
  aitoolkit
  GIT_REPOSITORY https://github.com/linkdd/aitoolkit.git
  GIT_TAG        v0.5.1
)
FetchContent_MakeAvailable(aitoolkit)

target_include_directories(PixelClean PRIVATE ${aitoolkit_SOURCE_DIR}/include)
```

验证：写一个最小 `main` 用 aitoolkit 的 BT + GOAP 跑通"拾斧砍树"示例，确认编译与链接。

### 5.3 阶段 1：黑板与世界状态

新增 `Character/NPCBrain.h`（保持 NPC.h/cpp 不动，便于灰度）：

```cpp
struct NPCBlackboard {
    GamePlayer* npc = nullptr;
    PathfindingDecorator* path = nullptr;
    Arms* arms = nullptr;
    // 感知缓存
    int sensoryFlags = 0;
    float playerDistance = 0.0f;
    float wanjiaAngle = 0.0f;
    // 记忆
    bool hasSuspicious = false;
    JPSVec2 suspiciousPos{};
    // 个性化参数（构造时随机化）
    float reactionDelay = 0.3f;
    float shootInterval = 0.8f;
    float aimError = 0.15f;
    // 运行时
    float fpsTime = 0.0f;
    float awareness = 0.0f;   // 察觉进度（实现反应延迟）
};

WorldState BuildWorldState(const NPCBlackboard& bb);  // 每帧由感知回写
```

`BuildWorldState` 复用现有 `GetSensoryMessages()` 逻辑，把 flags 映射到 `WorldState` bitset。

### 5.4 阶段 2：GOAP 动作实现

每个动作继承 aitoolkit 的 GOAP Action 基类，`execute()` 调用现有执行层。示例（射击）：

```cpp
class ShootAction : public goap::Action<NPCBlackboard> {
    bool can_run(const NPCBlackboard& bb) override {
        return (bb.sensoryFlags & SensoryMessages_Visible)
            && (bb.sensoryFlags & SensoryMessages_Range);
    }
    void plan_effects(WorldState& ws) override {
        ws.set(WS::PlayerHurt);   // 规划用：射击使玩家受伤
    }
    Status execute(NPCBlackboard& bb) override {
        // 复用现有 Attack() 中的射击冷却逻辑
        bb.arms->ShootBullets(...);
        return Status::Success;
    }
};
```

其余 `MoveToPlayer / MoveToCover / Reload / Flee / Investigate / PatrolWaypoint` 同理，`execute` 内部调用 `JPS` + `MovementComponent`，返回 `Running` 直到到达目标。

### 5.5 阶段 3：BT 节点与组装

```cpp
// 条件节点
struct IsLowHealth : bt::Condition<NPCBlackboard> { /* ... */ };
struct IsHurtNotVisible : bt::Condition<NPCBlackboard> { /* ... */ };
struct IsPlayerVisible : bt::Condition<NPCBlackboard> { /* ... */ };
struct IsPlayerInRange : bt::Condition<NPCBlackboard> { /* ... */ };
struct HasSuspicious : bt::Condition<NPCBlackboard> { /* ... */ };

// Planner 动作节点（包装 GOAP goal）
class PlannerAction : public bt::Action<NPCBlackboard> {
    goap::Goal goal_;
    std::vector<goap::Action<NPCBlackboard>*> actions_;
    std::vector<goap::Action<NPCBlackboard>*> plan_;
    size_t step_ = 0;
    Status tick(NPCBlackboard& bb) override { /* 见 4.5 工作流 */ }
};

// 组装树（aitoolkit DSL）
auto tree = bt::Selector{
    bt::Sequence{ IsLowHealth{},    PlannerAction{GoalSurvive{},     actions} },
    bt::Sequence{ IsHurtNotVisible{}, InjuryStagger{} },
    bt::Sequence{ IsPlayerVisible{},
        bt::Selector{
            bt::Sequence{ IsPlayerInRange{}, PlannerAction{GoalKillPlayer{}, actions} },
            PlannerAction{GoalApproach{}, actions}
        }
    },
    bt::Sequence{ HasSuspicious{},   PlannerAction{GoalInvestigate{}, actions} },
    PlannerAction{GoalPatrol{},      actions}
};
```

### 5.6 阶段 4：替换 FSM 主循环

灰度迁移：在 `NPC` 中新增 `NPCBrain brain_`，`Event()` 同时跑旧 FSM 与新 BT，用开关切换；验证稳定后删除旧 FSM。

```cpp
void NPC::Event(int Frame, float time) {
    mTime += time;
    FPSTime = time;

    // === 新架构 ===
    brain_.Update(time);          // 感知回写 + bt.tick() + GOAP 规划/执行

    // 统一施力与刷新（不变）
    mNPC->GetMovement()->Update(FPSTime);
    mNPC->UpData();
    mNPC->setGamePlayerMatrix(time, Frame);
}
```

迁移映射表（旧状态 → 新 BT 分支）：

| 旧 FSM 状态 | 新 BT 分支 | 旧逻辑去向 |
|-------------|-----------|-----------|
| Standby | Root 默认分支 / Patrol | 感知回写 + BT 优先级仲裁 |
| Patrol | Patrol PlannerAction | PatrolWaypoint 动作 |
| Chasing | ApproachPlayer PlannerAction | MoveToPlayer 动作 |
| Attack | KillPlayer PlannerAction | Shoot/MoveToCover/Reload 动作 |
| Injury | InjuryStagger Action | 硬直+转向（无需规划） |
| Escape | Survive PlannerAction | Flee 动作（**补全原空实现**） |

### 5.7 阶段 5：调参与个性化

在 `NPCBlackboard` 构造时对 `reactionDelay / shootInterval / aimError / TurnRate / AttackRange` 在区间内随机化，让每个 NPC 节奏不同；并在感知回写中实现"察觉进度"累计（玩家进视野后 `awareness` 随时间增长，达阈值才置 `PlayerVisible`），消除即时反应的呆板感。

---

## 6. 风险与对策

| 风险 | 影响 | 对策 |
|------|------|------|
| GOAP 规划开销（A\* 搜索） | 多 NPC 时帧预算吃紧 | ① 限制动作集规模（≤10）；② 缓存 plan，仅世界状态显著变化才重规划；③ 异步规划（复用现有线程池） |
| BT 每帧全树评估 | CPU 浪费 | ① 节流：感知 0.1s 一次而非每帧；② 分帧 tick：N 个 NPC 分组轮转 |
| 迁移引入回归 | 行为异常 | 灰度并存 + 开关切换，逐状态迁移验证 |
| aitoolkit 较新、文档少 | 踩坑成本 | 先跑官方示例验证；必要时备选 behavior-tree-lite + goap-cpp |
| GOAP 规划出"合理但奇怪"行为 | 体验问题 | 限制动作前置条件、调代价权重；关键行为用 BT 硬编码兜底 |
| 旧 FSM "歪门邪道"集成遗留 | 删除风险 | 阶段 4 验证稳定后再删 boost::sml 相关代码 |

---

## 7. 落地建议

1. **先验证库**（阶段 0，半天）：FetchContent 接入 aitoolkit，跑通官方 BT+GOAP 示例，确认编译/性能可接受。
2. **搭最小骨架**（阶段 1~3，2~3 天）：实现黑板 + 2 个动作（MoveToPlayer、Shoot）+ 1 个 PlannerAction，让一个 NPC 能"看见→规划接近→射击"跑通。
3. **灰度迁移**（阶段 4，1 周）：按迁移映射表逐状态切换，旧 FSM 保留作回退，每迁一个状态验证一次。
4. **调参润色**（阶段 5，2~3 天）：反应延迟、参数个性化、巡逻路线化。
5. **清理**：稳定后删除 boost::sml FSM 与 `Pathfinding()` 废弃方法。

> **关键提醒**：BT+GOAP 解决的是"决策呆板"，但"反应即时"与"同质化"两大呆板根因需靠阶段 5 的反应延迟 + 参数随机化收尾，二者缺一不可。

---

## 参考资料

- [GOBT: A Synergistic Approach to Game AI Using GOAP and Utility in Behavior Trees (JMIS 2023)](https://www.jmis.org/download/download_pdf?pid=jmis-10-4-321)
- [Behavior Trees and Utility Selection - deepwiki](https://deepwiki.com/ruvnet/sublinear-time-solver/10.3-behavior-trees-and-utility-selection)
- [Game AI Behavior Framework - egoai](https://www.egoai.com/blog/ai-behavior-framework)
- [aitoolkit - header-only C++ FSM/BT/Utility/GOAP 库](https://github.com/linkdd/aitoolkit)
- [behavior-tree-lite - C++23 header-only BT](https://pavelguzenfeld.com/projects/behavior-tree-lite/)
- [bt.cc - 数据与行为分离的 C++ BT 库](https://github.com/hit9/bt.cc)
- [goap-cpp - 基于 F.E.A.R. 论文的 C++ GOAP](https://gitmemories.com/index.php/cvra/goap-cpp)
- [BehaviorTree.CPP - 成熟的 C++ BT 库](https://github.com/BehaviorTree/BehaviorTree.CPP)
