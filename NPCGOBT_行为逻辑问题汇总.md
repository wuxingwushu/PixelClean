# NPCGOBT 行为逻辑 / 行为实现问题汇总

审阅对象：`Character/NPCGOBT.cpp`（1247 行）+ `Character/NPCGOBT.h`（163 行）
方法：逐行静态审阅 + 交叉验证上下游（`Character/GamePlayer.cpp`、`Character/Crowd.cpp`、`Arms/Arms.cpp`、`GOBT/gobot/layers.hpp` 的 tick 语义）
未做的事：没有实际运行游戏（本机缺 GLFW 等依赖，游戏未能构建），以下结论均为**代码级可证**的推断；条目 1、2 给出了精确的状态轨迹，可用 `GOBT/verify` 那套 harness 改造成状态序列复现。

**当前状态：条目 1~15 已按文末《建议的修复顺序》全部实施完毕（详见最后一节《修复记录》），`CharacterLib` 编译通过；仍未实机运行游戏。**

严重度含义：**P0** = 行为与设计意图明显不符、玩家可感知；**P1** = 逻辑缺陷/性能与日志污染；**P2** = 死代码、命名与注释不一致、可维护性。

---

## 结论速览

| # | 严重度 | 一句话 | 关键位置 |
|---|---|---|---|
| 1 | **P0** | 调查/搜索/绕圈/压制射击这一整套子系统被脱战逻辑在 1 帧内掐死，实际不可达 | `NPCGOBT.cpp:461` ↔ `:974` |
| 2 | P1 | Retreat 的 Effect 每帧被世界状态重新武装，目标在"满足/不满足"间抖动 → 每 2 帧重建一次子目标直到计时器耗尽 | `NPCGOBT.cpp:305` ↔ `:414` |
| 3 | P1 | 感知真值穿墙：瞄准/转向/调查方向都用 `Global::GamePlayerX/Y` 实时坐标，与"绝不隔墙"的注释自相矛盾 | `NPCGOBT.cpp:517`、`:1082`、`:1124`、`:1153` |
| 4 | P1 | 脱战兜底 `kDisengageLostTime(10s)` 是死条件，永远被 8s 记忆耗尽抢先 | `NPCGOBT.cpp:23`、`:455`、`:461` |
| 5 | P1 | 寻路没有超时，JPS 任务一旦不返回完成标志，NPC 永久静默站桩 | `NPCGOBT.cpp:710`、`:938` |
| 6 | P1 | 追击连续失败 3 次会让 `FightEnemy` 被挂起 180 tick（60fps ≈ 3s），期间**即使玩家走到脸上也不交战** | `NPCGOBT.cpp:866/931/954/995` + `layers.hpp:52` |
| 7 | P2 | 写而不读的状态：`lastDamageCount_`、`kInjured`、`kPlayerInViewField`、`SensoryMessages_Range` | `:409`、`:374`、`:492`、`:548` |
| 8 | P2 | `"patrol_done"` 是裸字符串且永不写入；一旦被外部写入，NPC 会无目标空转 | `:158` |
| 9 | P2 | 三处注释/日志与实际实现不符（单一写入方、威胁解除、最后方位压制） | `:406`、`:1117`、`:1206` |
| 10 | P2 | `rand()` 未在本文件播种 → 未启用 BlockS 模式时巡逻点每次运行完全相同 | `:619`、`:813`、`:917`、`:1123` |
| 11 | P2 | `mTime` 一物三用（硬直/重寻路/巡逻），待机为此不得不另开计时器 | `:700`、`:1012`、`:1139`、`:576` |
| 12 | P2 | 6 个动作全部未使用 `ctx` 形参；5 处重复的"恢复受控模式"样板 | 各动作入口 |
| 13 | P2 | 析构里无 sleep 的自旋等待 + stdout 刷屏；`delete mNPC` 早于等待寻路线程 | `:112-116` |
| 14 | P2 | `SetNPC` 传送后不清路径/寻路状态，残留路径会把 NPC 拉回旧路线 | `:1217` |
| 15 | P2 | 两处防御性检查量纲写错，条件恒为真/几乎不可达 | `:625`、`:748` |

---

