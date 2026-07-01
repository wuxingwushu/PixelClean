#include "NPCGOBT.h"
#include "../DebugLog.h"
#include "../Tool/Tool.h"
#include "../GlobalVariable.h"
#include <iostream>

namespace GAME {

// JPS 障碍检测回调（与原 NPC.cpp 一致）
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

    // 设置 NPC 专属移动参数（与原 NPC.cpp 一致）
    MovementComponent* mc = mNPC->GetMovement();
    if (mc) {
        mc->Config().MaxSpeed = 90.0f;
        mc->Config().TurnRate = 7.0f;
        mc->Config().RagdollMinTime = 0.4f;
        LOGD("NPCGOBT::NPCGOBT() 移动参数配置完成 | MaxSpeed=90.0, TurnRate=7.0, RagdollMinTime=0.4");
    } else {
        LOGD("NPCGOBT::NPCGOBT() 警告：MovementComponent 为空");
    }

    // 初始化 GOBT 组件
    mBlackboard = std::make_shared<gobot::SharedBlackboard>();
    mEventBus = std::make_shared<gobot::EventBus>();
    mGoalManager = std::make_shared<gobot::GoalManager>();
    mDecomposer = std::make_shared<gobot::TacticalDecomposer>();
    mSubtreeLib = std::make_shared<gobot::SubtreeLibrary>();
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

    // 目标 1：生存（优先级 100）——受伤时激活
    // 满足条件：injury_recovered == true（DoInjury 超时后设置，或伤害自然消除）
    mGoalManager->add_goal(std::make_shared<Goal>(
        "Survive",
        std::unordered_map<WorldKey, WorldValue>{{NPCWS::kInjuryRecovered, true}},
        100));
    LOGD("NPCGOBT::SetupGoals() 注册目标 [Survive] 优先级=100, 满足条件=%s=true",
         NPCWS::kInjuryRecovered.c_str());

    // 目标 2：与敌交战（优先级 80）——看到玩家时激活
    // 满足条件：player_visible == false（玩家消失视为威胁解除）
    mGoalManager->add_goal(std::make_shared<Goal>(
        "FightEnemy",
        std::unordered_map<WorldKey, WorldValue>{{NPCWS::kPlayerVisible, false}},
        80));
    LOGD("NPCGOBT::SetupGoals() 注册目标 [FightEnemy] 优先级=80, 满足条件=%s=false",
         NPCWS::kPlayerVisible.c_str());

    // 目标 3：巡逻区域（优先级 20）——默认行为，永不满足
    // 满足条件：patrol_done == true（此键永不设置，故永为 fallback）
    mGoalManager->add_goal(std::make_shared<Goal>(
        "PatrolArea",
        std::unordered_map<WorldKey, WorldValue>{{"patrol_done", true}},
        20));
    LOGD("NPCGOBT::SetupGoals() 注册目标 [PatrolArea] 优先级=20, 满足条件=patrol_done=true (无限fallback)");

    LOGD("NPCGOBT::SetupGoals() 目标注册完成，共3个目标");
}

