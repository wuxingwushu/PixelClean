#pragma once

// GOBT 框架头文件（统一入口）
#include "../GOBT/gobot/gobot.hpp"

// 游戏引擎头文件（与 NPC.h 保持一致）
#include "GamePlayer.h"
#include "../GameMods/PathfindingDecorator.h"
#include "../Tool/ContinuousData.h"
#include "../Tool/JPS.h"
#include "../Arms/Arms.h"

#include <random>

namespace GAME {

// 感官消息标志位（从 NPC.h 迁移）
enum SensoryMessagesFlags_
{
    SensoryMessages_None       = 0,
    SensoryMessages_VisualField = 1 << 0, // 在视野范围内
    SensoryMessages_Visible     = 1 << 1, // 可见（无遮挡）
};

// NPC 世界状态键名常量
// ★ 写入方约定（决定动作 Effect 是否生效）：
//   - SyncWorldState() 每帧强制写入：kPlayerVisible / kPlayerInRange / kPlayerEngaged；
//   - SyncWorldState() 只在边沿写入（新伤害 / 计时耗尽）：kRecentlyHurt；
//   - 动作侧拥有：kInjuryRecovered（Recover 成功）、kRecentlyHurt=false（Retreat 成功）。
//   对"每帧强制写入"的键写 Effect 等于没写（下一帧必被覆盖）。
namespace NPCWS {
    inline const std::string kPlayerVisible     = "player_visible";
    inline const std::string kPlayerInRange     = "player_in_range";
    inline const std::string kInjuryRecovered   = "injury_recovered";
    inline const std::string kPlayerEngaged     = "player_engaged";  // 仇恨锁存（交战状态）
    inline const std::string kRecentlyHurt      = "recently_hurt";   // 未处理的受伤威胁（驱动自保）
    inline const std::string kPatrolDone        = "patrol_done";     // 永不设置：PatrolArea 是无限兜底
}

class NPCGOBT
{
public:
    NPCGOBT(GamePlayer* npc, PathfindingDecorator* pathfinding, Arms* arms);
    ~NPCGOBT();

    // 主事件循环（与 NPC::Event 接口一致，可直接替换）
    void Event(int Frame, float time);

    // === 兼容 NPC 的对外接口 ===
    [[nodiscard]] VkCommandBuffer getCommandBuffer(int i) {
        return mNPC->getCommandBuffer(i);
    }
    GamePlayer* GetGamePlayer() { return mNPC; }
    bool GetDeathInBattle() { return mNPC->GetDeathInBattle(); }
    void InitCommandBuffer() { mNPC->InitCommandBuffer(); }
    evutil_socket_t GetKey() { return mNPC->GetKey(); }
    void SetNPC(int x, int y, float angle);

private:
    // === 巡逻状态机 ===
    enum PatrolState {
        PATROL_IDLE = 0,        // 空闲，需要选取目标
        PATROL_PATHFINDING,     // JPS 正在计算路径
        PATROL_MOVING,          // 沿路径移动中
        PATROL_FALLBACK,        // JPS 异常滞留/失败 → 方向碰撞巡逻（不依赖寻路结果）
    };

    PatrolState mPatrolState = PATROL_IDLE;
    JPSVec2 mPatrolTarget{};        // 当前巡逻目标点
    int mPatrolFailedCount = 0;     // 连续寻路失败次数
    bool mPatrolEntered = false;    // 是否已进入巡逻状态（用于重置状态机）

    // === 游戏对象引用 ===
    GamePlayer* mNPC = nullptr;
    PathfindingDecorator* wPathfinding = nullptr;
    Arms* wArms = nullptr;
    JPS* mJPS = nullptr;

    // === GOBT 组件 ===
    gobot::BlackboardPtr mBlackboard;
    gobot::EventBusPtr mEventBus;
    std::shared_ptr<gobot::GoalManager> mGoalManager;
    std::shared_ptr<gobot::StrategicPlanner> mPlanner;
    std::shared_ptr<gobot::TacticalDecomposer> mDecomposer;
    std::shared_ptr<gobot::SubtreeLibrary> mSubtreeLib;
    std::shared_ptr<gobot::Context> mContext;
    gobot::BTNodePtr mRoot;
    std::unique_ptr<gobot::BTExecutor> mExecutor;

