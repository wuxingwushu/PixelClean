#include <map>
#include <time.h>

class Timer
{
public:
	unsigned int MomentCount = 0;//当前有多少个对象
	const char** mMomentNameS;//储存名字
	clock_t* mMomentTimerS;//当前耗时


	int mMomentStackNumber = -1;//堆的用量+1
	unsigned int* mMomentLabelStack;//记录标签
	clock_t* mMomentTimeStack;//记录时间
	/************************************************************************************************************/
public:
	const unsigned int mHeapNumber = 60;//记录对象多少次时间数据
	unsigned int Count = 0;//当前有多少个对象
	const char** mNameS;//储存名字
	bool* mHeapBoolS;//是否开启时间记录
	float* mTimerS;//当前耗时
	float** mTimeHeapS;//耗时记录
	int mTimeHeapIndexS = 0;//更新那个数据
	float* mTimerPercentageS;//耗时占周期的百分比
	float* mTimeMax;//耗时记录最大值
	float* mTimeMin;//耗时记录最小值
	clock_t CyclesNumberTime = 0;//一个周期的时间

private:
	const unsigned int CyclesNumber = 60;//循环多少次处理一次
	int mStackNumber = -1;//堆的用量+1
	unsigned int* mLabelStack;//记录标签
	clock_t* mTimeStack;//记录时间
	clock_t* mTimeS;//对象单次时间
	std::map<const char*, unsigned int> mTimerMap{};//名字，对应数据索引
	unsigned int CyclesCount = 0;//循环次数
	unsigned int mNumber;//最多检测多少个对象

public:
	Timer(unsigned int Number) {
		mNumber = Number;

		mMomentTimerS = new clock_t[mNumber];
		mMomentNameS = new const char* [mNumber];
		mMomentTimeStack = new clock_t[mNumber];
		mMomentLabelStack = new unsigned int[mNumber];

		mTimerPercentageS = new float[mNumber];
		mTimerS = new float[mNumber];
		mNameS = new const char* [mNumber];
		mHeapBoolS = new bool[mNumber];
		mTimeS = new clock_t[mNumber];
		mTimeHeapS = new float*[mNumber];
		mTimeStack = new clock_t[mNumber];
		mLabelStack = new unsigned int[mNumber];
		mTimeMax = new float[mNumber];
		mTimeMin = new float[mNumber];
	}

	~Timer() {
		delete mTimerPercentageS;
		delete mNameS;
		delete mTimeS;
		delete mTimerS;
		for (size_t i = 0; i < Count; i++)
		{
			if (mHeapBoolS[i])
			{
				delete mTimeHeapS[i];
			}
		}
		delete mTimeHeapS;
		delete mHeapBoolS;
		delete mTimeStack;
		delete mLabelStack;
		delete mTimeMax;
		delete mTimeMin;
	}

	void StartTiming(const char* name, bool RecordBool = false) {
		if (mTimerMap.find(name) == mTimerMap.end())//判断是否存在键
		{
			mTimerMap.insert(std::make_pair(name, Count));
			mNameS[Count] = name;
			mHeapBoolS[Count] = RecordBool;
			if (RecordBool)
			{
				mTimeHeapS[Count] = new float[mHeapNumber];
				mTimeMax[Count] = -1000.0f;
				mTimeMin[Count] = 1000.0f;
			}
			mTimeS[Count] = 0;
			mStackNumber++;
			mLabelStack[mStackNumber] = Count;
			Count++;
			mTimeStack[mStackNumber] = clock();
			
		}
		else
		{
			mStackNumber++;//堆++
			mLabelStack[mStackNumber] = mTimerMap[name];//获取数组标签
			mTimeStack[mStackNumber] = clock();//获取当前时间
		}
	}

	void StartEnd() {
		mTimeS[mLabelStack[mStackNumber]] += (clock() - mTimeStack[mStackNumber]);//当前时间 减去 开始时间 获取 耗时
		mStackNumber--;//堆--
	}

	void MomentTiming(const char* name) {
		if (mTimerMap.find(name) == mTimerMap.end())//判断是否存在键
		{
			mTimerMap.insert(std::make_pair(name, MomentCount));
			mMomentNameS[MomentCount] = name;
			mMomentTimerS[MomentCount] = 0;
			mMomentStackNumber++;
			mMomentLabelStack[mMomentStackNumber] = MomentCount;
			MomentCount++;
			mMomentTimeStack[mMomentStackNumber] = clock();
		}
		else {
			mMomentStackNumber++;
			mMomentLabelStack[mMomentStackNumber] = mTimerMap[name];
			mMomentTimeStack[mMomentStackNumber] = clock();
		}
	}

	void MomentEnd() {
		mMomentTimerS[mMomentLabelStack[mMomentStackNumber]] = clock() - mMomentTimeStack[mMomentStackNumber];
		mMomentStackNumber--;
	}

	void RefreshTiming() {
		if (CyclesCount >= CyclesNumber)
		{
			CyclesNumberTime = (clock() - CyclesNumberTime);
			CyclesCount = 0;
			for (size_t i = 0; i < Count; i++)
			{
				if (mHeapBoolS[i])
				{
					mTimerPercentageS[i] = float(mTimeS[i] * 100) / CyclesNumberTime;
					mTimeHeapS[i][mTimeHeapIndexS] = float(mTimeS[i]) / (CLOCKS_PER_SEC * CyclesNumber);
					mTimeS[i] = 0;
					if (mTimeHeapS[i][mTimeHeapIndexS] > mTimeMax[i]) {
						mTimeMax[i] = mTimeHeapS[i][mTimeHeapIndexS];
					}
					else if (mTimeHeapS[i][mTimeHeapIndexS] < mTimeMin[i]) {
						mTimeMin[i] = mTimeHeapS[i][mTimeHeapIndexS];
					}
				}
				else
				{
					mTimerPercentageS[i] = float(mTimeS[i] * 100) / CyclesNumberTime;
					mTimerS[i] = float(mTimeS[i]) / (CLOCKS_PER_SEC * CyclesNumber);
					mTimeS[i] = 0;
				}
			}
			mTimeHeapIndexS++;
			if (mTimeHeapIndexS >= mHeapNumber)
			{
				mTimeHeapIndexS = 0;
			}
			CyclesNumberTime = clock();
		}
		else
		{
			CyclesCount++;
		}
	}
};
