// 运行时参数控制器（RTPC = Real-Time Parameter Control）。
// 将游戏参数（如速度、血量、高度）映射到音频参数（如音高、低通截止频率）。
// 支持曲线映射：通过一系列 (x, y) 关键点定义非线性映射关系。
// 例如：速度 0..100 映射到低通频率 500..22000。
// 使用双线性插值在关键点之间平滑过渡。
#pragma once
#include <string>
#include <vector>
#include <unordered_map>

namespace GAME::Audio {

struct RTPCCurvePoint { float x, y; };

// RTPC 映射定义：gameParamName 的值通过 curve 映射到 audioParamName
struct RTPCMapping
{
    std::string gameParamName;                     // 游戏参数名（如 "Speed"）
    std::string audioParamName;                    // 音频参数名（如 "LPF_Freq"）
    std::vector<RTPCCurvePoint> curve;             // 映射曲线
    float defaultValue = 0.0f;                     // 默认输出值
};

class RTPCController
{
public:
    // 注册一个游戏参数，设置默认值和取值范围
    void RegisterGameParameter(const std::string& name,
                               float defaultValue,
                               float minValue = 0.0f,
                               float maxValue = 1.0f);

    // 更新游戏参数的当前值（自动钳位到 min/max）
    void SetGameParameter(const std::string& name, float value);

    // 查询游戏参数的当前值
    float GetGameParameter(const std::string& name) const;

    // 添加一条映射规则
    void AddMapping(const RTPCMapping& mapping);

    // 查询映射后的音频参数值：根据 gameParam 的当前值，在 mapping 的曲线上插值
    float GetMappedValue(const std::string& gameParam,
                         const std::string& audioParam) const;

private:
    // 在曲线上对 x 进行线性插值
    float InterpolateCurve(const std::vector<RTPCCurvePoint>& curve, float x) const;

    struct ParamInfo
    {
        float current;
        float min;
        float max;
    };
    std::unordered_map<std::string, ParamInfo> mParams;  // 游戏参数表
    std::vector<RTPCMapping> mMappings;                   // 映射规则表
};

} // namespace GAME::Audio