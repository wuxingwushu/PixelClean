#pragma once
#include <corecrt_math.h>

template <typename Type>
class GridNavigation
{
private:
	//网格道路查询回调函数
	typedef bool (*_GetGridRoadCallback)(int x, int y, void* ptr);
	_GetGridRoadCallback mGetGridRoadCallback = nullptr;
	void* wClass = nullptr;
	//静态网格宽高
	const unsigned int mWide;//宽
	const unsigned int mHigh;//高
	const unsigned int mRadius;//查询半径
	Type* mGridRoadWidth;//网格道路宽度
public:
	GridNavigation(const unsigned int wide, const unsigned int high, const unsigned int radius):
	mWide(wide), mHigh(high), mRadius(radius)
	{mGridRoadWidth = new Type[wide * high];}

	~GridNavigation(){delete[] mGridRoadWidth;}

	//设置回调函数
	void SetRoadCallback(_GetGridRoadCallback Callback, void* ptr) {
		mGetGridRoadCallback = Callback;
		wClass = ptr;
	}
	//扫描全部网格
	void ScanGridNavigation();
	//添加障碍物
	void add(unsigned int x, unsigned int y);
	//移除障碍物
	void pop(unsigned int x, unsigned int y);
	//获取点到最近障碍物的距离
	inline Type Get(unsigned int x, unsigned int y) noexcept { return mGridRoadWidth[x + y * mWide]; }
private:
	inline Type* at(unsigned int x, unsigned int y) noexcept {return &mGridRoadWidth[x + y * mWide];}

	inline unsigned int pow(unsigned int x) noexcept {return x * x;}

	//检测最近的障碍物
	unsigned int ScanRoadWidth(int x, int y);

	/*
	正方形边框检测
	以（x，y）为中心，检测（Bx，By）所在的正方形边框是否有障碍
	*/
	inline bool BoxIsEdge(int x, int y, int Bx, int By);
};

template <typename TData>
void GridNavigation<TData>::ScanGridNavigation() {
	for (size_t x = 0; x < mWide; x++)
	{
		for (size_t y = 0; y < mHigh; y++)
		{
			*at(x, y) = ScanRoadWidth(x, y);
		}
	}
}

template <typename TData>
void GridNavigation<TData>::add(unsigned int x, unsigned int y) {
	if (!mGetGridRoadCallback(x, y, wClass))return;//回调函数检测没有障碍物
	*at(x, y) = 0;
	int P1, P2, posR;
	for (int i = 1; i <= mRadius; i++)//一层一层向往遍历正方形边框
	{
		/*
		* 判断条件为，检测点到障碍距离 大于 当前检测框的最小距离
		* 重新计算 检测点 到 添加点 的距离
		*/
		posR = pow(i);
		P1 = x + i;
		P2 = x - i;
		for (int j = -i; j <= i; j++)
		{
			if (Get(P1, y + j) > i) { *at(P1, y + j) = sqrt(posR + pow(j)); }//上边框
			if (Get(P2, y + j) > i) { *at(P2, y + j) = sqrt(posR + pow(j)); }//下边框
		}
		P1 = y + i;
		P2 = y - i;
		for (int j = -(i - 1); j <= (i - 1); j++)//去除已经检测的头和尾
		{
			if (Get(x + j, P1) > i) { *at(x + j, P1) = sqrt(posR + pow(j)); }//左边框
			if (Get(x + j, P2) > i) { *at(x + j, P2) = sqrt(posR + pow(j)); }//右边框
		}
	}
}