## 1. P0 — 调查/搜索子系统被脱战逻辑一帧内掐死

**现象**：NPC 一失去视线，追到下一次重新寻路时就放弃追击、回巡逻。`mInvestigating` / `mSearching`（绕圈搜索、就地搜索、到达目击点后搜索、搜索期压制射击）这几段代码在实战里基本跑不到。

**证据链（逐帧）**

1. 玩家可见期间，`GetSensoryMessages()` 每帧把可疑记忆刷新满：`:554-558` → `mSuspicious = true; mSuspiciousTimer = kSuspiciousMemory(8s)`。
2. 玩家躲进掩体那一帧：`SyncWorldState()` 的脱战判定 `bool memoryGone = !mSuspicious;`（`:461`）为 **false**（记忆还在），不脱战 —— 正确。
3. 同一帧行为树 tick，`DoChase()` 判定 `needRepath`（`:968`）走 `mSuspicious` 分支：`:973-975`
   ```cpp
   target = mSuspiciousPos;
   mSuspicious = false;              // 消费记忆（再次可见时由感官刷新）
   mInvestigating = !visible;        // 前往"旧目击点"才算调查
   ```
   **记忆被"消费"掉了**，但 `mSuspiciousTimer` 仍是 ~7.98s。
4. 下一帧 `SyncWorldState()`：`mEngaged` 为真且不可见 → `memoryGone = !mSuspicious = true` → `:464-470` 立刻脱战：
   ```cpp
   mEngaged = false;
   mSuspicious = false;
   mSuspiciousTimer = 0.0f;
   mSearching = false;
   mInvestigating = false;   // ← 刚置上的调查标志被清掉
   mPlayerLostTime = 0.0f;
   ```
   `kPlayerEngaged` 写 false → `FightEnemy` 目标被判定满足 → 规划器切回 `PatrolArea`（其分解策略在 `:237` 调 `ClearPathSafe()` 把在途路径丢掉）→ NPC 停步。

**结论**：`memoryGone` 的实际语义是"记忆被消费"，而不是注释/常量想表达的"记忆已耗尽（8s）"。两个子系统对同一个变量赋予了不同含义。

**影响**：`:869-935`（绕圈搜索 + 压制射击）、`:945-951`（无法到达目击点时就地搜索）、`:976-986`（不可见时的站距回退 `kStandOff`）、`:1031-1037`（到达目击点后开始搜索）全部成为事实上的死代码；玩家只要绕过一个墙角就能彻底摆脱追击。

**建议（任选，推荐 a）**

- a) 把 `:461` 改为不把"已认领"当"已耗尽"：
  ```cpp
  bool memoryGone = (mSuspiciousTimer <= 0.0f) && !mInvestigating && !mSearching;
  ```
- b) 不消费记忆：新增 `mLastKnownPos` / `mLastKnownValid`，`:974` 只标记"已认领"，`mSuspicious` 交给 8s 衰减；
- c) 明确分层：`mSuspicious`（是否知道某处有情况）与 `mSuspicionClaimedUntil`（是否已经派过路径）分开，脱战只看前者。

---

## 2. P1 — Retreat 的 Effect 每帧被重新武装 → 子目标抖动重建

**位置**：效果声明 `:301-308`（`recently_hurt=false`）↔ 世界状态每帧覆盖 `:414`（`ws.set(kRecentlyHurt, mRecentlyHurtTimer > 0.0f)`）↔ `:185-197` 分解策略每次调用都重置 `mFleeEntered/mFleeTimer` ↔ `:1204` 后撤完成判定。

**逐帧轨迹**

- 受击 → `mRecentlyHurtTimer = 2.5s`（`:378`）→ `SelfPreserve`(90) 抢占 `FightEnemy`(80)。
- `DoFlee` 每帧把 `awayLen` 与 180px、`mFleeTimer` 与 1.2s 比较（`:1204`）。距离一旦拉开，子目标在 1.2s 内（常常更快）返回 `Success`。
- `OperationalActionNode` 在返回 Success 时应用 Effect → `recently_hurt=false` → **同一 tick 内** `StrategicGoalNode`（`GOBT/gobot/layers.hpp:344-349`）用 `goal->satisfied_by(ws)` 看到 false → 目标判定满足 → 广播 `kGoalCompleted`。
- **下一帧** `SyncWorldState()` 把 `kRecentlyHurt` 重新写成 `true`（计时器还剩 ~1.3s）→ 目标又"不满足"了；由于当前目标没变，`StrategicGoalNode` 不会走 `:333` 的重分解分支，而是在 `layers.hpp:347` 用 `!bb.current_subgoal()` 触发重分解 → `:185-197` 把 `mFleeEntered/mFleeTimer` 清零 → 新子目标立刻又能成功。

