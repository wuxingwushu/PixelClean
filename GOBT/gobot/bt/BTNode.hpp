#pragma once

#include "gobot/core/Status.hpp"
#include "gobot/infra/Context.hpp"
#include <string>
#include <memory>

namespace gobot {

class BTNode {
public:
    explicit BTNode(std::string name = "") : name_(std::move(name)) {}
    virtual ~BTNode() = default;

    // 核心执行接口
    virtual Status tick(Context& ctx) = 0;

    // 重置节点运行状态（默认无操作，有状态节点覆盖）
    virtual void reset() {}

    const std::string& name() const { return name_; }

    // 禁用拷贝与赋值（节点通过智能指针共享）
    BTNode(const BTNode&) = delete;
    BTNode& operator=(const BTNode&) = delete;

protected:
    std::string name_;
};

// 节点智能指针
using BTNodePtr = std::shared_ptr<BTNode>;

} // namespace gobot
