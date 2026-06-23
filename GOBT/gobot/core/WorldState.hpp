#pragma once

#include "gobot/core/Types.hpp"
#include <variant>
#include <optional>
#include <stdexcept>

namespace gobot {

// 世界状态值：支持四种基础类型
using WorldValue = std::variant<bool, int, float, std::string>;

class WorldState {
public:
    WorldState() = default;
    explicit WorldState(const std::unordered_map<WorldKey, WorldValue>& init)
        : data_(init) {}

    // 类型安全读取
    template <typename T>
    std::optional<T> get(const WorldKey& key) const {
        auto it = data_.find(key);
        if (it == data_.end()) return std::nullopt;
        if (const T* p = std::get_if<T>(&it->second)) return *p;
        return std::nullopt;
    }

    // 无类型读取（用于条件匹配）
    std::optional<WorldValue> get_raw(const WorldKey& key) const {
        auto it = data_.find(key);
        if (it == data_.end()) return std::nullopt;
        return it->second;
    }

    // 类型安全写入
    template <typename T>
    void set(const WorldKey& key, T&& value) {
        data_[key] = WorldValue(std::forward<T>(value));
    }

    // 批量设置（用于应用 Effects）
    void apply(const std::unordered_map<WorldKey, WorldValue>& effects) {
        for (const auto& [k, v] : effects) {
            data_[k] = v;
        }
    }

    // 数值感知比较：int 与 float 之间可隐式转换比较
    static bool value_equal(const WorldValue& a, const WorldValue& b) {
        // 同类型直接比较
        if (a.index() == b.index()) return a == b;
        // int vs float 数值比较
        if (auto* ia = std::get_if<int>(&a)) {
            if (auto* fb = std::get_if<float>(&b)) return *ia == *fb;
        }
        if (auto* fa = std::get_if<float>(&a)) {
            if (auto* ib = std::get_if<int>(&b)) return *fa == *ib;
        }
        return false;
    }

    // 检查条件是否全部满足
    bool matches(const std::unordered_map<WorldKey, WorldValue>& conditions) const {
        for (const auto& [k, expected] : conditions) {
            auto it = data_.find(k);
            if (it == data_.end()) return false;
            if (!value_equal(it->second, expected)) return false;
        }
        return true;
    }

    // 深拷贝（用于规划）
    WorldState clone() const { return WorldState(data_); }

    // 只读访问
    const std::unordered_map<WorldKey, WorldValue>& raw() const { return data_; }

private:
    std::unordered_map<WorldKey, WorldValue> data_;
};

} // namespace gobot