**结果**：在剩余的 ~1.3s 内，以约 2 帧为周期反复执行「分解 → 新建 `Action`+`OperationalActionNode`（各一次 `make_shared`）→ 子目标成功 → 弹出 → 广播 `kSubgoalCompleted` + `kGoalCompleted` → 规划器 `restore_all_priorities()` 全量重排」；每个受击约 30~40 轮，附带每轮 3 行 `LOGD`（`Decompose[SelfPreserve]` / `开始后撤` / `后撤完成`）。

**影响**：日志与事件风暴、每秒上百次小额堆分配；实际后撤时长由 `mRecentlyHurtTimer`(2.5s) 决定，`kFleeDuration`(1.2s) 与 `kFleeDistance`(180) 只约束单轮，参数语义被悄悄改写；`:1206` 的"威胁解除 → Success"日志是错的（下一帧又开打）。

**一般规律（值得写进设计文档）**：`SyncWorldState` 每帧强制写 `kPlayerVisible / kPlayerInRange / kPlayerInViewField / kPlayerEngaged / kRecentlyHurt`，因此**任何 Action 对这几个键的 Effect 都在下一帧被冲掉**。目前唯一"动作侧拥有"的键是 `injury_recovered`（`:377` 只在受击时写 false，`Recover` 的 Effect 写 true 后不会被重新武装），所以只有它工作正常。

**建议**：把 `SelfPreserve` 的满足条件换成"动作侧拥有"的键（例如 `threat_cleared`，由 `DoFlee` 成功时置位、只在受击时清零），或在 `Goal` 语义上支持"当前子目标链完成即算达成"。

---

## 3. P1 — 感知真值穿墙（设计原则 vs 实现）

设计文档与本文件注释多处声明"NPC 只能依靠视线发现玩家，绝不隔墙感知、绝不隔墙攻击"，实现只做到了**"发现"这一半**：

- `GetSensoryMessages()` 无条件用实时真值算方位与距离（`:512-517`）：
  ```cpp
  float dx = Global::GamePlayerX - pos.x;
  float dy = Global::GamePlayerY - pos.y;
  mPlayerDistance = std::sqrt(dx * dx + dy * dy);
  wanjiaAngle = PhysicsBlock::EdgeVecToCosAngleFloat(glm::vec2{dx, dy});
  ```
  这是**每帧刷新的精确方位**，与 `SensoryMessages_Visible` 无关。
- `DoAttack()` 用 `wanjiaAngle` 转向（`:1082`）并用它开火（`:1124`）。视线断开后的 `kAttackLosGrace`(0.6s) 宽限里，炮塔会精确指向墙后的玩家实时位置；注释 `:1117` 写的是"朝最后方位压制"，实现是"朝实时坐标压制"。
- `DoInjury()` 硬直中 `SetLookAngle(wanjiaAngle)`（`:1153`）→ 被击中时立刻隔墙正对玩家。
- 被击中的调查方向同样取实时坐标（`:389-400`），而注释 `:380-381` 声称"只知道方向，不隔墙知道精确坐标"——那个方向就是从真值算出来的。

**守住了的部分（值得保留）**：进入交战只由 `Visible` 或受击触发（`:444-450`）；`mSuspiciousPos` 只在可见时刷新（`:551-558`）。所以"隔墙发现"确实被禁止了，问题只出在"已经知道玩家存在之后，持续追踪的精度"。

**影响**：公平性与观感——玩家能明显看到 NPC 隔着墙把炮口怼在自己身上；也让"最后目击位置"这套设计失去意义。

