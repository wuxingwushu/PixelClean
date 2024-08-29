#pragma once
#include <unordered_map>
#include <vector>
#include <stack>
#define TimerWindowsTime
#ifdef TimerWindowsTime
#include <Windows.h>
#include <profileapi.h>
#else
#include <time.h>
#endif


struct Moment
{
	const char* Name;	//储存名字
	__int64 Timer;		//当前耗时
};


struct TimerTime
{
	const char* Name;			//储存名字
	bool HeapBool;				//是否开启时间记录
	float Time;					//当前耗时
	float* TimeHeap;			//历史耗时记录
	float Percentage;			//耗时占周期的百分比
	float TimeMax;				//耗时记录最大值
	float TimeMin;				//耗时记录最小值
	//中间数据
	__int64 MiddleTime;			//累积周期每次的耗时
};

struct TimerStack
{
	unsigned int Label;	//记录标签检测对象数组位置
	__int64 Time;		//周期时间累积
};



class Timer
{
public:
	const unsigned int mHeapNumber = 60;	//记录对象多少次时间数据
	std::vector<TimerTime> mTimerTimeS{};	//每个检测节点数据
	std::vector<Moment> mMomentS{};			//每个单次检测节点数据
	int mTimeHeapIndexS = 0;				//更新那个数据
	__int64 CyclesNumberTime = 0;			//一个周期的时间
private:
	std::stack<TimerStack> mStack;			//检测节点 栈
	std::stack<TimerStack> mMomentStack;	//单次检测节点 栈
	std::unordered_map<const char*, unsigned int> mTimerMap{};//名字，对应数据索引
	
	const unsigned int CyclesNumber = 60;	//循环多少次处理一次
	unsigned int CyclesCount = 0;			//循环次数

	auto GetTime() {
#ifdef TimerWindowsTime
		LARGE_INTEGER t1; // 频率
		QueryPerformanceCounter(&t1);
		return t1.QuadPart;
#else
		return clock();
#endif
	}

	__int64 GetClocks() {
#ifdef TimerWindowsTime
		LARGE_INTEGER freq; // 频率
		QueryPerformanceFrequency(&freq);
		return freq.QuadPart;
#else
		return CLOCKS_PER_SEC;
#endif
	}

public:
	Timer(){}
	~Timer(){}

	inline void StartTiming(const char* name, bool RecordBool = false) {
		if (mTimerMap.find(name) == mTimerMap.end())//判断是否存在键
		{
			mTimerMap.insert({ name, mTimerTimeS.size() });//记录唯一节点
			mTimerTimeS.push_back({//添加耗时检测节点
				name,
				RecordBool,
				0,
				RecordBool ? new float[mHeapNumber] : nullptr,
				0,
				-1000.0f,
				1000.0f,
				0
				});
			mStack.push({ (unsigned int)(mTimerTimeS.size() - 1), GetTime() });//入栈
		}
		else
		{
			mStack.push({ mTimerMap[name], GetTime() });//入栈
		}
	}

	inline void StartEnd() {
		mTimerTimeS[mStack.top().Label].MiddleTime += (GetTime() - mStack.top().Time);//当前时间 减去 开始时间 获取 耗时
		mStack.pop();//出栈
	}

	inline void MomentTiming(const char* name) {
		if (mTimerMap.find(name) == mTimerMap.end())//判断是否存在键
		{
			mTimerMap.insert({ name, mMomentS.size() });//记录唯一节点
			mMomentS.push_back({ name, 0 });//添加耗时检测节点
			mMomentStack.push({ (unsigned int)(mMomentS.size() - 1), GetTime() });//入栈
		}
		else {
			mMomentStack.push({ mTimerMap[name], GetTime() });//入栈
		}
	}

	inline void MomentEnd() {
		mMomentS[mMomentStack.top().Label].Timer = GetTime() - mMomentStack.top().Time;//当前时间 减去 开始时间 获取 耗时
		mMomentStack.pop();//出栈
	}

	inline void RefreshTiming() {
		if (CyclesCount >= CyclesNumber)
		{
			CyclesNumberTime = (GetTime() - CyclesNumberTime);//一个周期耗时
			CyclesCount = 0;//清空周期计数器
			for (auto &TimeData : mTimerTimeS)//处理每个耗时节点
			{
				TimeData.Percentage = float(TimeData.MiddleTime * 100) / CyclesNumberTime;//计算耗时占一个周期的百分比
				if (TimeData.HeapBool) {
					TimeData.TimeHeap[mTimeHeapIndexS] = float(TimeData.MiddleTime) / (GetClocks() * CyclesNumber);//记录这个周期的耗时
					if (TimeData.TimeHeap[mTimeHeapIndexS] > TimeData.TimeMax) {//最大值
						TimeData.TimeMax = TimeData.TimeHeap[mTimeHeapIndexS];
					}
					else if (TimeData.TimeHeap[mTimeHeapIndexS] < TimeData.TimeMin) {//最小值
						TimeData.TimeMin = TimeData.TimeHeap[mTimeHeapIndexS];
					}
				}
				else {
					TimeData.Time = float(TimeData.MiddleTime) / (GetClocks() * CyclesNumber);//记录这个周期的耗时
				}
				TimeData.MiddleTime = 0;
			}
			++mTimeHeapIndexS;
			if (mTimeHeapIndexS >= mHeapNumber)
			{
				mTimeHeapIndexS = 0;
			}
			CyclesNumberTime = GetTime();
		}
		else
		{
			++CyclesCount;
		}
	}
};
