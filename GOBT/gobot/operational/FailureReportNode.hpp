#pragma once

#include "gobot/bt/LeafNode.hpp"
#include "gobot/infra/EventBus.hpp"
#include "gobot/core/Goal.hpp"

namespace gobot {

class FailureReportNode : public LeafNode {
public:
    FailureReportNode(EventBusPtr bus,
                      std::string event_type = events::kTacticalFailure,
                      std::string name = "FailureReportNode")
        : LeafNode(std::move(name))
        , event_bus_(std::move(bus))
        , event_type_(std::move(event_type)) {}

    Status tick(Context& ctx) override {
        SubgoalPtr sg = ctx.blackboard().current_subgoal();
        event_bus_->publish(event_type_, sg);
        return Status::Failure;
    }

private:
    EventBusPtr event_bus_;
    std::string event_type_;
};

} // namespace gobot
