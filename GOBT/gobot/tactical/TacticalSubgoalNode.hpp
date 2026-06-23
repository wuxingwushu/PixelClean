#pragma once

#include "gobot/bt/CompositeNode.hpp"

namespace gobot {

class TacticalSubgoalNode : public CompositeNode {
public:
    explicit TacticalSubgoalNode(std::string name = "TacticalSubgoalNode")
        : CompositeNode(std::move(name)) {}

    Status tick(Context& ctx) override {
        auto& bb = ctx.blackboard();
        if (!bb.current_subgoal()) {
            // 无子目标：所有子目标完成
            return Status::Success;
        }
        if (children_.empty()) {
            return Status::Failure;
        }
        return children_.front()->tick(ctx);
    }
};

} // namespace gobot
