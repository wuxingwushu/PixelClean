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

    // 设置 NPC 专属移动参数（与原 NPC.cpp 一致）
    MovementComponent* mc = mNPC->GetMovement();
    if (mc) {
        mc->Config().MaxSpeed = 90.0f;
        mc->Config().TurnRate = 7.0f;
        mc->Config().RagdollMinTime = 0.4f;
    }

    // 初始化 GOBT 组件
    mBlackboard = std::make_shared<gobot::SharedBlackboard>();
    mEventBus = std::make_shared<gobot::EventBus>();
    mGoalManager = std::make_shared<gobot::GoalManager>();
    mDecomposer = std::make_shared<gobot::TacticalDecomposer>();
    mSubtreeLib = std::make_shared<gobot::SubtreeLibrary>();

    // 注册目标、分解策略、子树
    SetupGoals();
    SetupDecomposer();
    SetupSubtreeLibrary();

    // 构建行为树
    BuildTree();
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

    // 目标 1：生存（优先级 100）——受伤时激活
    // 满足条件：injury_recovered == true（DoInjury 超时后设置，或伤害自然消除）
    mGoalManager->add_goal(std::make_shared<Goal>(
        "Survive",
        std::unordered_map<WorldKey, WorldValue>{{NPCWS::kInjuryRecovered, true}},
        100));

    // 目标 2：与敌交战（优先级 80）——看到玩家时激活
    // 满足条件：player_visible == false（玩家消失视为威胁解除）
    mGoalManager->add_goal(std::make_shared<Goal>(
        "FightEnemy",
        std::unordered_map<WorldKey, WorldValue>{{NPCWS::kPlayerVisible, false}},
        80));

    // 目标 3：巡逻区域（优先级 20）——默认行为，永不满足
    // 满足条件：patrol_done == true（此键永不设置，故永为 fallback）
    mGoalManager->add_goal(std::make_shared<Goal>(
        "PatrolArea",
        std::unordered_map<WorldKey, WorldValue>{{"patrol_done", true}},
        20));
}

void NPCGOBT::SetupDecomposer()
{
    using namespace gobot;
    auto bb = mBlackboard;

    // --- Survive 分解：硬直恢复 ---
    mDecomposer->register_strategy("Survive", [](const Goal&) {
        return std::vector<SubgoalPtr>{
            std::make_shared<Subgoal>(
                SubgoalType::Recover, "RecoverFromInjury",
                std::unordered_map<WorldKey, WorldValue>{{NPCWS::kInjuryRecovered, true}},
                1)
        };
    });

    // --- FightEnemy 分解：条件选择攻击或追击 ---
    // 根据当前世界状态决定子目标（动态分解）
    mDecomposer->register_strategy("FightEnemy", [bb](const Goal&) {
        auto ws = bb->world_state();
        bool inRange = ws->get<bool>(NPCWS::kPlayerInRange).value_or(false);

        if (inRange) {
            // 在攻击范围内 → 攻击子目标
            return std::vector<SubgoalPtr>{
                std::make_shared<Subgoal>(
                    SubgoalType::Combat, "AttackPlayer",
                    std::unordered_map<WorldKey, WorldValue>{{NPCWS::kPlayerVisible, false}},
                    3)
            };
        } else {
            // 不在范围内 → 追击子目标
            return std::vector<SubgoalPtr>{
                std::make_shared<Subgoal>(
                    SubgoalType::MoveTo, "ChasePlayer",
                    std::unordered_map<WorldKey, WorldValue>{{NPCWS::kPlayerInRange, true}},
                    3)
            };
        }
    });

    // --- PatrolArea 分解：待机 → 巡逻 循环 ---
    mDecomposer->register_strategy("PatrolArea", [](const Goal&) {
        return std::vector<SubgoalPtr>{
            std::make_shared<Subgoal>(
                SubgoalType::Standby, "Standby",
                std::unordered_map<WorldKey, WorldValue>{}, 1),
            std::make_shared<Subgoal>(
                SubgoalType::Patrol, "Patrol",
                std::unordered_map<WorldKey, WorldValue>{}, 1)
        };
    });
}