**建议**：在 `SyncWorldState()` 里维护 `mLastKnownPlayerPos` / `mLastKnownAngle`（仅 `Visible` 时更新），`DoAttack` 在 `!visible` 时用 lastKnown 瞄准与开火，`DoInjury` 用 lastKnown（或受击方向近似）转向。

---

## 4. P1 — 脱战兜底 `kDisengageLostTime` 是死条件

`mPlayerLostTime` 只在一处累加（`:455`），而**所有能刷新可疑记忆的路径同时也把它清零**：可见时 `:453`、受击时 `:383`。因此当 `mPlayerLostTime` 达到 10s 时，`mSuspiciousTimer`（上限 8s，`:557`）必然早已耗尽 → `memoryGone`（`:461`）先成立 → `lostTooLong`（`:462`）永远不会单独决定脱战，`:471-472` 的第三个日志分支不可达。

**建议**：要么删掉该常量与分支，要么让"记忆衰减"与"丢失计时"解耦（例如记忆只由"看见"刷新、丢失计时不受受击影响）。

---

## 5. P1 — 寻路无超时 → 永久静默站桩

- 巡逻：`:710-714` `PATROL_PATHFINDING` 只等 `mJPS->GetPathfindingCompleted()`，没有超时、没有失败恢复。
- 追击：`:938-940` 同理，卡住期间不会移动也不会开火（压制射击只在 `mSearching` 分支里）。

`mPathfindingCycle`(1.5s) 只用于"该不该重新提交寻路"（`:968`），不是"等太久就放弃"。线程池任务丢失、JPS 内部异常、进程退出竞态都会让 NPC 永久僵住且没有任何日志。

**建议**：加 `mPathfindingWait += FPSTime`，超过约 1s 就 `ClearPathSafe()` + 退回 `PATROL_IDLE` / 返回 `Failure`，并 `LOGD` 告警。

---

## 6. P1 — 连续 3 次追击失败 → 3 秒内无视玩家

`DoChase()` 有 4 个 `Failure` 出口（`:866`、`:931`、`:954`、`:995`）。GOBT 侧 `StrategicPlanner` 的阈值是 `kMaxConsecutiveFailures = 3`，达到后对当前目标执行 `suspend(name, 180)`；而 `GoalManager::select_top`（`GOBT/gobot/layers.hpp:52`）**直接跳过被挂起的目标**，不看其激活条件是否重新成立。挂起计数按 `select_top` 调用次数递减，60fps 下 180 tick ≈ 3 秒。

**后果**：只要连续 3 次追击失败（例如玩家站在 JPS 判定不可达的位置、被卡住的追击），`FightEnemy` 会被挂起 3 秒；这段时间里即使玩家走到 NPC 面前、`player_engaged` 为 true，NPC 也会继续执行 `PatrolArea`。

**建议**：给"重新看见玩家"加解除挂起（`mGoalManager->unsuspend("FightEnemy")`），或让挂起只对"目标不可达"类失败生效。

---

## 7-15. P2 清单

7. **写而不读的状态**
   - `lastDamageCount_`：`:409` 写，全文件无读（头文件 `NPCGOBT.h:99` 注释声称用于"检测新伤害"，实际新伤害判定直接看 `:371-372`）。
   - `NPCWS::kInjured`：`:85`、`:374` 写，仓库内无任何读取方。
   - `NPCWS::kPlayerInViewField`：`:89`、`:492` 写，无读取方。
   - `SensoryMessages_Range`：`:548` 置位，无任何调用方读取（范围判定实际走 `mInRangeLatched` / `mPlayerDistance`）。
   → 建议删除，或补上真正使用它们的条件节点（否则下个维护者会以为它们有效）。

8. **`"patrol_done"` 裸字符串**（`:158`）：不在 `NPCWS` 命名空间，且设计文档明确"此键永不设置"。风险在于一旦有其它系统写入 `true`，`PatrolArea` 被永久判定满足 → `select_top` 返回 `nullptr` → `StrategicGoalNode` 走 `layers.hpp:323-330` 返回 Success，NPC 不再产生任何移动输入（而 `MovementComponent` 是否保留上一帧输入决定了它是"站桩"还是"一直往前走"）。建议改成 `NPCWS::kPatrolDone` 常量并在注释里固定"永不设置"的约定。

