#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <functional>
#include <cstdint>

namespace gobot {

// 世界状态值类型：支持 bool / int / float / string
// 使用 variant 避免类型擦除，保持类型安全
using WorldKey = std::string;

// 前向声明
class WorldState;
class Goal;
class Subgoal;
class Action;
class Context;

// 智能指针别名
using WorldStatePtr = std::shared_ptr<WorldState>;
using GoalPtr = std::shared_ptr<Goal>;
using SubgoalPtr = std::shared_ptr<Subgoal>;
using ActionPtr = std::shared_ptr<Action>;

} // namespace gobot
