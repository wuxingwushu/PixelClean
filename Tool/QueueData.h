#pragma once

template <typename T>
class QueueData
{
private:
    unsigned int DataHeadIndex = 0;//从队列拿数据，但是不是弹出数据、
    unsigned int DataTailIndex = 0;

    unsigned int HeadIndex = 0;
    unsigned int TailIndex = 0;
    const unsigned int mMax;
    unsigned int mNumber = 0;
    T* mQueueData;

    unsigned int Max(unsigned int Index) {
        if (Index >= mMax)
        {
            return 0;
        }
        return Index;
    };
public:
    QueueData(unsigned int size) :mMax(size) {
        mQueueData = new T[size];
    };

    ~QueueData() {
        delete mQueueData;
    };

    //添加
    void add(T Parameter) {
        if (mNumber == mMax)
        {
            std::cout << "[Queue]Error: GoBeyond" << std::endl;
            return;
        }
        ++mNumber;
        mQueueData[TailIndex] = Parameter;
        TailIndex = Max(TailIndex + 1);
    };

    //弹出
    [[nodiscard]] T* pop() {
        if (mNumber == 0)
        {
            std::cout << "[Queue]Error: Empty" << std::endl;
            return nullptr;
        }
        --mNumber;
        T* Parameter = &mQueueData[HeadIndex];
        HeadIndex = Max(HeadIndex + 1);
        return Parameter;
    }

    //队列数量
    [[nodiscard]] unsigned int GetNumber() {
        return mNumber;
    }

    //拿队列数据初始化
    void InitData() {
        DataHeadIndex = HeadIndex;
        DataTailIndex = TailIndex-1;
    }

    //按顺序拿数据，不是弹出(调用前先 InitData() )
    T* popData() {
        T* Parameter = &mQueueData[DataHeadIndex];
        DataHeadIndex = Max(DataHeadIndex + 1);
        return Parameter;
    }

    T* addData() {
        T* Parameter = &mQueueData[DataTailIndex];
        DataTailIndex = Max(DataTailIndex - 1);
        return Parameter;
    }
};