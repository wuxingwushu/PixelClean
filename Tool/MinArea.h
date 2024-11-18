#pragma once
#include <stack>
#include <vector>
#include <algorithm>
#include <iostream>

struct vec2
{
    int x;
    int y;
};

class MinArea
{
private:
    struct AreaData
    {
        int x;
        int len;

        constexpr bool operator>(const AreaData &other) noexcept
        {
            return x > other.x;
        }

        constexpr bool operator<(const AreaData &other) noexcept
        {
            return x < other.x;
        }

        int GetEnd()
        {
            return x + len;
        }
    };

    const int mR;
    std::vector<std::vector<AreaData>> mArea{};

    // 一行數據的融合處理
    void addH(const int h, AreaData Data);

public:
    //              高度          範圍
    MinArea(const int H, const int R) : mR(R) { mArea.resize(H); };
    ~MinArea() {};

    // 添加點
    void add(int x, int y);

    // 添加矩形
    void addBox(int x, int y, unsigned int w, unsigned int h);

    // 獲取面積當中的所有點
    std::vector<vec2> GetData();
};

void MinArea::addH(const int h, AreaData Data)
{
    if (mArea[h].size() == 0)
    {
        mArea[h].push_back(Data);
    }
    else
    {
        // 找到比他大的第一個元素
        for (int i = 0; i < mArea[h].size(); ++i)
        {
            // 兩個元素之間
            if (mArea[h][i] > Data)
            {
                bool rh = false;
                if ((Data.x + Data.len) >= mArea[h][i].x)
                { // 和前面單元融合
                    mArea[h][i].len += mArea[h][i].x - Data.x;
                    mArea[h][i].x = Data.x;
                    rh = true;
                }
                //   前面有單元                    有交集可以融合
                if ((i >= 1) && (mArea[h][i - 1].GetEnd() >= Data.x))
                {
                    //                    是否都重合了
                    if (mArea[h][i - 1].GetEnd() >= Data.GetEnd())
                    {
                        return;
                    }
                    // 和后面單元融合
                    if (rh)
                    { // 和前面單元有融合
                        mArea[h][i - 1].len += mArea[h][i].len - (mArea[h][i - 1].GetEnd() - Data.x);
                        mArea[h].erase(mArea[h].begin() + i); // 剔除被融合單元
                    }
                    else
                    {
                        mArea[h][i - 1].len += Data.len - (mArea[h][i - 1].GetEnd() - Data.x);
                    }
                    return;
                }
                if (rh == false)
                { // 沒有發生融合，直接插入在中間
                    mArea[h].insert(mArea[h].begin() + i, Data);
                }
                return;
            }
        }

        //      有交集可以融合
        if (mArea[h].back().GetEnd() >= Data.x)
        {
            //                    是否都重合了
            if (mArea[h].back().GetEnd() >= Data.GetEnd())
            {
                return;
            }
            // 和后面單元融合
            mArea[h].back().len += Data.len - (mArea[h].back().GetEnd() - Data.x);
        }
        else
        {
            mArea[h].push_back(Data); // 不符合融合條件，直接加到後面
        }
    }
}

void MinArea::add(int x, int y)
{
    int wR = mR * 2 + 1;
    x -= mR;
    for (int iy = (y - mR); iy <= (y + mR); ++iy)
    {
        addH(iy, {x, wR});
    }
}

// 添加矩形
void MinArea::addBox(int x, int y, unsigned int w, unsigned int h)
{
    int wR = mR * 2 + 1 + w;
    x -= mR;
    for (int iy = (y - mR); iy <= (y + mR + h); ++iy)
    {
        addH(iy, {x, wR});
    }
}

std::vector<vec2> MinArea::GetData()
{
    std::vector<vec2> Data{};
    for (int y = 0; y < mArea.size(); ++y)
    {
        for (auto iD : mArea[y])
        {
            for (int x = iD.x; x < iD.GetEnd(); ++x)
            {
                Data.push_back({x, y});
                std::cout << x << " - " << y << std::endl;
            }
        }
        mArea[y].clear();
    }
    return Data;
}
