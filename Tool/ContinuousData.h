#pragma once
#include "Iterator.h"

template <typename TData>
class ContinuousData
{
private:
	TData* mData;
	unsigned int mMax = 0;//最多容量
	unsigned int mNumber = 0;//当前容量
public:
	ContinuousData(unsigned int size):
		mMax(size)
	{
		mData = new TData[mMax];
	};

	~ContinuousData() {
		delete mData;
	};

	inline void add(TData data) noexcept {
		if (mNumber == mMax)
		{
			Expansion();
		}
		mData[mNumber] = data;
		++mNumber;
	}

	inline void Delete(unsigned int index) noexcept {
		if (mNumber == 0)
		{
			return;
		}
		--mNumber;
		mData[index] = mData[mNumber];
	}


	inline void Delete(TData* Pointer) noexcept {
		--mNumber;
		*Pointer = mData[mNumber];
	}


	Iterator<TData> begin() {
		return Iterator<TData>(mData);
	}

	Iterator<TData> end() {
		return Iterator<TData>(mData + mNumber);
	}

	inline constexpr TData* Data() const noexcept {
		return mData;
	}

	[[nodiscard]] inline TData* GetData(unsigned int index) {
		return &mData[index];
	}

	[[nodiscard]] inline unsigned int GetNumber() const noexcept {
		return mNumber;
	}

	void Expansion() {
		TData* PData = mData;
		TData* LData = new TData[mMax * 2];
		TData* WData = LData;
		for (size_t i = 0; i < mMax; i++)
		{
			*LData = *PData;
			++PData;
			++LData;
		}
		delete mData;
		mData = WData;
		mMax *= 2;
	}
};