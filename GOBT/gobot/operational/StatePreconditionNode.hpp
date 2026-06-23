#pragma once

#include "gobot/bt/LeafNode.hpp"
#include "gobot/core/WorldState.hpp"
#include <unordered_map>

namespace gobot {

// 状态前提检查节点：校验世界状态是否满足指定条件
// 全部条件满足返回 Success，任一不满足返回 Failure
class StatePreconditionNode : public LeafNode {
public:
    explicit StatePreconditionNode(
            std::unordered_map<WorldKey, WorldValue> condition,
            std::string name = "StatePrecondition")
        : LeafNode(std::move(name))
        , condition_(std::move(condition)) {}

    Status tick(Context& ctx) override {
        const auto& ws = *ctx.blackboard().world_state();
        return ws.matches(condition_) ? Status::Success : Status::Failure;
    }

    const std::unordered_map<WorldKey, WorldValue>& condition() const {
        return condition_;
    }

private:
    std::unordered_map<WorldKey, WorldValue> condition_;
};

} // namespace gobot
