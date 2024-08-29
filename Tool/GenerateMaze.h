#pragma once
#include <iostream>
#include "../Tool/PileUp.h"
#include <random>



class GenerateMaze
{
public:
	struct Pos {
		int X;
		int Y;
	};
	typedef void (*_GenerateMazeCallback)(int x,int y, bool B, void* ptr);//画迷宫 回调函数类型
	_GenerateMazeCallback mGenerateMazeCallback = nullptr;//用户自定义画迷宫 回调函数
	void* mMazeData = nullptr;
private:
	int HEIGHT = 0;//高
	int WIDTH = 0;//宽
	int WallWide;//墙壁宽
	int RoadWide;//道路宽
	int AtlasWide = 0;//地图宽
	int AtlasHigh = 0;//地图高
	bool** block = nullptr;

	PileUp<Pos>* mPos1 = nullptr;
	PileUp<Pos>* mPos2 = nullptr;
public:
	GenerateMaze(int ww = 1, int rw = 1) {
		WallWide = ww;
		RoadWide = rw;
		mPos1 = new PileUp<Pos>(4);
		mPos2 = new PileUp<Pos>(4);
	}

	Pos GetMazeSize(int x, int y, int ww = 0, int rw = 0) {
		if (ww != 0) { WallWide = ww; }
		if (rw != 0) { RoadWide = rw; }
		WIDTH = x;
		if ((WIDTH % 2) == 0) {
			++WIDTH;
		}
		HEIGHT = y;
		if ((HEIGHT % 2) == 0) {
			++HEIGHT;
		}
		AtlasWide = (WIDTH / 2) * (WallWide + RoadWide) + WallWide;
		AtlasHigh = (HEIGHT / 2) * (WallWide + RoadWide) + WallWide;
		return { AtlasWide, AtlasHigh };
	}

	Pos GetMazeSize(int MazeSize, int ww = 0, int rw = 0) {
		if (ww != 0) { WallWide = ww; }
		if (rw != 0) { RoadWide = rw; }
		if (MazeSize < 3)MazeSize = 3;
		// 使用当前时间作为随机数生成器的种子，以获得更好的随机性
		unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
		std::default_random_engine generator(seed);
		// 定义一个在0到100之间的均匀分布
		std::uniform_int_distribution<int> distribution(3, MazeSize);
		WIDTH = distribution(generator);
		if ((WIDTH % 2) == 0) {
			++WIDTH;
		}
		HEIGHT = distribution(generator);
		if ((HEIGHT % 2) == 0) {
			++HEIGHT;
		}
		AtlasWide = (WIDTH / 2) * (WallWide + RoadWide) + WallWide;
		AtlasHigh = (HEIGHT / 2) * (WallWide + RoadWide) + WallWide;
		return { AtlasWide, AtlasHigh };
	}

	void RunGenerateMaze() {
		PileUp<Pos> mPos(WIDTH * HEIGHT);
		block = new bool* [WIDTH];
		for (size_t x = 0; x < WIDTH; ++x)
		{
			block[x] = new bool[HEIGHT] {true};
			for (size_t y = 0; y < HEIGHT; ++y)
			{
				block[x][y] = true;
			}
		}
		
		mPos.add(Pos{ (WIDTH / 2) + 1, (HEIGHT / 2) + 1 });
		while (mPos.GetNumber() != 0)
		{
			while (check(mPos.GetEnd()))
			{
				mPos.pop__();
				if (mPos.GetNumber() == 0) goto RGMreturn;
			}
			through(&mPos);
		}
	RGMreturn:
		int posW, posH, W, H;
		bool B;
		for (size_t x = 0; x < WIDTH; ++x)
		{
			for (size_t y = 0; y < HEIGHT; ++y)
			{
				B = (x % 2) == 0;
				posW = (x / 2) * (WallWide + RoadWide) + (B ? 0 : WallWide);
				W = B ? WallWide : RoadWide;
				B = (y % 2) == 0;
				posH = (y / 2) * (WallWide + RoadWide) + (B ? 0 : WallWide);
				H = B ? WallWide : RoadWide;
				for (size_t posx = 0; posx < W; posx++)
				{
					for (size_t posy = 0; posy < H; posy++)
					{
						mGenerateMazeCallback(posW + posx, posH + posy, block[x][y], mMazeData);
					}
				}
			}
		}

		for (size_t i = 0; i < WIDTH; ++i)
		{
			delete[] block[i];
		}
		delete[] block;
	}

	GenerateMaze(bool** Block, int X, int Y) :
		block(Block),
		HEIGHT(Y),
		WIDTH(X)
	{
		mPos1 = new PileUp<Pos>(4);
		mPos2 = new PileUp<Pos>(4);

		PileUp<Pos> mPos(X * Y);
		for (size_t x = 0; x < WIDTH; ++x)
		{
			for (size_t y = 0; y < HEIGHT; ++y)
			{
				block[x][y] = true;
			}
		}


		mPos.add(Pos{ (WIDTH / 2) + 1, (HEIGHT / 2) + 1 });

		while (mPos.GetNumber() != 0)
		{
			while (check(mPos.GetEnd()))
			{
				mPos.pop__();
				if (mPos.GetNumber() == 0) return;
			}
			through(&mPos);
		}
	}

