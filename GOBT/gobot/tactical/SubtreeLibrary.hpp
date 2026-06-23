#pragma once

#include "gobot/bt/BTNode.hpp"
#include "gobot/core/Goal.hpp"
#include <functional>
#include <unordered_map>

namespace gobot {

// 子树工厂签名：返回新构建的子树根节点
using SubtreeFactory = std::function<BTNodePtr()>;

// SubgoalType 的哈希函数，用于 unordered_map
struct SubgoalTypeHash {
    std::size_t operator()(SubgoalType t) const noexcept {
        return static_cast<std::size_t>(t);
    }
};

class SubtreeLibrary {
public:
    // 注册子树工厂
    void register_subtree(SubgoalType type, SubtreeFactory factory) {
        factories_[type] = std::move(factory);
    }

    // 获取子树实例（每次新建）
    BTNodePtr create_subtree(SubgoalType type) const {
        auto it = factories_.find(type);
        if (it == factories_.end()) return nullptr;
        return it->second();
    }

    bool has_subtree(SubgoalType type) const {
        return factories_.find(type) != factories_.end();
    }

private:
    std::unordered_map<SubgoalType, SubtreeFactory, SubgoalTypeHash> factories_;
};

} // namespace gobot
