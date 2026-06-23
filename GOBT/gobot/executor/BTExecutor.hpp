#pragma once

#include "gobot/bt/BTNode.hpp"
#include "gobot/infra/Context.hpp"
#include "gobot/infra/EventBus.hpp"
#include <functional>
#include <memory>

namespace gobot {

class BTExecutor {
public:
    BTExecutor(BTNodePtr root, std::shared_ptr<Context> ctx)
        : root_(std::move(root)), ctx_(std::move(ctx)) {}

    // 单次 Tick：执行树后派发延迟事件
    Status tick_once() {
        Status s = root_->tick(*ctx_);
        // tick 完成后 flush 延迟事件，避免回调在 tick 中途重入
        ctx_->event_bus().flush();
        return s;
    }

    // 持续 Tick 直到条件满足
    void tick_until(std::function<bool(Status)> done) {
        while (true) {
            Status s = tick_once();
            if (done(s)) break;
        }
    }

    // 重置整棵树
    void reset() {
        if (root_) root_->reset();
    }

    std::shared_ptr<Context> context() const { return ctx_; }

private:
    BTNodePtr root_;
    std::shared_ptr<Context> ctx_;
};

} // namespace gobot
