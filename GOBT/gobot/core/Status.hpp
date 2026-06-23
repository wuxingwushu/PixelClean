#pragma once

namespace gobot {

// BT 节点执行状态
enum class Status {
    Success,  // 成功完成
    Failure,  // 执行失败
    Running   // 执行中，未完成
};

} // namespace gobot
