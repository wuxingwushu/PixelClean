#pragma once
#include <assert.h>

struct MovePlateInfo
{
	bool UpData = false;
	int X = 0;
	int Y = 0;
};

template<typename PlateT>
class MovePlate
{
	typedef void (*_GenerateCallback)(PlateT* mT, int x, int y, void* Data);//生成回调函数
	typedef void (*_DeleteCallback)(PlateT* mT, void* Data);//销毁回调函数
private:
	_GenerateCallback mGenerateCallback = nullptr;//生成回调函数
	void* mGenerateData = nullptr;
	_DeleteCallback mDeleteCallback = nullptr;//销毁回调函数
	void* mDeleteData = nullptr;

	const unsigned int mEdge = 1;					//板块的边长
	const unsigned int mNumberX = 0, mNumberY = 0;	//板块的数量
	const unsigned int mOriginX = 0, mOriginY = 0;	//那个板块为中心
	PlateT* mPlate = nullptr;

	int mPosX = 0, mPosY = 0;
private:
	PlateT* LPlate = nullptr;
public:
	[[nodiscard]] inline int GetPosX() { return mPosX; }
	[[nodiscard]] inline int GetPosY() { return mPosY; }

	[[nodiscard]] inline int GetPlateX() { return (mOriginX - mPosX) * mEdge; }
	[[nodiscard]] inline int GetPlateY() { return (mOriginY - mPosY) * mEdge; }

	[[nodiscard]] inline int GetCalculatePosX(int x) { return (x / mEdge) - mPosX + mOriginX; }
	[[nodiscard]] inline int GetCalculatePosY(int y) { return (y / mEdge) - mPosY + mOriginY; }

	constexpr MovePlate(const unsigned int NumberX, const unsigned int NumberY, const unsigned int Edge,
		const unsigned int OriginX = 0, const unsigned int OriginY = 0) :
		mNumberX(NumberX), mNumberY(NumberY), mEdge(Edge),
		mOriginX(OriginX), mOriginY(OriginY)
	{
		assert(mEdge != 0 && "Error : mEdge = 0");
		mPlate = (PlateT*)new char[mNumberX * mNumberY * sizeof(PlateT)];
		LPlate = (PlateT*)new char[(mNumberX > mNumberY ? mNumberX : mNumberY) * sizeof(PlateT)];
	}

	~MovePlate()
	{
		delete (char*)mPlate;
		delete (char*)LPlate;
	}

	[[nodiscard]] inline PlateT* PlateAt(unsigned int x, unsigned int y) {
		return &mPlate[x * mNumberY + y];
	}

	//设置回调函数
	inline void SetCallback(_GenerateCallback G, void* GData, _DeleteCallback D, void* DData) noexcept {
		mGenerateCallback = G;
		mGenerateData = GData;
		mDeleteCallback = D;
		mDeleteData = DData;
	}

	//设置位置
	inline void SetPos(float x, float y) noexcept {
		mPosX = x / mEdge;
		mPosY = y / mEdge;
	}

	inline PlateT* GetPlate(unsigned int x, unsigned int y) noexcept {
		if (x >= mNumberX || y >= mNumberY) {
			return nullptr;
		}
		return PlateAt(x, y);
	}

	inline PlateT* CalculateGetPlate(float x, float y) noexcept {
		unsigned int xx = (x / mEdge) - mPosX + mOriginX;
		unsigned int yy = (y / mEdge) - mPosY + mOriginY;
		if (xx >= mNumberX || yy >= mNumberY) {
			return nullptr;
		}
		return PlateAt(xx, yy);
	}

	//更新当前监测位置，判断是否需要移动板块，返回板块是否移动信息
	MovePlateInfo UpData(float x, float y) {
		MovePlateInfo Info;
		Info.X = (x / mEdge) - mPosX;
		Info.Y = (y / mEdge) - mPosY;
		if ((fabs(Info.X) < mNumberX) && (fabs(Info.Y) < mNumberY)) {
			if (Info.Y != 0) {
				mPosY += Info.Y;
				MovePlateY(Info.Y, Info.X);
				Info.UpData = true;
				return Info;
			}
			if (Info.X != 0) {
				mPosX += Info.X;
				MovePlateX(Info.X);
				Info.UpData = true;
				return Info;
			}
		}
		else {//全部都要更新
			mPosX += Info.X;
			mPosY += Info.Y;
			for (size_t Nx = 0; Nx < mNumberX; ++Nx)
			{
				for (size_t Ny = 0; Ny < mNumberY; ++Ny)
				{
					mDeleteCallback(PlateAt(Nx, Ny), mDeleteData);
					mGenerateCallback(PlateAt(Nx, Ny), (Nx + mPosX - mOriginX), (Ny + mPosY - mOriginY), mGenerateData);
				}
			}
			Info.UpData = true;
		}
		return Info;
	}

private:

