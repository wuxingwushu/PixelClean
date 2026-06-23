#pragma once

#include "gobot/core/Types.hpp"
#include "gobot/core/Goal.hpp"
#include "gobot/core/WorldState.hpp"
#include <deque>
#include <optional>

namespace gobot {

class SharedBlackboard {
public:
    SharedBlackboard()
        : world_state_(std::make_shared<WorldState>()) {}

    // 世界状态
    WorldStatePtr world_state() const { return world_state_; }
    void set_world_state(WorldStatePtr ws) { world_state_ = std::move(ws); }

    // 当前战略目标
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

    // 子目标队列（战术层管理）
    const std::deque<SubgoalPtr>& subgoal_queue() const { return subgoal_queue_; }
    void set_subgoal_queue(std::deque<SubgoalPtr> q) {
        subgoal_queue_ = std::move(q);
    }
    void push_subgoal(SubgoalPtr sg) { subgoal_queue_.push_back(std::move(sg)); }
    SubgoalPtr current_subgoal() const {
        if (subgoal_queue_.empty()) return nullptr;
        return subgoal_queue_.front();
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

} // namespace gobot
