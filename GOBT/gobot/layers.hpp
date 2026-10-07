#pragma once

// gobot 三层决策层：战略（目标管理）/ 战术（子目标分解）/ 操作（原子动作）
// 依赖顺序自上而下，不可打乱：GoalManager → 分解器 → 子树库 → 规划器 → 各层节点。

#include "gobot/bt.hpp"

#include <algorithm>
#include <any>
#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace gobot {

// ---------------------------------------------------------------------------
// 战略层：目标管理
// ---------------------------------------------------------------------------
class GoalManager {
public:
    // 注册即参与调度：按 priority 降序排列，队首优先
    void add_goal(GoalPtr goal) {
        goals_.push_back(std::move(goal));
        sort_by_priority();
    }

    void remove_goal(const std::string& name) {
        goals_.erase(std::remove_if(goals_.begin(), goals_.end(),
                         [&name](const GoalPtr& g) { return g->name() == name; }),
            goals_.end());
        suspended_.erase(name);
    }

    // 挂起：暂停调度该目标 ticks 个 tick；ticks 至少为 1，避免"挂起即立刻恢复"
    void suspend(const std::string& name, int ticks) {
        suspended_[name] = (std::max)(ticks, 1);
    }

    void unsuspend(const std::string& name) { suspended_.erase(name); }

    bool is_suspended(const std::string& name) const { return suspended_.count(name) != 0; }

    // 选出当前最优先、且尚未满足的目标；全部满足/挂起时返回 nullptr
    GoalPtr select_top(const WorldState& ws) {
        tick_suspensions();
        for (const auto& goal : goals_) {
            if (is_suspended(goal->name())) continue;
            if (goal->satisfied_by(ws)) continue;
            return goal;
        }
        return nullptr;
    }

    // 动态调整优先级（失败降级用），调整后重新排序
    void adjust_priority(const std::string& name, int new_priority) {
        for (auto& goal : goals_) {
            if (goal->name() == name) {
                goal->set_priority(new_priority);
                break;
            }
        }
        sort_by_priority();
    }

    // 目标完成后恢复全部目标到注册时优先级
    void restore_all_priorities() {
        for (auto& goal : goals_) goal->restore_priority();
        sort_by_priority();
    }

    const std::vector<GoalPtr>& goals() const { return goals_; }

private:
    void sort_by_priority() {
        std::sort(goals_.begin(), goals_.end(),
            [](const GoalPtr& a, const GoalPtr& b) { return a->priority() > b->priority(); });
    }

    void tick_suspensions() {
        for (auto it = suspended_.begin(); it != suspended_.end();) {
            if (--it->second <= 0) it = suspended_.erase(it);
            else ++it;
        }
    }

    std::vector<GoalPtr> goals_;
    std::unordered_map<std::string, int> suspended_;
};

// ---------------------------------------------------------------------------
// 战术层：子目标分解
// ---------------------------------------------------------------------------
class TacticalDecomposer {
public:
    using DecomposeFn = std::function<std::vector<SubgoalPtr>(const Goal&)>;

    void register_strategy(const std::string& goal_name, DecomposeFn fn) {
        strategies_[goal_name] = std::move(fn);
    }

    // 未注册策略的目标退化为单个 Custom 子目标（直接以目标状态为达成条件）
    std::vector<SubgoalPtr> decompose(const Goal& goal) const {
        auto it = strategies_.find(goal.name());
        if (it == strategies_.end()) {
            return { std::make_shared<Subgoal>(SubgoalType::Custom, goal.name(), goal.target_state()) };
        }
        return it->second(goal);
    }

private:
    std::unordered_map<std::string, DecomposeFn> strategies_;
};

// ---------------------------------------------------------------------------
// 战术层：子树库
// ---------------------------------------------------------------------------
class SubtreeLibrary {
public:
    using SubtreeFactory = std::function<BTNodePtr()>;

    void register_subtree(SubgoalType type, SubtreeFactory factory) {
        factories_[type] = std::move(factory);
    }

    // 每次调用都新建子树实例，保证不同子目标之间不共享运行状态
    BTNodePtr create_subtree(SubgoalType type) const {
        auto it = factories_.find(type);
        if (it == factories_.end()) return nullptr;
        return it->second();
    }

    bool has_subtree(SubgoalType type) const { return factories_.count(type) != 0; }

private:
    struct SubgoalTypeHash {
        std::size_t operator()(SubgoalType t) const noexcept {
            return static_cast<std::size_t>(t);
        }
    };

    std::unordered_map<SubgoalType, SubtreeFactory, SubgoalTypeHash> factories_;
};

// ---------------------------------------------------------------------------
// 战略层：目标规划器（失败反馈 → 挂起 / 降级）
// ---------------------------------------------------------------------------
class StrategicPlanner {
public:
    // 连续失败阈值：达到后触发一次战略调整
    static constexpr int kMaxConsecutiveFailures = 3;
    // 挂起时长（tick）：约 3 秒，给环境自愈留出窗口
    static constexpr int kSuspensionTicks = 180;
    // 不可挂起目标的降级步长与累计上限
    static constexpr int kPriorityDowngradeStep = 5;
    static constexpr int kMaxDowngrade = 25;

