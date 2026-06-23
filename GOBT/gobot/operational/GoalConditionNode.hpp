#pragma once

#include "gobot/bt/LeafNode.hpp"
#include "gobot/core/Goal.hpp"

namespace gobot {

// 目标身份检查节点：检查当前目标是否为指定目标
// 用于在子树中做目标匹配守卫
class GoalConditionNode : public LeafNode {
public:
    explicit GoalConditionNode(std::string expected_goal_name,
                              std::string name = "GoalCondition")
        : LeafNode(std::move(name))
        , expected_(std::move(expected_goal_name)) {}

    Status tick(Context& ctx) override {
        auto& bb = ctx.blackboard();
        GoalPtr goal = bb.current_goal();
        if (!goal) return Status::Failure;
        return goal->name() == expected_ ? Status::Success : Status::Failure;
    }

    const std::string& expected_goal() const { return expected_; }

private:
    std::string expected_;
};

} // namespace gobot
