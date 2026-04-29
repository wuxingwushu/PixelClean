#pragma once

#include <unordered_map>
#include <vector>
#include <stack>
#include <cstdint>
#include <cfloat>
#include <limits>

#define TimerWindowsTime
#ifdef TimerWindowsTime
#include <Windows.h>
#include <profileapi.h>
#else
#include <time.h>
#endif


/*
====================================================
    Moment —— 单次耗时检测节点
    用于测量一次性操作的耗时（如初始化、窗口重构等），
    不跨周期累积，仅记录最近一次的原始时钟周期差值。
====================================================
*/
struct Moment
{
	const char* Name;
	int64_t Timer;
};

/*
====================================================
    TimerTime —— 周期性耗时检测节点
    用于测量每帧重复执行的代码段耗时（如游戏逻辑、渲染等），
    每 CyclesNumber 帧为一个统计周期，计算平均耗时和占比。

    TimeHeap 使用 std::vector<float> 管理历史记录数组，
    自动处理内存分配与释放，避免裸指针带来的内存泄漏
    和违反三/五法则的风险。
====================================================
*/
struct TimerTime
{
	const char* Name;
	bool HeapBool;
	float Time;
	std::vector<float> TimeHeap;
	float Percentage;
	float TimeMax;
	float TimeMin;
	int64_t MiddleTime;
};

/*
====================================================
    TimerStack —— 栈帧数据
    用于支持嵌套计时的压栈/出栈操作。
    Label 记录当前节点在对应 vector 中的索引，
    Time 记录压栈时刻的时间戳。
====================================================
*/
struct TimerStack
{
	unsigned int Label;
	int64_t Time;
};


/*
====================================================
    Timer —— 性能耗时检测器
    提供两种计时模式：
      1. 周期性计时（StartTiming/StartEnd/RefreshTiming）
         - 适用于每帧重复执行的代码段
         - 每 CyclesNumber 帧汇总一次统计数据
         - 可选开启历史记录（HeapBool），保留最近 mHeapNumber 个周期的耗时
      2. 单次计时（MomentTiming/MomentEnd）
         - 适用于一次性或非周期性操作
         - 仅记录最近一次的原始时钟周期差

    内部使用栈结构支持嵌套计时，使用哈希表实现 O(1) 名称查找。
====================================================
*/
class Timer
{
public:
	static constexpr unsigned int mHeapNumber = 60;
	static constexpr unsigned int CyclesNumber = 60;

	std::vector<TimerTime> mTimerTimeS{};
	std::vector<Moment> mMomentS{};
	unsigned int mTimeHeapIndexS = 0;
	int64_t CyclesNumberTime = 0;

private:
	std::stack<TimerStack> mStack;
	std::stack<TimerStack> mMomentStack;
	std::unordered_map<const char*, unsigned int> mTimerTimeMap{};
	std::unordered_map<const char*, unsigned int> mMomentMap{};

	unsigned int CyclesCount = 0;

	/*
	    GetTime —— 获取当前高精度时间戳
	    Windows 平台使用 QueryPerformanceCounter 获取 CPU 级精度计数器，
	    其他平台回退到 clock() 函数。返回值统一为 int64_t 类型。
	*/
	int64_t GetTime() {
#ifdef TimerWindowsTime
		LARGE_INTEGER t1;
		QueryPerformanceCounter(&t1);
		return t1.QuadPart;
#else
		return static_cast<int64_t>(clock());
#endif
	}

	/*
	    GetClocks —— 获取时钟频率
	    Windows 平台使用 QueryPerformanceFrequency 获取每秒计数器增量，
	    其他平台使用 CLOCKS_PER_SEC。用于将时钟周期差转换为秒。
	*/
	int64_t GetClocks() {
#ifdef TimerWindowsTime
		LARGE_INTEGER freq;
		QueryPerformanceFrequency(&freq);
		return freq.QuadPart;
#else
		return static_cast<int64_t>(CLOCKS_PER_SEC);
#endif
	}

public:
	/*
	    构造函数 —— 初始化周期起始时间戳
	    将 CyclesNumberTime 设为当前时间，避免首次周期计算时
	    因初始值为 0 而导致耗时计算为从系统启动至今的总时间。
	*/
	Timer() : CyclesNumberTime(GetTime()) {}

	/*
	    析构函数 —— 无需手动释放资源
	    由于 TimeHeap 已改为 std::vector<float>，
	    vector 析构时会自动释放内存，无需手动 delete[]。
	*/
	~Timer() = default;

	/*
	    StartTiming —— 开始周期性计时
	    参数：
	      name       - 计时节点名称，同时作为唯一标识键
	      RecordBool - 是否开启历史耗时记录（开启后保留最近 mHeapNumber 个周期的数据）

	    流程：
	      1. 在 mTimerTimeMap 中查找名称，若不存在则创建新节点并插入
	      2. 将节点索引和当前时间戳压入 mStack，支持嵌套调用
	*/
	inline void StartTiming(const char* name, bool RecordBool = false) {
		auto it = mTimerTimeMap.find(name);
		if (it == mTimerTimeMap.end())
		{
			unsigned int index = static_cast<unsigned int>(mTimerTimeS.size());
			mTimerTimeMap.insert({ name, index });
			mTimerTimeS.push_back({
				name,
				RecordBool,
				0.0f,
				RecordBool ? std::vector<float>(mHeapNumber, 0.0f) : std::vector<float>{},
				0.0f,
				-FLT_MAX,
				FLT_MAX,
				0
				});
			mStack.push({ index, GetTime() });
		}
		else
		{
			mStack.push({ it->second, GetTime() });
		}
	}

