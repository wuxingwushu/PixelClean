#pragma once

#include "gobot/bt/BTNode.hpp"

namespace gobot {

class DecoratorNode : public BTNode {
public:
    explicit DecoratorNode(BTNodePtr child, std::string name = "")
        : BTNode(std::move(name)), child_(std::move(child)) {}

    void reset() override {
        if (child_) child_->reset();
    }

    BTNodePtr child() const { return child_; }

protected:
    BTNodePtr child_;
};

} // namespace gobot
