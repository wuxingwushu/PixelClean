#pragma once

template <typename T>
class PileUp
{
private:
    unsigned int Index = 0;
    unsigned int Max;
    T* mPileUp;
public:
    PileUp(unsigned int size) {
        Max = size;
        mPileUp = new T[size];
    };

    ~PileUp() {
        delete mPileUp;
    };

    void add(T Parameter) {
        if (Index >= Max)
        {
            std::cout << "超出" << std::endl;
            return;
        }
        //memcpy(&mPileUp[Index], &Parameter, sizeof(T));
        mPileUp[Index] = Parameter;
        Index++;
    };

    T* pop() {
        if (Index == 0)
        {
            std::cout << "空" << std::endl;
            return nullptr;
        }
        Index--;
        return &mPileUp[Index];
    }

    unsigned int GetNumber() {
        return Index;
    }

    T GetEnd() {
        return mPileUp[Index - 1];
    }

    void pop__() {
        Index--;
    }

    T GetIndex(unsigned int I) {
        return mPileUp[I];
    }
};