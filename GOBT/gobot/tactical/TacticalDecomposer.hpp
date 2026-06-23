#pragma once

#include "gobot/core/Goal.hpp"
#include "gobot/core/WorldState.hpp"
#include <functional>
#include <unordered_map>
#include <vector>
#include <string>

namespace gobot {

// 分解函数签名：接收目标，返回子目标列表
using DecomposeFn = std::function<std::vector<SubgoalPtr>(const Goal&)>;

class TacticalDecomposer {
public:
    // 注册分解策略
    void register_strategy(const std::string& goal_name, DecomposeFn fn) {
        strategies_[goal_name] = std::move(fn);
    }

    // 分解目标
    std::vector<SubgoalPtr> decompose(const Goal& goal) const {
        auto it = strategies_.find(goal.name());
        if (it == strategies_.end()) {
            // 默认策略：目标本身作为单一子目标
            return { std::make_shared<Subgoal>(
                SubgoalType::Custom,
                goal.name(),
                goal.target_state()) };
        }
        return it->second(goal);
    }

private:
    std::unordered_map<std::string, DecomposeFn> strategies_;
};

} // namespace gobot
