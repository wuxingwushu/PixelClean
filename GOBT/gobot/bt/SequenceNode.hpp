#pragma once

#include "gobot/bt/CompositeNode.hpp"

namespace gobot {

// 顺序节点：依次执行子节点，任一失败则返回 Failure，全部成功返回 Success
// 记录当前执行位置，Running 时下 tick 从当前位置继续
class SequenceNode : public CompositeNode {
public:
    explicit SequenceNode(std::string name = "Sequence")
        : CompositeNode(std::move(name)) {}

    Status tick(Context& ctx) override {
        for (std::size_t i = current_index_; i < children_.size(); ++i) {
            Status status = children_[i]->tick(ctx);
            if (status == Status::Running) {
                current_index_ = i;
                return Status::Running;
            }
            if (status == Status::Failure) {
                current_index_ = 0;
                return Status::Failure;
            }
            // Success：继续下一个子节点
        }
        current_index_ = 0;
        return Status::Success;
    }

    void reset() override {
        current_index_ = 0;
        CompositeNode::reset();
    }

private:
    std::size_t current_index_ = 0;
};

} // namespace gobot
