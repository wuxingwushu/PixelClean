#pragma once
#include "Iterator.h"

template <typename TData>
class ContinuousData
{
private:
	TData* mData;				// 数据指针
	unsigned int mMax = 0;		// 最多容量
	unsigned int mNumber = 0;	// 当前容量
public:

	/**
	 * @brief 构造函数
	 * @param size 数组初始大小 */
	ContinuousData(unsigned int size):
		mMax(size)
	{
		mData = new TData[mMax];
	};

	~ContinuousData() {
		delete mData;
	};

	/**
	 * @brief 添加（引用）
	 * @param data 添加的数据 */
	inline void add(TData data) noexcept {
		if (mNumber == mMax)
		{
			Expansion();
		}
		mData[mNumber] = data;
		++mNumber;
	}

	/**
	 * @brief 销毁
	 * @param index 销毁第 index 的数据 */
	inline void Delete(unsigned int index) noexcept {
		if (mNumber == 0)
		{
			return;
		}
		--mNumber;
		mData[index] = mData[mNumber];
	}

	/**
	 * @brief 销毁对应数据
	 * @param Pointer 数据指针 */
	inline void Delete(TData* Pointer) noexcept {
		--mNumber;
		*Pointer = mData[mNumber];
	}

	/**
	 * @brief 迭代器头
	 * @return 迭代器 */
	Iterator<TData> begin() {
		return Iterator<TData>(mData);
	}

	/**
	 * @brief 迭代器尾
	 * @return 迭代器 */
	Iterator<TData> end() {
		return Iterator<TData>(mData + mNumber);
	}

	/**
	 * @brief 获取数据首指针
	 * @return 返回数据首指针 */
	inline constexpr TData* Data() const noexcept {
		return mData;
	}

	/**
	 * @brief 返回第 index 的数据指针
	 * @param index 第几个数据
	 * @return 返回对应数据指针 */
	[[nodiscard]] inline TData* GetData(unsigned int index) {
		return &mData[index];
	}

	/**
	 * @brief 获取数据数量
	 * @return 数据数量 */
	[[nodiscard]] inline unsigned int GetNumber() const noexcept {
		return mNumber;
	}


	/**
	 * @brief 数据扩容 */
	void Expansion() {
		TData* PData = mData;
		TData* LData = new TData[mMax * 2];
		TData* WData = LData;
		for (unsigned int i = 0; i < mMax; i++)
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