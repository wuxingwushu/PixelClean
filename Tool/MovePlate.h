#pragma once
#include <assert.h>

struct MovePlate2DInfo
{
	bool UpData = false;
	int X = 0;
	int Y = 0;
};

struct MovePlate3DInfo : MovePlate2DInfo
{
	int Z = 0;
};

template <typename PlateT, unsigned int EdgeNum = 2>
class MovePlate3D
{
	typedef void (*_GenerateCallback)(PlateT mT, int x, int y, int z, void* Data); // 生成回调函数
	typedef void (*_DeleteCallback)(PlateT mT, void* Data);						   // 销毁回调函数

private:
	_GenerateCallback mGenerateCallback = nullptr; // 生成回调函数
	void* mGenerateData = nullptr;
	_DeleteCallback mDeleteCallback = nullptr; // 销毁回调函数
	void* mDeleteData = nullptr;

	const unsigned int mEdge = 1;								 // 板块的边长
	const unsigned int mNumberX = 0, mNumberY = 0, mNumberZ = 0; // 板块的数量
	const unsigned int mOriginX = 0, mOriginY = 0, mOriginZ = 0; // 那个板块为中心
	PlateT* mPlate = nullptr;

	int mPosX = 0, mPosY = 0, mPosZ = 0;

private:
	PlateT* LPlate = nullptr; //  後續移動時，用於儲存板塊
public:
	MovePlate3D(
		const unsigned int NumberX,
		const unsigned int NumberY,
		const unsigned int NumberZ,
		const unsigned int Edge,
		const unsigned int OriginX = 0,
		const unsigned int OriginY = 0,
		const unsigned int OriginZ = 0)
		: mNumberX(NumberX), mNumberY(NumberY), mNumberZ(NumberZ),
		mOriginX(OriginX), mOriginY(OriginY), mOriginZ(OriginZ),
		mEdge(Edge)
	{
		unsigned int Num;
		if (mNumberX > mNumberY)
		{
			if (mNumberY > mNumberZ)
			{
				Num = mNumberX * mNumberY;
			}
			else
			{
				Num = mNumberX * mNumberZ;
			}
		}
		else
		{
			if (mNumberX > mNumberZ)
			{
				Num = mNumberY * mNumberX;
			}
			else
			{
				Num = mNumberY * mNumberZ;
			}
		}
		mPlate = new PlateT[mNumberX * mNumberY * mNumberZ + Num];
		LPlate = &mPlate[mNumberX * mNumberY * mNumberZ];
	};

	~MovePlate3D()
	{
		delete mPlate;
	};

	// 设置回调函数
	inline void SetCallback(_GenerateCallback G, void* GData, _DeleteCallback D, void* DData)
	{
		mGenerateCallback = G;
		mGenerateData = GData;
		mDeleteCallback = D;
		mDeleteData = DData;
	}

	// 设置位置
	inline void SetPos(float x, float y, float z)
	{
		mPosX = x / mEdge;
		mPosY = y / mEdge;
		mPosZ = z / mEdge;
	}

	PlateT& at(unsigned int x, unsigned int y, unsigned int z)
	{
		return mPlate[x + (y * mNumberX) + (z * mNumberX * mNumberY)];
	}

	inline int GetPosX() { return mPosX; }
	inline int GetPosY() { return mPosY; }
	inline int GetPosZ() { return mPosZ; }

	inline int GetPlateX() { return (mOriginX - mPosX) * mEdge; }
	inline int GetPlateY() { return (mOriginY - mPosY) * mEdge; }
	inline int GetPlateZ() { return (mOriginZ - mPosZ) * mEdge; }

	inline int GetCalculatePosX(int x) { return (x / mEdge) - mPosX + mOriginX; }
	inline int GetCalculatePosY(int y) { return (y / mEdge) - mPosY + mOriginY; }
	inline int GetCalculatePosZ(int z) { return (z / mEdge) - mPosZ + mOriginZ; }

	inline PlateT& GetPlate(unsigned int x, unsigned int y, unsigned int z)
	{
		if (x >= mNumberX || y >= mNumberY || z >= mNumberZ)
		{
			return nullptr;
		}
		return at(x, y, z);
	}

	inline PlateT& CalculateGetPlate(float x, float y, float z)
	{
		unsigned int xx = GetCalculatePosX(x);
		unsigned int yy = GetCalculatePosX(y);
		unsigned int zz = GetCalculatePosX(z);
		return GetPlate(xx, yy, zz);
	}

