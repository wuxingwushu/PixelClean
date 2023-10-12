#pragma once
#include "Iterator.h"

template <typename TData>
class ContinuousData
{
private:
	TData* mData;
	unsigned int mMax;//最多容量
	unsigned int mNumber = 0;//当前容量
public:
	ContinuousData(unsigned int size) {
		mMax = size;
		mData = new TData[mMax];
	};

	~ContinuousData() {
		delete mData;
	};

	void add(TData data) {
		if (mNumber == mMax)
		{
			return;
		}
		mData[mNumber] = data;
		++mNumber;
	}

	void Delete(unsigned int index) {
		if (mNumber == 0)
		{
			return;
		}
		--mNumber;
		mData[index] = mData[mNumber];
	}


	void Delete(TData* Pointer) {
		--mNumber;
		*Pointer = mData[mNumber];
	}


	Iterator<TData> begin() {
		return Iterator<TData>(mData);
	}

	Iterator<TData> end() {
		return Iterator<TData>(mData + mNumber);
	}

	[[nodiscard]] constexpr TData* Data() const noexcept {
		return mData;
	}

	[[nodiscard]] TData* GetData(unsigned int index) {
		return &mData[index];
	}

	[[nodiscard]] unsigned int GetNumber() const noexcept {
		return mNumber;
	}
};