template <typename TData>
void GridNavigation<TData>::pop(unsigned int x, unsigned int y) {
	if (mGetGridRoadCallback(x, y, wClass))return;//回调函数检测有障碍物
	*at(x, y) = ScanRoadWidth(x, y);
	int P1, P2, posR;
	for (int i = 1; i <= mRadius; i++)//一层一层向往遍历正方形边框
	{
		posR = sqrt(pow(i) * 2);
		P1 = x + i;
		P2 = x - i;
		for (int j = -i; j <= i; j++)
		{
			/*
			* 判断条件为，检测点到障碍距离 小于等于 当前检测框的最大距离
			* 进一步判断 检测点到障碍距离 等于 检测点到移除点的距离， 且 移除点在以检测点为中心的正方形边框没有障碍
			* 重新计算检测点到周围障碍最小距离
			*/
			if (Get(P1, y + j) <= posR) {//上边框
				if ((*at(P1, y + j) == (int)sqrt(pow(i) + pow(j))) && !BoxIsEdge(P1, y + j, x, y)) {
					*at(P1, y + j) = ScanRoadWidth(P1, y + j);
				}
			}
			if (Get(P2, y + j) <= posR) {//下边框
				if ((*at(P2, y + j) == (int)sqrt(pow(i) + pow(j))) && !BoxIsEdge(P2, y + j, x, y)) {
					*at(P2, y + j) = ScanRoadWidth(P2, y + j);
				}
			}
		}
		P1 = y + i;
		P2 = y - i;
		for (int j = -(i - 1); j <= (i - 1); j++)//去除已经检测的头和尾
		{
			if (Get(x + j, P1) <= posR) {//左边框
				if ((*at(x + j, P1) == (int)sqrt(pow(i) + pow(j))) && !BoxIsEdge(x + j, P1, x, y)) {
					*at(x + j, P1) = ScanRoadWidth(x + j, P1);
				}
			}
			if (Get(x + j, P2) <= posR) {//右边框
				if ((*at(x + j, P2) == (int)sqrt(pow(i) + pow(j))) && !BoxIsEdge(x + j, P2, x, y)) {
					*at(x + j, P2) = ScanRoadWidth(x + j, P2);
				}
			}
		}
	}
}

template <typename TData>
unsigned int GridNavigation<TData>::ScanRoadWidth(int x, int y) {
	if (mGetGridRoadCallback(x, y, wClass)) return 0;
	bool RoadTF = false;
	int P1, P2, posR = mRadius;
	for (int i = 1; i <= mRadius; i++)//一层一层向往遍历正方形边框
	{
		P1 = x + i;
		P2 = x - i;
		for (int j = -i; j <= i; j++)
		{
			if (mGetGridRoadCallback(P1, y + j, wClass)) { if (posR > abs(j))posR = abs(j); RoadTF = true; }//上边框
			if (mGetGridRoadCallback(P2, y + j, wClass)) { if (posR > abs(j))posR = abs(j); RoadTF = true; }//下边框
		}
		P1 = y + i;
		P2 = y - i;
		for (int j = -(i - 1); j <= (i - 1); j++)//去除已经检测的头和尾
		{
			if (mGetGridRoadCallback(x + j, P1, wClass)) { if (posR > abs(j))posR = abs(j); RoadTF = true; }//左边框
			if (mGetGridRoadCallback(x + j, P2, wClass)) { if (posR > abs(j))posR = abs(j); RoadTF = true; }//右边框
		}
		if (RoadTF)return sqrt(pow(posR) + pow(i));//已经在当前边框碰见了障碍，计算障碍的距离
	}
	return mRadius;//在范围内没有障碍
}

template <typename TData>
inline bool GridNavigation<TData>::BoxIsEdge(int x, int y, int Bx, int By) {
	int P1, P2, i;
	//计算(Bx,By)所在正方形边框的一半边长
	P1 = abs(x - Bx);
	P2 = abs(y - By);
	if(P1 > P2)i = P1;
	else i = P2;
	//遍历正方形边框
	P1 = x + i;
	P2 = x - i;
	for (int j = -i; j <= i; j++)
	{
		if (mGetGridRoadCallback(P1, y + j, wClass)) { return true; }//上边框
		if (mGetGridRoadCallback(P2, y + j, wClass)) { return true; }//下边框
	}
	P1 = y + i;
	P2 = y - i;
	for (int j = -(i - 1); j <= (i - 1); j++)//去除已经检测的头和尾
	{
		if (mGetGridRoadCallback(x + j, P1, wClass)) { return true; }//左边框
		if (mGetGridRoadCallback(x + j, P2, wClass)) { return true; }//右边框
	}
	return false;
}