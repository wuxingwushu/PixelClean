#pragma once
#include <time.h>
#include <assert.h>

enum QueueFlags_
{
    Queue_None      = 0,
    Queue_Timeout   = 1 << 1, // 开启  超时检测
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
    const unsigned int mMax;
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
        mQueue = new T[size];
        if (mFlags_ & Queue_Timeout) {
            mTimeS = new clock_t[size];
        }
    };

    ~Queue() {
        delete mQueue;
        if (mFlags_ & Queue_Timeout) {
            delete mTimeS;
        }
    };

    //添加
    inline void add(T Parameter) {
        if (mNumber == mMax)
        {
            std::cout << "[Queue]Error: GoBeyond" << std::endl;
            return;
        }
        ++mNumber;
        mQueue[TailIndex] = Parameter;
        if (mFlags_ & Queue_Timeout) {
            mTimeS[TailIndex] = clock();
        }
        TailIndex = Max(TailIndex + 1);
    };

    //弹出
    [[nodiscard]] inline T* pop() {
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

    void _pop() {
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
    [[nodiscard]] inline unsigned int GetNumber() {
        return mNumber;
    }

    //设置回调函数
    void SetPopCallback(unsigned int time, _PopCallback callback, void* _Class) {
        assert(mFlags_ & Queue_Timeout && "Not Turned On Queue_Timeout");
        mTime = time;
        mPopCallback = callback;
        mClass = _Class;
    }

    //超时事件
    inline void TimeoutEvent() {
        assert(mFlags_ & Queue_Timeout && "Not Turned On Queue_Timeout");
        if (mNumber == 0)return;

        if ((clock() - mTimeS[HeadIndex]) > mTime) {
            mPopCallback(&mQueue[HeadIndex], mClass);
            HeadIndex = Max(HeadIndex + 1);//弹出
        }
    }

    //拿队列数据初始化
    void InitData() {
        DataHeadIndex = HeadIndex;
        DataTailIndex = TailIndex - 1;
    }

    //按顺序拿数据，不是弹出(调用前先 InitData() )
    T* popData() {
        T* Parameter = &mQueue[DataHeadIndex];
        DataHeadIndex = Max(DataHeadIndex + 1);
        return Parameter;
    }

    T* addData() {
        T* Parameter = &mQueue[DataTailIndex];
        DataTailIndex = Max(DataTailIndex - 1);
        return Parameter;
    }
};