    StrategicPlanner(std::shared_ptr<GoalManager> gm, EventBusPtr bus)
        : goal_manager_(std::move(gm)), event_bus_(std::move(bus)) {
        if (event_bus_) {
            sub_id_ = event_bus_->subscribe(events::kTacticalFailure,
                [this](const std::any& payload) { on_tactical_failure(payload); });
            completed_sub_id_ = event_bus_->subscribe(events::kGoalCompleted,
                [this](const std::any&) { goal_manager_->restore_all_priorities(); });
        }
    }

    ~StrategicPlanner() {
        if (event_bus_) {
            event_bus_->unsubscribe(sub_id_);
            event_bus_->unsubscribe(completed_sub_id_);
        }
    }

    GoalPtr select_goal(const WorldState& ws) { return goal_manager_->select_top(ws); }

    void set_current_goal(GoalPtr goal) {
        current_goal_ = std::move(goal);
        failure_count_ = 0;
    }

    void reset_failure_count() { failure_count_ = 0; }

    const GoalPtr& current_goal() const { return current_goal_; }

private:
    // 累计失败达阈值后的战略调整：
    // 可挂起目标 → 暂时雪藏；生存等不可挂起目标 → 降级后继续参与竞争
    void on_tactical_failure(const std::any& payload) {
        // 只处理子目标级失败通知；负载类型不符时忽略
        if (!std::any_cast<SubgoalPtr>(&payload)) return;

        ++failure_count_;
        if (failure_count_ < kMaxConsecutiveFailures) return;
        failure_count_ = 0;

        if (!current_goal_) return;

        if (current_goal_->suspendible()) {
            goal_manager_->suspend(current_goal_->name(), kSuspensionTicks);
            return;
        }

        const int floor = current_goal_->base_priority() - kMaxDowngrade;
        const int next = (std::max)(current_goal_->priority() - kPriorityDowngradeStep, floor);
        goal_manager_->adjust_priority(current_goal_->name(), next);
    }

    std::shared_ptr<GoalManager> goal_manager_;
    EventBusPtr event_bus_;
    SubscriptionId sub_id_ = 0;
    SubscriptionId completed_sub_id_ = 0;
    GoalPtr current_goal_;
    int failure_count_ = 0;
};

// ---------------------------------------------------------------------------
// 战术层：子树引用节点
// ---------------------------------------------------------------------------
// 按当前子目标类型从子树库实例化子树并驱动执行；负责子目标完成/失败的收尾。
class SubtreeReferenceNode : public LeafNode {
public:
    // 子树包装器：为每次实例化的子树统一套壳（如 LayerBridgeNode）
    using SubtreeWrapper = std::function<BTNodePtr(BTNodePtr)>;

    SubtreeReferenceNode(std::shared_ptr<SubtreeLibrary> library, EventBusPtr bus,
        std::string name = "SubtreeReferenceNode")
        : LeafNode(std::move(name)), library_(std::move(library)), event_bus_(std::move(bus)) {}

    void set_subtree_wrapper(SubtreeWrapper wrapper) { wrapper_ = std::move(wrapper); }

    Status tick(Context& ctx) override {
        auto& bb = ctx.blackboard();
        auto subgoal = bb.current_subgoal();
        if (!subgoal) return Status::Success;

        if (!current_subtree_ || last_subgoal_ != subgoal) {
            BTNodePtr raw = library_->create_subtree(subgoal->type());
            if (!raw) {
                // 无对应子树：必须上报并弹出子目标。
                // 只上报不弹出会导致每 tick 重复上报、子目标滞留、失败计数无限累积。
                return end_subgoal(bb, subgoal, events::kTacticalFailure, Status::Failure);
            }
            current_subtree_ = wrapper_ ? wrapper_(raw) : raw;
            last_subgoal_ = subgoal;
            // 新建子树是干净的，重试计数从头算
            subgoal->reset_retry();
        }

        const Status status = current_subtree_->tick(ctx);

        if (status == Status::Success) {
            return end_subgoal(bb, subgoal, events::kSubgoalCompleted, Status::Success);
        }
        if (status == Status::Failure) {
            subgoal->increment_retry();
            if (!subgoal->can_retry()) {
                return end_subgoal(bb, subgoal, events::kTacticalFailure, Status::Failure);
            }
            // 还有重试机会：重置子树后转为 Running，下个 tick 从头重试
            if (current_subtree_) current_subtree_->reset();
            return Status::Running;
        }
        return status;
    }

    void reset() override {
        current_subtree_.reset();
        last_subgoal_.reset();
    }

private:
    // 子目标收尾：上报事件 → 出队 → 丢弃子树实例（三处逻辑完全一致）
    Status end_subgoal(SharedBlackboard& bb, const SubgoalPtr& subgoal,
        const char* event_type, Status result) {
        event_bus_->publish(event_type, subgoal);
        bb.pop_subgoal();
        current_subtree_.reset();
        last_subgoal_.reset();
        return result;
    }