	// 更新当前监测位置，判断是否需要移动板块，返回板块是否移动信息
	MovePlate3DInfo UpData(float x, float y, float z)
	{
		MovePlate3DInfo Info;
		Info.X = (x / mEdge) - mPosX;
		Info.Y = (y / mEdge) - mPosY;
		Info.Z = (z / mEdge) - mPosZ;
		if ((abs(Info.X) < mNumberX) && (abs(Info.Y) < mNumberY) && (abs(Info.Z) < mNumberZ))
		{
			if (Info.X != 0)
			{
				mPosX += Info.X;
				MovePlateX(Info.X, Info.Y, Info.Z);
				Info.UpData = true;
			}
			if (Info.Y != 0)
			{
				mPosY += Info.Y;
				MovePlateY(Info.Y, Info.Z);
				Info.UpData = true;
			}
			if (Info.Z != 0)
			{
				mPosZ += Info.Z;
				MovePlateZ(Info.Z);
				Info.UpData = true;
			}
			return Info;
		}
		else
		{ // 全部都要更新
			mPosX += Info.X;
			mPosY += Info.Y;
			mPosZ += Info.Z;
			for (size_t Nx = 0; Nx < mNumberX; ++Nx)
			{
				for (size_t Ny = 0; Ny < mNumberY; ++Ny)
				{
					for (size_t Nz = 0; Nz < mNumberZ; ++Nz)
					{
						mDeleteCallback(at(Nx, Ny, Nz), mDeleteData);
						mGenerateCallback(at(Nx, Ny, Nz), (Nx + mPosX - mOriginX), (Ny + mPosY - mOriginY), (Nz + mPosZ - mOriginZ), mGenerateData);
					}
				}
			}
			Info.UpData = true;
		}
		return Info;
	}

	void MovePlateX(int uX, int uY, int uZ)
	{
		int sZ, eZ;
		if (uZ > 0) {
			sZ = uZ;
			eZ = mNumberZ;
		}
		else {
			sZ = 0;
			eZ = mNumberZ + uZ;
		}
		int sY, eY;
		if (uY > 0) {
			sY = uY;
			eY = mNumberY;
		}
		else {
			sY = 0;
			eY = mNumberY + uY;
		}
		if (uX > 0)
		{
			for (int iy = sY; iy < eY; ++iy)
			{
				for (int iz = sZ; iz < eZ; ++iz)
				{
					for (int ix = 0; ix < uX; ++ix)
					{
						LPlate[ix] = at(ix, iy, iz);
					}
					for (int ix = 0; ix < (mNumberX - uX); ++ix)
					{
						at(ix, iy, iz) = at(ix + uX, iy, iz);
					}
					for (int ix = (mNumberX - uX); ix < mNumberX; ++ix)
					{
						at(ix, iy, iz) = LPlate[ix - (mNumberX - uX)];
						mDeleteCallback(at(ix, iy, iz), mDeleteData);
						mGenerateCallback(at(ix, iy, iz), (ix + mPosX - mOriginX), (iy + mPosY - mOriginY), (iz + mPosZ - mOriginZ), mGenerateData);
					}
				}
			}
		}
		else
		{
			for (int iy = sY; iy < eY; ++iy)
			{
				for (int iz = sZ; iz < eZ; ++iz)
				{
					for (int ix = 0; ix < -uX; ++ix)
					{
						LPlate[ix] = at(mNumberX - ix - 1, iy, iz);
					}
					for (int ix = (mNumberX + uX - 1); ix >= 0; --ix)
					{
						at(ix - uX, iy, iz) = at(ix, iy, iz);
					}
					for (int ix = 0; ix < -uX; ++ix)
					{
						at(ix, iy, iz) = LPlate[ix];
						mDeleteCallback(at(ix, iy, iz), mDeleteData);
						mGenerateCallback(at(ix, iy, iz), (ix + mPosX - mOriginX), (iy + mPosY - mOriginY), (iz + mPosZ - mOriginZ), mGenerateData);
					}
				}
			}
		}
	}

