#pragma once

// gobot 基础设施层：事件总线、共享黑板、运行上下文
// Context 以引用暴露上层组件，因此对战略/战术类型只做前向声明（避免循环依赖）。

#include "gobot/core.hpp"

#include <algorithm>
#include <any>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace gobot {

// ---------------------------------------------------------------------------
// 事件总线
// ---------------------------------------------------------------------------

using EventType = std::string;
using SubscriptionId = std::uint64_t;
using EventCallback = std::function<void(const std::any&)>;

namespace events {
inline constexpr const char* kStrategicInterrupt = "STRATEGIC_INTERRUPT";
inline constexpr const char* kTacticalFailure    = "TACTICAL_FAILURE";
inline constexpr const char* kSubgoalCompleted   = "SUBGOAL_COMPLETED";
inline constexpr const char* kGoalCompleted      = "GOAL_COMPLETED";
} // namespace events

class EventBus {
public:
    // 延迟派发：publish 只入队，flush 时统一派发。
    // tick 中途触发回调会重入修改状态，故运行期固定开启。
    void set_deferred(bool enabled) { deferred_ = enabled; }

    SubscriptionId subscribe(const EventType& type, EventCallback cb) {
        std::lock_guard<std::mutex> lock(mutex_);
        SubscriptionId id = next_id_++;
        subscribers_[type].emplace_back(id, std::move(cb));
        return id;
    }

    void unsubscribe(SubscriptionId id) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& [type, list] : subscribers_) {
            list.erase(std::remove_if(list.begin(), list.end(),
                           [id](const auto& pair) { return pair.first == id; }),
                list.end());
        }
    }

    void publish(const EventType& type, std::any payload = {}) {
        if (!deferred_) {
            dispatch(type, payload);
            return;
        }
        std::lock_guard<std::mutex> lock(mutex_);
        pending_.emplace_back(type, std::move(payload));
    }

    // 派发全部延迟事件（在 tick 结束后调用）
    void flush() {
        std::vector<std::pair<EventType, std::any>> to_dispatch;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            to_dispatch.swap(pending_);
        }
        for (auto& [type, payload] : to_dispatch) dispatch(type, payload);
    }

private:
    // 锁外调用回调：回调内允许再次 subscribe/publish
    void dispatch(const EventType& type, const std::any& payload) {
        std::vector<EventCallback> to_call;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = subscribers_.find(type);
            if (it == subscribers_.end()) return;
            to_call.reserve(it->second.size());
            for (const auto& [id, cb] : it->second) to_call.push_back(cb);
        }
        for (const auto& cb : to_call) cb(payload);
    }

    bool deferred_ = false;
    std::mutex mutex_;
    SubscriptionId next_id_ = 0;
    std::unordered_map<EventType, std::vector<std::pair<SubscriptionId, EventCallback>>> subscribers_;
    std::vector<std::pair<EventType, std::any>> pending_;
};

using EventBusPtr = std::shared_ptr<EventBus>;

// ---------------------------------------------------------------------------
// 共享黑板：三层之间唯一的状态交汇点
// ---------------------------------------------------------------------------
class SharedBlackboard {
public:
    SharedBlackboard() : world_state_(std::make_shared<WorldState>()) {}

    WorldStatePtr world_state() const { return world_state_; }
    void set_world_state(WorldStatePtr ws) { world_state_ = std::move(ws); }

    // 当前战略目标。切换时记录前一个目标并置 goal_changed_ 供订阅方自行消费。
    const GoalPtr& current_goal() const { return current_goal_; }
    void set_current_goal(GoalPtr g) {
        if (current_goal_ != g) {
            previous_goal_ = current_goal_;
            current_goal_ = std::move(g);
            goal_changed_ = true;
        }
    }
    const GoalPtr& previous_goal() const { return previous_goal_; }
    bool goal_changed() const { return goal_changed_; }
    void acknowledge_goal_change() { goal_changed_ = false; }

    // 子目标队列（战术层管理）：队首即当前子目标
    const std::deque<SubgoalPtr>& subgoal_queue() const { return subgoal_queue_; }
    void set_subgoal_queue(std::deque<SubgoalPtr> q) { subgoal_queue_ = std::move(q); }
    void push_subgoal(SubgoalPtr sg) { subgoal_queue_.push_back(std::move(sg)); }
    SubgoalPtr current_subgoal() const {
        return subgoal_queue_.empty() ? nullptr : subgoal_queue_.front();
    }
    void pop_subgoal() {
        if (!subgoal_queue_.empty()) subgoal_queue_.pop_front();
    }
    void clear_subgoals() { subgoal_queue_.clear(); }

private:
    WorldStatePtr world_state_;
    GoalPtr current_goal_;
    GoalPtr previous_goal_;
    bool goal_changed_ = false;
    std::deque<SubgoalPtr> subgoal_queue_;
};

using BlackboardPtr = std::shared_ptr<SharedBlackboard>;

// ---------------------------------------------------------------------------
// 运行上下文：节点执行期间访问三层组件的唯一入口
// ---------------------------------------------------------------------------
class GoalManager;
class TacticalDecomposer;
class SubtreeLibrary;

class Context {
public:
    Context(BlackboardPtr bb,
            EventBusPtr bus,
            std::shared_ptr<GoalManager> gm,
            std::shared_ptr<TacticalDecomposer> decomposer,
            std::shared_ptr<SubtreeLibrary> subtree_lib)
        : blackboard_(std::move(bb))
        , event_bus_(std::move(bus))
        , goal_manager_(std::move(gm))
        , decomposer_(std::move(decomposer))
        , subtree_library_(std::move(subtree_lib)) {}

    SharedBlackboard& blackboard() { return *blackboard_; }
    EventBus& event_bus() { return *event_bus_; }
    GoalManager& goal_manager() { return *goal_manager_; }
    TacticalDecomposer& decomposer() { return *decomposer_; }
    SubtreeLibrary& subtree_library() { return *subtree_library_; }

    const SharedBlackboard& blackboard() const { return *blackboard_; }
    const EventBus& event_bus() const { return *event_bus_; }
    const GoalManager& goal_manager() const { return *goal_manager_; }
    const TacticalDecomposer& decomposer() const { return *decomposer_; }
    const SubtreeLibrary& subtree_library() const { return *subtree_library_; }

private:
    BlackboardPtr blackboard_;
    EventBusPtr event_bus_;
    std::shared_ptr<GoalManager> goal_manager_;
    std::shared_ptr<TacticalDecomposer> decomposer_;
    std::shared_ptr<SubtreeLibrary> subtree_library_;
};

} // namespace gobot
