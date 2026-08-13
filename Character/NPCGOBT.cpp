#include "NPCGOBT.h"
#include "../DebugLog.h"
#include "../Tool/Tool.h"
#include "../GlobalVariable.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>

namespace GAME {

namespace {
    // ========================================================================
    // 行为参数（集中于此便于调参）
    // ========================================================================
    constexpr float kVisibleGrace       = 0.25f;   // 可见性锁存宽限（秒）
    constexpr int   kRangeHysteresis    = 25;      // 攻击范围滞回带宽（<90 进入，>115 退出）
    constexpr float kAttackLosGrace     = 0.6f;    // 攻击中短暂丢失视野的压制射击宽限（秒）
    constexpr float kEngageLeashMult    = 1.5f;    // 丢失视野时的脱战距离上限 = ChaseRange × 此值
    constexpr float kMuzzleOffset       = 12.0f;   // 炮口偏移（>坦克对角半径11.3，任何朝向开火都不会打到自己）
    constexpr float kBlindSpread        = 0.25f;   // 搜索压制射击散布（弧度）
    constexpr float kSuppressInterval   = 1.2f;    // 追击/搜索中压制射击间隔（秒）
    constexpr float kDisengageLostTime  = 10.0f;   // 脱战兜底：连续丢失视野上限（秒）
    constexpr float kSuspiciousMemory   = 8.0f;    // 可疑位置记忆时长（秒）
    constexpr float kSearchDuration     = 3.0f;    // 最后目击位置搜索时长（秒）
    constexpr float kOrbitRadius        = 55.0f;   // 搜索绕行半径（绕可疑点转圈侦察）
    constexpr float kStandOff           = 70.0f;   // 不可见目标的前进站立距离（留绕行/视线空间）
    constexpr float kRecentlyHurtDuration = 2.5f;  // 受伤威胁记忆时长（秒，驱动自保）
    constexpr float kFleeDuration       = 1.2f;    // 后撤最短时长（秒）
    constexpr float kFleeDistance       = 180.0f;  // 后撤目标距离（像素）
    constexpr float kRepathPlayerDrift  = 80.0f;   // 追击提前重寻路：玩家偏离路径终点（像素）
    constexpr float kIdealAttackDist    = 50.0f;   // 攻击理想距离
    constexpr float kStrafePeriod       = 1.2f;    // 侧移换向周期（秒）
    constexpr int   kJpsClampMargin     = 5;       // JPS 目标钳制到范围边缘的内缩量
}

// JPS 障碍检测回调
// 注意：GetPixelWallNumber 语义为"可通行"（true=道路/开阔，false=墙/越界），
// 名称有误导性但与 JPS isValid（true=可行走）语义一致。
static bool AStarGetWall(int x, int y, void* P) {
    PathfindingDecorator* Pathfinding = (PathfindingDecorator*)P;
    return Pathfinding->GetPixelWallNumber(x, y);
}

// ============================================================================
// 构造与析构
// ============================================================================

NPCGOBT::NPCGOBT(GamePlayer* npc, PathfindingDecorator* pathfinding, Arms* arms)
{
    mNPC = npc;
    wPathfinding = pathfinding;
    wArms = arms;
    mNPC->GetObjectCollision()->angle = 0.01f;
    mJPS = new JPS(mRange, 50000);
    mJPS->SetObstaclesCallback(AStarGetWall, wPathfinding);

    LOGD("NPCGOBT::NPCGOBT() 构造开始 | NPC=%p, Pathfinding=%p, Arms=%p, Range=%d, JPS内存=%d",
         (void*)npc, (void*)pathfinding, (void*)arms, mRange, 50000);

    // 设置 NPC 专属移动参数
    // ★ 关键：MaxSpeed 必须 ≥ 玩家速度(120)，否则追击永远追不上
    //   （旧值 90 → 玩家一跑 NPC 只能永远跟在后面，再也进入不了攻击范围）
    MovementComponent* mc = mNPC->GetMovement();
    if (mc) {
        mc->Config().MaxSpeed = 125.0f;
        mc->Config().TurnRate = 9.0f;    // 转向更快：近战/绕圈时快速重新锁定目标
        mc->Config().RagdollMinTime = 0.4f;
        LOGD("NPCGOBT::NPCGOBT() 移动参数配置完成 | MaxSpeed=125.0, TurnRate=9.0, RagdollMinTime=0.4");
    } else {
        LOGD("NPCGOBT::NPCGOBT() 警告：MovementComponent 为空");
    }

    // 初始化 GOBT 组件
    mBlackboard = std::make_shared<gobot::SharedBlackboard>();
    mEventBus = std::make_shared<gobot::EventBus>();
    mGoalManager = std::make_shared<gobot::GoalManager>();
    mDecomposer = std::make_shared<gobot::TacticalDecomposer>();
    mSubtreeLib = std::make_shared<gobot::SubtreeLibrary>();

    // 世界状态默认值：消除"未初始化键"依赖。
    // 出生即视为：未受伤 / 已恢复 / 未交战 / 无威胁。
    {
        auto& ws = *mBlackboard->world_state();
        ws.set(NPCWS::kInjured, false);
        ws.set(NPCWS::kInjuryRecovered, true);
        ws.set(NPCWS::kPlayerVisible, false);
        ws.set(NPCWS::kPlayerInRange, false);
        ws.set(NPCWS::kPlayerInViewField, false);
        ws.set(NPCWS::kPlayerEngaged, false);
        ws.set(NPCWS::kRecentlyHurt, false);
    }
    LOGD("NPCGOBT::NPCGOBT() GOBT组件初始化完成 | Blackboard=%p, EventBus=%p, GoalManager=%p, Decomposer=%p, SubtreeLib=%p",
         (void*)mBlackboard.get(), (void*)mEventBus.get(), (void*)mGoalManager.get(),
         (void*)mDecomposer.get(), (void*)mSubtreeLib.get());

    // 注册目标、分解策略、子树
    SetupGoals();
    SetupDecomposer();
    SetupSubtreeLibrary();

    // 构建行为树
    BuildTree();

    LOGD("NPCGOBT::NPCGOBT() 构造完成");
}

NPCGOBT::~NPCGOBT()
{
    LOGD("NPCGOBT::~NPCGOBT() called");
    // GOBT 组件由 shared_ptr 自动释放
    delete mNPC;
    while (!mJPS->GetPathfindingCompleted()) {
        std::cout << "~NPCGOBT() AStar 等待线程结束" << std::endl;
    }
    delete mJPS;
}

// ============================================================================
// GOBT 初始化
// ============================================================================

void NPCGOBT::SetupGoals()
{
    using namespace gobot;

    LOGD("NPCGOBT::SetupGoals() 开始注册目标");

    // 目标 1：生存（优先级 100）——受伤时激活；不可挂起（硬直必须完整执行）
    auto survive = std::make_shared<Goal>(
        "Survive",
        std::unordered_map<WorldKey, WorldValue>{{NPCWS::kInjuryRecovered, true}},
        100);
    survive->set_suspendible(false);
    mGoalManager->add_goal(survive);
    LOGD("NPCGOBT::SetupGoals() 注册目标 [Survive] 优先级=100, 满足条件=%s=true (不可挂起)",
         NPCWS::kInjuryRecovered.c_str());

    // 目标 2：自保（优先级 90）——受伤威胁未消退时与敌人拉开距离
    mGoalManager->add_goal(std::make_shared<Goal>(
        "SelfPreserve",
        std::unordered_map<WorldKey, WorldValue>{{NPCWS::kRecentlyHurt, false}},
        90));
    LOGD("NPCGOBT::SetupGoals() 注册目标 [SelfPreserve] 优先级=90, 满足条件=%s=false",
         NPCWS::kRecentlyHurt.c_str());

    // 目标 3：与敌交战（优先级 80）——仇恨锁存(engaged)期间激活
    mGoalManager->add_goal(std::make_shared<Goal>(
        "FightEnemy",
        std::unordered_map<WorldKey, WorldValue>{{NPCWS::kPlayerEngaged, false}},
        80));
    LOGD("NPCGOBT::SetupGoals() 注册目标 [FightEnemy] 优先级=80, 满足条件=%s=false",
         NPCWS::kPlayerEngaged.c_str());

    // 目标 4：巡逻区域（优先级 20）——默认行为，永不满足
    mGoalManager->add_goal(std::make_shared<Goal>(
        "PatrolArea",
        std::unordered_map<WorldKey, WorldValue>{{"patrol_done", true}},
        20));
    LOGD("NPCGOBT::SetupGoals() 注册目标 [PatrolArea] 优先级=20, 满足条件=patrol_done=true (无限fallback)");

    LOGD("NPCGOBT::SetupGoals() 目标注册完成，共4个目标");
}

void NPCGOBT::SetupDecomposer()
{
    using namespace gobot;
    auto bb = mBlackboard;

    LOGD("NPCGOBT::SetupDecomposer() 开始注册分解策略");

    // --- Survive 分解：硬直恢复 ---
    mDecomposer->register_strategy("Survive", [](const Goal& goal) {
        LOGD("NPCGOBT::Decompose[Survive] 目标 '%s' → 子目标=[RecoverFromInjury]",
             goal.name().c_str());
        return std::vector<SubgoalPtr>{
            std::make_shared<Subgoal>(
                SubgoalType::Recover, "RecoverFromInjury",
                std::unordered_map<WorldKey, WorldValue>{{NPCWS::kInjuryRecovered, true}},
                1)
        };
    });

    // --- SelfPreserve 分解：后撤拉开距离 ---
    mDecomposer->register_strategy("SelfPreserve", [this](const Goal& goal) {
        // 重置后撤状态（中断后重新进入时从零计时）
        mFleeEntered = false;
        mFleeTimer = 0.0f;
        LOGD("NPCGOBT::Decompose[SelfPreserve] 目标 '%s' → 子目标=[Retreat]",
             goal.name().c_str());
        return std::vector<SubgoalPtr>{
            std::make_shared<Subgoal>(
                SubgoalType::Flee, "Retreat",
                std::unordered_map<WorldKey, WorldValue>{{NPCWS::kRecentlyHurt, false}},
                1)
        };
    });

    // --- FightEnemy 分解：基于锁存世界状态选择攻击或追击 ---
    // 攻击条件：进入攻击范围 且 看得见——NPC 只能依靠视线发现玩家，绝不隔墙感知
    mDecomposer->register_strategy("FightEnemy", [bb, this](const Goal&) {
        auto ws = bb->world_state();
        bool inRange = ws->get<bool>(NPCWS::kPlayerInRange).value_or(false);
        bool visible = ws->get<bool>(NPCWS::kPlayerVisible).value_or(false);

        // 丢弃旧路径（无 JPS 任务在途时），避免沿用巡逻残留路径
        ClearPathSafe();

        if (inRange && visible) {
            LOGD("NPCGOBT::Decompose[FightEnemy] inRange=%s visible=%s → 子目标=[AttackPlayer]",
                 inRange ? "true" : "false", visible ? "true" : "false");
            return std::vector<SubgoalPtr>{
                std::make_shared<Subgoal>(
                    SubgoalType::Combat, "AttackPlayer",
                    std::unordered_map<WorldKey, WorldValue>{{NPCWS::kPlayerVisible, false}},
                    3)
            };
        } else {
            LOGD("NPCGOBT::Decompose[FightEnemy] inRange=%s visible=%s → 子目标=[ChasePlayer]",
                 inRange ? "true" : "false", visible ? "true" : "false");
            return std::vector<SubgoalPtr>{
                std::make_shared<Subgoal>(
                    SubgoalType::MoveTo, "ChasePlayer",
                    std::unordered_map<WorldKey, WorldValue>{{NPCWS::kPlayerInRange, true}},
                    3)
            };
        }
    });

    // --- PatrolArea 分解：待机 → 巡逻 循环 ---
    mDecomposer->register_strategy("PatrolArea", [this](const Goal& goal) {
        // 重置巡逻状态机与待机计时（目标切换/循环重入时从零开始）
        mPatrolEntered = false;
        mPatrolState = PATROL_IDLE;
        mPatrolFailedCount = 0;
        mStandbyEntered = false;
        ClearPathSafe();
        LOGD("NPCGOBT::Decompose[PatrolArea] 目标 '%s' → 子目标=[Standby, Patrol] 循环",
             goal.name().c_str());
        return std::vector<SubgoalPtr>{
            std::make_shared<Subgoal>(
                SubgoalType::Standby, "Standby",
                std::unordered_map<WorldKey, WorldValue>{}, 1),
            std::make_shared<Subgoal>(
                SubgoalType::Patrol, "Patrol",
                std::unordered_map<WorldKey, WorldValue>{}, 1)
        };
    });

    LOGD("NPCGOBT::SetupDecomposer() 分解策略注册完成，共4个策略");
}

void NPCGOBT::SetupSubtreeLibrary()
{
    using namespace gobot;

    LOGD("NPCGOBT::SetupSubtreeLibrary() 开始注册子树工厂");

    // --- RecoverFromInjury 子树 ---
    // ★ 修复：旧前置条件 {injured:true} 只在伤害队列非空的当帧成立
    //   （队列每帧被 UpData() 清空），导致硬直开始 1 帧后前置失败、被立即打断。
    //   现改为 {injury_recovered:false}：整个硬直期间成立。
    // 效果：Success 后设置 injury_recovered=true —— 该键的唯一恢复写入方，
    //   保证 1.0s 硬直完整执行后才认为 Survive 目标满足。
    mSubtreeLib->register_subtree(SubgoalType::Recover, [this]() -> BTNodePtr {
        auto action = std::make_shared<Action>(
            "InjuryAction",
            std::unordered_map<WorldKey, WorldValue>{{NPCWS::kInjuryRecovered, false}},
            std::unordered_map<WorldKey, WorldValue>{{NPCWS::kInjuryRecovered, true}},
            [this](Context& ctx) { return DoInjury(ctx); });
        return std::make_shared<OperationalActionNode>(action);
    });
    LOGD("NPCGOBT::SetupSubtreeLibrary() 注册子树 [Recover → InjuryAction] (前置条件: injury_recovered=false)");

    // --- AttackPlayer 子树 ---
    // 前置条件：交战状态。可见性与攻击范围在 DoAttack 内用严格感官/锁存值判定。
    mSubtreeLib->register_subtree(SubgoalType::Combat, [this]() -> BTNodePtr {
        auto action = std::make_shared<Action>(
            "AttackAction",
            std::unordered_map<WorldKey, WorldValue>{{NPCWS::kPlayerEngaged, true}},
            std::unordered_map<WorldKey, WorldValue>{},
            [this](Context& ctx) { return DoAttack(ctx); });
        return std::make_shared<OperationalActionNode>(action);
    });
    LOGD("NPCGOBT::SetupSubtreeLibrary() 注册子树 [Combat → AttackAction] (前置条件: playerEngaged)");

    // --- ChasePlayer 子树 ---
    // 前置条件：交战状态。追击/最后目击调查逻辑在 DoChase 内。
    mSubtreeLib->register_subtree(SubgoalType::MoveTo, [this]() -> BTNodePtr {
        auto action = std::make_shared<Action>(
            "ChaseAction",
            std::unordered_map<WorldKey, WorldValue>{{NPCWS::kPlayerEngaged, true}},
            std::unordered_map<WorldKey, WorldValue>{},
            [this](Context& ctx) { return DoChase(ctx); });
        return std::make_shared<OperationalActionNode>(action);
    });
    LOGD("NPCGOBT::SetupSubtreeLibrary() 注册子树 [MoveTo → ChaseAction] (前置条件: playerEngaged)");

    // --- Retreat 子树（SelfPreserve）---
    // 效果：recently_hurt=false → 威胁解除，SelfPreserve 目标满足
    mSubtreeLib->register_subtree(SubgoalType::Flee, [this]() -> BTNodePtr {
        auto action = std::make_shared<Action>(
            "RetreatAction",
            std::unordered_map<WorldKey, WorldValue>{},
            std::unordered_map<WorldKey, WorldValue>{{NPCWS::kRecentlyHurt, false}},
            [this](Context& ctx) { return DoFlee(ctx); });
        return std::make_shared<OperationalActionNode>(action);
    });
    LOGD("NPCGOBT::SetupSubtreeLibrary() 注册子树 [Flee → RetreatAction] (效果: recently_hurt=false)");

    // --- Standby 子树 ---
    mSubtreeLib->register_subtree(SubgoalType::Standby, [this]() -> BTNodePtr {
        auto action = std::make_shared<Action>(
            "StandbyAction",
            std::unordered_map<WorldKey, WorldValue>{},
            std::unordered_map<WorldKey, WorldValue>{},
            [this](Context& ctx) { return DoStandby(ctx); });
        return std::make_shared<OperationalActionNode>(action);
    });
    LOGD("NPCGOBT::SetupSubtreeLibrary() 注册子树 [Standby → StandbyAction] (无条件)");

    // --- Patrol 子树 ---
    mSubtreeLib->register_subtree(SubgoalType::Patrol, [this]() -> BTNodePtr {
        auto action = std::make_shared<Action>(
            "PatrolAction",
            std::unordered_map<WorldKey, WorldValue>{},
            std::unordered_map<WorldKey, WorldValue>{},
            [this](Context& ctx) { return DoPatrol(ctx); });
        return std::make_shared<OperationalActionNode>(action);
    });
    LOGD("NPCGOBT::SetupSubtreeLibrary() 注册子树 [Patrol → PatrolAction] (无条件)");

    LOGD("NPCGOBT::SetupSubtreeLibrary() 子树注册完成，共6个子树");
}

void NPCGOBT::BuildTree()
{
    using namespace gobot;

    LOGD("NPCGOBT::BuildTree() 开始构建3层GOBT行为树");

    auto built = TreeBuilder()
        .with_blackboard(mBlackboard)
        .with_event_bus(mEventBus)
        .with_goal_manager(mGoalManager)
        .with_decomposer(mDecomposer)
        .with_subtree_library(mSubtreeLib)
        .build();

    mRoot = built.root;
    mContext = built.context;
    mPlanner = built.strategic_planner;
    mExecutor = std::make_unique<BTExecutor>(mRoot, mContext);

    LOGD("NPCGOBT::BuildTree() 构建完成 | Root=%p, Context=%p, Planner=%p, Executor=%p",
         (void*)mRoot.get(), (void*)mContext.get(), (void*)mPlanner.get(), (void*)mExecutor.get());
}

// ============================================================================
// 世界状态同步（"希望/目标"层的事实输入）
// ============================================================================

void NPCGOBT::SyncWorldState()
{
    int flags = GetSensoryMessages();
    auto& ws = *mBlackboard->world_state();

    // ============================================================
    // 1) 伤害 → 受伤 / 恢复 / 威胁
    // ============================================================
    int currentDamage = mNPC->mPixelQueue->GetNumber();
    bool newDamage = currentDamage > 0;

    ws.set(NPCWS::kInjured, newDamage);
    if (newDamage) {
        // 新伤：激活 Survive（硬直）；刷新受伤威胁计时（驱动 SelfPreserve）
        ws.set(NPCWS::kInjuryRecovered, false);
        mRecentlyHurtTimer = kRecentlyHurtDuration;
        // 被击中即仇恨并调查攻击方向：
        //  - 看得见 → 记下玩家精确位置；
        //  - 看不见 → 只沿受伤方向前进一段距离作为调查点（只知道方向，不隔墙知道精确坐标）
        mEngaged = true;
        mPlayerLostTime = 0.0f;
        if (flags & SensoryMessages_Visible) {
            mSuspicious = true;
            mSuspiciousPos = {(int)Global::GamePlayerX, (int)Global::GamePlayerY};
            mSuspiciousTimer = kSuspiciousMemory;
        } else {
            glm::vec2 npcPos = mNPC->GetObjectCollision()->pos;
            glm::vec2 dir{Global::GamePlayerX - npcPos.x, Global::GamePlayerY - npcPos.y};
            float len = glm::length(dir);
            if (len > 0.001f) {
                dir = dir / len;
            } else {
                dir = qianjinfang;
            }
            constexpr float kDamageInvestigateDist = 120.0f;
            mSuspicious = true;
            mSuspiciousPos = {(int)(npcPos.x + dir.x * kDamageInvestigateDist),
                              (int)(npcPos.y + dir.y * kDamageInvestigateDist)};
            mSuspiciousTimer = kSuspiciousMemory;
        }
        LOGD("NPCGOBT::SyncWorldState() 检测到新伤害 | damageCount=%d, injuryRecovered=false → 激活 [Survive], 仇恨并交战",
             currentDamage);
    }
    // ★ 关键修复：不再因伤害队列归零而置 injury_recovered=true。
    //   该键只由 Recover 动作成功后的 Effect 写入（单一写入方），
    //   否则硬直开始 1 帧后目标即被误判满足而切走（旧实现的硬直实际只持续 1 帧）。
    lastDamageCount_ = currentDamage;

    if (mRecentlyHurtTimer > 0.0f) {
        mRecentlyHurtTimer -= FPSTime;
    }
    ws.set(NPCWS::kRecentlyHurt, mRecentlyHurtTimer > 0.0f);

    // ============================================================
    // 2) 可见性锁存（0.25s 宽限，防视野锥/射线抖动导致目标抖振）
    // ============================================================
    if (flags & SensoryMessages_Visible) {
        mVisibleLostTime = 0.0f;
        mVisibleLatched = true;
    } else if (mVisibleLatched) {
        mVisibleLostTime += FPSTime;
        if (mVisibleLostTime > kVisibleGrace) {
            mVisibleLatched = false;
        }
    }

    // ============================================================
    // 3) 攻击范围锁存（<90 进入，>115 退出，消除攻击/追击边界抖振）
    // ============================================================
    if (mPlayerDistance < AttackRange) {
        mInRangeLatched = true;
    } else if (mPlayerDistance > AttackRange + kRangeHysteresis) {
        mInRangeLatched = false;
    }

    // ============================================================
    // 4) 交战状态机（仇恨/脱战滞回）
    //    - 进入：只有"看见玩家"或"被击中"才能发现玩家（绝不隔墙感知）
    //    - 保持：只要还能看见就一直保持；看不见时受
    //      距离上限(ChaseRange×1.5) / 记忆耗尽(8s) / 丢失超时(10s) 约束
    // ============================================================
    if (!mEngaged) {
        if (flags & SensoryMessages_Visible) {
            mEngaged = true;
            mPlayerLostTime = 0.0f;
            LOGD("NPCGOBT::SyncWorldState() 发现玩家(可见, 距离=%.1f) → 进入交战, 激活 [FightEnemy]",
                 mPlayerDistance);
        }
    } else {
        if (flags & SensoryMessages_Visible) {
            mPlayerLostTime = 0.0f;
        } else {
            mPlayerLostTime += FPSTime;
        }

        // 只在"看不见"时才考虑脱战（看得见就永远保持仇恨）
        if (!(flags & SensoryMessages_Visible)) {
            bool outOfLeash = mPlayerDistance > ChaseRange * kEngageLeashMult;
            bool memoryGone = !mSuspicious;
            bool lostTooLong = mPlayerLostTime > kDisengageLostTime;

            if (outOfLeash || memoryGone || lostTooLong) {
                mEngaged = false;
                mSuspicious = false;
                mSuspiciousTimer = 0.0f;
                mSearching = false;
                mInvestigating = false;
                mPlayerLostTime = 0.0f;   // 重置丢失计时，避免残留值影响下次攻击宽限
                LOGD("NPCGOBT::SyncWorldState() 脱战(%s) → [FightEnemy] 满足, 遗忘可疑位置",
                     outOfLeash ? "距离过远" : (memoryGone ? "调查无果" : "丢失视野超时"));
            }
        }
    }

    // ============================================================
    // 5) 可疑位置记忆衰减
    // ============================================================
    if (mSuspicious && mSuspiciousTimer > 0.0f) {
        mSuspiciousTimer -= FPSTime;
        if (mSuspiciousTimer <= 0.0f) {
            mSuspicious = false;
        }
    }

    // ============================================================
    // 6) 写入世界状态（供目标选择/分解/前置条件读取）
    // ============================================================
    ws.set(NPCWS::kPlayerVisible, mVisibleLatched);
    ws.set(NPCWS::kPlayerInRange, mInRangeLatched);
    ws.set(NPCWS::kPlayerInViewField, (flags & SensoryMessages_VisualField) != 0);
    ws.set(NPCWS::kPlayerEngaged, mEngaged);
}

// ============================================================================
// 感官系统（每帧一次采样，结果缓存共享）
// ============================================================================

int NPCGOBT::GetSensoryMessages()
{
    // 每帧只采样一次：SyncWorldState 与动作执行器共享缓存结果，
    // 避免同一帧内重复执行射线检测（旧实现每帧 2~3 次全图射线）。
    if (mSensoryFrameStamp == mFrameCounter) {
        return mSensoryCachedFlags;
    }
    mSensoryFrameStamp = mFrameCounter;

    int flags = SensoryMessages_None;
    glm::vec2 pos = mNPC->GetObjectCollision()->pos;

    float dx = Global::GamePlayerX - pos.x;
    float dy = Global::GamePlayerY - pos.y;
    mPlayerDistance = std::sqrt(dx * dx + dy * dy);

    // 玩家相对 NPC 位置角度
    wanjiaAngle = PhysicsBlock::EdgeVecToCosAngleFloat(glm::vec2{dx, dy});
    float AngleCha = wanjiaAngle - mNPC->GetObjectCollision()->angle;
    if (AngleCha > 3.14f) AngleCha -= 6.28f;
    if (AngleCha < -3.14f) AngleCha += 6.28f;

    // 视野锥：±90°（±1.57 弧度）
    if (fabs(AngleCha) < 1.57f) {
        flags |= SensoryMessages_VisualField;

        // 射线检测：无遮挡即可见（不设距离上限——看到就是看到）
        // ★ 关键：RadialCollisionDetection 内部做 世界坐标→网格坐标 转换
        //   （FixedMaze/Labyrinth 已修复 +mOriginX/mOriginY 的偏移缺失，
        //   否则地图以原点为中心时射线查错区域、视线永远被墙挡住）
        // ★ 稳健性：起点取坦克前端(10px)，终点在玩家前方 12px 截断——
        //   避免贴身时自身贴图格/玩家贴墙导致中心点射线被误判遮挡。
        glm::vec2 eyePos = pos + PhysicsBlock::vec2angle(
            glm::vec2{10.0f, 0.0f}, mNPC->GetObjectCollision()->angle);
        glm::vec2 toPlayer{Global::GamePlayerX - pos.x, Global::GamePlayerY - pos.y};
        float toPlayerLen = glm::length(toPlayer);
        glm::vec2 lookEnd = toPlayerLen > 13.0f
            ? glm::vec2{Global::GamePlayerX, Global::GamePlayerY} - (toPlayer / toPlayerLen) * 12.0f
            : glm::vec2{Global::GamePlayerX, Global::GamePlayerY};
        PhysicsBlock::CollisionInfoI LInfo = wPathfinding->RadialCollisionDetection(
            eyePos.x, eyePos.y, lookEnd.x, lookEnd.y);
        if (!LInfo.Collision) {
            flags |= SensoryMessages_Visible;
        }
    }

    // 攻击范围判定（严格值；世界状态使用锁存值）
    if (mPlayerDistance < AttackRange) {
        flags |= SensoryMessages_Range;
    }

    // ★ 关键规则：只有"看得见"才能刷新可疑位置记忆。
    //   绝不通过近身感知/伤害等其他渠道持续更新玩家坐标——
    //   NPC 不可以隔着墙壁发现或追踪玩家。
    if (flags & SensoryMessages_Visible) {
        mSuspicious = true;
        mSuspiciousPos = {(int)Global::GamePlayerX, (int)Global::GamePlayerY};
        mSuspiciousTimer = kSuspiciousMemory;
    }

    mSensoryCachedFlags = flags;
    return flags;
}

// ============================================================================
// 动作执行器
// ============================================================================

// --- 待机（对应原 FSM Standby 状态的执行逻辑） ---
gobot::Status NPCGOBT::DoStandby(gobot::Context& ctx)
{
    // 恢复受控模式（击飞态由 MovementComponent 自动切回）
    if (mNPC->GetMovement()->GetMode() != MovementMode::Ragdoll) {
        mNPC->GetMovement()->SetMode(MovementMode::Controlled);
    }

    // 首次进入待机：重置独立计时器（避免受其他动作 mTime 归零影响）
    if (!mStandbyEntered) {
        mStandbyTimer = 0.0f;
        mStandbyEntered = true;
        LOGD("NPCGOBT::DoStandby() 首次进入待机, 独立计时器已清零");
    }

    // 累加独立待机计时器
    mStandbyTimer += FPSTime;

    // 待机时缓慢转向前进方向
    mNPC->GetMovement()->SetLookAngle(PhysicsBlock::EdgeVecToCosAngleFloat(qianjinfang));

    // 1.5s 后转巡逻（使用独立计时器，不受 mTime 归零影响）
    if (mStandbyTimer > 1.5f) {
        mStandbyEntered = false; // 重置标志，为下次进入待机准备
        LOGD("NPCGOBT::DoStandby() 待机超时(%.2f>1.5) → Success, 将切换到巡逻", mStandbyTimer);
        return gobot::Status::Success;
    }
    return gobot::Status::Running;
}

// --- 安全清空路径：无 JPS 任务在途时执行（避免与线程池写竞争） ---
void NPCGOBT::ClearPathSafe()
{
    if (mJPS && mJPS->GetPathfindingCompleted()) {
        LPath.clear();
    }
    mJpsSubmitted = false;
}

// ============================================================================
// 辅助方法
// ============================================================================

// --- 在 NPC 周围寻找随机可行走位置（用于巡逻目标） ---
JPSVec2 NPCGOBT::FindRandomWalkablePosition(const glm::vec2& currentPos)
{
    // 搜索半径：使用 JPS 范围的一半，确保寻路能成功
    const int patrolRange = mRange / 2;
    const int maxAttempts = 100;

    for (int i = 0; i < maxAttempts; ++i) {
        int rx = (int)currentPos.x + (rand() % (patrolRange * 2)) - patrolRange;
        int ry = (int)currentPos.y + (rand() % (patrolRange * 2)) - patrolRange;

        // 检查是否在 JPS 合法范围内（相对起点）
        int dx = rx - (int)currentPos.x;
        int dy = ry - (int)currentPos.y;
        if (dx < -mRange || dx >= mRange || dy < -mRange || dy >= mRange) {
            continue;
        }

        // 检查该点是否可通行（true=可行走，与 JPS 回调语义一致）
        if (!AStarGetWall(rx, ry, wPathfinding)) {
            continue; // 不可通行
        }

        // 检查周围 3×3 范围确保 NPC 能站住
        bool valid = true;
        for (int nx = -1; nx <= 1 && valid; ++nx) {
            for (int ny = -1; ny <= 1 && valid; ++ny) {
                if (!AStarGetWall(rx + nx, ry + ny, wPathfinding)) {
                    valid = false;
                }
            }
        }
        if (!valid) continue;

        LOGD("NPCGOBT::FindRandomWalkablePosition() 找到有效巡逻目标 | 当前位置=(%.1f,%.1f) 目标=(%d,%d) 尝试次数=%d",
             currentPos.x, currentPos.y, rx, ry, i + 1);
        return {rx, ry};
    }

    // 所有尝试都失败
    LOGD("NPCGOBT::FindRandomWalkablePosition() 未找到有效巡逻目标(%d次尝试) | 位置=(%.1f,%.1f)",
         maxAttempts, currentPos.x, currentPos.y);
    return {INT_MIN, INT_MIN};
}

// --- 巡逻（随机坐标 + JPS 寻路） ---
gobot::Status NPCGOBT::DoPatrol(gobot::Context& ctx)
{
    if (mNPC->GetMovement()->GetMode() != MovementMode::Ragdoll) {
        mNPC->GetMovement()->SetMode(MovementMode::Controlled);
    }

    glm::vec2 pos = mNPC->GetObjectCollision()->pos;

    // 首次进入巡逻：重置状态机（应对从其他目标切换回来的情况）
    if (!mPatrolEntered) {
        mPatrolState = PATROL_IDLE;
        mPatrolFailedCount = 0;
        mPatrolEntered = true;
        LOGD("NPCGOBT::DoPatrol() 首次进入巡逻，状态机已重置 | 位置=(%.1f,%.1f)", pos.x, pos.y);
    }

    switch (mPatrolState) {

    // ====================================================================
    // PATROL_IDLE: 选取随机目标 + 提交 JPS 寻路
    // ====================================================================
    case PATROL_IDLE:
    {
        // 安全防护：上一任务（可能来自被中断的追击）仍在计算 → 等待
        if (!mJPS->GetPathfindingCompleted()) {
            return gobot::Status::Running;
        }

        // 尝试寻找随机可行走位置
        mPatrolTarget = FindRandomWalkablePosition(pos);

        // 没找到有效位置 → 降级到方向碰撞巡逻
        if (mPatrolTarget.x == INT_MIN && mPatrolTarget.y == INT_MIN) {
            LOGD("NPCGOBT::DoPatrol() [IDLE] 无法找到巡逻目标，降级到方向碰撞巡逻");
            return DoPatrolFallback(pos);
        }

        // 提交 JPS 寻路
        LPath.clear();
        TOOL::mThreadPool->enqueue(&JPS::FindPath, mJPS,
            JPSVec2{(int)pos.x, (int)pos.y}, mPatrolTarget, &LPath,
            JPSVec2{wPathfinding->PathfindingDecoratorDeviationX,
                    wPathfinding->PathfindingDecoratorDeviationY});
        mTime = 0;
        mPatrolState = PATROL_PATHFINDING;
        LOGD("NPCGOBT::DoPatrol() [IDLE] JPS寻路已提交 | 起点=(%d,%d) → 目标=(%d,%d)",
             (int)pos.x, (int)pos.y, mPatrolTarget.x, mPatrolTarget.y);
        return gobot::Status::Running;
    }

    // ====================================================================
    // PATROL_PATHFINDING: 等待 JPS 计算结果
    // ====================================================================
    case PATROL_PATHFINDING:
    {
        if (!mJPS->GetPathfindingCompleted()) {
            return gobot::Status::Running;
        }

        if (!LPath.empty()) {
            // 路径找到！开始移动
            mPatrolFailedCount = 0;
            mPatrolState = PATROL_MOVING;
            LOGD("NPCGOBT::DoPatrol() [PATHFINDING] JPS成功, 路径点数=%zu → MOVING", LPath.size());
            return gobot::Status::Running;
        }

        // JPS 完成但路径为空 → 目标不可达
        mPatrolFailedCount++;
        LOGD("NPCGOBT::DoPatrol() [PATHFINDING] JPS未找到路径(失败%d次) | 目标=(%d,%d)",
             mPatrolFailedCount, mPatrolTarget.x, mPatrolTarget.y);

        if (mPatrolFailedCount >= 3) {
            // 连续失败 3 次 → 降级到方向碰撞巡逻
            mPatrolFailedCount = 0;
            LOGD("NPCGOBT::DoPatrol() [PATHFINDING] 连续3次寻路失败，降级到方向碰撞巡逻");
            return DoPatrolFallback(pos);
        }

        // 重新选取目标
        mPatrolState = PATROL_IDLE;
        return gobot::Status::Running;
    }

    // ====================================================================
    // PATROL_MOVING: 沿路径前进
    // ====================================================================
    case PATROL_MOVING:
    {
        // 安全检查：目标距离太远（goal 切换导致旧状态残留）→ 重新选取
        glm::vec2 toTarget = glm::vec2{(float)mPatrolTarget.x - pos.x, (float)mPatrolTarget.y - pos.y};
        if (LPath.empty() || glm::length(toTarget) > mRange * 1.5f) {
            mPatrolState = PATROL_IDLE;
            LOGD("NPCGOBT::DoPatrol() [MOVING] 路径/目标异常，重新选取 | 目标距离=%.1f",
                 glm::length(toTarget));
            return gobot::Status::Running;
        }

        // 沿路径前进（与 DoChase 相同的路径跟随逻辑）
        JPSVec2 yiPOS = LPath.back();
        glm::vec2 toWaypoint = glm::vec2{(float)(yiPOS.x) - pos.x, (float)(yiPOS.y) - pos.y};
        float distToWaypoint = glm::length(toWaypoint);

        if (distToWaypoint < 9.0f) {
            LPath.pop_back();
            if (LPath.empty()) {
                mPatrolState = PATROL_IDLE;
                LOGD("NPCGOBT::DoPatrol() [MOVING] 到达终点，选取下一个巡逻目标");
                return gobot::Status::Running;
            }
            yiPOS = LPath.back();
            toWaypoint = glm::vec2{(float)(yiPOS.x) - pos.x, (float)(yiPOS.y) - pos.y};
            distToWaypoint = glm::length(toWaypoint);
            if (distToWaypoint < 0.001f) return gobot::Status::Running;
        }

        // 朝向路径点 + 设置移动方向
        float YAngle = PhysicsBlock::EdgeVecToCosAngleFloat(toWaypoint);
        mNPC->GetMovement()->SetLookAngle(YAngle);
        glm::vec2 dir = toWaypoint / distToWaypoint;
        mNPC->GetMovement()->SetMoveInput(Vec2_{dir.x, dir.y});
        qianjinfang = dir; // 记录当前巡逻方向（用于降级 fallback）

        return gobot::Status::Running;
    }
    }

    return gobot::Status::Running;
}

// --- 方向碰撞巡逻（JPS 失败时的降级 fallback） ---
// ★ 修复：旧实现把 GetPixelWallNumber（true=可通行）当作"是墙"使用，
//   判断完全颠倒 —— 前方可通行时反而转身、前方是墙时却前进撞墙。
gobot::Status NPCGOBT::DoPatrolFallback(const glm::vec2& pos)
{
    glm::vec2 Lpos = pos + qianjinfang * 2.0f;

    // 前方可通行 → 前进
    if (wPathfinding->GetPixelWallNumber(
            Lpos.x + wPathfinding->PathfindingDecoratorDeviationX,
            Lpos.y + wPathfinding->PathfindingDecoratorDeviationY)) {
        float YAngle = PhysicsBlock::EdgeVecToCosAngleFloat(qianjinfang);
        mNPC->GetMovement()->SetLookAngle(YAngle);
        mNPC->GetMovement()->SetMoveInput(Vec2_{qianjinfang.x, qianjinfang.y});
        return gobot::Status::Running;
    }

    // 前方有墙：尝试其他 3 个方向（同样要求可通行）
    const glm::vec2 allDirs[4] = {{1,0}, {-1,0}, {0,1}, {0,-1}};
    int currentIdx = -1;
    for (int i = 0; i < 4; ++i) {
        if (allDirs[i].x == qianjinfang.x && allDirs[i].y == qianjinfang.y) {
            currentIdx = i;
            break;
        }
    }
    int startIdx = (currentIdx >= 0) ? (currentIdx + 1) % 4 : (rand() % 4);

    for (int attempt = 0; attempt < 3; ++attempt) {
        int idx = (startIdx + attempt) % 4;
        glm::vec2 testDir = allDirs[idx];
        glm::vec2 testPos = pos + testDir * 2.0f;

        if (wPathfinding->GetPixelWallNumber(
                testPos.x + wPathfinding->PathfindingDecoratorDeviationX,
                testPos.y + wPathfinding->PathfindingDecoratorDeviationY)) {
            qianjinfang = testDir;
            float YAngle = PhysicsBlock::EdgeVecToCosAngleFloat(testDir);
            mNPC->GetMovement()->SetLookAngle(YAngle);
            mNPC->GetMovement()->SetMoveInput(Vec2_{testDir.x, testDir.y});
            LOGD("NPCGOBT::DoPatrolFallback() 找到新方向 (%.1f,%.1f) → Running",
                 testDir.x, testDir.y);
            return gobot::Status::Running;
        }
    }

    // 全部堵死 → 返回待机
    mTime = 0;
    mPatrolEntered = false; // 重置进入标志，下次巡逻时重新初始化
    LOGD("NPCGOBT::DoPatrolFallback() 所有方向堵死 → Success, 返回待机");
    return gobot::Status::Success;
}

// --- 追击（含最后目击位置调查/搜索） ---
gobot::Status NPCGOBT::DoChase(gobot::Context& ctx)
{
    if (mNPC->GetMovement()->GetMode() != MovementMode::Ragdoll) {
        mNPC->GetMovement()->SetMode(MovementMode::Controlled);
    }

    int flags = GetSensoryMessages();
    bool visible = (flags & SensoryMessages_Visible) != 0;
    glm::vec2 pos = mNPC->GetObjectCollision()->pos;

    // 1) 已进入攻击范围(锁存)且可见 → Success → 重新分解为攻击
    if (mInRangeLatched && visible) {
        mJpsSubmitted = false;
        mSearching = false;
        mInvestigating = false;
        LOGD("NPCGOBT::DoChase() 已进入攻击范围且可见 → Success, 将切换到攻击");
        return gobot::Status::Success;
    }

    // 2) 完全丢失目标(不可见且无可疑记忆) → Failure → 目标重评估(回巡逻)
    if (!visible && !mSuspicious) {
        mJpsSubmitted = false;
        mSearching = false;
        mInvestigating = false;
        LOGD("NPCGOBT::DoChase() 完全丢失目标(不可见且无可疑位置) → Failure");
        return gobot::Status::Failure;
    }

    // 3) 搜索模式：围绕最后目击位置主动绕圈侦察（绕开遮挡重新获取视线）
    //    ★ 绝不隔墙开火：看不见就不打，只移动到能看见的位置再攻击
    if (mSearching) {
        mSearchTimer += FPSTime;

        if (visible) {
            // 重新发现玩家 → 退出搜索，恢复追击
            mSearching = false;
            ClearPathSafe();
            LOGD("NPCGOBT::DoChase() 搜索中重新发现玩家 → 恢复追击");
            // 继续执行下方寻路逻辑
        } else {
            glm::vec2 toSusp{(float)mSuspiciousPos.x - pos.x, (float)mSuspiciousPos.y - pos.y};
            float distToSusp = glm::length(toSusp);

            // 面向可疑点扫视
            float base = PhysicsBlock::EdgeVecToCosAngleFloat(toSusp);
            float sweep = sinf(mSearchTimer * 3.0f) * 1.2f;
            mNPC->GetMovement()->SetLookAngle(base + sweep);

            // 主动绕圈：以可疑点为中心切向移动（远则靠近，近则后退拉开）
            // 绕行方向持久化（mOrbitDir）：只有撞墙才换向，避免贴墙时逐帧抖动
            glm::vec2 tangent = distToSusp > 0.001f
                ? glm::vec2{-toSusp.y, toSusp.x} / distToSusp
                : qianjinfang;
            glm::vec2 move;
            if (distToSusp > kOrbitRadius + 15.0f) {
                move = distToSusp > 0.001f ? toSusp / distToSusp : tangent; // 靠近
            } else if (distToSusp < kOrbitRadius - 15.0f) {
                move = distToSusp > 0.001f ? -(toSusp / distToSusp) : -tangent; // 后退
            } else {
                move = tangent * (float)mOrbitDir; // 绕行（持久方向）
            }
            // 前方是墙则反向绕行（仅此时换向）
            glm::vec2 probe = pos + move * 16.0f;
            if (!wPathfinding->GetPixelWallNumber(
                    probe.x + wPathfinding->PathfindingDecoratorDeviationX,
                    probe.y + wPathfinding->PathfindingDecoratorDeviationY)) {
                mOrbitDir = -mOrbitDir;
                move = -move;
            }
            mNPC->GetMovement()->SetMoveInput(Vec2_{move.x, move.y});

            // 搜索中保持压制射击：向最后目击方位持续开火（1.2s 间隔），
            // 边绕圈边打，不再"站着只看不动手"
            mShootCooldown -= FPSTime;
            if (mShootCooldown <= 0.0f && distToSusp > 0.001f) {
                mShootCooldown = kSuppressInterval;
                float fireAngle = base + ((rand() % 1000) / 1000.0f - 0.5f) * 2.0f * kBlindSpread;
                glm::vec2 shootPos = pos + PhysicsBlock::vec2angle(
                    glm::vec2{kMuzzleOffset, 0.0f}, fireAngle);
                LOGD("NPCGOBT::DoChase() 搜索压制射击 → 可疑位置(%d,%d) 开火角=%.3f",
                     mSuspiciousPos.x, mSuspiciousPos.y, fireAngle);
                wArms->ShootBullets(shootPos.x, shootPos.y, fireAngle, 500, 0);
            }

            if (mSearchTimer > kSearchDuration) {
                mSearching = false;
                mSuspicious = false;
                mSuspiciousTimer = 0.0f;
                mJpsSubmitted = false;
                LOGD("NPCGOBT::DoChase() 搜索超时(%.2fs)未发现玩家 → Failure, 放弃", mSearchTimer);
                return gobot::Status::Failure;
            }
            return gobot::Status::Running;
        }
    }

    // 4) JPS 计算中 → 等待
    if (!mJPS->GetPathfindingCompleted()) {
        return gobot::Status::Running;
    }

    // 5) 空路径死循环检测（已提交但未找到路径）
    if (LPath.empty() && mJpsSubmitted) {
        mJpsSubmitted = false;
        if (mInvestigating) {
            // 无法到达最后目击位置 → 就地搜索
            mInvestigating = false;
            mSearching = true;
            mSearchTimer = 0.0f;
            LOGD("NPCGOBT::DoChase() 无法到达最后目击位置 → 就地搜索(%.1fs)", kSearchDuration);
            return gobot::Status::Running;
        }
        LOGD("NPCGOBT::DoChase() JPS未找到路径(目标不可达) → Failure");
        return gobot::Status::Failure;
    }
    if (!LPath.empty()) {
        mJpsSubmitted = false;
    }

    // 6) 重新寻路判定：无路径 / 周期到 / 玩家明显偏离原路径终点
    glm::vec2 playerPos{Global::GamePlayerX, Global::GamePlayerY};
    bool playerDrifted = false;
    if (visible && !LPath.empty()) {
        glm::vec2 toPathEnd{(float)LPath.front().x - playerPos.x,
                            (float)LPath.front().y - playerPos.y};
        playerDrifted = glm::length(toPathEnd) > kRepathPlayerDrift;
    }
    bool needRepath = LPath.empty() || (mTime > mPathfindingCycle) || playerDrifted;

    if (needRepath) {
        JPSVec2 target;
        if (mSuspicious) {
            target = mSuspiciousPos;
            mSuspicious = false;              // 消费记忆（再次可见时由感官刷新）
            mInvestigating = !visible;        // 前往"旧目击点"才算调查
            if (mInvestigating) {
                // 不可见目标：目标点向自己回退一段站立距离，
                // 避免直接走到玩家脸上（贴脸死角/无绕行空间），
                // 留出距离后搜索时主动绕圈即可重新获取视线。
                glm::vec2 back{pos.x - (float)target.x, pos.y - (float)target.y};
                float backLen = glm::length(back);
                if (backLen > 0.001f && backLen > kStandOff) {
                    back = (back / backLen) * kStandOff;
                    target = {(int)((float)target.x + back.x), (int)((float)target.y + back.y)};
                }
            }
            LOGD("NPCGOBT::DoChase() 重新寻路 → 前往最后目击位置=(%d,%d) (调查=%s)",
                 target.x, target.y, mInvestigating ? "Y" : "N");
        } else if (visible) {
            target = {(int)playerPos.x, (int)playerPos.y};
            mInvestigating = false;
            LOGD("NPCGOBT::DoChase() 重新寻路 → 追击玩家=(%d,%d)", target.x, target.y);
        } else {
            mJpsSubmitted = false;
            return gobot::Status::Failure;
        }

        // 距离过远时截断到寻路范围内（内缩避免边界目标被 JPS 判非法）
        glm::vec2 toTarget = glm::vec2{(float)target.x - pos.x, (float)target.y - pos.y};
        while (fabs(toTarget.x) >= mRange - kJpsClampMargin ||
               fabs(toTarget.y) >= mRange - kJpsClampMargin) {
            toTarget *= 0.5f;
        }
        JPSVec2 clampedTarget = {(int)(pos.x + toTarget.x), (int)(pos.y + toTarget.y)};

        LPath.clear();
        TOOL::mThreadPool->enqueue(&JPS::FindPath, mJPS,
            JPSVec2{(int)pos.x, (int)pos.y}, clampedTarget, &LPath,
            JPSVec2{wPathfinding->PathfindingDecoratorDeviationX,
                    wPathfinding->PathfindingDecoratorDeviationY});
        mJpsSubmitted = true;
        mTime = 0;
        LOGD("NPCGOBT::DoChase() JPS寻路已提交 | 起点=(%d,%d) → 终点=(%d,%d)",
             (int)pos.x, (int)pos.y, clampedTarget.x, clampedTarget.y);
        return gobot::Status::Running;
    }

    // 7) 沿路径前进
    if (LPath.empty()) {
        return gobot::Status::Running;
    }

    JPSVec2 yiPOS = LPath.back();
    glm::vec2 toWaypoint = glm::vec2{(float)(yiPOS.x) - pos.x, (float)(yiPOS.y) - pos.y};
    float distToWaypoint = glm::length(toWaypoint);

    // 到达当前路径点 → 移除并取下一个
    if (distToWaypoint < 9.0f) {
        LPath.pop_back();
        if (LPath.empty()) {
            if (mInvestigating) {
                // 到达最后目击位置 → 开始搜索
                mInvestigating = false;
                mSearching = true;
                mSearchTimer = 0.0f;
                LOGD("NPCGOBT::DoChase() 到达最后目击位置 → 开始搜索(%.1fs)", kSearchDuration);
            }
            return gobot::Status::Running;
        }
        yiPOS = LPath.back();
        toWaypoint = glm::vec2{(float)(yiPOS.x) - pos.x, (float)(yiPOS.y) - pos.y};
        distToWaypoint = glm::length(toWaypoint);
        if (distToWaypoint < 0.001f) return gobot::Status::Running;
    }

    // 朝向路径点 + 设置移动方向
    float YAngle = PhysicsBlock::EdgeVecToCosAngleFloat(toWaypoint);
    mNPC->GetMovement()->SetLookAngle(YAngle);
    glm::vec2 dir = toWaypoint / distToWaypoint;
    mNPC->GetMovement()->SetMoveInput(Vec2_{dir.x, dir.y});

    return gobot::Status::Running;
}

// --- 攻击（距离保持 + 侧向走位 + 距离自适应瞄准误差） ---
gobot::Status NPCGOBT::DoAttack(gobot::Context& ctx)
{
    if (mNPC->GetMovement()->GetMode() != MovementMode::Ragdoll) {
        mNPC->GetMovement()->SetMode(MovementMode::Controlled);
    }

    int flags = GetSensoryMessages();
    bool visible = (flags & SensoryMessages_Visible) != 0;

    // 失去目标判定：
    // 1) 超出滞回带宽(>115) → 立即失败 → 重新分解（追击）
    // 2) 短暂丢失视野(<0.6s) → 继续朝最后已知方位压制射击，
    //    防止"只开一枪就永久放弃"（LOS 抖动/短暂绕柱不再中断攻击）
    // ★ 绝不隔墙攻击：看不见超过宽限时间就停止开火
    if (mPlayerDistance > AttackRange + kRangeHysteresis) {
        LOGD("NPCGOBT::DoAttack() 超出攻击范围(距离=%.1f>%d) → Failure",
             mPlayerDistance, AttackRange + kRangeHysteresis);
        return gobot::Status::Failure;
    }
    if (!visible && mPlayerLostTime > kAttackLosGrace) {
        LOGD("NPCGOBT::DoAttack() 丢失视野超时(%.2fs>%.2fs) → Failure, 转追击",
             mPlayerLostTime, kAttackLosGrace);
        return gobot::Status::Failure;
    }

    // 朝向玩家（身体平滑转向）
    mNPC->GetMovement()->SetLookAngle(wanjiaAngle);

    glm::vec2 pos = mNPC->GetObjectCollision()->pos;
    glm::vec2 toPlayer = glm::vec2{Global::GamePlayerX - pos.x, Global::GamePlayerY - pos.y};
    float dist = mPlayerDistance;
    glm::vec2 moveDir = dist > 0.001f ? toPlayer / dist : glm::vec2{1.0f, 0.0f};

    if (dist < kIdealAttackDist - 15.0f) {
        // 太近：后退
        mNPC->GetMovement()->SetMoveInput(Vec2_{-moveDir.x, -moveDir.y});
    } else if (dist > kIdealAttackDist + 15.0f) {
        // 太远：靠近
        mNPC->GetMovement()->SetMoveInput(Vec2_{moveDir.x, moveDir.y});
    } else {
        // 距离适中(35~65)：横向走位（周期性换向 + 撞墙换向），
        // 不做前后振荡，避免"贴身前后踱步"观感
        mStrafeTimer += FPSTime;
        if (mStrafeTimer > kStrafePeriod) {
            mStrafeTimer = 0.0f;
            mStrafeDir = -mStrafeDir;
        }
        glm::vec2 strafe = {-moveDir.y, moveDir.x};
        glm::vec2 probe = pos + strafe * (float)mStrafeDir * 16.0f;
        if (!wPathfinding->GetPixelWallNumber(
                probe.x + wPathfinding->PathfindingDecoratorDeviationX,
                probe.y + wPathfinding->PathfindingDecoratorDeviationY)) {
            mStrafeDir = -mStrafeDir; // 侧向有墙 → 换向
        }
        mNPC->GetMovement()->SetMoveInput(Vec2_{strafe.x * (float)mStrafeDir,
                                                 strafe.y * (float)mStrafeDir});
    }

    // ============================================================
    // 射击：朝玩家实时方位（wanjiaAngle）瞄准，不等身体转完
    //  - 炮口沿开火方向偏移 12（>坦克对角半径），任何朝向都不会打中自己
    //  - 仅看得见时开火（丢失视野宽限内朝最后方位压制）
    // ============================================================
    mShootCooldown -= FPSTime;
    if (mShootCooldown <= 0.0f) {
        mShootCooldown = mShootInterval;
        float dynamicError = 0.05f + (dist / (float)ChaseRange) * 0.10f;
        float aimError = ((rand() % 1000) / 1000.0f - 0.5f) * 2.0f * dynamicError;
        float fireAngle = wanjiaAngle + aimError;
        glm::vec2 shootPos = pos + PhysicsBlock::vec2angle(
            glm::vec2{kMuzzleOffset, 0.0f}, fireAngle);
        LOGD("NPCGOBT::DoAttack() 射击! 距离=%.1f 可见=%s 开火角=%.3f(误差=%.3f)",
             dist, visible ? "Y" : "N", fireAngle, aimError);
        wArms->ShootBullets(shootPos.x, shootPos.y, fireAngle, 500, 0);
    }

    return gobot::Status::Running;
}

// --- 受伤恢复（硬直；Effect 将 injury_recovered=true，满足 Survive） ---
gobot::Status NPCGOBT::DoInjury(gobot::Context& ctx)
{
    // 新伤害刷新硬直计时（连续受击 → 硬直重新计时）
    if (mNPC->mPixelQueue->GetNumber() > 0) {
        mTime = 0.0f;
        injuryEntered_ = true;
    }

    // 首次进入：重置计时器
    if (!injuryEntered_) {
        mTime = 0.0f;
        injuryEntered_ = true;
        LOGD("NPCGOBT::DoInjury() 首次进入受伤状态, 计时器重置");
    }

    // 硬直：冻结移动，仍可转向朝向攻击者
    mNPC->GetMovement()->SetMode(MovementMode::Frozen);
    mNPC->GetMovement()->SetLookAngle(wanjiaAngle);

    // 1.0s 硬直后恢复
    if (mTime > 1.0f) {
        injuryEntered_ = false;
        LOGD("NPCGOBT::DoInjury() 硬直结束(mTime=%.2f>1.0) → Success, 恢复行动", mTime);
        return gobot::Status::Success;
    }
    return gobot::Status::Running;
}

// --- 后撤（SelfPreserve：受伤后与敌人拉开距离） ---
gobot::Status NPCGOBT::DoFlee(gobot::Context& ctx)
{
    if (mNPC->GetMovement()->GetMode() != MovementMode::Ragdoll) {
        mNPC->GetMovement()->SetMode(MovementMode::Controlled);
    }

    glm::vec2 pos = mNPC->GetObjectCollision()->pos;
    glm::vec2 away{pos.x - Global::GamePlayerX, pos.y - Global::GamePlayerY};
    float awayLen = glm::length(away);
    glm::vec2 baseDir = awayLen > 0.001f ? away / awayLen : qianjinfang;

    if (!mFleeEntered) {
        mFleeEntered = true;
        mFleeTimer = 0.0f;
        LOGD("NPCGOBT::DoFlee() 开始后撤 | 远离玩家方向=(%.2f,%.2f)", baseDir.x, baseDir.y);
    }
    mFleeTimer += FPSTime;

    // 后撤方向探测：依次尝试 0°, ±45°, ±90°, ±135°, 180°，取第一个可通行方向
    static const float kFleeAngles[8] = {
        0.0f, 0.7854f, -0.7854f, 1.5708f, -1.5708f, 2.3562f, -2.3562f, 3.14159f
    };
    glm::vec2 moveDir = baseDir;
    for (float ang : kFleeAngles) {
        glm::vec2 d{baseDir.x * cosf(ang) - baseDir.y * sinf(ang),
                    baseDir.x * sinf(ang) + baseDir.y * cosf(ang)};
        glm::vec2 probe = pos + d * 16.0f;
        if (wPathfinding->GetPixelWallNumber(
                probe.x + wPathfinding->PathfindingDecoratorDeviationX,
                probe.y + wPathfinding->PathfindingDecoratorDeviationY)) {
            moveDir = d;
            break;
        }
    }

    mNPC->GetMovement()->SetLookAngle(PhysicsBlock::EdgeVecToCosAngleFloat(moveDir));
    mNPC->GetMovement()->SetMoveInput(Vec2_{moveDir.x, moveDir.y});

    // 撤退完成：拉开足够距离或时间到
    if (awayLen > kFleeDistance || mFleeTimer > kFleeDuration) {
        mFleeEntered = false;
        LOGD("NPCGOBT::DoFlee() 后撤完成(距离=%.1f, 时间=%.2f) → Success, 威胁解除",
             awayLen, mFleeTimer);
        return gobot::Status::Success;
    }
    return gobot::Status::Running;
}

// ============================================================================
// 对外接口
// ============================================================================

void NPCGOBT::SetNPC(int x, int y, float angle)
{
    LOGD("NPCGOBT::SetNPC() 设置NPC位置=(%d,%d) 角度=%.3f", x, y, angle);
    mNPC->GetObjectCollision()->pos = Vec2_{static_cast<FLOAT_>(x), static_cast<FLOAT_>(y)};
    mNPC->GetObjectCollision()->angle = angle;
    mNPC->setGamePlayerMatrix(3, true);
}

void NPCGOBT::Event(int Frame, float time)
{
    mTime += time;
    FPSTime = time;
    mFrameCounter++;

    // 1. 同步世界状态（感官 → WorldState，含交战状态机与各锁存量）
    SyncWorldState();

    // 2. 执行 GOBT 行为树（替代原 FSM process_event）
    mExecutor->tick_once();

    // 3. 统一更新移动（与原 NPC::Event 一致）
    mNPC->GetMovement()->Update(FPSTime);

    // 4. 更新 NPC 损伤
    mNPC->UpData();

    // 5. 更新模型位置
    mNPC->setGamePlayerMatrix(time, Frame);
}

} // namespace GAME