	void MovePlateY(int uY, int uZ)
	{
		int sZ, eZ;
		if (uZ > 0) {
			sZ = uZ;
			eZ = mNumberZ;
		}
		else {
			sZ = 0;
			eZ = mNumberZ + uZ;
		}
		if (uY > 0)
		{
			for (int ix = 0; ix < mNumberX; ++ix)
			{
				for (int iz = sZ; iz < eZ; ++iz)
				{
					for (int iy = 0; iy < uY; ++iy)
					{
						LPlate[iy] = at(ix, iy, iz);
					}
					for (int iy = 0; iy < (mNumberY - uY); ++iy)
					{
						at(ix, iy, iz) = at(ix, iy + uY, iz);
					}
					for (int iy = (mNumberY - uY); iy < mNumberY; ++iy)
					{
						at(ix, iy, iz) = LPlate[iy - (mNumberY - uY)];
						mDeleteCallback(at(ix, iy, iz), mDeleteData);
						mGenerateCallback(at(ix, iy, iz), (ix + mPosX - mOriginX), (iy + mPosY - mOriginY), (iz + mPosZ - mOriginZ), mGenerateData);
					}
				}
			}
		}
		else
		{
			for (int ix = 0; ix < mNumberY; ++ix)
			{
				for (int iz = sZ; iz < eZ; ++iz)
				{
					for (int iy = 0; iy < -uY; ++iy)
					{
						LPlate[iy] = at(ix, mNumberY - iy - 1, iz);
					}
					for (int iy = (mNumberY + uY - 1); iy >= 0; --iy)
					{
						at(ix, iy - uY, iz) = at(ix, iy, iz);
					}
					for (int iy = 0; iy < -uY; ++iy)
					{
						at(ix, iy, iz) = LPlate[iy];
						mDeleteCallback(at(ix, iy, iz), mDeleteData);
						mGenerateCallback(at(ix, iy, iz), (ix + mPosX - mOriginX), (iy + mPosY - mOriginY), (iz + mPosZ - mOriginZ), mGenerateData);
					}
				}
			}
		}
	}

	void MovePlateZ(int uZ)
	{
		if (uZ > 0)
		{
			for (int ix = 0; ix < mNumberX; ++ix)
			{
				for (int iy = 0; iy < mNumberY; ++iy)
				{
					for (int iz = 0; iz < uZ; ++iz)
					{
						LPlate[iz] = at(ix, iy, iz);
					}
					for (int iz = 0; iz < (mNumberZ - uZ); ++iz)
					{
						at(ix, iy, iz) = at(ix, iy + uZ, iz);
					}
					for (int iz = (mNumberZ - uZ); iz < mNumberZ; ++iz)
					{
						at(ix, iy, iz) = LPlate[iz - (mNumberZ - uZ)];
						mDeleteCallback(at(ix, iy, iz), mDeleteData);
						mGenerateCallback(at(ix, iy, iz), (ix + mPosX - mOriginX), (iy + mPosY - mOriginY), (iz + mPosZ - mOriginZ), mGenerateData);
					}
				}
			}
		}
		else
		{
			for (int ix = 0; ix < mNumberY; ++ix)
			{
				for (int iy = 0; iy < mNumberY; ++iy)
				{
					for (int iz = 0; iz < -uZ; ++iz)
					{
						LPlate[iz] = at(ix, iy, mNumberZ - iz - 1);
					}
					for (int iz = (mNumberZ + uZ - 1); iz >= 0; --iz)
					{
						at(ix, iy, iz - uZ) = at(ix, iy, iz);
					}
					for (int iz = 0; iz < -uZ; ++iz)
					{
						at(ix, iy, iz) = LPlate[iy];
						mDeleteCallback(at(ix, iy, iz), mDeleteData);
						mGenerateCallback(at(ix, iy, iz), (ix + mPosX - mOriginX), (iy + mPosY - mOriginY), (iz + mPosZ - mOriginZ), mGenerateData);
					}
				}
			}
		}
	}
};

template<typename PlateT>
class MovePlate2D
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

	constexpr MovePlate2D(const unsigned int NumberX, const unsigned int NumberY, const unsigned int Edge,
		const unsigned int OriginX = 0, const unsigned int OriginY = 0) :
		mNumberX(NumberX), mNumberY(NumberY), mEdge(Edge),
		mOriginX(OriginX), mOriginY(OriginY)
	{
		assert(mEdge != 0 && "Error : mEdge = 0");
		mPlate = (PlateT*)new char[(mNumberX * mNumberY + (mNumberX > mNumberY ? mNumberX : mNumberY)) * sizeof(PlateT)];
		LPlate = &mPlate[mNumberX * mNumberY];
	}

	~MovePlate2D()
	{
		delete (char*)mPlate;
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
	MovePlate2DInfo UpData(float x, float y) {
		MovePlate2DInfo Info;
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
		int sX, eX;
		if (uX > 0) {
			sX = uX;
			eX = mNumberX;
		}
		else {
			sX = 0;
			eX = mNumberX + uX;
		}
		if (uY > 0) {
			for (size_t ix = sX; ix < eX; ++ix)
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
					mDeleteCallback(PlateAt(ix, iy), mDeleteData);
					mGenerateCallback(PlateAt(ix, iy), (ix + mPosX - mOriginX), (iy + mPosY - mOriginY), mGenerateData);
				}
			}
		}
		else {
			for (size_t ix = sX; ix < eX; ++ix)
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
					mDeleteCallback(PlateAt(ix, iy), mDeleteData);
					mGenerateCallback(PlateAt(ix, iy), (ix + mPosX - mOriginX), (iy + mPosY - mOriginY), mGenerateData);
				}
			}
		}
	}
};