void NPCGOBT::SetupSubtreeLibrary()
{
    using namespace gobot;

    // --- RecoverFromInjury 子树 ---
    // 对应原 FSM 的 Injury 状态：冻结移动、面向玩家、超时恢复
    // 效果：Success 后自动设置 injury_recovered=true（由 OperationalActionNode 应用）
    mSubtreeLib->register_subtree(SubgoalType::Recover, [this]() -> BTNodePtr {
        auto action = std::make_shared<Action>(
            "InjuryAction",
            std::unordered_map<WorldKey, WorldValue>{{NPCWS::kInjured, true}},
            std::unordered_map<WorldKey, WorldValue>{{NPCWS::kInjuryRecovered, true}},
            [this](Context& ctx) { return DoInjury(ctx); });
        return std::make_shared<OperationalActionNode>(action);
    });

    // --- AttackPlayer 子树 ---
    // 对应原 FSM 的 Attack 状态：射击、保持距离
    mSubtreeLib->register_subtree(SubgoalType::Combat, [this]() -> BTNodePtr {
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

    // --- ChasePlayer 子树 ---
    // 对应原 FSM 的 Chasing 状态：JPS 寻路追击
    mSubtreeLib->register_subtree(SubgoalType::MoveTo, [this]() -> BTNodePtr {
        auto action = std::make_shared<Action>(
            "ChaseAction",
            std::unordered_map<WorldKey, WorldValue>{{NPCWS::kPlayerVisible, true}},
            std::unordered_map<WorldKey, WorldValue>{},
            [this](Context& ctx) { return DoChase(ctx); });
        return std::make_shared<OperationalActionNode>(action);
    });

    // --- Standby 子树 ---
    // 对应原 FSM 的 Standby 状态：转向、1.5s 后转巡逻
    mSubtreeLib->register_subtree(SubgoalType::Standby, [this]() -> BTNodePtr {
        auto action = std::make_shared<Action>(
            "StandbyAction",
            std::unordered_map<WorldKey, WorldValue>{},
            std::unordered_map<WorldKey, WorldValue>{},
            [this](Context& ctx) { return DoStandby(ctx); });
        return std::make_shared<OperationalActionNode>(action);
    });

    // --- Patrol 子树 ---
    // 对应原 FSM 的 Patrol 状态：前进、遇墙转向
    mSubtreeLib->register_subtree(SubgoalType::Patrol, [this]() -> BTNodePtr {
        auto action = std::make_shared<Action>(
            "PatrolAction",
            std::unordered_map<WorldKey, WorldValue>{},
            std::unordered_map<WorldKey, WorldValue>{},
            [this](Context& ctx) { return DoPatrol(ctx); });
        return std::make_shared<OperationalActionNode>(action);
    });
}

void NPCGOBT::BuildTree()
{
    using namespace gobot;

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
    } else if (currentDamage == 0) {
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

    // 视野锥：±90°（±1.57 弧度）
    if (fabs(AngleCha) < 1.57f) {
        flags |= SensoryMessages_VisualField;

        // 射线检测：视野内且无遮挡
        PhysicsBlock::CollisionInfoI LInfo = wPathfinding->RadialCollisionDetection(
            pos.x, pos.y, Global::GamePlayerX, Global::GamePlayerY);
        if (!LInfo.Collision) {
            mSuspicious = true;
            mSuspiciousPos = {(int)Global::GamePlayerX, (int)Global::GamePlayerY};
            flags |= SensoryMessages_Visible;
        }
    }

    // 攻击范围判定
    if (mPlayerDistance < AttackRange) {
        flags |= SensoryMessages_Range;
    }
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

    // 待机时缓慢转向前进方向
    mNPC->GetMovement()->SetLookAngle(PhysicsBlock::EdgeVecToCosAngleFloat(qianjinfang));

    // 1.5s 后转巡逻
    if (mTime > 1.5f) {
        return gobot::Status::Success;
    }
    return gobot::Status::Running;
}

// --- 巡逻（对应原 FSM Patrol 状态的执行逻辑） ---
gobot::Status NPCGOBT::DoPatrol(gobot::Context& ctx)
{
    if (mNPC->GetMovement()->GetMode() != MovementMode::Ragdoll) {
        mNPC->GetMovement()->SetMode(MovementMode::Controlled);
    }

    glm::vec2 pos = mNPC->GetObjectCollision()->pos;
    glm::vec2 Lpos = pos + qianjinfang * 2.0f;

    // 检测前方是否有墙
    if (!wPathfinding->GetPixelWallNumber(
            Lpos.x + wPathfinding->PathfindingDecoratorDeviationX,
            Lpos.y + wPathfinding->PathfindingDecoratorDeviationY)) {
        // 前方无墙：设置朝向 + 方向
        float YAngle = PhysicsBlock::EdgeVecToCosAngleFloat(qianjinfang);
        mNPC->GetMovement()->SetLookAngle(YAngle);
        mNPC->GetMovement()->SetMoveInput(Vec2_{qianjinfang.x, qianjinfang.y});
        return gobot::Status::Running;
    } else {
        // 前方有墙：随机选新方向，返回 Success 触发转待机
        switch (rand() % 4) {
            case 0: qianjinfang = {1, 0};  break;
            case 1: qianjinfang = {-1, 0}; break;
            case 2: qianjinfang = {0, 1};  break;
            case 3: qianjinfang = {0, -1}; break;
        }
        mTime = 0;
        return gobot::Status::Success;
    }
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
        return gobot::Status::Success;
    }

    // 完全丢失目标 → Failure（触发目标重评估 → 巡逻）
    if (!(flags & SensoryMessages_Visible) && !mSuspicious) {
        return gobot::Status::Failure;
    }

    // 寻路中，等待
    if (!mJPS->GetPathfindingCompleted()) {
        return gobot::Status::Running;
    }

    glm::vec2 pos = mNPC->GetObjectCollision()->pos;

    // 判断是否需要重新寻路
    bool needRepath = LPath.empty() || (mTime > mPathfindingCycle);

    if (needRepath) {
        JPSVec2 target;
        if (mSuspicious) {
            // 前往最后目击位置
            target = mSuspiciousPos;
            mSuspicious = false;
        } else if (flags & SensoryMessages_Visible) {
            // 仍能看到玩家 → 追玩家
            target = {(int)Global::GamePlayerX, (int)Global::GamePlayerY};
        } else {
            // 完全丢失 → Failure
            return gobot::Status::Failure;
        }

        // 距离过远时截断到寻路范围内
        glm::vec2 toTarget = glm::vec2{target.x - pos.x, target.y - pos.y};
        while (fabs(toTarget.x) > mRange || fabs(toTarget.y) > mRange) {
            toTarget *= 0.5f;
        }
        JPSVec2 clampedTarget = {(int)(pos.x + toTarget.x), (int)(pos.y + toTarget.y)};

        LPath.clear();
        TOOL::mThreadPool->enqueue(&JPS::FindPath, mJPS,
            JPSVec2{(int)pos.x, (int)pos.y}, clampedTarget, &LPath,
            JPSVec2{wPathfinding->PathfindingDecoratorDeviationX,
                    wPathfinding->PathfindingDecoratorDeviationY});
        hsuldad = 0;
        mTime = 0;
        return gobot::Status::Running;
    }

    // === 沿路径前进 ===
    if (LPath.empty()) return gobot::Status::Running;

    JPSVec2 yiPOS = LPath.back();
    glm::vec2 toWaypoint = glm::vec2{(float)(yiPOS.x) - pos.x, (float)(yiPOS.y) - pos.y};
    float distToWaypoint = glm::length(toWaypoint);

    // 到达当前路径点 → 移除并取下一个
    if (distToWaypoint < 9.0f) {
        LPath.pop_back();
        if (LPath.empty()) return gobot::Status::Running;
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

// --- 攻击（对应原 FSM Attack 状态的执行逻辑） ---
gobot::Status NPCGOBT::DoAttack(gobot::Context& ctx)
{
    if (mNPC->GetMovement()->GetMode() != MovementMode::Ragdoll) {
        mNPC->GetMovement()->SetMode(MovementMode::Controlled);
    }

    int flags = GetSensoryMessages();

    // 失去目标 → Failure（触发重新分解 → 追击或巡逻）
    if (!((flags & SensoryMessages_Range) && (flags & SensoryMessages_Visible))) {
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
    } else if (dist > idealDist + 10.0f) {
        mNPC->GetMovement()->SetMoveInput(Vec2_{moveDir.x, moveDir.y});
    }

    // 射击（带冷却 + 瞄准误差）
    mShootCooldown -= FPSTime;
    if (mShootCooldown <= 0.0f) {
        mShootCooldown = mShootInterval;
        glm::vec2 shootPos = pos + PhysicsBlock::vec2angle(
            glm::vec2{9.0f, 0.0f}, mNPC->GetObjectCollision()->angle);
        float aimError = ((rand() % 100) / 100.0f - 0.5f) * 0.15f;
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
    }

    // 进入硬直：冻结移动，但仍可转向朝向玩家
    mNPC->GetMovement()->SetMode(MovementMode::Frozen);
    mNPC->GetMovement()->SetLookAngle(wanjiaAngle);

    // 超时恢复（1.0s 硬直后恢复行动）
    if (mTime > 1.0f) {
        injuryEntered_ = false; // 重置标志，为下次受伤准备
        return gobot::Status::Success;
    }
    return gobot::Status::Running;
}

// ============================================================================
// 对外接口
// ============================================================================

void NPCGOBT::SetNPC(int x, int y, float angle)
{
    mNPC->GetObjectCollision()->pos = Vec2_{static_cast<FLOAT_>(x), static_cast<FLOAT_>(y)};
    mNPC->GetObjectCollision()->angle = angle;
    mNPC->setGamePlayerMatrix(3, true);
}

void NPCGOBT::Event(int Frame, float time)
{
    mTime += time;
    FPSTime = time;

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
