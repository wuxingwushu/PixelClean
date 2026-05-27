// RTPC 控制器实现。
// SetGameParameter 自动钳位到 [min, max]，未注册参数自动创建默认 [0, 1] 范围。
// GetMappedValue：先查 gameParam 当前值，再在 mappings 中匹配 (gameParam, audioParam) 组合，对 curve 插值。
// InterpolateCurve：曲线外推取端点值，内插使用线性插值 t = (x - x0) / (x1 - x0)。
#include "RTPCController.h"
#include <algorithm>

namespace GAME::Audio {

void RTPCController::RegisterGameParameter(const std::string& name,
                                            float defaultValue,
                                            float minValue,
                                            float maxValue)
{
    mParams[name] = { defaultValue, minValue, maxValue };
}

void RTPCController::SetGameParameter(const std::string& name, float value)
{
    auto it = mParams.find(name);
    if (it != mParams.end())
    {
        // 已有注册 → 钳位到 [min, max]
        it->second.current = std::max(it->second.min, std::min(it->second.max, value));
    }
    else
    {
        // 未注册 → 自动创建默认 [0, 1] 范围
        mParams[name] = { value, 0.0f, 1.0f };
    }
}

float RTPCController::GetGameParameter(const std::string& name) const
{
    auto it = mParams.find(name);
    if (it != mParams.end())
        return it->second.current;
    return 0.0f;
}

void RTPCController::AddMapping(const RTPCMapping& mapping)
{
    mMappings.push_back(mapping);
}

float RTPCController::GetMappedValue(const std::string& gameParam,
                                      const std::string& audioParam) const
{
    // 获取游戏参数当前值
    float paramValue = 0.0f;
    auto paramIt = mParams.find(gameParam);
    if (paramIt != mParams.end())
        paramValue = paramIt->second.current;

    // 查找匹配的映射规则
    for (auto& mapping : mMappings)
    {
        if (mapping.gameParamName == gameParam && mapping.audioParamName == audioParam)
        {
            return InterpolateCurve(mapping.curve, paramValue);
        }
    }

    // 无匹配映射 → 直接返回参数值
    return paramValue;
}

float RTPCController::InterpolateCurve(
    const std::vector<RTPCCurvePoint>& curve, float x) const
{
    if (curve.empty()) return 0.0f;
    if (curve.size() == 1) return curve[0].y;

    // 外推：取端点值
    if (x <= curve.front().x) return curve.front().y;
    if (x >= curve.back().x) return curve.back().y;

    // 内插：找到 x 所在的区间 [curve[i], curve[i+1]] 线性插值
    for (size_t i = 0; i < curve.size() - 1; ++i)
    {
        if (x >= curve[i].x && x <= curve[i + 1].x)
        {
            float t = (x - curve[i].x) / (curve[i + 1].x - curve[i].x);
            return curve[i].y + t * (curve[i + 1].y - curve[i].y);
        }
    }
    return curve[0].y;
}

} // namespace GAME::Audio