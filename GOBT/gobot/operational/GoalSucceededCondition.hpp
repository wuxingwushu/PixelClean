#pragma once

#include "gobot/bt/LeafNode.hpp"
#include "gobot/core/Goal.hpp"

namespace gobot {

// 目标达成检查节点：检查当前战略目标是否已被世界状态满足
class GoalSucceededCondition : public LeafNode {
public:
    explicit GoalSucceededCondition(std::string name = "GoalSucceeded")
        : LeafNode(std::move(name)) {}

    Status tick(Context& ctx) override {
        auto& bb = ctx.blackboard();
        GoalPtr goal = bb.current_goal();
        if (!goal) {
            return Status::Success; // 无目标视为已满足
        }
        return goal->satisfied_by(*bb.world_state())
            ? Status::Success : Status::Failure;
    }
};

} // namespace gobot
