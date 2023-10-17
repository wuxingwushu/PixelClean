#pragma once
#include <map>

template<class DataT, class Handle>
class BlockData
{
public:
	struct BlockDataT
	{
		int mNumber = 0;			//数据有效数量
		DataT* DataS = nullptr;		//数据
		Handle* mHandle = nullptr;	//这些数据对于的引用对象

		//获得空数据索引
		unsigned int add() {
			return mNumber++;
		}

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

	//数据对于的索引信息
	struct BlockDataInfo
	{
		unsigned int mIndex;	//第几个数据
		BlockDataT* mBlockDataT;//数据在那个区块
	};
private:
	typedef void (*_Callback)(DataT* data, Handle* handle,void* ptr);//销毁 添加函数类型

	_Callback AddCallback = nullptr;
	_Callback PopCallback = nullptr;
	void* CallbackClass = nullptr;

	std::vector<DataT> DelayPopData;
	std::vector<DataT> DelayAddData;
	std::mutex mMutex;					// 互斥锁
	unsigned int mMax;					//数据最大
	unsigned int mBlockSize;			//区块数据数量
	BlockDataT* mBlockDataTS = nullptr;	//区块
	std::map<DataT, BlockDataInfo> Dictionary;	//索引对应数据
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
	//	数据总量，一个区块的数据量，一个区块的控制器
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

	//设置回调函数
	void SetCallback(_Callback _add, _Callback _pop, void* _Class) {
		AddCallback = _add;
		PopCallback = _pop;
		CallbackClass = _Class;
	}

	//延迟注册数据
	BlockDataInfo DelayRegistration() {
		std::lock_guard<std::mutex> lock(mMutex);  // 加锁
		unsigned int Index = mBlockDataAvailableS[mAvailableNumber - 1]->add();
		if (Index == 0) {
			Increase(mBlockDataAvailableS[mAvailableNumber - 1]);
		}
		if (mBlockDataAvailableS[mAvailableNumber - 1]->mNumber == mBlockSize) {
			return { Index, mBlockDataAvailableS[--mAvailableNumber] };
		}
		return { Index, mBlockDataAvailableS[mAvailableNumber - 1] };
	}

	//注册
	void Registration(BlockDataInfo info) {
		std::lock_guard<std::mutex> lock(mMutex);  // 加锁
		Dictionary[info.mBlockDataT->DataS[info.mIndex]] = info;
	}

	//延迟添加（线程安全）
	void DelayAddMutex(DataT data) {
		std::lock_guard<std::mutex> lock(mMutex);  // 加锁
		DelayAddData.push_back(data);
	}

	//延迟添加
	void DelayAdd(DataT data) {
		DelayAddData.push_back(data);
	}

	//添加全部延迟数据
	void WholeAdd() {
		for (auto data : DelayAddData)
		{
			add(data);
		}
		DelayAddData.clear();
	}

	//延迟弹出（线程安全）
	void DelayPopMutex(DataT data) {
		std::lock_guard<std::mutex> lock(mMutex);  // 加锁
		DelayPopData.push_back(data);
	}

	//延迟弹出
	void DelayPop(DataT data) {
		DelayPopData.push_back(data);
	}

	//弹出全部延迟数据
	void WholePop() {
		for (auto data : DelayPopData)
		{
			pop(data);
		}
		DelayPopData.clear();
	}

	//添加数据
	void add(DataT data) {
		unsigned int Index = mBlockDataAvailableS[mAvailableNumber - 1]->add(data);
		Dictionary[data] = BlockDataInfo{ Index, mBlockDataAvailableS[mAvailableNumber - 1] };
		if (Index == 0) {
			Increase(mBlockDataAvailableS[mAvailableNumber - 1]);
		}
		if (mBlockDataAvailableS[mAvailableNumber - 1]->mNumber == mBlockSize) {
			return mBlockDataAvailableS[--mAvailableNumber];
		}
		AddCallback(&mBlockDataAvailableS[mAvailableNumber - 1]->DataS[Index], mBlockDataAvailableS[mAvailableNumber - 1], CallbackClass);
	}

	//弹出数据
	void pop(DataT data) {
		if (Dictionary.find(data) != Dictionary.end())//判断是否存在键
		{
			BlockDataInfo info = Dictionary[data];
			PopCallback(&info.mBlockDataT->DataS[info.mIndex], info.mBlockDataT->mHandle, CallbackClass);
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
		}
	}

	unsigned int GetApplyNumber() { return mAtWorkNumber; }

	BlockDataT** GetBlockDataS() { return mBlockDataAtWorkS; }
};