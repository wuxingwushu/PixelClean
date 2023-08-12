#pragma once

template <typename T>
class Queue
{
private:
    unsigned int HeadIndex = 0;
    unsigned int TailIndex = 0;
    unsigned int mMax;
    unsigned int mNumber = 0;
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
            std::cout << "[Queue]Error: GoBeyond" << std::endl;
            return;
        }
        mQueue[TailIndex] = Parameter;
        mNumber++;
        TailIndex = Max(TailIndex + 1);
    };

    [[nodiscard]] T* pop() {
        if (TailIndex == HeadIndex)
        {
            std::cout << "[Queue]Error: Empty" << std::endl;
            return nullptr;
        }
        T Parameter = mQueue[HeadIndex];
        mNumber--;
        HeadIndex = Max(HeadIndex + 1);
        return &Parameter;
    }

    unsigned int Max(unsigned int Index) {
        if (Index >= mMax)
        {
            return 0;
        }
        return Index;
    };

    [[nodiscard]] unsigned int GetNumber() {
        return mNumber;
    }
};