	/*
	    StartEnd —— 结束最近一次周期性计时
	    从栈顶取出起始时间戳，计算本次耗时并累加到 MiddleTime，
	    然后弹出栈顶元素。

	    安全检查：若栈为空（StartEnd 与 StartTiming 不匹配），
	    直接返回避免栈下溢崩溃。
	*/
	inline void StartEnd() {
		if (mStack.empty()) return;
		mTimerTimeS[mStack.top().Label].MiddleTime += (GetTime() - mStack.top().Time);
		mStack.pop();
	}

	/*
	    MomentTiming —— 开始单次计时
	    与 StartTiming 类似，但使用独立的 mMomentMap 和 mMomentStack，
	    避免与周期性计时节点名称冲突导致索引越界。
	*/
	inline void MomentTiming(const char* name) {
		auto it = mMomentMap.find(name);
		if (it == mMomentMap.end())
		{
			unsigned int index = static_cast<unsigned int>(mMomentS.size());
			mMomentMap.insert({ name, index });
			mMomentS.push_back({ name, 0 });
			mMomentStack.push({ index, GetTime() });
		}
		else {
			mMomentStack.push({ it->second, GetTime() });
		}
	}

	/*
	    MomentEnd —— 结束最近一次单次计时
	    计算原始时钟周期差值并存储，不进行周期累积。

	    安全检查：若栈为空，直接返回避免栈下溢崩溃。
	*/
	inline void MomentEnd() {
		if (mMomentStack.empty()) return;
		mMomentS[mMomentStack.top().Label].Timer = GetTime() - mMomentStack.top().Time;
		mMomentStack.pop();
	}

	/*
	    RefreshTiming —— 刷新周期性计时统计数据
	    每帧调用一次。每 CyclesNumber 帧执行一次统计计算：

	    1. 计算本周期总耗时 CyclesNumberTime
	    2. 遍历所有周期性节点：
	       - 计算耗时占周期的百分比（Percentage）
	       - 若开启历史记录（HeapBool），将本周期平均耗时写入环形缓冲区，
	         并更新最大值/最小值
	       - 若未开启历史记录，仅记录本周期平均耗时（Time）
	    3. 重置 MiddleTime 为 0，等待下一周期累积
	    4. 推进环形缓冲区索引 mTimeHeapIndexS

	    安全检查：CyclesNumberTime 和时钟频率为 0 时跳过计算，
	    避免除零异常。
	*/
	inline void RefreshTiming() {
		if (CyclesCount >= CyclesNumber)
		{
			int64_t currentTime = GetTime();
			int64_t cycleDuration = currentTime - CyclesNumberTime;
			CyclesCount = 0;

			int64_t clocks = GetClocks();
			if (cycleDuration <= 0 || clocks <= 0) {
				CyclesNumberTime = currentTime;
				return;
			}

			int64_t totalCycleSeconds = clocks * CyclesNumber;

			for (auto& TimeData : mTimerTimeS)
			{
				TimeData.Percentage = static_cast<float>(TimeData.MiddleTime) * 100.0f / static_cast<float>(cycleDuration);

				if (TimeData.HeapBool) {
					float seconds = static_cast<float>(TimeData.MiddleTime) / static_cast<float>(totalCycleSeconds);
					TimeData.TimeHeap[mTimeHeapIndexS] = seconds;
					if (seconds > TimeData.TimeMax) {
						TimeData.TimeMax = seconds;
					}
					if (seconds < TimeData.TimeMin) {
						TimeData.TimeMin = seconds;
					}
				}
				else {
					TimeData.Time = static_cast<float>(TimeData.MiddleTime) / static_cast<float>(totalCycleSeconds);
				}
				TimeData.MiddleTime = 0;
			}

			mTimeHeapIndexS = (mTimeHeapIndexS + 1) % mHeapNumber;
			CyclesNumberTime = currentTime;
		}
		else
		{
			++CyclesCount;
		}
	}
};


/*
====================================================
    TimerScope —— RAII 周期计时守卫
    在构造时调用 StartTiming，析构时自动调用 StartEnd，
    确保即使代码路径因异常或提前返回而跳过 End 调用，
    也不会造成栈状态不一致。

    用法：
        {
            TimerScope scope(timer, "游戏逻辑", true);
            GameLoop();
        }  // 自动调用 StartEnd
====================================================
*/
class TimerScope
{
public:
	TimerScope(Timer* timer, const char* name, bool recordBool = false)
		: mTimer(timer), mActive(true)
	{
		if (mTimer) mTimer->StartTiming(name, recordBool);
	}

	~TimerScope()
	{
		if (mActive && mTimer) mTimer->StartEnd();
	}

	TimerScope(const TimerScope&) = delete;
	TimerScope& operator=(const TimerScope&) = delete;

	void Cancel() { mActive = false; }

private:
	Timer* mTimer;
	bool mActive;
};

/*
====================================================
    MomentScope —— RAII 单次计时守卫
    与 TimerScope 类似，用于 MomentTiming/MomentEnd 配对。

    用法：
        {
            MomentScope scope(timer, "初始化窗口");
            initWindow();
        }  // 自动调用 MomentEnd
====================================================
*/
class MomentScope
{
public:
	MomentScope(Timer* timer, const char* name)
		: mTimer(timer), mActive(true)
	{
		if (mTimer) mTimer->MomentTiming(name);
	}

	~MomentScope()
	{
		if (mActive && mTimer) mTimer->MomentEnd();
	}

	MomentScope(const MomentScope&) = delete;
	MomentScope& operator=(const MomentScope&) = delete;

	void Cancel() { mActive = false; }

private:
	Timer* mTimer;
	bool mActive;
};
