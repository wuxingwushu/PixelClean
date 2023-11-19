#pragma once
#include "Iterator.h"

template <typename T>
class PileUp
{
private:
    unsigned int Index = 0;
    unsigned int Max;
    T* mPileUp;
public:
    PileUp(unsigned int size):
        Max(size)
    {
        mPileUp = new T[Max];
    };

    ~PileUp() {
        delete mPileUp;
    };

    inline void add(T Parameter) noexcept {
        if (Index >= Max)
        {
            std::cout << "[PileUp]Error: GoBeyond" << std::endl;
            return;
        }
        //memcpy(&mPileUp[Index], &Parameter, sizeof(T));
        mPileUp[Index] = Parameter;
        ++Index;
    };

    [[nodiscard]] inline T* pop() noexcept {
        if (Index == 0)
        {
            std::cout << "[PileUp]Error: Empty" << std::endl;
            return nullptr;
        }
        --Index;
        return &mPileUp[Index];
    }

    [[nodiscard]] inline unsigned int GetNumber() const noexcept {
        return Index;
    }

    [[nodiscard]] inline T* GetData() noexcept {
        return mPileUp;
    }

    [[nodiscard]] inline T GetEnd() {
        return mPileUp[Index - 1];
    }

    inline void pop__() noexcept {
        if (Index == 0)return;
        --Index;
    }

    [[nodiscard]] inline T GetIndex(unsigned int I) {
        return mPileUp[I];
    }

    inline void ClearAll() noexcept {
        Index = 0;
    }

    inline Iterator<T> begin() {
        return Iterator<T>(mPileUp);
    }

    inline Iterator<T> end() {
        return Iterator<T>(mPileUp + Index);
    }
};