#pragma once

#include "gobot/bt/LeafNode.hpp"
#include "gobot/core/Action.hpp"

namespace gobot {

class OperationalActionNode : public LeafNode {
public:
    OperationalActionNode(ActionPtr action,
                          std::string name = "")
        : LeafNode(name.empty() ? action->name() : std::move(name))
        , action_(std::move(action)) {}

    Status tick(Context& ctx) override {
        auto& ws = *ctx.blackboard().world_state();
        // 前提校验
        if (!action_->precond_met(ws)) {
            return Status::Failure;
        }
        // 执行动作
        Status status = action_->execute(ctx);
        if (status == Status::Success) {
            // 应用效果
            ws.apply(action_->effects());
        }
        return status;
    }

    const ActionPtr& action() const { return action_; }

private:
    ActionPtr action_;
};

} // namespace gobot
