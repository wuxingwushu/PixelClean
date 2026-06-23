#pragma once

#include "gobot/bt/BTNode.hpp"
#include <vector>

namespace gobot {

class CompositeNode : public BTNode {
public:
    using BTNode::BTNode;

    void add_child(BTNodePtr child) {
        children_.push_back(std::move(child));
    }

    const std::vector<BTNodePtr>& children() const { return children_; }

    void reset() override {
        for (auto& child : children_) {
            if (child) child->reset();
        }
    }

protected:
    std::vector<BTNodePtr> children_;
};

} // namespace gobot
