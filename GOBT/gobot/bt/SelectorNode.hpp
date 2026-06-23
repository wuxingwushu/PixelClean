#pragma once

#include "gobot/bt/CompositeNode.hpp"

namespace gobot {

// 选择节点：依次执行子节点，任一成功则返回 Success，全部失败返回 Failure
// 记录当前执行位置，Running 时下 tick 从当前位置继续
class SelectorNode : public CompositeNode {
public:
    explicit SelectorNode(std::string name = "Selector")
        : CompositeNode(std::move(name)) {}

    Status tick(Context& ctx) override {
        for (std::size_t i = current_index_; i < children_.size(); ++i) {
            Status status = children_[i]->tick(ctx);
            if (status == Status::Running) {
                current_index_ = i;
                return Status::Running;
            }
            if (status == Status::Success) {
                current_index_ = 0;
                return Status::Success;
            }
            // Failure：尝试下一个子节点
        }
        current_index_ = 0;
        return Status::Failure;
    }

    void reset() override {
        current_index_ = 0;
        CompositeNode::reset();
    }

private:
    std::size_t current_index_ = 0;
};

} // namespace gobot