9. **注释/日志与实现不符**
   - `:406-408` 声称 `injury_recovered` "只由 Recover 动作成功后的 Effect 写入（单一写入方）"——实际 `:377` 也写（受击时置 false）。准确说法是"只有置 **true** 是单一写入方"。
   - `:1206` "后撤完成 → 威胁解除"：下一帧 `kRecentlyHurt` 就被重新写成 true（见条目 2）。
   - `:1117` "朝最后方位压制"：实际用实时方位（见条目 3）。

10. **`rand()` 播种**：本文件四处用 `rand()`（`:619-620`、`:813`、`:917`、`:1123`），全仓库只有 `GameMods/BlockS/BlockS.cpp:282` 调 `srand(time(NULL))`；若该模式未启用，巡逻目标序列每次运行完全一致（对一个开放地图的 NPC 而言是明显的可预测性）。建议改用 `<random>` + 每实例 `std::mt19937`（也顺带摆脱全局状态与取模偏差）。

11. **`mTime` 一物三用**：受击硬直计时（`:1139`/`:1156`）、追击重寻路周期（`:968`/`:1012` 置零）、巡逻提交计时（`:700` 置零）。`DoStandby` 不得不另开 `mStandbyTimer`（`:576-584` 的注释就是这次踩坑的记录）。当前动作互斥所以没爆，但任何"一帧内跑两个动作"的改动都会立刻出错。建议按用途拆成独立成员。

12. **样板重复**：`GetMovement()->GetMode() != MovementMode::Ragdoll → SetMode(Controlled)` 在 `:572`、`:659`、`:843`、`:1058`、`:1167` 各写一遍；6 个动作执行器（`DoStandby/DoPatrol/DoChase/DoAttack/DoInjury/DoFlee`）**全部没有使用 `ctx` 形参**（签名由 `gobot::ActionExecutor` 强制，但 `/W4` 下应有 6 个 C4100，值得确认 `Character` 目标是否真的开了 `/W4`）。

13. **析构**（`:108-117`）
    ```cpp
    delete mNPC;                                        // 先删 NPC
    while (!mJPS->GetPathfindingCompleted()) {          // 再无 sleep 自旋
        std::cout << "~NPCGOBT() AStar 等待线程结束" << std::endl;
    }
    delete mJPS;
    ```
    无 `sleep` 的自旋 + 每轮一行 stdout；若在途任务永不完成，析构会永久卡住主线程。顺序上也应"先等寻路结束，再 `delete mNPC`"（当前 JPS 回调只用 `wPathfinding`，所以暂时没踩到 UAF，但这是个定时炸弹）。
    顺带核实：`Arms::RegisterTankBulletHandler`（`Arms/Arms.cpp:69-76`）把回调存进 `GamePlayer` 自身，随 `delete mNPC` 一起销毁，**不存在悬垂回调**。

14. **`SetNPC` 传送不清理运动状态**（`:1217-1223`）：直接改物理体坐标与角度，不重置 `LPath` / `mJpsSubmitted` / `mPatrolState` / `mInvestigating`。复活或传送后，残留路径会立刻把 NPC 往旧路线拉；`PATROL_MOVING` 里的距离检查（`:748`）可能来不及兜住。

15. **量纲写错的防御性检查**
    - `:625-627`：采样半径是 `mRange/2 = 150`（`:615`），而合法性检查用 `mRange = 300` → `continue` 分支不可达。
    - `:748`：`目标距离 > mRange * 1.5 = 450`，而巡逻目标最远只可能偏离 ~212px → 该兜底几乎不可达。

---

## 已核实**没有**问题的点（避免误伤）

