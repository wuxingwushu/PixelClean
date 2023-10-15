#pragma once
#include <map>

template<class DataT, class Handle, class Compare>
class BlockData
{
public:
	struct BlockDataT
	{
		int mNumber = 0;			//数据有效数量
		DataT* DataS = nullptr;		//数据
		Handle* mHandle = nullptr;	//这些数据对于的引用对象

		//添加数据
		unsigned int add(DataT data) {
			DataS[mNumber] = data;
			return mNumber++;
		}

		//删除数据
		DataT pop(unsigned int Index) {
			--mNumber;
			std::swap(DataS[Index], DataS[mNumber]);
			return DataS[Index];
		}
	};
private:

	//数据对于的索引信息
	struct BlockDataInfo
	{
		unsigned int mIndex;	//第几个数据
		BlockDataT* mBlockDataT;//数据在那个区块
	};

	unsigned int mMax;					//数据最大
	unsigned int mBlockSize;			//区块数据数量
	BlockDataT* mBlockDataTS = nullptr;	//区块
	std::map<DataT, BlockDataInfo, Compare> Dictionary;	//索引对应数据
	DataT* mData = nullptr;				//全部数据

	unsigned int mAvailableNumber = 0;			//没有饱和的区块数量
	BlockDataT** mBlockDataAvailableS = nullptr;//没有饱和的区块

	unsigned int mAtWorkNumber = 0;				//正在工作的区块数量
	BlockDataT** mBlockDataAtWorkS = nullptr;	//正在工作的区块

	void Increase(BlockDataT* Pointer) {
		mBlockDataAtWorkS[mAtWorkNumber++] = Pointer;
	}

	void Alignment(BlockDataT* Pointer) {
		for (size_t i = 0; i < mAtWorkNumber; i++)
		{
			if (mBlockDataAtWorkS[i] == Pointer) {
				--mAtWorkNumber;
				if (i == mAtWorkNumber) {
					return;
				}
				mBlockDataAtWorkS[i] = mBlockDataAtWorkS[mAtWorkNumber];
				return;
			}
		}
	}
public:
	BlockData(unsigned int Size, unsigned int BlockSize, Handle* HandleS)
		:mMax(Size), mBlockSize(BlockSize)
	{
		mAvailableNumber = (mMax / mBlockSize) + ((mMax % mBlockSize) == 0 ? 0 : 1);
		mBlockDataTS = new BlockDataT[mAvailableNumber];
		mBlockDataAvailableS = new BlockDataT*[mAvailableNumber];
		mBlockDataAtWorkS = new BlockDataT*[mAvailableNumber];
		mData = new DataT[mBlockSize * mAvailableNumber];
		DataT* P = mData;
		for (size_t i = 0; i < mAvailableNumber; ++i)
		{
			mBlockDataTS[i].DataS = P;
			mBlockDataTS[i].mHandle = HandleS;
			++HandleS;
			P += mBlockSize;
			mBlockDataAvailableS[i] = &(mBlockDataTS[i]);
		}
	}

	~BlockData()
	{
		delete mData;
		delete mBlockDataTS;
		delete mBlockDataAvailableS;
		delete mBlockDataAtWorkS;
	}

	BlockDataT* add(DataT data) {
		unsigned int Index = mBlockDataAvailableS[mAvailableNumber - 1]->add(data);
		//Dictionary.insert(std::make_pair(data, BlockDataInfo{ Index, mBlockDataAvailableS[mAvailableNumber - 1] }));
		Dictionary[data] = BlockDataInfo{ Index, mBlockDataAvailableS[mAvailableNumber - 1] };
		if (Index == 0) {
			Increase(mBlockDataAvailableS[mAvailableNumber - 1]);
		}
		if (mBlockDataAvailableS[mAvailableNumber - 1]->mNumber == mBlockSize) {
			return mBlockDataAvailableS[--mAvailableNumber];
		}
		return mBlockDataAvailableS[mAvailableNumber - 1];
	}

	BlockDataT* pop(DataT data) {
		if (Dictionary.find(data) != Dictionary.end())//判断是否存在键
		{
			BlockDataInfo info = Dictionary[data];
			
			if (info.mBlockDataT->mNumber == mBlockSize) {//因为要弹出，所有添加到未饱和状态
				mBlockDataAvailableS[mAvailableNumber++] = info.mBlockDataT;
			}
			else if(info.mBlockDataT->mNumber == 1){
				Alignment(info.mBlockDataT);
			}
			if (info.mBlockDataT->mNumber-1 == info.mIndex) {
				--info.mBlockDataT->mNumber;
			}
			else {
				DataT T = info.mBlockDataT->pop(info.mIndex);
				BlockDataT* i = Dictionary[T].mBlockDataT;
				Dictionary[T] = { info.mIndex, i };
			}
			Dictionary.erase(data);
			return info.mBlockDataT;
		}
	}

	unsigned int GetApplyNumber() { return mAtWorkNumber; }

	BlockDataT** GetBlockDataS() { return mBlockDataAtWorkS; }
};