#pragma once

#include "gobot/infra/SharedBlackboard.hpp"
#include "gobot/infra/EventBus.hpp"

namespace gobot {

// 前向声明，避免循环依赖
class GoalManager;
class TacticalDecomposer;
class SubtreeLibrary;

class Context {
public:
    Context(BlackboardPtr bb,
            EventBusPtr bus,
            std::shared_ptr<GoalManager> gm,
            std::shared_ptr<TacticalDecomposer> decomposer,
            std::shared_ptr<SubtreeLibrary> subtree_lib)
        : blackboard_(std::move(bb))
        , event_bus_(std::move(bus))
        , goal_manager_(std::move(gm))
        , decomposer_(std::move(decomposer))
        , subtree_library_(std::move(subtree_lib)) {}

    // 访问器（非 const 版本）
    SharedBlackboard& blackboard() { return *blackboard_; }
    EventBus& event_bus() { return *event_bus_; }
    GoalManager& goal_manager() { return *goal_manager_; }
    TacticalDecomposer& decomposer() { return *decomposer_; }
    SubtreeLibrary& subtree_library() { return *subtree_library_; }

    // 访问器（const 版本）
    const SharedBlackboard& blackboard() const { return *blackboard_; }
    const EventBus& event_bus() const { return *event_bus_; }
    const GoalManager& goal_manager() const { return *goal_manager_; }
    const TacticalDecomposer& decomposer() const { return *decomposer_; }
    const SubtreeLibrary& subtree_library() const { return *subtree_library_; }

private:
    BlackboardPtr blackboard_;
    EventBusPtr event_bus_;
    std::shared_ptr<GoalManager> goal_manager_;
    std::shared_ptr<TacticalDecomposer> decomposer_;
    std::shared_ptr<SubtreeLibrary> subtree_library_;
};

} // namespace gobot