	void GetGenerateMaze(bool** Block, int X, int Y, unsigned int K)
	{
		WIDTH = X;
		if ((WIDTH % 2) == 0) {
			++WIDTH;
		}
		HEIGHT = Y;
		if ((HEIGHT % 2) == 0) {
			++HEIGHT;
		}
		block = Block;
		bool NewBlock = false;
		if (block == nullptr) {
			NewBlock = true;
			block = new bool* [WIDTH];
			for (size_t i = 0; i < WIDTH; ++i)
			{
				block[i] = new bool[HEIGHT];
			}
		}

		for (size_t x = 0; x < WIDTH; ++x)
		{
			for (size_t y = 0; y < HEIGHT; ++y)
			{
				block[x][y] = true;
			}
		}

		PileUp<Pos>* mPos = new PileUp<Pos>(WIDTH * HEIGHT);
		int TposX, TposY;
		TposX = WIDTH / 2;
		TposY = HEIGHT / 2;
		if (!(TposX % 2)) {
			--TposX;
		}
		if (!(TposY % 2)) {
			--TposY;
		}
		mPos->add(Pos{ TposX, TposY });

		while (mPos->GetNumber() != 0)
		{
			while (check(mPos->GetEnd()))
			{
				mPos->pop__();
				if (mPos->GetNumber() == 0) goto daozhel;
			}
			through(mPos);
		}
	daozhel:
		delete mPos;

		


		int NX = ((WIDTH / 2) * K) + 1;
		int NY = ((HEIGHT / 2) * K) + 1;

		int posNX, posNY;
		for (size_t x = 0; x < NX; ++x)
		{
			for (size_t y = 0; y < NY; ++y)
			{
				posNX = (x / K) * 2;
				if ((x % K) != 0) {
					++posNX;
				}
				posNY = (y / K) * 2;
				if ((y % K) != 0) {
					++posNY;
				}
				mGenerateMazeCallback(x, y, block[posNX][posNY], mMazeData);
			}
		}

		if (NewBlock) {
			for (size_t i = 0; i < X; ++i)
			{
				delete[] block[i];
			}
			delete[] block;
		}
	}

	~GenerateMaze() {
		delete mPos1;
		delete mPos2;
	}

	void SetGenerateMazeCallback(_GenerateMazeCallback Callback, void* Lclass) {
		mGenerateMazeCallback = Callback;
		mMazeData = Lclass;
	}

	bool check(Pos pns) {
		//上
		if (pns.Y - 2 >= 0) {
			if (block[pns.X][pns.Y - 2] == true) {
				return 0;
			}
		}
		//下
		if (pns.Y + 2 <= HEIGHT - 1) {
			if (block[pns.X][pns.Y + 2] == true) {
				return 0;
			}
		}
		//左
		if (pns.X - 2 >= 0) {
			if (block[pns.X - 2][pns.Y] == true) {
				return 0;
			}
		}
		//右
		if (pns.X + 2 <= WIDTH - 1) {
			if (block[pns.X + 2][pns.Y] == true) {
				return 0;
			}
		}
		return 1;
	}

	void through(PileUp<Pos>* PUp) {
		mPos1->ClearAll();
		mPos2->ClearAll();
		Pos pns = PUp->GetEnd();
		//上
		if (pns.Y - 2 >= 0) {
			if (block[pns.X][pns.Y - 2] == true) {
				mPos1->add(Pos{ pns.X, pns.Y - 1 });
				mPos2->add(Pos{ pns.X, pns.Y - 2 });
			}
		}
		//下
		if (pns.Y + 2 <= HEIGHT - 1) {
			if (block[pns.X][pns.Y + 2] == true) {
				mPos1->add(Pos{ pns.X, pns.Y + 1 });
				mPos2->add(Pos{ pns.X, pns.Y + 2 });
			}
		}
		//左
		if (pns.X - 2 >= 0) {
			if (block[pns.X - 2][pns.Y] == true) {
				mPos1->add(Pos{ pns.X - 1, pns.Y });
				mPos2->add(Pos{ pns.X - 2, pns.Y });
			}
		}
		//右
		if (pns.X + 2 <= WIDTH - 1) {
			if (block[pns.X + 2][pns.Y] == true) {
				mPos1->add(Pos{ pns.X + 1, pns.Y });
				mPos2->add(Pos{ pns.X + 2, pns.Y });
			}
		}
		if (mPos1->GetNumber() != 0) {
			int BIndexea = rand() % mPos1->GetNumber();
			Pos B1 = mPos1->GetIndex(BIndexea);
			Pos B2 = mPos2->GetIndex(BIndexea);
			block[B1.X][B1.Y] = false;
			block[B2.X][B2.Y] = false;
			PUp->add(B2);
		}
	}
};
