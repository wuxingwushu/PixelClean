#pragma once
#include "ContinuousData.h"
#include "Timer.h"

#define TOOL_MemoryPool		// 开启  内存池		(https://github.com/cacay/MemoryPool)//这个就根据自己的需求进行使用（线程不安全，建议为每一个线程申请一个内存池）
#define TOOL_MPMCQueue		// 开启  MPMC队列	(https://github.com/rigtorp/MPMCQueue)//线程安全的队列，先进先出
#define TOOL_ThreadPool		// 开启  线程池		(https://github.com/progschj/ThreadPool)
#define TOOL_SpdLog			// 开启  Spd日志	(https://github.com/gabime/spdlog)
//#define TOOL_Log			// 开启  轻量级日志 (https://github.com/rxi/log.c)
#define TOOL_PerlinNoise	// 开启  柏林噪声	(https://github.com/Huoi/noise-visualizer)
#define TOOL_ContinuousMap	// 开启  连续映射
#define TOOL_Convert		// 开启  类型转换
#define TOOL_FPS			// 开启  FPS检测
//#define TOOL_Timing			// 开启  耗时检测


#ifdef TOOL_MemoryPool
#include "MemoryPool.h"
#endif

#ifdef TOOL_MPMCQueue
#include "MPMCQueue.h"
#endif

#ifdef TOOL_ThreadPool
#include "ThreadPool.h"
#endif

#ifdef TOOL_SpdLog			//需要自己导入库
#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/basic_file_sink.h"
#endif

#ifdef TOOL_Log
#include "log.h"
#endif

#ifdef TOOL_PerlinNoise
#include "PerlinNoise.h"
#endif

#ifdef TOOL_ContinuousMap
#include "ContinuousMap.h"
#endif

#ifdef TOOL_Convert
#include <string>
#include <clocale>
#endif

#if defined TOOL_FPS || defined TOOL_Timing
#include <time.h>
#endif





namespace TOOL {
	extern Timer* mTimer;
	void InitTimer();


#ifdef TOOL_ThreadPool
	extern ThreadPool* mThreadPool;//声明全局类（唯一）
	extern unsigned int mThreadCount;

	void InitThreadPool(unsigned int ThreadCount = 0);//初始化线程池
	void DeleteThreadPool();//销毁线程池
#endif

#ifdef TOOL_SpdLog
	extern spdlog::logger* Error;
	extern spdlog::logger* VulKanError;

	void InitSpdLog();//初始化Spd日志
	void DeleteSpdLog();//销毁Spd日志
#endif

#ifdef TOOL_Log
	extern FILE* fpError;
	extern FILE* fpVulKanError;

	void InitLog();//初始化轻量级日志
	void DeleteLog();//销毁轻量级日志
#endif

#ifdef TOOL_PerlinNoise
	extern PerlinNoise* mPerlinNoise;

	void InitPerlinNoise();//初始化柏林噪声
	void DeletePerlinNoise();//销毁柏林噪声
#endif
	
#ifdef TOOL_Convert
	void* memcpyf(void* dest, const void* src, size_t n, size_t b);//反方向复制，

	template<typename T, typename U, size_t>void TypenameMemcpy(T* t, U* u, size_t n);

	template<typename T>std::string toString(const T& t);//常用于将各种类型转换为string类型

	void ConvertIntTo4Char(const int IntValue, char* Chars);//将一个int转换为4个char

	void Convert4CharToInt(const char* Chars, int* IntValue);//将4个char转换位一个int

	std::string ws2s(const std::wstring& ws);//string 转 wstring

	std::wstring s2ws(const std::string& s);//wstring 转 string

	std::string UnicodeToUtf8(const std::string& str);//Unicode 转到 utf8

	std::string Utf8ToUnicode(const std::string& utf8_str);//utf8 转到 Unicode
#endif

#ifdef TOOL_FPS
/************************************************- FPS -************************************************/
	extern clock_t kaishi_time, jieshu_time;//储存时间戳
	extern int number_time;//当前第几帧
	extern const int number;//多少帧刷新一次
	extern const double miao_time;//用来计算FPS的数
	extern double FPSNumber;//帧数
	extern double FPStime;//帧时间

	const int values_number = 60;//记录多少FPS数据
	extern float values[values_number];//储存FPS数据
	extern float Max_values; //FPS数据 最大值
	extern float Min_values;//FPS数据 最小值
	extern double Mean_values;//平均帧数

	extern void FPS();
#endif


#ifdef TOOL_Timing
/*********************************************- 耗时检测 -*********************************************/
	
/*
功能暂时不太完善，但是可以使用，按照规定使用就可以了，

1. 他们三，RefreshTiming()，EndTiming()，StartTiming()，必须在一次循环都得运行一边
2. 一个  StartTiming()  就必须有一个  StartEnd()  。
3. RefreshTiming() 只可以有一个。
4. StartTiming()， MomentTiming() 嵌套最多100层。
5. StartTiming()，设置的名字必须是唯一的，不可以重复。

6. MomentTiming(), MomentEnd()这是检测间接运行的代码片段的时耗
7. MomentTiming() 名字可以不唯一，需要一个 int 来储存索引。
8. 一个  MomentTiming()  就必须有一个  MomentEnd()  。

例子：
#include "Timing.h"//引用头文件就可以了

int main(){
	while(1){
	TOOL::StartTiming("唯一的名字", true);
	-- 被检测耗时的区域 --
	TOOL::StartEnd();

	RefreshTiming();
	}

	return 0;
}
*/
	
	extern clock_t TemporaryCycleTime;//周期总耗时开始时间戳
	extern clock_t CycleTime;//周期总耗时
	extern int Gap;//间隔
	extern int CurrentCount;//现在是第次轮回
	extern bool DetectionSwitch;//更新开关
	extern bool DetectionQuantityName;//第一次检测开关
	extern int Quantity;//总检测数量
	extern int MomentQuantity;
	extern int DetectionCount;//现在是检测第几个

	//嵌套堆载
	extern clock_t TemporaryConsumetime[100];//临时时间堆载
	extern clock_t TemporaryConsumeName[100];//临时嵌套索引堆载
	extern int TemporaryTimeQuantity; //堆载指针

	//结果数据
	const int DetectionNumber = 1000;//最多检测数量
	extern int ConsumeNumber;//最多检测数量
	extern char* Consume_name[DetectionNumber];//储存检测的名字
	extern char* MomentConsume_name[DetectionNumber];
	extern clock_t TemporaryConsume_time[DetectionNumber];//储存检测的周期总累加耗时
	extern double Consume_time[DetectionNumber];//储存检测的百分比
	extern double Consume_Second[DetectionNumber];//储存检测的秒
	extern double MomentConsume_Second[DetectionNumber];

	//时间记录
	const int SecondVectorNumber = 60;//最多记录检测数量
	extern bool SecondVectorBool[DetectionNumber]; //秒数据 最大值
	extern float* Consume_SecondVector[DetectionNumber];//储存检测的秒数组
	extern int SecondVectorIndex[DetectionNumber];//秒数组索引
	extern float Max_Secondvalues[SecondVectorNumber]; //秒数据 最大值
	extern float Min_Secondvalues[SecondVectorNumber];//秒数据 最小值

	//                       唯一名字       是否记录时间数据
	extern void StartTiming(char* name, bool RecordBool = false);
	extern void StartEnd();
	extern void RefreshTiming();
	//                       唯一名字     储存独立索引
	extern void MomentTiming(char* name, int* Index);
	extern void MomentEnd();
#endif
}