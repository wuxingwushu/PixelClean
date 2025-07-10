#pragma once
#include <time.h>
#include <assert.h>
#include <iostream>

enum QueueFlags_
{
    Queue_None      = 0,
    Queue_Timeout   = 1 << 0,   // 开启  超时检测
    Queue_Expansion = 1 << 1,   // 开启  扩容
};

template <typename T>
class Queue
{
private:
    typedef void (*_PopCallback)(T* data, void* ptr);

    unsigned int DataHeadIndex = 0;//从队列拿数据，但是不是弹出数据、
    unsigned int DataTailIndex = 0;

    unsigned int HeadIndex = 0;
    unsigned int TailIndex = 0;
    unsigned int mMax;
    unsigned int mNumber = 0;
    QueueFlags_ mFlags_;

    T* mQueue = nullptr;
   
    
    clock_t* mTimeS = nullptr;
    unsigned int mTime = 2000;
    _PopCallback mPopCallback = nullptr;//弹出回调函数
    void* mClass = nullptr;

    inline unsigned int Max(unsigned int Index) {
        if (Index >= mMax)
        {
            return 0;
        }
        return Index;
    };
public:
    constexpr Queue(unsigned int size, QueueFlags_ flags = Queue_None):mMax(size), mFlags_(flags){
        mQueue = new T[mMax];
        if (mFlags_ & Queue_Timeout) {
            mTimeS = new clock_t[mMax];
        }
    };

    ~Queue() {
        delete mQueue;
        if (mFlags_ & Queue_Timeout) {
            delete mTimeS;
        }
    };

    //添加
    inline void add(T Parameter) noexcept {
        if (mNumber == mMax)
        {
            if (mFlags_ & Queue_Expansion) {
                Expansion();
            }
            else {
                std::cout << "[Queue]Error: GoBeyond" << std::endl;
                return;
            }
        }
        ++mNumber;
        mQueue[TailIndex] = Parameter;
        if (mFlags_ & Queue_Timeout) {
            mTimeS[TailIndex] = clock();
        }
        TailIndex = Max(TailIndex + 1);
    };

    //弹出
    [[nodiscard]] inline T* pop() noexcept {
        assert(!(mFlags_ & Queue_Timeout) && "Queue_Timeout Manual Not Supported pop");

        if (mNumber == 0)
        {
            std::cout << "[Queue]Error: Empty" << std::endl;
            return nullptr;
        }
        --mNumber;
        T* Parameter = &mQueue[HeadIndex];
        HeadIndex = Max(HeadIndex + 1);
        return Parameter;
    }

    inline void _pop() noexcept {
        assert(!(mFlags_ & Queue_Timeout) && "Queue_Timeout Manual Not Supported pop");
        if (mNumber == 0)
        {
            std::cout << "[Queue]Error: Empty" << std::endl;
            return;
        }
        --mNumber;
        HeadIndex = Max(HeadIndex + 1);
    }

    //队列数量
    [[nodiscard]] inline unsigned int GetNumber() noexcept {
        return mNumber;
    }

    //设置回调函数
    inline void SetPopCallback(unsigned int time, _PopCallback callback, void* _Class) noexcept {
        assert(mFlags_ & Queue_Timeout && "Not Turned On Queue_Timeout");
        mTime = time;
        mPopCallback = callback;
        mClass = _Class;
    }

    //超时事件
    inline void TimeoutEvent() noexcept {
        assert(mFlags_ & Queue_Timeout && "Not Turned On Queue_Timeout");
        if (mNumber == 0)return;

        if ((clock() - mTimeS[HeadIndex]) > mTime) {
            mPopCallback(&mQueue[HeadIndex], mClass);
            HeadIndex = Max(HeadIndex + 1);//弹出
        }
    }

    void Expansion() {
        T* PQueue = mQueue;
        T* LQueue = new T[mMax * 2];
        T* WQueue = LQueue;
        for (size_t i = 0; i < mMax; i++)
        {
            *LQueue = *PQueue;
            ++PQueue;
            ++LQueue;
        }
        delete mQueue;
        mQueue = WQueue;
        if (mFlags_ & Queue_Timeout) {
            clock_t* PTimeS = mTimeS;
            clock_t* LTimeS= new clock_t[mMax * 2];
            clock_t* WTimeS = LTimeS;
            for (size_t i = 0; i < mMax; i++)
            {
                *LTimeS = *PTimeS;
                ++PTimeS;
                ++LTimeS;
            }
            delete mTimeS;
            mTimeS = WTimeS;
        }

        mMax *= 2;
    }

    //拿队列数据初始化
    inline void InitData() noexcept {
        DataHeadIndex = HeadIndex;
        DataTailIndex = TailIndex - 1;
    }

    //按顺序拿数据，不是弹出(调用前先 InitData() )
    [[nodiscard]] inline T* popData() noexcept {
        T* Parameter = &mQueue[DataHeadIndex];
        DataHeadIndex = Max(DataHeadIndex + 1);
        return Parameter;
    }

    [[nodiscard]] inline T* addData() noexcept {
        T* Parameter = &mQueue[DataTailIndex];
        DataTailIndex = Max(DataTailIndex - 1);
        return Parameter;
    }
};