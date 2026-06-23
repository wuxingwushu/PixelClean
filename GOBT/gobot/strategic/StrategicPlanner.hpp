#pragma once

#include "gobot/core/Goal.hpp"
#include "gobot/strategic/GoalManager.hpp"
#include "gobot/infra/SharedBlackboard.hpp"
#include "gobot/infra/EventBus.hpp"
#include <unordered_map>
#include <memory>

namespace gobot {

class StrategicPlanner {
public:
    explicit StrategicPlanner(std::shared_ptr<GoalManager> gm,
                              EventBusPtr bus)
        : goal_manager_(std::move(gm)), event_bus_(std::move(bus)) {
        // 订阅战术层失败事件
        sub_id_ = event_bus_->subscribe(events::kTacticalFailure,
            [this](const std::any& payload) {
                this->on_tactical_failure(payload);
            });
    }

    ~StrategicPlanner() {
        if (event_bus_) {
            event_bus_->unsubscribe(sub_id_);
        }
    }

    // 选择当前应追求的目标（委托给 GoalManager）
    GoalPtr select_goal(const WorldState& ws) {
        return goal_manager_->select_top(ws);
    }

    // 通知 planner 当前正在追求的目标
    void set_current_goal(GoalPtr goal) {
        current_goal_ = std::move(goal);
        failure_count_ = 0; // 目标切换时重置失败计数
    }

    // 失败处理：累计失败次数，超阈值则降级当前目标优先级
    void on_tactical_failure(const std::any& payload) {
        // payload 期望为 SubgoalPtr
        try {
            auto sg = std::any_cast<SubgoalPtr>(payload);
            (void)sg; // 当前简单实现不区分子目标
        } catch (const std::bad_any_cast&) {
            return;
        }
        failure_count_++;
        if (failure_count_ >= kMaxConsecutiveFailures) {
            // 降级当前目标优先级，使其他目标有机会被选中
            if (current_goal_) {
                int new_priority = current_goal_->priority() - kPriorityDowngradeStep;
                goal_manager_->adjust_priority(current_goal_->name(), new_priority);
            }
            failure_count_ = 0;
        }
    }

    void reset_failure_count() { failure_count_ = 0; }

private:
    static constexpr int kMaxConsecutiveFailures = 3;
    static constexpr int kPriorityDowngradeStep = 5;
    std::shared_ptr<GoalManager> goal_manager_;
    EventBusPtr event_bus_;
    SubscriptionId sub_id_ = 0;
    GoalPtr current_goal_;
    int failure_count_ = 0;
};

} // namespace gobot