void NPCGOBT::SetupDecomposer()
{
    using namespace gobot;
    auto bb = mBlackboard;

    LOGD("NPCGOBT::SetupDecomposer() 开始注册分解策略");

    // --- Survive 分解：硬直恢复 ---
    mDecomposer->register_strategy("Survive", [](const Goal& goal) {
        LOGD("NPCGOBT::Decompose[Survive] 分解目标 '%s'(优先级=%d) → 子目标=[RecoverFromInjury]",
             goal.name().c_str(), goal.priority());
        return std::vector<SubgoalPtr>{
            std::make_shared<Subgoal>(
                SubgoalType::Recover, "RecoverFromInjury",
                std::unordered_map<WorldKey, WorldValue>{{NPCWS::kInjuryRecovered, true}},
                1)
        };
    });
    LOGD("NPCGOBT::SetupDecomposer() 注册分解策略 [Survive → RecoverFromInjury]");

    // --- FightEnemy 分解：条件选择攻击或追击 ---
    // 根据当前世界状态决定子目标（动态分解）
    mDecomposer->register_strategy("FightEnemy", [bb](const Goal& goal) {
        auto ws = bb->world_state();
        bool inRange = ws->get<bool>(NPCWS::kPlayerInRange).value_or(false);

        if (inRange) {
            LOGD("NPCGOBT::Decompose[FightEnemy] 目标='%s'(优先级=%d) | kPlayerInRange=true → 子目标=[AttackPlayer]",
                 goal.name().c_str(), goal.priority());
            return std::vector<SubgoalPtr>{
                std::make_shared<Subgoal>(
                    SubgoalType::Combat, "AttackPlayer",
                    std::unordered_map<WorldKey, WorldValue>{{NPCWS::kPlayerVisible, false}},
                    3)
            };
        } else {
            LOGD("NPCGOBT::Decompose[FightEnemy] 目标='%s'(优先级=%d) | kPlayerInRange=false → 子目标=[ChasePlayer]",
                 goal.name().c_str(), goal.priority());
            return std::vector<SubgoalPtr>{
                std::make_shared<Subgoal>(
                    SubgoalType::MoveTo, "ChasePlayer",
                    std::unordered_map<WorldKey, WorldValue>{{NPCWS::kPlayerInRange, true}},
                    3)
            };
        }
    });
    LOGD("NPCGOBT::SetupDecomposer() 注册分解策略 [FightEnemy → AttackPlayer/ChasePlayer 动态分解]");

    // --- PatrolArea 分解：待机 → 巡逻 循环 ---
    mDecomposer->register_strategy("PatrolArea", [](const Goal& goal) {
        LOGD("NPCGOBT::Decompose[PatrolArea] 目标='%s'(优先级=%d) → 子目标=[Standby, Patrol] 循环",
             goal.name().c_str(), goal.priority());
        return std::vector<SubgoalPtr>{
            std::make_shared<Subgoal>(
                SubgoalType::Standby, "Standby",
                std::unordered_map<WorldKey, WorldValue>{}, 1),
            std::make_shared<Subgoal>(
                SubgoalType::Patrol, "Patrol",
                std::unordered_map<WorldKey, WorldValue>{}, 1)
        };
    });
    LOGD("NPCGOBT::SetupDecomposer() 注册分解策略 [PatrolArea → Standby → Patrol 循环]");

    LOGD("NPCGOBT::SetupDecomposer() 分解策略注册完成，共3个策略");
}

