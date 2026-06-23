#pragma once

#include <string>
#include <any>
#include <functional>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <algorithm>
#include <cstdint>
#include <memory>

namespace gobot {

// 事件类型标识
using EventType = std::string;

// 订阅者句柄，用于取消订阅
using SubscriptionId = std::uint64_t;

// 事件回调签名
using EventCallback = std::function<void(const std::any&)>;

// 预定义事件类型常量
namespace events {
    inline constexpr const char* kStrategicInterrupt = "STRATEGIC_INTERRUPT";
    inline constexpr const char* kTacticalFailure   = "TACTICAL_FAILURE";
    inline constexpr const char* kSubgoalCompleted   = "SUBGOAL_COMPLETED";
    inline constexpr const char* kGoalCompleted      = "GOAL_COMPLETED";
}

class EventBus {
public:
    // 设置延迟派发模式：publish 入队，flush 时统一派发
    // 避免回调在 tick 执行中途修改状态导致重入问题
    void set_deferred(bool enabled) { deferred_ = enabled; }

    // 订阅事件，返回订阅 ID（用于取消）
    SubscriptionId subscribe(const EventType& type, EventCallback cb) {
        std::lock_guard<std::mutex> lock(mutex_);
        SubscriptionId id = next_id_++;
        subscribers_[type].emplace_back(id, std::move(cb));
        return id;
    }

    // 取消订阅
    void unsubscribe(SubscriptionId id) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& [type, list] : subscribers_) {
            list.erase(
                std::remove_if(list.begin(), list.end(),
                    [id](const auto& pair) { return pair.first == id; }),
                list.end());
        }
    }

    // 发布事件
    // 延迟模式下入队，立即模式下同步派发
    void publish(const EventType& type, std::any payload = {}) {
        if (deferred_) {
            std::lock_guard<std::mutex> lock(mutex_);
            pending_.emplace_back(type, std::move(payload));
        } else {
            dispatch(type, payload);
        }
    }

    // 派发所有延迟队列中的事件（在 tick 结束后调用）
    void flush() {
        std::vector<std::pair<EventType, std::any>> to_dispatch;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            to_dispatch.swap(pending_);
        }
        for (auto& [type, payload] : to_dispatch) {
            dispatch(type, payload);
        }
    }

private:
    // 实际派发逻辑（锁外调用）
    void dispatch(const EventType& type, const std::any& payload) {
        std::vector<EventCallback> to_call;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = subscribers_.find(type);
            if (it != subscribers_.end()) {
                to_call.reserve(it->second.size());
                for (const auto& [id, cb] : it->second) {
                    to_call.push_back(cb);
                }
            }
        }
        for (const auto& cb : to_call) {
            cb(payload);
        }
    }

    bool deferred_ = false;
    std::mutex mutex_;
    SubscriptionId next_id_ = 0;
    std::unordered_map<EventType,
                       std::vector<std::pair<SubscriptionId, EventCallback>>>
        subscribers_;
    std::vector<std::pair<EventType, std::any>> pending_;
};

using EventBusPtr = std::shared_ptr<EventBus>;

} // namespace gobot
