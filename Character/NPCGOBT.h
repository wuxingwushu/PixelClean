#pragma once

// GOBT 框架头文件
#include "../GOBT/gobot/builder/TreeBuilder.hpp"
#include "../GOBT/gobot/executor/BTExecutor.hpp"
#include "../GOBT/gobot/bt/SequenceNode.hpp"
#include "../GOBT/gobot/operational/OperationalActionNode.hpp"
#include "../GOBT/gobot/operational/StatePreconditionNode.hpp"

// 游戏引擎头文件（与 NPC.h 保持一致）
#include "GamePlayer.h"
#include "../GameMods/PathfindingDecorator.h"
#include "../Tool/ContinuousData.h"
#include "../Tool/JPS.h"
#include "../Arms/Arms.h"

namespace GAME {

// 感官消息标志位（从 NPC.h 迁移，保持二进制兼容）
enum SensoryMessagesFlags_
{
    SensoryMessages_None       = 0,
    SensoryMessages_VisualField = 1 << 0, // 在视野范围内
    SensoryMessages_Visible     = 1 << 1, // 可见（无遮挡）
    SensoryMessages_Range       = 1 << 2, // 攻击范围内
};

// NPC 世界状态键名常量
namespace NPCWS {
    inline const std::string kInjured           = "injured";
    inline const std::string kPlayerVisible     = "player_visible";
    inline const std::string kPlayerInRange     = "player_in_range";
    inline const std::string kPlayerInViewField = "player_in_viewfield";
    inline const std::string kInjuryRecovered   = "injury_recovered";
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
    int ChaseRange = 250;              // 追击触发范围
    int mRange = 300;                  // 寻路范围
    float FPSTime = 0;                 // 帧时间
    float mTime = 0.0f;                // 计时器
    const float mPathfindingCycle = 1.5f; // 寻路最小周期
    std::vector<JPSVec2> LPath;        // 寻路路径
    float wanjiaAngle = 0.0f;          // NPC到玩家的角度
    bool mSuspicious = false;          // 是否有可疑位置
    JPSVec2 mSuspiciousPos{};          // 可疑位置
    float mPlayerDistance = 0.0f;      // NPC到玩家距离
    float mShootCooldown = 0.0f;       // 射击冷却
    const float mShootInterval = 0.8f; // 射击间隔
    int hsuldad = 0;
    int lastDamageCount_ = 0;          // 上次伤害队列长度（检测新伤害）
    bool injuryEntered_ = false;       // 是否已进入受伤状态（用于重置计时器）

    // === 感官系统 ===
    int GetSensoryMessages();

    // === 世界状态同步 ===
    void SyncWorldState();

    // === GOBT 初始化 ===
    void SetupGoals();
    void SetupDecomposer();
    void SetupSubtreeLibrary();
    void BuildTree();

    // === 动作执行器（对应原 FSM 各状态的行为逻辑） ===
    gobot::Status DoStandby(gobot::Context& ctx);
    gobot::Status DoPatrol(gobot::Context& ctx);
    gobot::Status DoChase(gobot::Context& ctx);
    gobot::Status DoAttack(gobot::Context& ctx);
    gobot::Status DoInjury(gobot::Context& ctx);
};

} // namespace GAME
