#pragma once

// gobot 行为树节点层：节点基类、组合节点、装饰器、叶节点

#include "gobot/infra.hpp"

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace gobot {

// ---------------------------------------------------------------------------
// 节点基类
// ---------------------------------------------------------------------------
class BTNode {
public:
    explicit BTNode(std::string name = "") : name_(std::move(name)) {}
    virtual ~BTNode() = default;

    virtual Status tick(Context& ctx) = 0;

    // 清空节点运行状态；有状态节点必须覆盖，否则中断后无法从头重来
    virtual void reset() {}

    const std::string& name() const { return name_; }

    // 节点通过智能指针共享，禁止拷贝
    BTNode(const BTNode&) = delete;
    BTNode& operator=(const BTNode&) = delete;

protected:
    std::string name_;
};

using BTNodePtr = std::shared_ptr<BTNode>;

// ---------------------------------------------------------------------------
// 组合节点
// ---------------------------------------------------------------------------
class CompositeNode : public BTNode {
public:
    using BTNode::BTNode;

    void add_child(BTNodePtr child) { children_.push_back(std::move(child)); }

    const std::vector<BTNodePtr>& children() const { return children_; }

    void reset() override {
        for (auto& child : children_) {
            if (child) child->reset();
        }
    }

protected:
    std::vector<BTNodePtr> children_;
};

// ---------------------------------------------------------------------------
// 装饰器
// ---------------------------------------------------------------------------
class DecoratorNode : public BTNode {
public:
    explicit DecoratorNode(BTNodePtr child, std::string name = "")
        : BTNode(std::move(name)), child_(std::move(child)) {}

    void reset() override {
        if (child_) child_->reset();
    }

    BTNodePtr child() const { return child_; }

protected:
    BTNodePtr child_;
};

// ---------------------------------------------------------------------------
// 叶节点
// ---------------------------------------------------------------------------
// 叶节点无子节点；有内部状态的叶节点（如子树引用）需自行覆盖 reset()
class LeafNode : public BTNode {
public:
    using BTNode::BTNode;
};

} // namespace gobot
