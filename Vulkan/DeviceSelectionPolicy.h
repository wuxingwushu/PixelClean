#pragma once

//--------------------------------------------------------------------------------------------------
// “渲染设备”的选取优先级。
//
// 这是用户明确要求的一条产品规则：
//      GPU 独显 > CPU 集显 > CPU 软件渲染
//
// 单独放在这个头文件里、且**不依赖任何 Vulkan 头**，是为了让这条规则能脱离真实显卡被验证
// （本机只有一块没有 Vulkan 驱动的老核显，跑不出多设备场景）。Vulkan/device.cpp 与
// Vulkan/instance.cpp 都直接调用这里的实现，所以测试验过的就是真正在跑的代码，
// 不存在“测试里一份规则、程序里另一份规则”的脱节。
//--------------------------------------------------------------------------------------------------

#include <string>

namespace VulKan {

	//设备类型的档位。数值越大越优先，档位是**硬性**主排序键：
	//档位不同就绝不再看其它任何分数。
	//
	//为什么必须是硬性的：rateDevice() 的分数里混着 maxImageDimension2D（可达 32768），
	//于是“2D 上限 16384 的集显”会压过“2D 上限 8192 的老独显”；“没有几何着色器就 -100000”
	//也会把一台不支持几何着色器的独显压到集显下面。这两条都违反「独显 > 集显」，
	//所以分数只能降级为“同一档位内分优劣”的次级依据。
	//
	//这些整数与 VkPhysicalDeviceType 的取值一一对应（见 device.cpp 里的 static_assert 断言）。
	enum DeviceTier : int {
		DEVICE_TIER_CPU = 0,			//CPU 软件渲染（SwiftShader 等）
		DEVICE_TIER_VIRTUAL = 1,		//虚拟显卡
		DEVICE_TIER_OTHER = 1,			//未知/其它硬件
		DEVICE_TIER_INTEGRATED = 2,		//集显（核显）
		DEVICE_TIER_DISCRETE = 3,		//独显
	};

	//把 VkPhysicalDeviceType 的整数值映射成档位。
	//入参用 int 而不是 VkPhysicalDeviceType，就是为了让本头文件不依赖 Vulkan 头。
	//
	//这些整数就是 VkPhysicalDeviceType 的真实取值；device.cpp 里有 static_assert 钉住它们，
	//枚举值若变化会在编译期报错，不会静默错位。
	//
	//虚拟显卡与“其它”归到同一档（1）：Vulkan 只保证这五种设备类型，无法再细分；
	//它们仍是硬件加速设备（不是 CPU 软件渲染），但通常不如真实集显，所以排在集显之后、CPU 之前。
	inline int deviceTierFromType(int deviceType) {
		switch (deviceType) {
		case 2:  return DEVICE_TIER_DISCRETE;	//VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU   == 2
		case 1:  return DEVICE_TIER_INTEGRATED;	//VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU == 1
		case 3:  return DEVICE_TIER_VIRTUAL;	//VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU    == 3
		case 0:  return DEVICE_TIER_OTHER;		//VK_PHYSICAL_DEVICE_TYPE_OTHER          == 0
		case 4:  return DEVICE_TIER_CPU;		//VK_PHYSICAL_DEVICE_TYPE_CPU            == 4
		default: return DEVICE_TIER_OTHER;		//驱动报了没见过的类型：当未知硬件处理
		}
	}

	//一台候选设备里与“选谁”有关的全部信息（不含 Vulkan 句柄，便于单独测试）
	struct DeviceSelectionCandidate {
		int score = 0;					//rateDevice() 的分数：仅在同一个档位内用于分优劣
		bool suitable = false;			//是否满足最低要求（不满足时选出来只会崩）
		bool isCpu = false;				//是不是 CPU 软件渲染设备
		int tier = DEVICE_TIER_OTHER;	//性能档位：独显 3 > 集显 2 > 虚拟显卡/未知 1 > CPU 软件 0
		std::string name;				//设备名，供“指定设备”模式匹配
		std::string rejectReason;		//不满足最低要求时的原因（仅用于提示）
	};

	//候选 a 是否应该排在 b 前面。逐条比较、先决出的胜出，**判据的顺序就是优先级**：
	//  1) 可用的排前面（不满足硬性要求的设备选出来只会崩，见 physicalDeviceMeetsMinimumRequirements）
	//  2) CPU 模式下软件设备排最前；其余模式下硬件设备一律排在 CPU 软件设备前面 ——
	//     只要机器上有显卡，就绝不能因为分数把软件设备选出来
	//  3) 指定模式下点名的设备排最前（用户明确点的那台要压过“性能更高”的其它显卡）。
	//     注意这条排在 2) 之后，所以它只在**硬件设备之间**分胜负：点名一台 CPU 软件设备不会生效。
	//     这是有意的——界面上的设备列表本来就不含 CPU 设备（instance.cpp 探测时会跳过 CPU 设备，
	//     由列表里的「CPU 软件渲染」那一项代表，对应 wantCpuDevice），所以用户没有别的途径
	//     点名一台软件设备。何况指定设备模式下 instance.cpp 的 ensureVulkanIcdAvailable()
	//     也不会去加载软件 ICD，那台设备根本不会出现，这里“让硬件赢”不会掩盖任何真实需求。
	//  4) 性能档位：独显 > 集显 > 虚拟显卡/未知硬件 > CPU 软件渲染
	//  5) 同档位内才比 rateDevice() 的分数
	//
	//wantLowestScore（「自动选择最低性能」）把 4)、5) 两级一起反向 —— 连档位也反，
	//所以它在“有独显也有集显”的机器上会选集显，而不是“最高档位里分数最低的那张独显”。
	//这与该模式的语义一致（省电/兼容性测试：故意要最弱的那台）。但 2) 不反向：
	//只要有硬件设备，它在任何模式下都不会去选 CPU 软件渲染。
	inline bool candidatePrecedes(const DeviceSelectionCandidate& a, const DeviceSelectionCandidate& b,
		bool wantCpuDevice, bool wantSpecificDevice, bool wantLowestScore, const std::string& wantedName) {
		if (a.suitable != b.suitable) return a.suitable;//可用的排前面

		if (wantCpuDevice) {
			//CPU 软件渲染：软件设备排最前
			if (a.isCpu != b.isCpu) return a.isCpu;
			return a.score > b.score;
		}

		//其余模式：硬件设备一律排在 CPU 软件设备前面
		if (a.isCpu != b.isCpu) return b.isCpu;

		if (wantSpecificDevice) {
			//设置里指定了设备名：匹配的排最前（名字对不上就退化成“最高性能”）
			const bool aWanted = (a.name == wantedName);
			const bool bWanted = (b.name == wantedName);
			if (aWanted != bWanted) return aWanted;
		}

		//性能档位是硬性的主排序键：档位不同就不再看分数（分数只在同档位内有意义）
		if (a.tier != b.tier) return wantLowestScore ? (a.tier < b.tier) : (a.tier > b.tier);

		return wantLowestScore ? (a.score < b.score) : (a.score > b.score);
	}

}//namespace VulKan
