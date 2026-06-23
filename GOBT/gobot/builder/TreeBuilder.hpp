#pragma once

#include "gobot/bt/BTNode.hpp"
#include "gobot/infra/Context.hpp"
#include "gobot/strategic/GoalManager.hpp"
#include "gobot/strategic/StrategicPlanner.hpp"
#include "gobot/strategic/StrategicGoalNode.hpp"
#include "gobot/tactical/TacticalDecomposer.hpp"
#include "gobot/tactical/SubtreeLibrary.hpp"
#include "gobot/tactical/TacticalSubgoalNode.hpp"
#include "gobot/tactical/SubtreeReferenceNode.hpp"
#include "gobot/operational/LayerBridgeNode.hpp"
#include "gobot/operational/FailureReportNode.hpp"
#include <memory>

namespace gobot {

// 构建结果：包含树根节点与运行所需的所有组件
// Context 以 shared_ptr 持有，保证 BTExecutor 可安全引用
struct BuiltTree {
    BTNodePtr root;
    BlackboardPtr blackboard;
    EventBusPtr event_bus;
    std::shared_ptr<GoalManager> goal_manager;
    std::shared_ptr<StrategicPlanner> strategic_planner;
    std::shared_ptr<TacticalDecomposer> decomposer;
    std::shared_ptr<SubtreeLibrary> subtree_library;
    std::shared_ptr<Context> context;

    BuiltTree(BTNodePtr r, BlackboardPtr bb, EventBusPtr bus,
              std::shared_ptr<GoalManager> gm,
              std::shared_ptr<StrategicPlanner> sp,
              std::shared_ptr<TacticalDecomposer> td,
              std::shared_ptr<SubtreeLibrary> sl)
        : root(std::move(r))
        , blackboard(std::move(bb))
        , event_bus(std::move(bus))
        , goal_manager(std::move(gm))
        , strategic_planner(std::move(sp))
        , decomposer(std::move(td))
        , subtree_library(std::move(sl))
        , context(std::make_shared<Context>(
              blackboard, event_bus, goal_manager, decomposer, subtree_library)) {}
};

class TreeBuilder {
public:
    TreeBuilder& with_blackboard(BlackboardPtr bb) {
        blackboard_ = std::move(bb);
        return *this;
    }

    TreeBuilder& with_event_bus(EventBusPtr bus) {
        event_bus_ = std::move(bus);
        return *this;
    }

    TreeBuilder& with_goal_manager(std::shared_ptr<GoalManager> gm) {
        goal_manager_ = std::move(gm);
        return *this;
    }

    TreeBuilder& with_decomposer(std::shared_ptr<TacticalDecomposer> td) {
        decomposer_ = std::move(td);
        return *this;
    }

    TreeBuilder& with_subtree_library(std::shared_ptr<SubtreeLibrary> sl) {
        subtree_library_ = std::move(sl);
        return *this;
    }

    // 是否启用 LayerBridgeNode 包装操作层子树（默认启用）
    TreeBuilder& with_layer_bridge(bool enabled) {
        enable_layer_bridge_ = enabled;
        return *this;
    }

    BuiltTree build() {
        // 默认创建未提供的组件
        if (!blackboard_) blackboard_ = std::make_shared<SharedBlackboard>();
        if (!event_bus_) event_bus_ = std::make_shared<EventBus>();
        if (!goal_manager_) goal_manager_ = std::make_shared<GoalManager>();
        if (!decomposer_) decomposer_ = std::make_shared<TacticalDecomposer>();
        if (!subtree_library_) subtree_library_ = std::make_shared<SubtreeLibrary>();

        // 启用 EventBus 延迟派发，避免 tick 中回调重入
        event_bus_->set_deferred(true);

        auto planner = std::make_shared<StrategicPlanner>(
            goal_manager_, event_bus_);

        // 战术层：SubtreeReferenceNode（持有子树库）
        auto subtree_ref = std::make_shared<SubtreeReferenceNode>(
            subtree_library_, event_bus_);

        // 接入 LayerBridgeNode：包装操作层子树以支持战略中断
        if (enable_layer_bridge_) {
            auto bus = event_bus_;
            subtree_ref->set_subtree_wrapper(
                [bus](BTNodePtr child) -> BTNodePtr {
                    return std::make_shared<LayerBridgeNode>(child, bus);
                });
        }

        // 战术层入口
        auto tactical = std::make_shared<TacticalSubgoalNode>();
        tactical->add_child(subtree_ref);

        // 战略层入口
        auto strategic = std::make_shared<StrategicGoalNode>(planner);
        strategic->add_child(tactical);

        return BuiltTree(strategic, blackboard_, event_bus_,
                         goal_manager_, planner,
                         decomposer_, subtree_library_);
    }

private:
    BlackboardPtr blackboard_;
    EventBusPtr event_bus_;
    std::shared_ptr<GoalManager> goal_manager_;
    std::shared_ptr<TacticalDecomposer> decomposer_;
    std::shared_ptr<SubtreeLibrary> subtree_library_;
    bool enable_layer_bridge_ = true;
};

} // namespace gobot
