#pragma once

#include "gobot/bt/DecoratorNode.hpp"
#include "gobot/infra/EventBus.hpp"

namespace gobot {

class LayerBridgeNode : public DecoratorNode {
public:
    LayerBridgeNode(BTNodePtr child,
                    EventBusPtr bus,
                    std::string name = "LayerBridgeNode")
        : DecoratorNode(std::move(child), std::move(name))
        , event_bus_(std::move(bus)) {
        // 订阅战略层中断
        subscription_id_ = event_bus_->subscribe(
            events::kStrategicInterrupt,
            [this](const std::any&) { this->interrupted_ = true; });
    }

    ~LayerBridgeNode() override {
        if (event_bus_) {
            event_bus_->unsubscribe(subscription_id_);
        }
    }

    Status tick(Context& ctx) override {
        if (interrupted_) {
            interrupted_ = false;
            if (child_) child_->reset();
            return Status::Running; // 让上层重新决策
        }
        return child_ ? child_->tick(ctx) : Status::Failure;
    }

private:
    EventBusPtr event_bus_;
    SubscriptionId subscription_id_ = 0;
    bool interrupted_ = false;
};

} // namespace gobot