	//板块 X 的移动
	void MovePlateX(int uX) {
		if (uX > 0) {
			for (size_t iy = 0; iy < mNumberY; ++iy)
			{
				for (size_t ix = 0; ix < uX; ++ix)
				{
					LPlate[ix] = *PlateAt(ix, iy);
				}
				for (size_t ix = 0; ix < (mNumberX - uX); ++ix)
				{
					*PlateAt(ix, iy) = *PlateAt(ix + uX, iy);
				}
				for (size_t ix = (mNumberX - uX); ix < mNumberX; ++ix)
				{
					*PlateAt(ix, iy) = LPlate[ix - (mNumberX - uX)];
					mDeleteCallback(PlateAt(ix, iy), mDeleteData);
					mGenerateCallback(PlateAt(ix, iy), (ix + mPosX - mOriginX), (iy + mPosY - mOriginY), mGenerateData);
				}
			}
		}
		else {
			for (size_t iy = 0; iy < mNumberY; ++iy)
			{
				for (size_t ix = 0; ix < -uX; ++ix)
				{
					LPlate[ix] = *PlateAt(mNumberX - ix - 1, iy);
				}
				for (int ix = (mNumberX + uX - 1); ix >= 0; --ix)
				{
					*PlateAt(ix - uX, iy) = *PlateAt(ix, iy);
				}
				for (size_t ix = 0; ix < -uX; ++ix)
				{
					*PlateAt(ix, iy) = LPlate[ix];
					mDeleteCallback(PlateAt(ix, iy), mDeleteData);
					mGenerateCallback(PlateAt(ix, iy), (ix + mPosX - mOriginX), (iy + mPosY - mOriginY), mGenerateData);
				}
			}
		}
	}

	//板块 Y 的移动
	void MovePlateY(int uY, int uX) {
		int tX, wX;
		if (uX > 0) {
			tX = uX;
			wX = 0;
		}
		else {
			tX = 0;
			wX = uX;
		}
		if (uY > 0) {
			for (size_t ix = 0; ix < mNumberX; ++ix)
			{
				for (size_t iy = 0; iy < uY; ++iy)
				{
					LPlate[iy] = *PlateAt(ix, iy);
				}
				for (size_t iy = 0; iy < (mNumberY - uY); ++iy)
				{
					*PlateAt(ix, iy) = *PlateAt(ix, iy + uY);
				}
				for (size_t iy = (mNumberY - uY); iy < mNumberY; ++iy)
				{
					*PlateAt(ix, iy) = LPlate[iy - (mNumberY - uY)];
					if ((ix >= tX) && (ix < (mNumberX + wX))) {
						mDeleteCallback(PlateAt(ix, iy), mDeleteData);
						mGenerateCallback(PlateAt(ix, iy), (ix + mPosX - mOriginX), (iy + mPosY - mOriginY), mGenerateData);
					}
				}
			}
		}
		else {
			for (size_t ix = 0; ix < mNumberX; ++ix)
			{
				for (size_t iy = 0; iy < -uY; ++iy)
				{
					LPlate[iy] = *PlateAt(ix, mNumberY - iy - 1);
				}
				for (int iy = (mNumberY + uY - 1); iy >= 0; --iy)
				{
					*PlateAt(ix, iy - uY) = *PlateAt(ix, iy);
				}
				for (size_t iy = 0; iy < -uY; ++iy)
				{
					*PlateAt(ix, iy) = LPlate[iy];
					if ((ix >= tX) && (ix < (mNumberX + wX))) {
						mDeleteCallback(PlateAt(ix, iy), mDeleteData);
						mGenerateCallback(PlateAt(ix, iy), (ix + mPosX - mOriginX), (iy + mPosY - mOriginY), mGenerateData);
					}
				}
			}
		}
	}
};