void NPCGOBT::SetupSubtreeLibrary()
{
    using namespace gobot;

    LOGD("NPCGOBT::SetupSubtreeLibrary() 开始注册子树工厂");

    // --- RecoverFromInjury 子树 ---
    // 对应原 FSM 的 Injury 状态：冻结移动、面向玩家、超时恢复
    // 效果：Success 后自动设置 injury_recovered=true（由 OperationalActionNode 应用）
    mSubtreeLib->register_subtree(SubgoalType::Recover, [this]() -> BTNodePtr {
        LOGD("NPCGOBT::SubtreeFactory[Recover] 创建 InjuryAction 子树实例");
        auto action = std::make_shared<Action>(
            "InjuryAction",
            std::unordered_map<WorldKey, WorldValue>{{NPCWS::kInjured, true}},
            std::unordered_map<WorldKey, WorldValue>{{NPCWS::kInjuryRecovered, true}},
            [this](Context& ctx) { return DoInjury(ctx); });
        return std::make_shared<OperationalActionNode>(action);
    });
    LOGD("NPCGOBT::SetupSubtreeLibrary() 注册子树 [Recover → InjuryAction]");

    // --- AttackPlayer 子树 ---
    // 对应原 FSM 的 Attack 状态：射击、保持距离
    mSubtreeLib->register_subtree(SubgoalType::Combat, [this]() -> BTNodePtr {
        LOGD("NPCGOBT::SubtreeFactory[Combat] 创建 AttackAction 子树实例");
        auto action = std::make_shared<Action>(
            "AttackAction",
            std::unordered_map<WorldKey, WorldValue>{
                {NPCWS::kPlayerVisible, true},
                {NPCWS::kPlayerInRange, true}
            },
            std::unordered_map<WorldKey, WorldValue>{},
            [this](Context& ctx) { return DoAttack(ctx); });
        return std::make_shared<OperationalActionNode>(action);
    });
    LOGD("NPCGOBT::SetupSubtreeLibrary() 注册子树 [Combat → AttackAction] (前置条件: playerVisible && playerInRange)");

    // --- ChasePlayer 子树 ---
    // 对应原 FSM 的 Chasing 状态：JPS 寻路追击
    mSubtreeLib->register_subtree(SubgoalType::MoveTo, [this]() -> BTNodePtr {
        LOGD("NPCGOBT::SubtreeFactory[MoveTo] 创建 ChaseAction 子树实例");
        auto action = std::make_shared<Action>(
            "ChaseAction",
            std::unordered_map<WorldKey, WorldValue>{{NPCWS::kPlayerVisible, true}},
            std::unordered_map<WorldKey, WorldValue>{},
            [this](Context& ctx) { return DoChase(ctx); });
        return std::make_shared<OperationalActionNode>(action);
    });
    LOGD("NPCGOBT::SetupSubtreeLibrary() 注册子树 [MoveTo → ChaseAction] (前置条件: playerVisible)");

    // --- Standby 子树 ---
    // 对应原 FSM 的 Standby 状态：转向、1.5s 后转巡逻
    mSubtreeLib->register_subtree(SubgoalType::Standby, [this]() -> BTNodePtr {
        LOGD("NPCGOBT::SubtreeFactory[Standby] 创建 StandbyAction 子树实例");
        auto action = std::make_shared<Action>(
            "StandbyAction",
            std::unordered_map<WorldKey, WorldValue>{},
            std::unordered_map<WorldKey, WorldValue>{},
            [this](Context& ctx) { return DoStandby(ctx); });
        return std::make_shared<OperationalActionNode>(action);
    });
    LOGD("NPCGOBT::SetupSubtreeLibrary() 注册子树 [Standby → StandbyAction] (无条件)");

    // --- Patrol 子树 ---
    // 对应原 FSM 的 Patrol 状态：前进、遇墙转向
    mSubtreeLib->register_subtree(SubgoalType::Patrol, [this]() -> BTNodePtr {
        LOGD("NPCGOBT::SubtreeFactory[Patrol] 创建 PatrolAction 子树实例");
        auto action = std::make_shared<Action>(
            "PatrolAction",
            std::unordered_map<WorldKey, WorldValue>{},
            std::unordered_map<WorldKey, WorldValue>{},
            [this](Context& ctx) { return DoPatrol(ctx); });
        return std::make_shared<OperationalActionNode>(action);
    });
    LOGD("NPCGOBT::SetupSubtreeLibrary() 注册子树 [Patrol → PatrolAction] (无条件)");

    LOGD("NPCGOBT::SetupSubtreeLibrary() 子树注册完成，共5个子树");
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
// 世界状态同步
// ============================================================================

void NPCGOBT::SyncWorldState()
{
    int flags = GetSensoryMessages();
    auto& ws = *mBlackboard->world_state();

    int currentDamage = mNPC->mPixelQueue->GetNumber();
    bool isInjured = currentDamage > 0;

    // 伤害检测逻辑：
    // - 新伤害（队列增长）→ injury_recovered = false，激活 Survive 目标
    // - 伤害自然消除（队列归零）→ injury_recovered = true
    // - DoInjury 超时后通过 Action 效果设置 injury_recovered = true（强制恢复）
    if (currentDamage > lastDamageCount_) {
        ws.set(NPCWS::kInjuryRecovered, false);
        LOGD("NPCGOBT::SyncWorldState() 检测到新伤害 | damageCount=%d (上次=%d), 设置 injuryRecovered=false, 将激活 [Survive] 目标",
             currentDamage, lastDamageCount_);
    } else if (currentDamage == 0) {
        if (lastDamageCount_ > 0) {
            LOGD("NPCGOBT::SyncWorldState() 伤害消除 | damageCount=0 (上次=%d), 设置 injuryRecovered=true",
                 lastDamageCount_);
        }
        ws.set(NPCWS::kInjuryRecovered, true);
    }
    lastDamageCount_ = currentDamage;

    ws.set(NPCWS::kInjured, isInjured);
    ws.set(NPCWS::kPlayerVisible, (flags & SensoryMessages_Visible) != 0);
    ws.set(NPCWS::kPlayerInRange, (flags & SensoryMessages_Range) != 0);
    ws.set(NPCWS::kPlayerInViewField, (flags & SensoryMessages_VisualField) != 0);
}

// ============================================================================
// 感官系统（从 NPC.cpp 完整迁移）
// ============================================================================

int NPCGOBT::GetSensoryMessages()
{
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

    LOGD("NPCGOBT::GetSensoryMessages() NPC位置=(%.1f,%.1f), 玩家位置=(%.1f,%.1f), 距离=%.1f, 角度差=%.2f",
         pos.x, pos.y, Global::GamePlayerX, Global::GamePlayerY, mPlayerDistance, AngleCha);

    // 视野锥：±90°（±1.57 弧度）
    if (fabs(AngleCha) < 1.57f) {
        flags |= SensoryMessages_VisualField;
        LOGD("NPCGOBT::GetSensoryMessages() 玩家在视野锥内 | angleCha=%.2f < 1.57", AngleCha);

        // 射线检测：视野内且无遮挡
        PhysicsBlock::CollisionInfoI LInfo = wPathfinding->RadialCollisionDetection(
            pos.x, pos.y, Global::GamePlayerX, Global::GamePlayerY);
        if (!LInfo.Collision) {
            mSuspicious = true;
            mSuspiciousPos = {(int)Global::GamePlayerX, (int)Global::GamePlayerY};
            flags |= SensoryMessages_Visible;
            LOGD("NPCGOBT::GetSensoryMessages() 玩家可见(无遮挡) | 可疑位置=(%d,%d)",
                 mSuspiciousPos.x, mSuspiciousPos.y);
        } else {
            LOGD("NPCGOBT::GetSensoryMessages() 玩家在视野内但有遮挡，不可见");
        }
    } else {
        LOGD("NPCGOBT::GetSensoryMessages() 玩家不在视野锥内 | angleCha=%.2f >= 1.57", AngleCha);
    }

    // 攻击范围判定
    if (mPlayerDistance < AttackRange) {
        flags |= SensoryMessages_Range;
        LOGD("NPCGOBT::GetSensoryMessages() 玩家在攻击范围内 | distance=%.1f < AttackRange=%d",
             mPlayerDistance, AttackRange);
    } else {
        LOGD("NPCGOBT::GetSensoryMessages() 玩家不在攻击范围内 | distance=%.1f >= AttackRange=%d",
             mPlayerDistance, AttackRange);
    }

    LOGD("NPCGOBT::GetSensoryMessages() 结果标志位 | VisualField=%s, Visible=%s, InRange=%s",
         (flags & SensoryMessages_VisualField) ? "Y" : "N",
         (flags & SensoryMessages_Visible) ? "Y" : "N",
         (flags & SensoryMessages_Range) ? "Y" : "N");
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
        LOGD("NPCGOBT::DoStandby() 待机超时(mStandbyTimer=%.2f>1.5) → Success, 将切换到巡逻 | 前进方向=(%.1f,%.1f)",
             mStandbyTimer, qianjinfang.x, qianjinfang.y);
        return gobot::Status::Success;
    }
    LOGD("NPCGOBT::DoStandby() 待机中 mStandbyTimer=%.2f/1.5 | 全局mTime=%.2f | 前进方向=(%.1f,%.1f)",
         mStandbyTimer, mTime, qianjinfang.x, qianjinfang.y);
    return gobot::Status::Running;
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

        // 检查该点是否可通行（用 JPS 相同的障碍回调）
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

// --- 巡逻（使用随机坐标+JPS寻路的新方案） ---
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
        LOGD("NPCGOBT::DoPatrol() [IDLE] 选取巡逻目标 | 位置=(%.1f,%.1f)", pos.x, pos.y);

        // 尝试寻找随机可行走位置
        mPatrolTarget = FindRandomWalkablePosition(pos);

        // 没找到有效位置 → 降级到旧的方向碰撞巡逻
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
            LOGD("NPCGOBT::DoPatrol() [PATHFINDING] JPS 计算中... | 目标=(%d,%d)",
                 mPatrolTarget.x, mPatrolTarget.y);
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
            LOGD("NPCGOBT::DoPatrol() [MOVING] 路径/目标异常，重新选取 | 位置=(%.1f,%.1f) 目标距离=%.1f",
                 pos.x, pos.y, glm::length(toTarget));
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
                LOGD("NPCGOBT::DoPatrol() [MOVING] 到达终点，选取下一个巡逻目标 | 位置=(%.1f,%.1f)",
                     pos.x, pos.y);
                return gobot::Status::Running;
            }
            yiPOS = LPath.back();
            toWaypoint = glm::vec2{(float)(yiPOS.x) - pos.x, (float)(yiPOS.y) - pos.y};
            distToWaypoint = glm::length(toWaypoint);
            if (distToWaypoint < 0.001f) return gobot::Status::Running;
            LOGD("NPCGOBT::DoPatrol() [MOVING] 到达路径点, 前进到下一路径点=(%d,%d) | 剩余路径点数=%zu, 距离=%.1f",
                 yiPOS.x, yiPOS.y, LPath.size(), distToWaypoint);
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

