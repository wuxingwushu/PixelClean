// 房间声学系统实现。
//
// RoomAcousticsDetector::Detect 算法：
//   1. 从检测点向周围发射 mNumRays 条射线（均匀分布 360°）
//   2. 调用外部 RaycastFunc 获取每条射线的命中距离
//   3. 统计命中率、平均/最小/最大距离
//   4. 命中率 ≥ 75% → 判定为室内
//      - 根据平均距离估算房间体积和混响时间
//      - 根据长宽比判定是否为走廊（长宽比 > 3:1 + 命中率 > 85%）
//      - 走廊启用回声效果
//   5. 命中率 < 75% → 室外（无混响/回声）
//
// RoomAcousticsApplier::Update 算法：
//   - 使用非对称 blend 速度：室内切换快（2x dt），室外恢复慢（6x dt）
//   - 通过 SoLoud::fadeFilterParameter 平滑调节滤波器的 Wet/RoomSize
#include "RoomAcoustics.h"
#include <algorithm>
#include <cfloat>

namespace GAME::Audio {

void RoomAcousticsDetector::SetRaycastCallback(RaycastFunc func)
{
    mRaycastFunc = func;
}

void RoomAcousticsDetector::SetNumRays(int count)
{
    mNumRays = count;
}

void RoomAcousticsDetector::SetMaxRayDistance(float dist)
{
    mMaxRayDist = dist;
}

RoomInfo RoomAcousticsDetector::Detect(float posX, float posY)
{
    RoomInfo result;

    // 未设置射线回调 → 默认室外
    if (!mRaycastFunc)
    {
        result.isIndoor = false;
        return result;
    }

    float totalDist = 0.0f;
    int hitCount = 0;
    float minDist = FLT_MAX;
    float maxDist = 0.0f;

    const float PI = 3.14159265358979323846f;

    // 向周围发射射线
    for (int i = 0; i < mNumRays; ++i)
    {
        float angle = (float)i / mNumRays * 2.0f * PI;
        float hitDist = mRaycastFunc(angle, mMaxRayDist);

        if (hitDist < mMaxRayDist)
        {
            totalDist += hitDist;
            hitCount++;

            if (hitDist < minDist) minDist = hitDist;
            if (hitDist > maxDist) maxDist = hitDist;
        }
    }

    float hitRatio = (float)hitCount / mNumRays;

    // 射线命中率 ≥ 75% 判定为室内
    if (hitRatio >= 0.75f)
    {
        result.isIndoor = true;
        float avgDist = totalDist / hitCount;

        result.avgWallDist = avgDist;
        // 球体体积近似：4πr³/3 ≈ 4r³
        result.estimatedVolume = avgDist * avgDist * avgDist * 4.0f;

        // 混响时间 = 体积 × 0.008，限制在 0.2~2.5 秒
        result.targetReverbTime =
            std::max(0.2f, std::min(2.5f, result.estimatedVolume * 0.008f));

        // 反射系数：房间越小反射越强
        result.reflectivity =
            std::max(0.2f, std::min(0.8f, 1.0f - result.estimatedVolume / 5000.0f));

        // 低通截止频率：小房间低通更低（声波难以传播）
        result.targetLPF =
            std::max(2000.0f, std::min(18000.0f, 4000.0f + result.estimatedVolume * 3.0f));

        // 走廊检测：最大距/最小距 > 3 且命中率 > 85%
        float aspectRatio = (minDist > 0.001f) ? maxDist / minDist : 1.0f;
        result.isCorridor = (aspectRatio > 3.0f && hitRatio > 0.85f);
        result.echoLevel = result.isCorridor ?
            std::max(0.2f, std::min(0.6f, aspectRatio * 0.15f)) : 0.0f;
    }
    else
    {
        // 室外：无混响无回声
        result.isIndoor = false;
        result.targetReverbTime = 0.1f;
        result.reflectivity = 0.1f;
        result.targetLPF = 22000.0f;
        result.echoLevel = 0.0f;
    }

    return result;
}

void RoomAcousticsApplier::Initialize(SoLoud::Soloud* soloud, BusSystem* busSystem)
{
    mSoloud = soloud;
    mBusSystem = busSystem;

    // 初始化混响滤波器参数（初始为干声）
    mReverbFilter.setParams(0, 0.5f, 0.5f, 1.0f);
    mEchoFilter.setParams(0.2f, 0.6f);

    // 挂载到 Environment 总线的不同 slot
    mBusSystem->SetFilter(BusType::Environment, 0, &mReverbFilter);
    mBusSystem->SetFilter(BusType::Environment, 1, &mEchoFilter);
}

void RoomAcousticsApplier::SetTargetRoom(const RoomInfo& room)
{
    if (room.isIndoor)
    {
        mTargetReverbWet = std::max(0.0f, std::min(1.0f, room.reflectivity));
        mTargetReverbRoom = std::max(0.1f, std::min(1.0f, room.targetReverbTime / 2.5f));
        mTargetEchoWet = room.echoLevel;
    }
    else
    {
        // 室外目标：无混响无回声
        mTargetReverbWet = 0.0f;
        mTargetReverbRoom = 0.1f;
        mTargetEchoWet = 0.0f;
    }
}

void RoomAcousticsApplier::Update(float deltaTime)
{
    // 非对称混合速度：进入室内快（灵敏度高），回到室外慢（避免频繁切换抖动）
    float blendFactor = mCurrentReverbWet < mTargetReverbWet
        ? deltaTime * 2.0f    // 进入：快速响应
        : deltaTime * 6.0f;   // 退出：缓慢衰减

    blendFactor = std::min(blendFactor, 1.0f);

    // 平滑插值
    mCurrentReverbWet += (mTargetReverbWet - mCurrentReverbWet) * blendFactor;
    mCurrentReverbRoom += (mTargetReverbRoom - mCurrentReverbRoom) * blendFactor;
    mCurrentEchoWet += (mTargetEchoWet - mCurrentEchoWet) * blendFactor;

    // 将当前值应用到 Environment 总线的滤波器上
    SoLoud::handle envHandle = mBusSystem->GetBusHandle(BusType::Environment);

    // Slot 0: 混响滤波器 → 调节湿信号和空间大小
    mSoloud->fadeFilterParameter(envHandle, 0,
        SoLoud::FreeverbFilter::WET, mCurrentReverbWet, 0.1f);
    mSoloud->fadeFilterParameter(envHandle, 0,
        SoLoud::FreeverbFilter::ROOMSIZE, mCurrentReverbRoom, 0.1f);

    // Slot 1: 回声滤波器 → 调节回声强度
    mSoloud->fadeFilterParameter(envHandle, 1,
        SoLoud::EchoFilter::WET, mCurrentEchoWet, 0.1f);
}

} // namespace GAME::Audio