    std::shared_ptr<SubtreeLibrary> library_;
    EventBusPtr event_bus_;
    SubtreeWrapper wrapper_;
    BTNodePtr current_subtree_;
    SubgoalPtr last_subgoal_;
};

// ---------------------------------------------------------------------------
// 战术层：子目标驱动节点
// ---------------------------------------------------------------------------
// 子目标链的中间层：把控制权交给首个（也是唯一的）子树引用子节点
class TacticalSubgoalNode : public CompositeNode {
public:
    explicit TacticalSubgoalNode(std::string name = "TacticalSubgoalNode")
        : CompositeNode(std::move(name)) {}

    Status tick(Context& ctx) override {
        if (!ctx.blackboard().current_subgoal()) return Status::Success;
        if (children_.empty()) return Status::Failure;
        return children_.front()->tick(ctx);
    }
};

// ---------------------------------------------------------------------------
// 战略层：目标驱动节点
// ---------------------------------------------------------------------------
// 目标级节点：挑目标 → 分解子目标 → 驱动战术子树 → 判定达成并广播
class StrategicGoalNode : public CompositeNode {
public:
    StrategicGoalNode(std::shared_ptr<StrategicPlanner> planner,
        std::string name = "StrategicGoalNode")
        : CompositeNode(std::move(name)), planner_(std::move(planner)) {}

    Status tick(Context& ctx) override {
        auto& bb = ctx.blackboard();
        GoalPtr goal = planner_->select_goal(*bb.world_state());

        if (!goal) {
            if (bb.current_goal()) {
                bb.set_current_goal(nullptr);
                bb.clear_subgoals();
                goal_completed_published_ = false;
            }
            return Status::Success;
        }

        // 目标切换：清空旧子目标，重新分解
        if (bb.current_goal() != goal) {
            bb.set_current_goal(goal);
            bb.clear_subgoals();
            planner_->set_current_goal(goal);
            goal_completed_published_ = false;
            push_subgoals(ctx, goal);
        }

        if (children_.empty()) return Status::Failure;

        const Status status = children_.front()->tick(ctx);
        if (status == Status::Success) {
            if (!goal->satisfied_by(*bb.world_state())) {
                // 子目标全部完成但目标未达成：重新分解继续推进
                if (!bb.current_subgoal()) push_subgoals(ctx, goal);
                return Status::Running;
            }
            if (!goal_completed_published_) {
                ctx.event_bus().publish(events::kGoalCompleted, goal);
                goal_completed_published_ = true;
            }
        }
        return status;
    }

    void reset() override {
        goal_completed_published_ = false;
        CompositeNode::reset();
    }

private:
    void push_subgoals(Context& ctx, const GoalPtr& goal) {
        auto& bb = ctx.blackboard();
        for (auto& subgoal : ctx.decomposer().decompose(*goal)) {
            bb.push_subgoal(std::move(subgoal));
        }
    }

    std::shared_ptr<StrategicPlanner> planner_;
    bool goal_completed_published_ = false;
};

// ---------------------------------------------------------------------------
// 操作层：原子动作执行
// ---------------------------------------------------------------------------
// 前置条件不满足直接失败；执行成功才写入效果
class OperationalActionNode : public LeafNode {
public:
    explicit OperationalActionNode(ActionPtr action, std::string name = "")
        : LeafNode(name.empty() ? action->name() : std::move(name)), action_(std::move(action)) {}

    Status tick(Context& ctx) override {
        WorldState& ws = *ctx.blackboard().world_state();
        if (!action_->precond_met(ws)) return Status::Failure;

        const Status status = action_->execute(ctx);
        if (status == Status::Success) ws.apply(action_->effects());
        return status;
    }

    const ActionPtr& action() const { return action_; }

private:
    ActionPtr action_;
};

// ---------------------------------------------------------------------------
// 跨层桥接
// ---------------------------------------------------------------------------
// 战略中断拦截：收到 kStrategicInterrupt 时重置子树并让本 tick 保持 Running，
// 使下个 tick 从干净状态重新规划，避免带着上个目标的中间状态继续执行。
class LayerBridgeNode : public DecoratorNode {
public:
    LayerBridgeNode(BTNodePtr child, EventBusPtr bus, std::string name = "LayerBridgeNode")
        : DecoratorNode(std::move(child), std::move(name)), event_bus_(std::move(bus)) {
        if (event_bus_) {
            subscription_id_ = event_bus_->subscribe(events::kStrategicInterrupt,
                [this](const std::any&) { this->interrupted_ = true; });
        }
    }

    ~LayerBridgeNode() override {
        if (event_bus_) event_bus_->unsubscribe(subscription_id_);
    }

    Status tick(Context& ctx) override {
        if (interrupted_) {
            interrupted_ = false;
            if (child_) child_->reset();
            return Status::Running;
        }
        return child_ ? child_->tick(ctx) : Status::Failure;
    }

private:
    EventBusPtr event_bus_;
    SubscriptionId subscription_id_ = 0;
    bool interrupted_ = false;
};

} // namespace gobot
