#pragma once

#include "gobot/core/Goal.hpp"
#include "gobot/strategic/GoalManager.hpp"
#include "gobot/infra/SharedBlackboard.hpp"
#include "gobot/infra/EventBus.hpp"
#include <unordered_map>
#include <memory>
#include <algorithm>

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
        // 目标完成 = 局势已改变：恢复所有目标的基础优先级，
        // 否则失败降级会永久累积，目标最终被雪藏（NPC 越玩越"佛系"）
        completed_sub_id_ = event_bus_->subscribe(events::kGoalCompleted,
            [this](const std::any&) {
                goal_manager_->restore_all_priorities();
            });
    }

    ~StrategicPlanner() {
        if (event_bus_) {
            event_bus_->unsubscribe(sub_id_);
            event_bus_->unsubscribe(completed_sub_id_);
        }
    }

    // 选择当前应追求的目标（委托给 GoalManager，挂起目标自动跳过）
    GoalPtr select_goal(const WorldState& ws) {
        return goal_manager_->select_top(ws);
    }

    // 通知 planner 当前正在追求的目标
    void set_current_goal(GoalPtr goal) {
        current_goal_ = std::move(goal);
        failure_count_ = 0; // 目标切换时重置失败计数
    }

    // 失败处理：累计失败次数，超阈值时挂起当前目标（冷却后自动恢复）
    void on_tactical_failure(const std::any& payload) {
        // payload 期望为 SubgoalPtr
        try {
            auto sg = std::any_cast<SubgoalPtr>(payload);
            (void)sg; // 当前简单实现不区分子目标
        } catch (const std::bad_any_cast&) {
            return;
        }
        failure_count_++;
        if (failure_count_ < kMaxConsecutiveFailures) {
            return;
        }
        failure_count_ = 0;
        if (!current_goal_) {
            return;
        }

        if (current_goal_->suspendible()) {
            // 挂起目标一段时间：让低优先级目标（如巡逻）获得机会；
            // 冷却结束后目标自动恢复参与选择，不会被永久雪藏。
            goal_manager_->suspend(current_goal_->name(), kSuspensionTicks);
        } else {
            // 不可挂起目标（如 Survive）：仅做有限降级，保证其仍可被选中
            int floor_priority = current_goal_->base_priority() - kMaxDowngrade;
            int new_priority = (std::max)(current_goal_->priority() - kPriorityDowngradeStep,
                                          floor_priority);
            goal_manager_->adjust_priority(current_goal_->name(), new_priority);
        }
    }

    void reset_failure_count() { failure_count_ = 0; }

private:
    static constexpr int kMaxConsecutiveFailures = 3;
    // 挂起冷却（选择周期 ≈ 帧；60fps 下约 3 秒）
    static constexpr int kSuspensionTicks = 180;
    static constexpr int kPriorityDowngradeStep = 5;
    static constexpr int kMaxDowngrade = 25;  // 降级封底（相对基础优先级）

    std::shared_ptr<GoalManager> goal_manager_;
    EventBusPtr event_bus_;
    SubscriptionId sub_id_ = 0;
    SubscriptionId completed_sub_id_ = 0;
    GoalPtr current_goal_;
    int failure_count_ = 0;
};

} // namespace gobot
