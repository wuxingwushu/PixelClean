#pragma once

#include "gobot/bt/DecoratorNode.hpp"
#include "gobot/infra/SharedBlackboard.hpp"

namespace gobot {

// 目标切换装饰器：检测 current_goal 变化时重置子树运行状态
// 用于需要目标切换时清理中间状态的子树
class GoalSwitchDecorator : public DecoratorNode {
public:
    GoalSwitchDecorator(BTNodePtr child,
                        std::string name = "GoalSwitchDecorator")
        : DecoratorNode(std::move(child), std::move(name)) {}

    Status tick(Context& ctx) override {
        auto& bb = ctx.blackboard();
        GoalPtr current = bb.current_goal();

        if (last_goal_ != current) {
            // 目标已切换：重置子树
            if (child_) child_->reset();
            last_goal_ = current;
        }

        return child_ ? child_->tick(ctx) : Status::Failure;
    }

    void reset() override {
        last_goal_.reset();
        DecoratorNode::reset();
    }

private:
    GoalPtr last_goal_;
};

} // namespace gobot
