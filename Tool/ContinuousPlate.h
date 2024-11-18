#pragma once
#include <assert.h>

template<typename T>
class ContinuousPlate
{
	typedef void (*_GenerateCallback)(T* mT, int x, int y, void* Data);//生成回调函数
	typedef void (*_DeleteCallback)(T* mT, void* Data);//销毁回调函数

private:
	int mX = 0, mY = 0;//当前位置
	unsigned int mEdge;//移动多少距离，板块跟着移动
	unsigned int mNumberX, mNumberY;//板块数量
	unsigned int mOriginX = 0, mOriginY = 0;//将那个板块设为原点
	T* mPlate;//板块

	int moveX = 0, moveY = 0;//板块移动

	_GenerateCallback GenerateCallback = nullptr;//生成回调函数
	void* GenerateData = nullptr;
	_DeleteCallback DeleteCallback = nullptr;//销毁回调函数
	void* DeleteData = nullptr;

	inline T* at(unsigned int x, unsigned int y) {
		assert((x * mNumberY + y) < (mNumberX * mNumberY) && "Error : Array Crossing The Boundary");
		return &mPlate[x * mNumberY + y];
	}

	//板块 X 的移动
	void MovePlateX(int uX) {
		int MX, MY;
		if (uX > 0)
		{
			for (size_t x = 0; x < uX; ++x)
			{
				for (size_t y = 0; y < mNumberY; ++y)
				{
					MX = moveX + x;
					MY = moveY + y;
					if (MX >= mNumberX)
					{
						MX -= mNumberX;
					}
					if (MY >= mNumberY)
					{
						MY -= mNumberY;
					}
					DeleteCallback(at(MX, MY), DeleteData);
					GenerateCallback(at(MX, MY), (x + mX - mOriginX), (y + mY - mOriginY), GenerateData);
				}
			}
			moveX += uX;
			if (moveX >= mNumberX) {
				moveX -= mNumberX;
			}
		}
		else
		{
			for (size_t x = (mNumberX + uX); x < mNumberX; ++x)
			{
				for (size_t y = 0; y < mNumberY; ++y)
				{
					MX = moveX + x;
					MY = moveY + y;
					if (MX >= mNumberX)
					{
						MX -= mNumberX;
					}
					if (MY >= mNumberY)
					{
						MY -= mNumberY;
					}
					DeleteCallback(at(MX, MY), DeleteData);
					GenerateCallback(at(MX, MY), (x + mX - mOriginX), (y + mY - mOriginY), GenerateData);
				}
			}
			moveX += uX;
			if (moveX < 0) {
				moveX += mNumberX;
			}
		}
	}

	//板块 Y 的移动
	void MovePlateY(int uY) {
		int MX, MY;
		if (uY > 0)
		{
			for (size_t x = 0; x < mNumberX; ++x)
			{
				for (size_t y = 0; y < uY; ++y)
				{
					MX = moveX + x;
					MY = moveY + y;
					if (MX >= mNumberX)
					{
						MX -= mNumberX;
					}
					if (MY >= mNumberY)
					{
						MY -= mNumberY;
					}
					DeleteCallback(at(MX, MY), DeleteData);
					GenerateCallback(at(MX, MY), (x + mX - mOriginX), (y + mY - mOriginY), GenerateData);
				}
			}
			moveY += uY;
			if (moveY >= mNumberY) {
				moveY -= mNumberY;
			}
		}
		else
		{
			for (size_t x = 0; x < mNumberX; ++x)
			{
				for (size_t y = mNumberY + uY; y < mNumberY; ++y)
				{
					MX = moveX + x;
					MY = moveY + y;
					if (MX >= mNumberX)
					{
						MX -= mNumberX;
					}
					if (MY >= mNumberY)
					{
						MY -= mNumberY;
					}
					DeleteCallback(at(MX, MY), DeleteData);
					GenerateCallback(at(MX, MY), (x + mX - mOriginX), (y + mY - mOriginY), GenerateData);
				}
			}
			moveY += uY;
			if (moveY < 0) {
				moveY += mNumberY;
			}
		}
	}

public:
	/**
	 * @brief 构造函数
	 * @param x 板块大小
	 * @param y 板块大小
	 * @param edge 板块间距 */
	ContinuousPlate(unsigned int x, unsigned int y, unsigned int edge) :
		mNumberX(x),
		mNumberY(y),
		mEdge(edge)
	{
		assert(mEdge != 0 && "Error : mEdge = 0");
		mPlate = new T[mNumberX * mNumberY];
	}

	/**
	 * @brief 设置起始位置
	 * @param x 起始位置
	 * @param y 起始位置 */
	inline void SetPos(float x, float y) noexcept {
		mX = int(x) / mEdge;
		mY = int(y) / mEdge;
	}

	/**
	 * @brief 设置回调函数
	 * @param G 生成回调函数
	 * @param GData 生成回调函数传入数据的指针
	 * @param D 销毁回调函数
	 * @param DData 销毁回调函数传入数据的指针 */
	inline void SetCallback(_GenerateCallback G, void* GData, _DeleteCallback D, void* DData) noexcept {
		GenerateCallback = G;
		GenerateData = GData;
		DeleteCallback = D;
		DeleteData = DData;
	}

	/**
	 * @brief 设置连续板块的中心
	 * @param x 中心位置
	 * @param y 中心位置 */
	inline void SetOrigin(unsigned int x, unsigned int y) noexcept {
		mOriginX = x;
		mOriginY = y;
	}

	~ContinuousPlate() {
		delete[] mPlate;
	}

	/**
	 * @brief 获取那个板块
	 * @param x 板块位置
	 * @param y 板块位置
	 * @return 板块指针 */
	[[nodiscard]] inline T* GetPlate(unsigned int x, unsigned int y) noexcept {
		int MX = moveX + x;
		int MY = moveY + y;
		if (MX >= mNumberX)
		{
			MX -= mNumberX;
		}
		if (MY >= mNumberY)
		{
			MY -= mNumberY;
		}
		return at(MX,MY);
	}

	/**
	 * @brief 更新当前监测位置
	 * @param x 位置
	 * @param y 位置
	 * @note 判断是否需要移动板块
	 * @return 返回板块是否移动 */
	bool UpData(int x, int y) {
		int uX = mX - (x / mEdge);
		int uY = mY - (y / mEdge);
		bool UpDataBool = false;
		if ((fabs(uX) < mNumberX) && (fabs(uY) < mNumberY)) {
			if (uX != 0)
			{
				mX -= uX;
				MovePlateX(uX);
				UpDataBool = true;
			}
			if (uY != 0)
			{
				mY -= uY;
				MovePlateY(uY);
				UpDataBool = true;
			}
		}
		else
		{
			mX = (x / mEdge);
			mY = (y / mEdge);
			for (size_t Nx = 0; Nx < mNumberX; ++Nx)
			{
				for (size_t Ny = 0; Ny < mNumberY; ++Ny)
				{
					DeleteCallback(at(Nx, Ny), DeleteData);
					GenerateCallback(at(Nx, Ny), (Nx + mX - mOriginX), (Ny + mY - mOriginY), GenerateData);
				}
			}
			UpDataBool = true;
		}
		return UpDataBool;
	}
};