    // === 游戏状态（从 NPC 迁移） ===
    glm::vec2 qianjinfang = {1, 0};   // 前进方向
    int AttackRange = 90;              // 攻击范围
    int ChaseRange = 400;              // 感知/交战尺度（脱战距离上限 = 此值×1.5；地图 644 宽）
    int mRange = 300;                  // 寻路范围
    float FPSTime = 0;                 // 帧时间
    float mRepathTimer = 0.0f;         // 追击重寻路周期计时（仅 DoChase 使用）
    float mInjuryTimer = 0.0f;         // 受伤硬直计时（仅 DoInjury 使用）
    float mPathfindWait = 0.0f;        // 已等待 JPS 任务返回的时长（滞留超时兜底）
    const float mPathfindingCycle = 1.5f; // 寻路最小周期
    std::vector<JPSVec2> LPath;        // 寻路路径
    float wanjiaAngle = 0.0f;          // NPC到玩家的角度（仅"可见"帧有效，禁止用于隔墙瞄准）
    bool mSuspicious = false;          // 是否有可用的最后目击点
    JPSVec2 mSuspiciousPos{};          // 最后目击位置（仅可见时刷新）
    float mPlayerDistance = 0.0f;      // NPC到玩家距离（原始感官值：仅用于范围滞回/脱战判定）
    float mShootCooldown = 0.0f;       // 射击冷却
    const float mShootInterval = 0.8f; // 射击间隔
    bool injuryEntered_ = false;       // 是否已进入受伤状态（用于重置计时器）
    bool mStandbyEntered = false;      // 是否已进入待机状态（用于重置待机计时器）
    float mStandbyTimer = 0.0f;        // 待机独立计时器（不受其他动作重置计时器影响）
    bool mJpsSubmitted = false;        // 是否已提交JPS寻路（用于检测空路径导致死循环）
    std::mt19937 mRng{std::random_device{}()}; // 本实例独立随机源（取代未播种的 rand()）

    // === 感知缓存（每帧一次感官采样，动作执行器共享，避免重复射线） ===
    long long mFrameCounter = 0;       // 帧计数
    long long mSensoryFrameStamp = -1; // 缓存的感官结果所属帧
    int mSensoryCachedFlags = 0;       // 缓存的感官标志

    // === 交战状态（滞回/防抖） ===
    bool mEngaged = false;             // 已进入交战（仇恨锁存）
    float mPlayerLostTime = 0.0f;      // 连续丢失视野时长
    bool mVisibleLatched = false;      // 可见性锁存（宽限 0.25s）
    float mVisibleLostTime = 0.0f;     // 可见性宽限计时
    bool mInRangeLatched = false;      // 攻击范围锁存（<90 进入，>115 退出）

    // === 可疑位置记忆（衰减） ===
    float mSuspiciousTimer = 0.0f;     // 可疑记忆剩余时间

    // === 最后目击记录（仅"看得见"时刷新；看不见时用它瞄准/转向，绝不读穿墙真值） ===
    glm::vec2 mLastKnownPlayerPos{0.0f, 0.0f}; // 最后目击到的玩家世界坐标
    float mLastKnownPlayerAngle = 0.0f;        // NPC→最后目击位置的角度
    bool mLastKnownValid = false;              // 是否已有有效记录

    // === 追击搜索（最后目击位置调查） ===
    bool mInvestigating = false;       // 正前往最后目击位置
    bool mSearching = false;           // 正在最后目击位置搜索
    float mSearchTimer = 0.0f;         // 搜索计时
    int mOrbitDir = 1;                 // 绕圈方向（撞墙才换向，防逐帧抖动）

    // === 受伤威胁（SelfPreserve 目标） ===
    float mRecentlyHurtTimer = 0.0f;   // 最近受伤计时

    // === 攻击走位 ===
    float mStrafeTimer = 0.0f;         // 侧移计时
    int mStrafeDir = 1;                // 侧移方向

    // === 后撤（Retreat 子目标） ===
    bool mFleeEntered = false;         // 已进入后撤
    float mFleeTimer = 0.0f;           // 后撤计时

    // === 感官系统 ===
    int GetSensoryMessages();

    // === 世界状态同步 ===
    void SyncWorldState();

    // === GOBT 初始化 ===
    void SetupGoals();
    void SetupDecomposer();
    void SetupSubtreeLibrary();
    void BuildTree();

    // === 辅助方法 ===
    JPSVec2 FindRandomWalkablePosition(const glm::vec2& currentPos);
    gobot::Status DoPatrolFallback(const glm::vec2& pos);
    void ClearPathSafe();   // 无 JPS 任务在途时清空路径（避免与线程池写竞争）
    void EnsureControlled();// 恢复受控移动模式（击飞态由 MovementComponent 自行结束）
    // 随机数工具：统一走本实例的 mRng（取代未播种的全局 rand()）
    int   RandInt(int lo, int hi) { return std::uniform_int_distribution<int>(lo, hi)(mRng); }
    float Rand01() { return std::uniform_real_distribution<float>(0.0f, 1.0f)(mRng); }

    // === 动作执行器（对应原 FSM 各状态的行为逻辑） ===
    gobot::Status DoStandby(gobot::Context& ctx);
    gobot::Status DoPatrol(gobot::Context& ctx);
    gobot::Status DoChase(gobot::Context& ctx);
    gobot::Status DoAttack(gobot::Context& ctx);
    gobot::Status DoInjury(gobot::Context& ctx);
    gobot::Status DoFlee(gobot::Context& ctx);
};

} // namespace GAME
