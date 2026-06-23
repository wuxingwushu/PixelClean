#pragma once

#include "gobot/core/Types.hpp"
#include "gobot/core/WorldState.hpp"
#include "gobot/core/Status.hpp"
#include <functional>
#include <unordered_map>
#include <string>

namespace gobot {

class Context;

// 动作执行函数签名：接收上下文，返回状态
using ActionExecutor = std::function<Status(Context&)>;

// 原子动作：含前提、效果、执行函数、代价
class Action {
public:
    Action(std::string name,
           std::unordered_map<WorldKey, WorldValue> precondition,
           std::unordered_map<WorldKey, WorldValue> effects,
           ActionExecutor executor,
           float cost = 1.0f)
        : name_(std::move(name))
        , precondition_(std::move(precondition))
        , effects_(std::move(effects))
        , executor_(std::move(executor))
        , cost_(cost) {}

    const std::string& name() const { return name_; }
    float cost() const { return cost_; }
    const std::unordered_map<WorldKey, WorldValue>& precondition() const {
        return precondition_;
    }
    const std::unordered_map<WorldKey, WorldValue>& effects() const {
        return effects_;
    }

    // 前提校验
    bool precond_met(const WorldState& ws) const {
        return ws.matches(precondition_);
    }

    // 执行动作（不应用效果，由调用方决定）
    Status execute(Context& ctx) const { return executor_(ctx); }

private:
    std::string name_;
    std::unordered_map<WorldKey, WorldValue> precondition_;
    std::unordered_map<WorldKey, WorldValue> effects_;
    ActionExecutor executor_;
    float cost_;
};

} // namespace gobot
