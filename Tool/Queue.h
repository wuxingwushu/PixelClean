#pragma once

template <typename T>
class Queue
{
private:
    unsigned int HeadIndex = 0;
    unsigned int TailIndex = 0;
    unsigned int mMax;
    T* mQueue;
public:
    Queue(unsigned int size) {
        mMax = size + 1;
        mQueue = new T[size + 1];
    };

    ~Queue() {
        delete mQueue;
    };

    void add(T Parameter) {
        if (Max(TailIndex + 1) == HeadIndex)
        {
            std::cout << "超出" << std::endl;
            return;
        }
        mQueue[TailIndex] = Parameter;
        TailIndex = Max(TailIndex + 1);
    };

    T pop() {
        if (TailIndex == HeadIndex)
        {
            std::cout << "空" << std::endl;
            return 0;
        }
        T Parameter = mQueue[HeadIndex];
        HeadIndex = Max(HeadIndex + 1);
        return Parameter;
    }

    unsigned int Max(unsigned int Index) {
        if (Index >= mMax)
        {
            return 0;
        }
        return Index;
    };
};