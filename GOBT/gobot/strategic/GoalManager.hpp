#pragma once

#include "gobot/core/Goal.hpp"
#include "gobot/core/WorldState.hpp"
#include <vector>
#include <algorithm>
#include <string>
#include <unordered_map>

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
        suspended_.erase(name);
    }

    // === 目标挂起机制（替代永久降级） ===
    // 失败后挂起目标 ticks 个选择周期（≈帧数），冷却结束自动恢复参与选择。
    // 相比"降级 + 封底"，挂起保证其他目标（如巡逻 fallback）真正获得机会，
    // 且目标不会被永久雪藏。
    void suspend(const std::string& name, int ticks) {
        suspended_[name] = (std::max)(ticks, 1);
    }

    void unsuspend(const std::string& name) {
        suspended_.erase(name);
    }

    bool is_suspended(const std::string& name) const {
        return suspended_.count(name) != 0;
    }

    // 选择最高优先级且未满足、未挂起的目标。
    // 每次调用推进挂起计时（一次选择 ≈ 一帧）。
    GoalPtr select_top(const WorldState& ws) {
        tick_suspensions();
        for (const auto& goal : goals_) {
            if (is_suspended(goal->name())) continue;
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

    // 恢复所有目标到注册时的基础优先级
    // （失败降级应是暂时的：局势变化后必须恢复，否则目标会被永久雪藏）
    void restore_all_priorities() {
        for (const auto& goal : goals_) {
            goal->restore_priority();
        }
        std::sort(goals_.begin(), goals_.end(),
            [](const GoalPtr& a, const GoalPtr& b) {
                return a->priority() > b->priority();
            });
    }

    const std::vector<GoalPtr>& goals() const { return goals_; }

private:
    void tick_suspensions() {
        for (auto it = suspended_.begin(); it != suspended_.end();) {
            if (--it->second <= 0) {
                it = suspended_.erase(it);
            } else {
                ++it;
            }
        }
    }

    std::vector<GoalPtr> goals_;
    // 目标名 → 剩余挂起周期数
    std::unordered_map<std::string, int> suspended_;
};

} // namespace gobot
