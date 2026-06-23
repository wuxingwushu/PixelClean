#pragma once

#include "gobot/core/Goal.hpp"
#include "gobot/core/WorldState.hpp"
#include <vector>
#include <algorithm>
#include <string>

namespace gobot {

class GoalManager {
public:
    // 添加目标到池
    void add_goal(GoalPtr goal) {
        goals_.push_back(std::move(goal));
        // 保持按优先级降序
        std::sort(goals_.begin(), goals_.end(),
            [](const GoalPtr& a, const GoalPtr& b) {
                return a->priority() > b->priority();
            });
    }

    void remove_goal(const std::string& name) {
        goals_.erase(
            std::remove_if(goals_.begin(), goals_.end(),
                [&name](const GoalPtr& g) { return g->name() == name; }),
            goals_.end());
    }

    // 选择最高优先级且未满足的目标
    GoalPtr select_top(const WorldState& ws) const {
        for (const auto& goal : goals_) {
            if (!goal->satisfied_by(ws)) {
                return goal;
            }
        }
        return nullptr;
    }

    // 调整优先级（运行时动态调整）
    void adjust_priority(const std::string& name, int new_priority) {
        for (auto& goal : goals_) {
            if (goal->name() == name) {
                goal->set_priority(new_priority);
                break;
            }
        }
        std::sort(goals_.begin(), goals_.end(),
            [](const GoalPtr& a, const GoalPtr& b) {
                return a->priority() > b->priority();
            });
    }

    const std::vector<GoalPtr>& goals() const { return goals_; }

private:
    std::vector<GoalPtr> goals_;
};

} // namespace gobot
