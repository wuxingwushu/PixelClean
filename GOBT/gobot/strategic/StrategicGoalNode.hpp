#pragma once

#include "gobot/bt/CompositeNode.hpp"
#include "gobot/strategic/StrategicPlanner.hpp"
#include "gobot/tactical/TacticalDecomposer.hpp"
#include <memory>

namespace gobot {

class StrategicGoalNode : public CompositeNode {
public:
    StrategicGoalNode(std::shared_ptr<StrategicPlanner> planner,
                      std::string name = "StrategicGoalNode")
        : CompositeNode(std::move(name))
        , planner_(std::move(planner)) {}

    Status tick(Context& ctx) override {
        auto& bb = ctx.blackboard();
        // 通过 planner 选目标（planner 内部委托给 GoalManager）
        GoalPtr goal = planner_->select_goal(*bb.world_state());

        if (!goal) {
            // 无目标：清空当前目标，返回成功（空闲）
            if (bb.current_goal()) {
                bb.set_current_goal(nullptr);
                bb.clear_subgoals();
                goal_completed_published_ = false;
            }
            return Status::Success;
        }

        // 目标切换：重新分解，通知 planner，重置完成标志
        if (bb.current_goal() != goal) {
            bb.set_current_goal(goal);
            bb.clear_subgoals();
            planner_->set_current_goal(goal);
            goal_completed_published_ = false;
            auto subgoals = ctx.decomposer().decompose(*goal);
            for (auto& sg : subgoals) {
                bb.push_subgoal(std::move(sg));
            }
        }

        // 进入战术层（第一个子节点）
        if (children_.empty()) {
            return Status::Failure;
        }

        Status status = children_.front()->tick(ctx);

        if (status == Status::Success) {
            // 战术层成功：检查目标是否真正满足
            if (!goal->satisfied_by(*bb.world_state())) {
                // 目标未满足但子目标队列已空：重新分解（处理情况变化）
                if (!bb.current_subgoal()) {
                    auto subgoals = ctx.decomposer().decompose(*goal);
                    for (auto& sg : subgoals) {
                        bb.push_subgoal(std::move(sg));
                    }
                }
                return Status::Running;
            }
            // 目标完成：仅发布一次事件
            if (!goal_completed_published_) {
                ctx.event_bus().publish(events::kGoalCompleted, goal);
                goal_completed_published_ = true;
            }
        }
        return status;
    }

    void reset() override {
        goal_completed_published_ = false;
        CompositeNode::reset();
    }

private:
    std::shared_ptr<StrategicPlanner> planner_;
    bool goal_completed_published_ = false;
};

} // namespace gobot
