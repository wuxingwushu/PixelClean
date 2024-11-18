#pragma once


class GridTest
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
	unsigned int* mGridRoadWidth;//网格道路距離場
public:
    GridTest(const unsigned int wide, const unsigned int high, const unsigned int radius):
	mWide(wide), mHigh(high), mRadius(radius)
	{mGridRoadWidth = new unsigned int[wide * high];}

	~GridTest(){delete[] mGridRoadWidth;}

    // 设置回调函数
	void SetRoadCallback(_GetGridRoadCallback Callback, void* ptr) {
		mGetGridRoadCallback = Callback;
		wClass = ptr;
	}
	// 扫描全部网格
	void ScanGridNavigation();

    // 添加點
    void add(int x, int y);

    // 移除點
    void pop(int x, int y);

	unsigned int Get(unsigned int x, unsigned int y) { return mGridRoadWidth[x + y * mWide]; }
private:
	unsigned int& at(unsigned int x, unsigned int y) { return mGridRoadWidth[x + y * mWide]; }

    // 獲取邊框是否有障礙物
    bool GetFK(int x, int y, int K);

    // 獲取邊框的碰撞範圍
    unsigned int GetRawg(int x, int y, unsigned int stin = 0);

    // 設置邊框的碰撞範圍
    void SetRawg(int x, int y, int K);

    // 移除點範圍更新
    void SetPopRawg(int x, int y, int K);
};

// 獲取邊框是否有障礙物
bool GridTest::GetFK(int x, int y, int K) {
    int XY = x + K;
    int K1 = y + K, K2 = y - K;
    for(int j = x - K; j <= XY; ++j) {
        if(mGetGridRoadCallback(j, K1, nullptr) || mGetGridRoadCallback(j, K2, nullptr)) {// 檢測上下邊框是否有障礙物
            return true;
        }
    }
    XY = y + K - 1;
    K1 = x + K, K2 = x - K;
    for(int j = y - K + 1; j <= XY; ++j) {// 去除已经检测的头和尾
        if(mGetGridRoadCallback(K1, j, nullptr) || mGetGridRoadCallback(K2, j, nullptr)) {// 檢測左右邊框是否有障礙物
            return true;
        }
    }
    return false;
}

// 獲取網格點的碰撞距離
unsigned int GridTest::GetRawg(int x, int y, unsigned int stin) {
    for (unsigned int i = stin; i < mRadius; ++i)// 循環一層一層向外檢測是否有障礙物
    {
        if(GetFK(x, y, i)){ return (i); }
    }
    return mRadius;
}

// 設置點周圍一圈網格
void GridTest::SetRawg(int x, int y, int K) {
    int XY = x + K;
    int K1 = y + K, K2 = y - K;
    for(int j = x - K; j <= XY; ++j) {
        if(Get(j, K1) > K)at(j, K1) = K;// 上邊框
        if(Get(j, K2) > K)at(j, K2) = K;// 下邊框
    }
    XY = y + K - 1;
    K1 = x + K, K2 = x - K;
    for(int j = y - K + 1; j <= XY; ++j) {// 去除已经检测的头和尾
        if(Get(K1, j) > K)at(K1, j) = K;// 右邊框
        if(Get(K2, j) > K)at(K2, j) = K;// 左邊框
    }
}

// 掃描全圖網格
void GridTest::ScanGridNavigation() {
    for (unsigned int x = 0; x < mWide; ++x)
    {
        for (unsigned int y = 0; y < mHigh; ++y)
        {
            at(x, y) = GetRawg(x, y);
        }
    }
}

// 添加點
void GridTest::add(int x, int y) {
    at(x, y) = 0;
    for (unsigned int W = 1; W < mRadius; ++W)// 一層一層向外重新更新點距離
    {
        SetRawg(x, y, W);
    }
}

// 設置點周圍一圈網格
void GridTest::SetPopRawg(int x, int y, int K) {
    int XY = x + K;
    int K1 = y + K, K2 = y - K;
    for(int j = x - K; j <= XY; ++j) {
        if(Get(j, K1) == K)at(j, K1) = GetRawg(j, K1, K);// 上邊框
        if(Get(j, K2) == K)at(j, K2) = GetRawg(j, K2, K);// 下邊框
    }
    XY = y + K - 1;
    K1 = x + K, K2 = x - K;
    for(int j = y - K + 1; j <= XY; ++j) {// 去除已经检测的头和尾
        if(Get(K1, j) == K)at(K1, j) = GetRawg(K1, j, K);// 右邊框
        if(Get(K2, j) == K)at(K2, j) = GetRawg(K2, j, K);// 左邊框
    }
}

// 移除點
void GridTest::pop(int x, int y) {
    at(x, y) = GetRawg(x, y);
    for (unsigned int W = 1; W < mRadius; ++W)
    {
        SetPopRawg(x, y, W);
    }
}
