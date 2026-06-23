#pragma once

#include "gobot/bt/BTNode.hpp"

namespace gobot {

class LeafNode : public BTNode {
public:
    using BTNode::BTNode;
    // 叶节点通常无状态，reset 默认空操作
};

} // namespace gobot
