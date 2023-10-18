#pragma once

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
	PlateT** mPlate = nullptr;

	int mPosX = 0, mPosY = 0;
private:
	PlateT** LPlateX = nullptr;
	PlateT* LPlateY = nullptr;
public:
	int GetPosX() { return mPosX; }
	int GetPosY() { return mPosY; }

	int GetPlateX() { return (mOriginX - mPosX) * mEdge; }
	int GetPlateY() { return (mOriginY - mPosY) * mEdge; }

	int GetCalculatePosX(int x) { return (x / mEdge) - mPosX + mOriginX; }
	int GetCalculatePosY(int y) { return (y / mEdge) - mPosY + mOriginY; }

	constexpr MovePlate(const unsigned int NumberX, const unsigned int NumberY, const unsigned int Edge,
		const unsigned int OriginX = 0, const unsigned int OriginY = 0) :
		mNumberX(NumberX), mNumberY(NumberY), mEdge(Edge),
		mOriginX(OriginX), mOriginY(OriginY)
	{
		mPlate = new PlateT*[mNumberX];
		for (size_t i = 0; i < mNumberX; ++i)
		{
			mPlate[i] = new PlateT[mNumberY];
		}
		LPlateX = new PlateT*[mNumberX];	
		LPlateY = new PlateT[mNumberY];
	}

	~MovePlate()
	{
		for (size_t i = 0; i < mNumberX; ++i)
		{
			delete mPlate[i];
		}
		delete mPlate;
		delete LPlateX;
		delete LPlateY;
	}

	//设置回调函数
	inline void SetCallback(_GenerateCallback G, void* GData, _DeleteCallback D, void* DData) {
		mGenerateCallback = G;
		mGenerateData = GData;
		mDeleteCallback = D;
		mDeleteData = DData;
	}

	//设置位置
	inline void SetPos(float x, float y) {
		mPosX = x / mEdge;
		mPosY = y / mEdge;
	}

	PlateT* GetPlate(unsigned int x, unsigned int y) {
		if (x >= mNumberX || y >= mNumberY) {
			return nullptr;
		}
		return &mPlate[x][y];
	}

	PlateT* CalculateGetPlate(float x, float y) {
		unsigned int xx = (x / mEdge) - mPosX + mOriginX;
		unsigned int yy = (y / mEdge) - mPosY + mOriginY;
		if (xx >= mNumberX || yy >= mNumberY) {
			return nullptr;
		}
		return &mPlate[xx][yy];
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
			}
			if (Info.X != 0) {
				mPosX += Info.X;
				MovePlateX(Info.X);
				Info.UpData = true;
			}
		}
		else {//全部都要更新
			mPosX += Info.X;
			mPosY += Info.Y;
			for (size_t Nx = 0; Nx < mNumberX; ++Nx)
			{
				for (size_t Ny = 0; Ny < mNumberY; ++Ny)
				{
					mDeleteCallback(&mPlate[Nx][Ny], mDeleteData);
					mGenerateCallback(&mPlate[Nx][Ny], (Nx + mPosX - mOriginX), (Ny + mPosY - mOriginY), mGenerateData);
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
			for (size_t ix = 0; ix < uX; ++ix)
			{
				LPlateX[ix] = mPlate[ix];
			}
			for (size_t ix = 0; ix < (mNumberX - uX); ++ix)
			{
				mPlate[ix] = mPlate[ix + uX];
			}
			for (size_t ix = (mNumberX - uX); ix < mNumberX; ++ix)
			{
				mPlate[ix] = LPlateX[ix - (mNumberX - uX)];
				for (size_t iy = 0; iy < mNumberY; ++iy)
				{
					mDeleteCallback(&mPlate[ix][iy], mDeleteData);
					mGenerateCallback(&mPlate[ix][iy], (ix + mPosX - mOriginX), (iy + mPosY - mOriginY), mGenerateData);
				}
			}
		}
		else {
			for (size_t ix = 0; ix < -uX; ++ix)
			{
				LPlateX[ix] = mPlate[mNumberX - ix - 1];
			}
			for (int ix = (mNumberX + uX - 1); ix >= 0; --ix)
			{
				mPlate[ix-uX] = mPlate[ix];
			}
			for (size_t ix = 0; ix < -uX; ++ix)
			{
				mPlate[ix] = LPlateX[ix];
				for (size_t iy = 0; iy < mNumberY; ++iy)
				{
					mDeleteCallback(&mPlate[ix][iy], mDeleteData);
					mGenerateCallback(&mPlate[ix][iy], (ix + mPosX - mOriginX), (iy + mPosY - mOriginY), mGenerateData);
				}
			}
		}
	}

	//板块 Y 的移动
	void MovePlateY(int uY, int uX) {
		int tX = 0, wX = 0;
		if (uX > 0) {
			tX = uX;
		}
		else {
			wX = uX;
		}
		if (uY > 0) {
			for (size_t ix = tX; ix < mNumberX + wX; ++ix)
			{
				for (size_t iy = 0; iy < uY; ++iy)
				{
					LPlateY[iy] = mPlate[ix][iy];
				}
				for (size_t iy = 0; iy < (mNumberY - uY); ++iy)
				{
					mPlate[ix][iy] = mPlate[ix][iy + uY];
				}
				for (size_t iy = (mNumberY - uY); iy < mNumberY; ++iy)
				{
					mPlate[ix][iy] = LPlateY[iy - (mNumberY - uY)];
					mDeleteCallback(&mPlate[ix][iy], mDeleteData);
					mGenerateCallback(&mPlate[ix][iy], (ix + mPosX - mOriginX), (iy + mPosY - mOriginY), mGenerateData);
				}
			}
		}
		else {
			for (size_t ix = tX; ix < mNumberX + wX; ++ix)
			{
				for (size_t iy = 0; iy < -uY; ++iy)
				{
					LPlateY[iy] = mPlate[ix][mNumberY - iy - 1];
				}
				for (int iy = (mNumberY + uY - 1); iy >= 0; --iy)
				{
					mPlate[ix][iy - uY] = mPlate[ix][iy];
				}
				for (size_t iy = 0; iy < -uY; ++iy)
				{
					mPlate[ix][iy] = LPlateY[iy];
					mDeleteCallback(&mPlate[ix][iy], mDeleteData);
					mGenerateCallback(&mPlate[ix][iy], (ix + mPosX - mOriginX), (iy + mPosY - mOriginY), mGenerateData);
				}
			}
		}
	}
};