- **伤害队列时序自洽**：`GamePlayer::UpData()`（`Character/GamePlayer.cpp:366-377`）每帧排空 `mPixelQueue`，而 `NPCGOBT::Event` 的顺序是 `SyncWorldState()`(`:1232`) → `tick_once()`(`:1235`) → `UpData()`(`:1241`)，所以 `:371` 与 `:1139` 的 `GetNumber() > 0` 恰好等价于"本帧受到伤害"。
- **`LPath` 无数据竞争**：所有 `clear()` / `enqueue()` 之前都先检查 `GetPathfindingCompleted()`（`:681`、`:712`、`:938`，以及 `ClearPathSafe()`(`:601`)），JPS 写缓冲与主线程读缓冲不会重叠。
- **感官采样每帧只做一次**：`mSensoryFrameStamp`/`mFrameCounter`(`:504-507`、`:1229`) 让 `SyncWorldState` 与各动作共享同一次射线检测。
- **所有权无双重释放**：`Crowd::NPCEvent`（`Character/Crowd.cpp:182`）删除 `NPCGOBT`，`GamePlayer` 由 `Crowd` `new` 出来后交给 `NPCGOBT` 独占（`:129`/`:142`、`:152`/`:169`），析构里 `delete mNPC` 与之一致。
- **"隔墙发现"确实被禁止**：`:444-450`、`:551-558`。
- **子目标重试语义使用正确**：`retry_limit` 1（Recover/Retreat/Standby/Patrol）与 3（Attack/Chase）与各自动作的失败频度匹配。

---

## 建议的修复顺序

1. 条目 1（一行判断即可解除整套子系统的封印）——收益最高、改动最小。
2. 条目 2、4（参数语义与日志可信度），可与 1 同批改。
3. 条目 3（lastKnown 缓存）——涉及观感与公平性，改动中等。
4. 条目 5、6（健壮性）。
5. 条目 7~15（清理，可与上面任意批次并行）。

**回归验证建议**：把 `GOBT/verify/scenario.inc` 扩展成"状态序列驱动"（脚本按帧喂 `kPlayerVisible/kPlayerEngaged/kRecentlyHurt` 等键，再断言目标切换与子目标流水），即可在不开游戏的情况下复现并锁定条目 1、2、6 的行为。

---

## 修复记录（已实施）

改动范围：`Character/NPCGOBT.cpp`（1247 → 1369 行）、`Character/NPCGOBT.h`（163 → 180 行，新增最后目击记录/随机数/独立计时器等成员与 `PATROL_FALLBACK` 状态），均按上面的修复顺序执行；累计 `+247 / -108` 行。

**验证方式**：`cmake --build build/debug --target CharacterLib --config Debug`（只重编 `Crowd.cpp`、`NPCGOBT.cpp`）→ 产出 `CharacterLib.lib`、**退出码 0、无任何 error**；告警全部是基线同类（`PathfindingDecoratorDeviation*` / `RadialCollisionDetection` 的无符号→float C4244、`Tool/MemoryPool.h` C4624、`Tool/ThreadPool.h` C4996），未引入新类别。**仍未实机运行游戏**——行为正确性目前是代码级论证 + 编译验证，建议按上面的"回归验证建议"补一个状态序列测试。

