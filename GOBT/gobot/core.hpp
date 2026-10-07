#pragma once

// gobot 核心数据层：节点状态、世界状态、目标/子目标、原子动作
// 该层不依赖框架其它部分，仅使用 STL（C++17）。

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace gobot {

// 行为树节点执行状态
enum class Status {
    Success,  // 完成
    Failure,  // 失败
    Running   // 执行中，下个 tick 继续
};

// 世界状态值：bool / int / float / string，用 variant 保持类型安全
using WorldKey = std::string;
using WorldValue = std::variant<bool, int, float, std::string>;

class WorldState;
class Goal;
class Subgoal;
class Action;
class Context;

using WorldStatePtr = std::shared_ptr<WorldState>;
using GoalPtr = std::shared_ptr<Goal>;
using SubgoalPtr = std::shared_ptr<Subgoal>;
using ActionPtr = std::shared_ptr<Action>;

// ---------------------------------------------------------------------------
// 世界状态
// ---------------------------------------------------------------------------
class WorldState {
public:
    WorldState() = default;
    explicit WorldState(const std::unordered_map<WorldKey, WorldValue>& init) : data_(init) {}

    // 类型安全读取；键不存在或类型不符返回 nullopt
    template <typename T>
    std::optional<T> get(const WorldKey& key) const {
        auto it = data_.find(key);
        if (it == data_.end()) return std::nullopt;
        if (const T* p = std::get_if<T>(&it->second)) return *p;
        return std::nullopt;
    }

    // 无类型读取（条件匹配用）
    std::optional<WorldValue> get_raw(const WorldKey& key) const {
        auto it = data_.find(key);
        if (it == data_.end()) return std::nullopt;
        return it->second;
    }

    template <typename T>
    void set(const WorldKey& key, T&& value) {
        data_[key] = WorldValue(std::forward<T>(value));
    }

    // 批量写入（应用动作 Effects）
    void apply(const std::unordered_map<WorldKey, WorldValue>& effects) {
        for (const auto& [k, v] : effects) data_[k] = v;
    }

    // 数值感知比较：int 与 float 可跨类型比较（int 提升为 float，与旧实现一致）
    static bool value_equal(const WorldValue& a, const WorldValue& b) {
        if (a.index() == b.index()) return a == b;
        const int* ia = std::get_if<int>(&a);
        const int* ib = std::get_if<int>(&b);
        const float* fa = std::get_if<float>(&a);
        const float* fb = std::get_if<float>(&b);
        if (ia && fb) return static_cast<float>(*ia) == *fb;
        if (fa && ib) return *fa == static_cast<float>(*ib);
        return false;
    }

    // 条件全部满足才为 true；缺失的键视为不满足
    bool matches(const std::unordered_map<WorldKey, WorldValue>& conditions) const {
        for (const auto& [k, expected] : conditions) {
            auto it = data_.find(k);
            if (it == data_.end() || !value_equal(it->second, expected)) return false;
        }
        return true;
    }

    WorldState clone() const { return WorldState(data_); }

    const std::unordered_map<WorldKey, WorldValue>& raw() const { return data_; }

private:
    std::unordered_map<WorldKey, WorldValue> data_;
};

// ---------------------------------------------------------------------------
// 目标与子目标
// ---------------------------------------------------------------------------

// 子目标类型枚举：子树库按此映射到具体子树
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

// 目标：期望达成的世界状态
class Goal {
public:
    Goal(std::string name,
         std::unordered_map<WorldKey, WorldValue> target_state,
         int priority = 0)
        : name_(std::move(name))
        , target_state_(std::move(target_state))
        , priority_(priority)
        , base_priority_(priority) {}

    const std::string& name() const { return name_; }
    int priority() const { return priority_; }
    void set_priority(int p) { priority_ = p; }

    // 注册时的初始优先级，用于失败降级后的恢复
    int base_priority() const { return base_priority_; }
    void restore_priority() { priority_ = base_priority_; }

    // 失败后是否允许挂起（默认允许）。生存类目标（Survive）必须为 false：永不被雪藏。
    void set_suspendible(bool s) { suspendible_ = s; }
    bool suspendible() const { return suspendible_; }

    const std::unordered_map<WorldKey, WorldValue>& target_state() const { return target_state_; }

    bool satisfied_by(const WorldState& ws) const { return ws.matches(target_state_); }

    bool operator==(const Goal& other) const { return name_ == other.name_; }
    bool operator!=(const Goal& other) const { return !(*this == other); }

private:
    std::string name_;
    std::unordered_map<WorldKey, WorldValue> target_state_;
    int priority_;
    int base_priority_ = 0;
    bool suspendible_ = true;
};

// 子目标：战术层分解产物，由子树库按 type 实例化执行
class Subgoal {
public:
    Subgoal(SubgoalType type,
            std::string name,
            std::unordered_map<WorldKey, WorldValue> target_state,
            int retry_limit = 1)
        : type_(type)
        , name_(std::move(name))
        , target_state_(std::move(target_state))
        , retry_limit_(retry_limit) {}

    SubgoalType type() const { return type_; }
    const std::string& name() const { return name_; }
    const std::unordered_map<WorldKey, WorldValue>& target_state() const { return target_state_; }
    int retry_limit() const { return retry_limit_; }
    int retry_count() const { return retry_count_; }
    void increment_retry() { ++retry_count_; }
    bool can_retry() const { return retry_count_ < retry_limit_; }
    void reset_retry() { retry_count_ = 0; }
    bool satisfied_by(const WorldState& ws) const { return ws.matches(target_state_); }

private:
    SubgoalType type_;
    std::string name_;
    std::unordered_map<WorldKey, WorldValue> target_state_;
    int retry_limit_;
    int retry_count_ = 0;
};

// ---------------------------------------------------------------------------
// 原子动作
// ---------------------------------------------------------------------------

// 动作执行函数签名
using ActionExecutor = std::function<Status(Context&)>;

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
    const std::unordered_map<WorldKey, WorldValue>& precondition() const { return precondition_; }
    const std::unordered_map<WorldKey, WorldValue>& effects() const { return effects_; }

    bool precond_met(const WorldState& ws) const { return ws.matches(precondition_); }

    // 只执行，不写效果：效果由调用方（OperationalActionNode）在 Success 时应用
    Status execute(Context& ctx) const { return executor_(ctx); }

private:
    std::string name_;
    std::unordered_map<WorldKey, WorldValue> precondition_;
    std::unordered_map<WorldKey, WorldValue> effects_;
    ActionExecutor executor_;
    float cost_;
};

} // namespace gobot
