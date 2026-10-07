#pragma once

// gobot 运行时层：执行器、构建产物、树构建器

#include "gobot/layers.hpp"

#include <functional>
#include <memory>
#include <string>
#include <utility>

namespace gobot {

// ---------------------------------------------------------------------------
// 执行器
// ---------------------------------------------------------------------------
class BTExecutor {
public:
    BTExecutor(BTNodePtr root, std::shared_ptr<Context> ctx)
        : root_(std::move(root)), context_(std::move(ctx)) {}

    // 单次 tick；结束后统一派发本 tick 内累计的事件
    Status tick_once() {
        const Status status = root_->tick(*context_);
        context_->event_bus().flush();
        return status;
    }

    // 反复 tick 直到 done 返回 true
    void tick_until(std::function<bool(Status)> done) {
        while (true) {
            const Status status = tick_once();
            if (done(status)) break;
        }
    }

    void reset() {
        if (root_) root_->reset();
    }

    Context& context() { return *context_; }

private:
    BTNodePtr root_;
    std::shared_ptr<Context> context_;
};

// ---------------------------------------------------------------------------
// 构建产物：持有全部组件，防止 shared_ptr 提前析构
// ---------------------------------------------------------------------------
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
        std::shared_ptr<GoalManager> gm, std::shared_ptr<StrategicPlanner> sp,
        std::shared_ptr<TacticalDecomposer> td, std::shared_ptr<SubtreeLibrary> sl)
        : root(std::move(r))
        , blackboard(std::move(bb))
        , event_bus(std::move(bus))
        , goal_manager(std::move(gm))
        , strategic_planner(std::move(sp))
        , decomposer(std::move(td))
        , subtree_library(std::move(sl))
        // context 必须最后构造：其余组件已就位
        , context(std::make_shared<Context>(blackboard, event_bus, goal_manager, decomposer, subtree_library)) {}
};

// ---------------------------------------------------------------------------
// 树构建器：链式配置 + build() 组装默认结构
// ---------------------------------------------------------------------------
// 默认骨架：StrategicGoalNode → TacticalSubgoalNode → SubtreeReferenceNode
// 未提供的组件在 build() 时用默认实例补齐。
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

    // 是否给每棵子树套 LayerBridgeNode（默认开启）
    TreeBuilder& with_layer_bridge(bool enabled) {
        enable_layer_bridge_ = enabled;
        return *this;
    }

    BuiltTree build() {
        if (!blackboard_) blackboard_ = std::make_shared<SharedBlackboard>();
        if (!event_bus_) event_bus_ = std::make_shared<EventBus>();
        if (!goal_manager_) goal_manager_ = std::make_shared<GoalManager>();
        if (!decomposer_) decomposer_ = std::make_shared<TacticalDecomposer>();
        if (!subtree_library_) subtree_library_ = std::make_shared<SubtreeLibrary>();

        // 事件延迟派发：避免回调在 tick 中途改状态造成重入
        event_bus_->set_deferred(true);

        auto planner = std::make_shared<StrategicPlanner>(goal_manager_, event_bus_);
        auto subtree_ref = std::make_shared<SubtreeReferenceNode>(subtree_library_, event_bus_);

        if (enable_layer_bridge_) {
            EventBusPtr bus = event_bus_;
            subtree_ref->set_subtree_wrapper([bus](BTNodePtr child) -> BTNodePtr {
                return std::make_shared<LayerBridgeNode>(std::move(child), bus);
            });
        }

        auto tactical = std::make_shared<TacticalSubgoalNode>();
        tactical->add_child(subtree_ref);

        auto strategic = std::make_shared<StrategicGoalNode>(planner);
        strategic->add_child(tactical);

        return BuiltTree(strategic, blackboard_, event_bus_, goal_manager_, planner,
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
