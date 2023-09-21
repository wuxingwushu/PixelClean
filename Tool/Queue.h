#pragma once

template <typename T>
class Queue
{
private:
    unsigned int HeadIndex = 0;
    unsigned int TailIndex = 0;
    const unsigned int mMax;
    unsigned int mNumber = 0;
    T* mQueue;

    unsigned int Max(unsigned int Index) {
        if (Index >= mMax)
        {
            return 0;
        }
        return Index;
    };
public:
    Queue(unsigned int size):mMax(size) {
        mQueue = new T[size];
    };

    ~Queue() {
        delete mQueue;
    };

    //添加
    void add(T Parameter) {
        if (mNumber == mMax)
        {
            std::cout << "[Queue]Error: GoBeyond" << std::endl;
            return;
        }
        ++mNumber;
        mQueue[TailIndex] = Parameter;
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
        T* Parameter = &mQueue[HeadIndex];
        HeadIndex = Max(HeadIndex + 1);
        return Parameter;
    }

    //队列数量
    [[nodiscard]] unsigned int GetNumber() {
        return mNumber;
    }
};