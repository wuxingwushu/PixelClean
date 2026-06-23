#pragma once

#include "gobot/bt/BTNode.hpp"
#include "gobot/tactical/SubtreeLibrary.hpp"
#include "gobot/infra/EventBus.hpp"
#include <memory>
#include <functional>

namespace gobot {

// 子树包装器：允许在创建的子树外层套装饰器（如 LayerBridgeNode）
using SubtreeWrapper = std::function<BTNodePtr(BTNodePtr)>;

class SubtreeReferenceNode : public BTNode {
public:
    explicit SubtreeReferenceNode(std::shared_ptr<SubtreeLibrary> lib,
                                  EventBusPtr bus,
                                  std::string name = "SubtreeReferenceNode")
        : BTNode(std::move(name))
        , library_(std::move(lib))
        , event_bus_(std::move(bus)) {}

    // 设置子树包装器（用于接入 LayerBridgeNode 等装饰器）
    void set_subtree_wrapper(SubtreeWrapper wrapper) {
        wrapper_ = std::move(wrapper);
    }

    Status tick(Context& ctx) override {
        auto& bb = ctx.blackboard();
        SubgoalPtr sg = bb.current_subgoal();
        if (!sg) {
            return Status::Success;
        }

        // 子目标变化时重建子树
        if (!current_subtree_ || last_subgoal_ != sg) {
            BTNodePtr raw = library_->create_subtree(sg->type());
            if (!raw) {
                // 无对应子树：上报失败
                event_bus_->publish(events::kTacticalFailure, sg);
                return Status::Failure;
            }
            // 应用包装器（如 LayerBridgeNode）
            current_subtree_ = wrapper_ ? wrapper_(raw) : raw;
            last_subgoal_ = sg;
            sg->reset_retry();
        }

        Status status = current_subtree_->tick(ctx);
        if (status == Status::Success) {
            // 子目标完成
            event_bus_->publish(events::kSubgoalCompleted, sg);
            bb.pop_subgoal();
            current_subtree_.reset();
            last_subgoal_.reset();
        } else if (status == Status::Failure) {
            // 子目标失败：重试或上报
            sg->increment_retry();
            if (!sg->can_retry()) {
                event_bus_->publish(events::kTacticalFailure, sg);
                bb.pop_subgoal();
                current_subtree_.reset();
                last_subgoal_.reset();
            } else {
                // 重试：重置子树，等待下 tick 重新执行
                if (current_subtree_) current_subtree_->reset();
                status = Status::Running;
            }
        }
        return status;
    }

    void reset() override {
        if (current_subtree_) current_subtree_->reset();
        current_subtree_.reset();
        last_subgoal_.reset();
    }

private:
    std::shared_ptr<SubtreeLibrary> library_;
    EventBusPtr event_bus_;
    SubtreeWrapper wrapper_;
    BTNodePtr current_subtree_;
    SubgoalPtr last_subgoal_;
};

} // namespace gobot
