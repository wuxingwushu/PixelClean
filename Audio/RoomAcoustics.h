// 房间声学系统。基于 Raycast 检测玩家所在房间的声学特征。
// 分为两个组件：
//   1. RoomAcousticsDetector：通过多条射线探测墙距，评估房间大小/形状。
//   2. RoomAcousticsApplier：根据检测结果自动调节混响和回声参数（平滑过渡）。
//      混响使用 FreeverbFilter，回声使用 EchoFilter，都挂在 Environment 总线上。
#pragma once
#include "soloud.h"
#include "soloud_freeverbfilter.h"
#include "soloud_echofilter.h"
#include "AudioBus.h"
#include <functional>
#include <vector>

namespace GAME::Audio {

// 房间检测结果
struct RoomInfo
{
    bool   isIndoor = false;           // 是否处于室内
    float  avgWallDist = 100.0f;       // 平均墙距
    float  estimatedVolume = 10000.0f; // 估算房间体积
    float  targetReverbTime = 0.3f;    // 目标混响时间（秒）
    float  targetLPF = 22000.0f;       // 目标低通截止频率
    float  reflectivity = 0.3f;        // 反射系数（0~1）
    float  echoLevel = 0.0f;           // 回声强度（0~1，仅走廊）
    bool   isCorridor = false;         // 是否为走廊（长宽比 > 3:1）
};

// 房间检测器。通过多方向 Raycast 探测周围的墙壁距离。
// 需要外部注入射线检测回调函数（与游戏物理引擎对接）。
class RoomAcousticsDetector
{
public:
    // 射线回调：angle=方向角（弧度），maxDist=最大探测距离 → 返回命中距离
    // 未命中时返回 maxDist
    using RaycastFunc = std::function<float(float angle, float maxDist)>;

    void SetRaycastCallback(RaycastFunc func);
    void SetNumRays(int count);       // 射线数量，越多越精确（默认 16）
    void SetMaxRayDistance(float dist); // 最大探测距离（默认 50）

    // 在 (posX, posY) 位置执行房间检测，返回 RoomInfo
    RoomInfo Detect(float posX, float posY);

private:
    RaycastFunc mRaycastFunc;
    int mNumRays = 16;
    float mMaxRayDist = 50.0f;
};

// 房间声学效果器。根据目标房间参数平滑调节混响和回声效果。
class RoomAcousticsApplier
{
public:
    // 初始化滤波器并挂载到 Environment 总线
    void Initialize(SoLoud::Soloud* soloud, BusSystem* busSystem);

    // 设置目标房间参数，驱动混响/回声向目标值平滑过渡
    void SetTargetRoom(const RoomInfo& room);

    // 每帧调用，执行混响/回声参数的平滑插值
    void Update(float deltaTime);

    float GetCurrentReverbWet() const  { return mCurrentReverbWet; }
    float GetCurrentReverbRoom() const { return mCurrentReverbRoom; }
    float GetCurrentEchoWet() const    { return mCurrentEchoWet; }

private:
    SoLoud::Soloud* mSoloud = nullptr;
    BusSystem* mBusSystem = nullptr;

    // 目标值：由 SetTargetRoom 设置
    float mTargetReverbWet = 0.0f;
    float mTargetReverbRoom = 0.0f;
    float mTargetEchoWet = 0.0f;

    // 当前值：由 Update 逐步逼近
    float mCurrentReverbWet = 0.0f;
    float mCurrentReverbRoom = 0.0f;
    float mCurrentEchoWet = 0.0f;

    SoLoud::FreeverbFilter mReverbFilter; // Schroeder 混响
    SoLoud::EchoFilter mEchoFilter;       // 简单延迟回声
};

} // namespace GAME::Audio