| # | 处置 | 关键改动（`NPCGOBT.cpp` 行号为修复后） |
|---|---|---|
| 1 | 已修 | 脱战判定改为 `bool memoryGone = (mSuspiciousTimer <= 0.0f) && !mInvestigating && !mSearching;`（`:504`）；记忆衰减不再被 `mSuspicious` 门控（认领目击点后会清该标志，旧写法会把计时冻结）；`DoChase` 不再"认领即消费"记忆，改为 `if (mSuspicious && !visible)`（`:1050`）——旧写法让 ≤1.5s 后的下一次重寻路直接 Failure，3 次失败即挂起 `FightEnemy` |
| 2 | 已修 | 世界状态对 `kRecentlyHurt` 改为**边沿写入**（新伤害置 true、计时耗尽置 false，`:440` 附近）；Retreat 的 Effect 因此持久生效，目标不再每 2 帧被重新武装 |
| 3 | 已修（留 1 处） | 新增 `mLastKnownPlayerPos / mLastKnownPlayerAngle / mLastKnownValid`，仅在可见帧刷新（`:598`）；瞄准/转向/调查方向/撤离方向全部改用最后目击信息；**保留**：`DoAttack` 入口仍用真值距离早退（保守做法，不泄露坐标） |
| 4 | 随 1 复活 | `kDisengageLostTime(10s)` 不再是死条件：8s 记忆只有在"既不再调查也不再搜索"之后才算耗尽，10s 兜底重新成为最后一道闸；改的只是注释与措辞，无逻辑改动 |
| 5 | 已修 | 新增 `constexpr float kPathfindTimeout = 2.0f`（`:33`）与 `mPathfindWait`；`DoChase` 滞留超时 → Failure（`:1010`），`DoPatrol` 滞留超时 → 进入新状态 `PATROL_FALLBACK`（`:767`）；新增 `PATROL_FALLBACK` 分支独占降级巡逻（`:847`），避免旧实现"只跑 1 帧就回 IDLE"的来回横跳 |
| 6 | 已修 | 两处 `mGoalManager->unsuspend("FightEnemy")`：受到新伤害时（`:411`）、由不可见转为可见时（`:484`，带 `is_suspended` 前置判断） |
| 7 | 已修 | 删除只写不读的 `kInjured`、`kPlayerInViewField`、`SensoryMessages_Range`、`lastDamageCount_`（全仓库已无引用） |
| 8 | 已修 | 新增 `NPCWS::kPatrolDone` 常量并在头文件注明"永不设置：PatrolArea 是无限兜底" |
| 9 | 已修 | 三处注释/日志重写为与实现一致：世界状态的唯一写入方约定、Retreat Effect 为何现在真正生效、`DoAttack`"绝不朝玩家实时坐标开火" |
| 10 | 已修 | 新增成员 `std::mt19937 mRng{std::random_device{}()}` 与 `RandInt/Rand01` 包装，替换 4 处 `rand()`（巡逻点采样、降级巡逻起点、压制射击散布、瞄准误差）；顺带修掉 `FindRandomWalkablePosition` 在 `patrolRange < 1` 时的 `% 0` UB |
| 11 | 已修 | 删除一物三用的 `mTime`，拆为 `mInjuryTimer`（DoInjury 自持，`:1226`）、`mRepathTimer`（DoChase 自持，`:915`）、`mPathfindWait`（JPS 等待） |
| 12 | 已修 | 新增 `void EnsureControlled()`（`:653`）替代 5 处重复样板（`:619`/`:711`/`:914`/`:1140`/`:1262`）；6 个动作的 `ctx` 形参改无名（确认确实未使用） |
| 13 | 已修 | 析构顺序改为"先等 JPS → `delete mJPS` → `delete mNPC`"（`:133`）；等待用 `sleep_for(1ms)`，日志每 500ms 一条 LOGD 而非每轮一行 stdout，超时上限 2s 后打印警告继续 |
| 14 | 已修 | `SetNPC` 传送后清理残留状态：`ClearPathSafe()` + 清运动输入、`mPathfindWait`/`mRepathTimer`、巡逻状态与失败计数、待机/受伤进入标志、交战与锁存标志、调查/搜索/记忆/最后目击（`:1317` 起）。`SetNPC` 全仓库无调用者，属兼容接口，加强是安全的 |
| 15 | 已修 | 删除 `FindRandomWalkablePosition` 里恒为假的越界检查（采样半径本就只有 JPS 窗口一半）；`PATROL_MOVING` 的 `> mRange * 1.5f` 改为 `> (float)mRange`（恢复 JPS 窗口语义，才兜得住传送/换目标后的残留路径） |

**本次未做、仍留待观察的点**

- `DoAttack` 入口的 `mPlayerDistance > AttackRange + kRangeHysteresis` 仍用真值距离早退（见条目 3）——若将来要彻底消除，需要把"是否在攻击距离内"也改成基于最后目击点的估计，代价是可能对空气开火。
- 条目 4 的兜底窗口依赖"记忆 8s + 搜索 3s"这两个常量的相对关系；将来调整 `kSuspiciousMemory` / `kSearchDuration` 时需重新核对 `kDisengageLostTime` 是否仍只是"最后一道闸"。
- 条目 5 的 `PATROL_IDLE` 里"等上一个 JPS 任务完成"那段**故意不加超时**：JPS 实例被复用，旧任务在途时再次提交会让线程池并发写同一个 `LPath`。这是刻意保留的等待点，不是遗漏。
- `SetNPC` 仍无调用者；`PATROL_FALLBACK` 在地图 JPS 长期不可用时会一直走方向碰撞巡逻——这是设计上的降级路径，不是缺陷。
