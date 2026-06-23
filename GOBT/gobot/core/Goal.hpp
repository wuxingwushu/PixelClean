#pragma once

#include "gobot/core/Types.hpp"
#include "gobot/core/WorldState.hpp"
#include <unordered_map>
#include <string>

namespace gobot {

// 目标：期望达成的世界状态
class Goal {
public:
    Goal(std::string name,
         std::unordered_map<WorldKey, WorldValue> target_state,
         int priority = 0)
        : name_(std::move(name))
        , target_state_(std::move(target_state))
        , priority_(priority) {}

    const std::string& name() const { return name_; }
    int priority() const { return priority_; }
    void set_priority(int p) { priority_ = p; }
    const std::unordered_map<WorldKey, WorldValue>& target_state() const {
        return target_state_;
    }

    // 检查世界状态是否满足目标
    bool satisfied_by(const WorldState& ws) const {
        return ws.matches(target_state_);
    }

    // 用于比较与去重
    bool operator==(const Goal& other) const { return name_ == other.name_; }
    bool operator!=(const Goal& other) const { return !(*this == other); }

private:
    std::string name_;
    std::unordered_map<WorldKey, WorldValue> target_state_;
    int priority_;
};

// 子目标类型枚举：用于子树库映射
enum class SubgoalType {
    MoveTo,
    AcquireItem,
    Interact,
    Combat,
    Flee,
    Standby,
    Patrol,
    Recover,
    Custom
};

// 子目标：战术层分解产物
class Subgoal {
public:
    Subgoal(SubgoalType type,
            std::string name,
            std::unordered_map<WorldKey, WorldValue> target_state,
            int retry_limit = 1)
        : type_(type)
        , name_(std::move(name))
        , target_state_(std::move(target_state))
        , retry_limit_(retry_limit)
        , retry_count_(0) {}

    SubgoalType type() const { return type_; }
    const std::string& name() const { return name_; }
    const std::unordered_map<WorldKey, WorldValue>& target_state() const {
        return target_state_;
    }
    int retry_limit() const { return retry_limit_; }
    int retry_count() const { return retry_count_; }
    void increment_retry() { ++retry_count_; }
    bool can_retry() const { return retry_count_ < retry_limit_; }
    void reset_retry() { retry_count_ = 0; }
    bool satisfied_by(const WorldState& ws) const {
        return ws.matches(target_state_);
    }

private:
    SubgoalType type_;
    std::string name_;
    std::unordered_map<WorldKey, WorldValue> target_state_;
    int retry_limit_;
    int retry_count_;
};

} // namespace gobot