// --- 方向碰撞巡逻（旧方案，作为新方案寻路失败时的降级 fallback） ---
gobot::Status NPCGOBT::DoPatrolFallback(const glm::vec2& pos)
{
    glm::vec2 Lpos = pos + qianjinfang * 2.0f;

    LOGD("NPCGOBT::DoPatrolFallback() 位置=(%.1f,%.1f) 方向=(%.1f,%.1f)",
         pos.x, pos.y, qianjinfang.x, qianjinfang.y);

    // 检测前方是否有墙
    if (!wPathfinding->GetPixelWallNumber(
            Lpos.x + wPathfinding->PathfindingDecoratorDeviationX,
            Lpos.y + wPathfinding->PathfindingDecoratorDeviationY)) {
        // 前方无墙：前进
        float YAngle = PhysicsBlock::EdgeVecToCosAngleFloat(qianjinfang);
        mNPC->GetMovement()->SetLookAngle(YAngle);
        mNPC->GetMovement()->SetMoveInput(Vec2_{qianjinfang.x, qianjinfang.y});
        return gobot::Status::Running;
    }

    // 前方有墙：尝试其他 3 个方向
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

        if (!wPathfinding->GetPixelWallNumber(
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

// --- 追击（对应原 FSM GoToLocation 状态的执行逻辑） ---
gobot::Status NPCGOBT::DoChase(gobot::Context& ctx)
{
    if (mNPC->GetMovement()->GetMode() != MovementMode::Ragdoll) {
        mNPC->GetMovement()->SetMode(MovementMode::Controlled);
    }

    int flags = GetSensoryMessages();

    // 在攻击范围内且可见 → Success（触发重新分解 → 攻击）
    if ((flags & SensoryMessages_Range) && (flags & SensoryMessages_Visible)) {
        mJpsSubmitted = false;
        LOGD("NPCGOBT::DoChase() 已进入攻击范围且可见 → Success, 将切换到攻击 位置=(%.1f,%.1f)",
             mNPC->GetObjectCollision()->pos.x, mNPC->GetObjectCollision()->pos.y);
        return gobot::Status::Success;
    }

    // 完全丢失目标 → Failure（触发目标重评估 → 巡逻）
    if (!(flags & SensoryMessages_Visible) && !mSuspicious) {
        mJpsSubmitted = false;
        LOGD("NPCGOBT::DoChase() 完全丢失目标(不可见且无可疑位置) → Failure, 将切换到巡逻");
        return gobot::Status::Failure;
    }

    glm::vec2 pos = mNPC->GetObjectCollision()->pos;

    // 寻路中，等待
    if (!mJPS->GetPathfindingCompleted()) {
        LOGD("NPCGOBT::DoChase() JPS寻路进行中... 等待路径计算完成 | 位置=(%.1f,%.1f)", pos.x, pos.y);
        return gobot::Status::Running;
    }

    // === 检测JPS空路径死循环 ===
    // 如果JPS已提交过(LPath为空且mJpsSubmitted=true)但返回空结果 → 目标不可达
    if (LPath.empty() && mJpsSubmitted) {
        mJpsSubmitted = false;
        LOGD("NPCGOBT::DoChase() JPS寻路完成但未找到路径(目标不可达) → Failure, 将切换到巡逻 | 位置=(%.1f,%.1f)", pos.x, pos.y);
        return gobot::Status::Failure;
    }

    // 路径不为空 → 有有效路径，清除提交标志
    if (!LPath.empty()) {
        mJpsSubmitted = false;
    }

    // 判断是否需要重新寻路（LPath为空时首次进入，或路径过期）
    bool needRepath = LPath.empty() || (mTime > mPathfindingCycle);

    if (needRepath) {
        JPSVec2 target;
        if (mSuspicious) {
            // 前往最后目击位置
            target = mSuspiciousPos;
            mSuspicious = false;
            LOGD("NPCGOBT::DoChase() 重新寻路 → 前往最后目击位置=(%d,%d) | 位置=(%.1f,%.1f)",
                 target.x, target.y, pos.x, pos.y);
        } else if (flags & SensoryMessages_Visible) {
            // 仍能看到玩家 → 追玩家
            target = {(int)Global::GamePlayerX, (int)Global::GamePlayerY};
            LOGD("NPCGOBT::DoChase() 重新寻路 → 追击玩家=(%d,%d) | 位置=(%.1f,%.1f)",
                 target.x, target.y, pos.x, pos.y);
        } else {
            // 完全丢失 → Failure
            mJpsSubmitted = false;
            LOGD("NPCGOBT::DoChase() 重新寻路条件但不可见且无可疑位置 → Failure");
            return gobot::Status::Failure;
        }

        // 距离过远时截断到寻路范围内
        glm::vec2 toTarget = glm::vec2{target.x - pos.x, target.y - pos.y};
        glm::vec2 originalToTarget = toTarget;
        while (fabs(toTarget.x) > mRange || fabs(toTarget.y) > mRange) {
            toTarget *= 0.5f;
        }
        JPSVec2 clampedTarget = {(int)(pos.x + toTarget.x), (int)(pos.y + toTarget.y)};

        if (originalToTarget.x != toTarget.x || originalToTarget.y != toTarget.y) {
            LOGD("NPCGOBT::DoChase() 目标距离过远，截断 | 原始偏移=(%.1f,%.1f) → 截断后偏移=(%.1f,%.1f) 钳制目标=(%d,%d)",
                 originalToTarget.x, originalToTarget.y, toTarget.x, toTarget.y, clampedTarget.x, clampedTarget.y);
        }

        LPath.clear();
        TOOL::mThreadPool->enqueue(&JPS::FindPath, mJPS,
            JPSVec2{(int)pos.x, (int)pos.y}, clampedTarget, &LPath,
            JPSVec2{wPathfinding->PathfindingDecoratorDeviationX,
                    wPathfinding->PathfindingDecoratorDeviationY});
        mJpsSubmitted = true;
        hsuldad = 0;
        mTime = 0;
        LOGD("NPCGOBT::DoChase() JPS寻路已提交 | 起点=(%d,%d) → 终点=(%d,%d) | mPathfindingCycle=%.1f",
             (int)pos.x, (int)pos.y, clampedTarget.x, clampedTarget.y, mPathfindingCycle);
        return gobot::Status::Running;
    }

    // === 沿路径前进 ===
    if (LPath.empty()) {
        LOGD("NPCGOBT::DoChase() 路径为空，等待中...");
        return gobot::Status::Running;
    }

    JPSVec2 yiPOS = LPath.back();
    glm::vec2 toWaypoint = glm::vec2{(float)(yiPOS.x) - pos.x, (float)(yiPOS.y) - pos.y};
    float distToWaypoint = glm::length(toWaypoint);

    // 到达当前路径点 → 移除并取下一个
    if (distToWaypoint < 9.0f) {
        LPath.pop_back();
        if (LPath.empty()) {
            LOGD("NPCGOBT::DoChase() 到达路径终点 | 位置=(%.1f,%.1f)", pos.x, pos.y);
            return gobot::Status::Running;
        }
        yiPOS = LPath.back();
        toWaypoint = glm::vec2{(float)(yiPOS.x) - pos.x, (float)(yiPOS.y) - pos.y};
        distToWaypoint = glm::length(toWaypoint);
        if (distToWaypoint < 0.001f) return gobot::Status::Running;
        LOGD("NPCGOBT::DoChase() 到达路径点, 前进到下一路径点=(%d,%d) | 剩余路径点数=%zu, 距离=%.1f",
             yiPOS.x, yiPOS.y, LPath.size(), distToWaypoint);
    }

    // 朝向路径点 + 设置移动方向
    float YAngle = PhysicsBlock::EdgeVecToCosAngleFloat(toWaypoint);
    mNPC->GetMovement()->SetLookAngle(YAngle);
    glm::vec2 dir = toWaypoint / distToWaypoint;
    mNPC->GetMovement()->SetMoveInput(Vec2_{dir.x, dir.y});

    return gobot::Status::Running;
}

// --- 攻击（对应原 FSM Attack 状态的执行逻辑） ---
gobot::Status NPCGOBT::DoAttack(gobot::Context& ctx)
{
    if (mNPC->GetMovement()->GetMode() != MovementMode::Ragdoll) {
        mNPC->GetMovement()->SetMode(MovementMode::Controlled);
    }

    int flags = GetSensoryMessages();

    // 失去目标 → Failure（触发重新分解 → 追击或巡逻）
    if (!((flags & SensoryMessages_Range) && (flags & SensoryMessages_Visible))) {
        LOGD("NPCGOBT::DoAttack() 失去目标(不可见或不在攻击范围) → Failure, 将切换到追击/巡逻");
        return gobot::Status::Failure;
    }

    // 朝向玩家
    mNPC->GetMovement()->SetLookAngle(wanjiaAngle);

    // 保持中距离：太近后退，太远靠近
    glm::vec2 pos = mNPC->GetObjectCollision()->pos;
    glm::vec2 toPlayer = glm::vec2{Global::GamePlayerX - pos.x, Global::GamePlayerY - pos.y};
    float dist = mPlayerDistance;
    float idealDist = 50.0f;
    glm::vec2 moveDir = glm::normalize(toPlayer);
    if (dist < idealDist - 10.0f) {
        mNPC->GetMovement()->SetMoveInput(Vec2_{-moveDir.x, -moveDir.y});
        LOGD("NPCGOBT::DoAttack() 玩家太近(%.1f<%.1f), 后退", dist, idealDist - 10.0f);
    } else if (dist > idealDist + 10.0f) {
        mNPC->GetMovement()->SetMoveInput(Vec2_{moveDir.x, moveDir.y});
        LOGD("NPCGOBT::DoAttack() 玩家太远(%.1f>%.1f), 靠近", dist, idealDist + 10.0f);
    } else {
        LOGD("NPCGOBT::DoAttack() 距离适中(%.1f), 保持位置", dist);
    }

    // 射击（带冷却 + 瞄准误差）
    mShootCooldown -= FPSTime;
    if (mShootCooldown <= 0.0f) {
        mShootCooldown = mShootInterval;
        glm::vec2 shootPos = pos + PhysicsBlock::vec2angle(
            glm::vec2{9.0f, 0.0f}, mNPC->GetObjectCollision()->angle);
        float aimError = ((rand() % 100) / 100.0f - 0.5f) * 0.15f;
        LOGD("NPCGOBT::DoAttack() 射击! 位置=(%.1f,%.1f) 角度=%.3f(误差=%.3f) 冷却=%.1f",
             shootPos.x, shootPos.y, mNPC->GetObjectCollision()->angle + aimError, aimError, mShootInterval);
        wArms->ShootBullets(shootPos.x, shootPos.y,
            mNPC->GetObjectCollision()->angle + aimError, 500, 0);
    }

    return gobot::Status::Running;
}

// --- 受伤恢复（对应原 FSM Injury 状态的执行逻辑） ---
gobot::Status NPCGOBT::DoInjury(gobot::Context& ctx)
{
    // 首次进入：重置计时器（对应原 FSM 转入 Injury 时的 mTime = 0.0）
    if (!injuryEntered_) {
        mTime = 0.0f;
        injuryEntered_ = true;
        LOGD("NPCGOBT::DoInjury() 首次进入受伤状态, 计时器重置");
    }

    // 进入硬直：冻结移动，但仍可转向朝向玩家
    mNPC->GetMovement()->SetMode(MovementMode::Frozen);
    mNPC->GetMovement()->SetLookAngle(wanjiaAngle);

    LOGD("NPCGOBT::DoInjury() 硬直中 mTime=%.2f/1.0 | 面向玩家角度=%.3f", mTime, wanjiaAngle);

    // 超时恢复（1.0s 硬直后恢复行动）
    if (mTime > 1.0f) {
        injuryEntered_ = false; // 重置标志，为下次受伤准备
        LOGD("NPCGOBT::DoInjury() 硬直结束(mTime=%.2f>1.0) → Success, 恢复行动", mTime);
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

    LOGD("=== NPCGOBT::Event() Frame=%d, deltaTime=%.4f, totalTime=%.2f ===", Frame, time, mTime);

    // 1. 同步世界状态（感官 → WorldState）
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
