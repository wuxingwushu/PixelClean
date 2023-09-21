#pragma once
#include <iostream>
#include "../Tool/PileUp.h"



class GenerateMaze
{
	struct Pos {
		int X;
		int Y;
	};

	typedef void (*_GenerateMazeCallback)(int x,int y, bool B, void* ptr);//销毁 回调函数类型
	_GenerateMazeCallback mGenerateMazeCallback = nullptr;//用户自定义销毁 回调函数
	void* mMazeData = nullptr;

private:
	int HEIGHT = 0;//高
	int WIDTH = 0;//宽
	bool** block = nullptr;

	PileUp<Pos>* mPos1 = nullptr;
	PileUp<Pos>* mPos2 = nullptr;
public:
	GenerateMaze() {
		mPos1 = new PileUp<Pos>(4);
		mPos2 = new PileUp<Pos>(4);
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
			WIDTH++;
		}
		HEIGHT = Y;
		if ((HEIGHT % 2) == 0) {
			HEIGHT++;
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
