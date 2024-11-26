#include <vector>
#include <cmath>
#include <iostream>

struct vec2
{
    int x;
    int y;
};

class View
{
private:
    float ViewX;
    float ViewY;
    int mR;
    

public:
    View(float viewX, float viewY, int r) : ViewX(viewX), ViewY(viewY), mR(r) {};
    ~View() {};

    // (-89 ~ 89)
    void GetViewG(float ViewAngle){
        int R;
        vec2 Line1;
        vec2 Line2;
        std::vector<vec2> DataGao{};
        float Sin = sin(ViewAngle);
        float Cos = cos(ViewAngle);

        // 算出線段結束的位置
        if (abs(Sin) > abs(Cos))
        {
            R = (Sin < 0 ? -mR : mR);
            Line1 = {int(R / sin(ViewAngle + ViewX / 2) * cos(ViewAngle + ViewX / 2)), R};
            Line2 = {int(R / sin(ViewAngle - ViewX / 2) * cos(ViewAngle - ViewX / 2)), R};
        }
        else
        {
            R = (Cos < 0 ? -mR : mR);
            Line1 = {R, int(R / cos(ViewAngle + ViewX / 2) * sin(ViewAngle + ViewX / 2))};
            Line2 = {R, int(R / cos(ViewAngle - ViewX / 2) * sin(ViewAngle - ViewX / 2))};
        }

        std::vector<vec2> DataDrop1{};
        std::vector<vec2> DataDrop2{};

        // 將線段沿途的點都記錄下來
        Bresenham(0, 0, Line1.x, Line1.y, mR, DataDrop1);
        Bresenham(0, 0, Line2.x, Line2.y, mR, DataDrop2);

        if (abs(Sin) > abs(Cos))
        {
            // 最後點 Y軸 如果位置不等於 R
            if (DataDrop1.back().y != R)
            {
                // 將 Y軸 相同數值最後的一個作為 關鍵點
                for (auto &i : DataDrop1)
                {
                    DataDrop1[abs(i.y)] = i;
                }
                // 將 Y軸 所以數組最後一個保存，其餘都丟棄
                DataDrop1.erase(DataDrop1.begin() + abs(DataDrop1.back().y), DataDrop1.end() - 1);
                // 將 Y軸 補全到 R;
                R = DataDrop1.back().x;
                if (Sin > 0)
                {
                    for (int i = DataDrop1.size(); i <= abs(R); ++i)
                    {
                        DataDrop1.push_back({R, i});
                    }
                }
                else
                {
                    for (int i = -DataDrop1.size(); i >= -abs(R); --i)
                    {
                        DataDrop1.push_back({R, i});
                    }
                }
            }
            if (DataDrop2.back().y != R)
            {
                // 將 Y軸 相同數值最後的一個作為 關鍵點
                for (auto &i : DataDrop2)
                {
                    DataDrop2[abs(i.y)] = i;
                }
                // 將 Y軸 所以數組最後一個保存，其餘都丟棄
                DataDrop2.erase(DataDrop2.begin() + abs(DataDrop2.back().y), DataDrop2.end() - 1);
                // 將 Y軸 補全到 R;
                R = DataDrop2.back().x;
                if (Sin > 0)
                {
                    for (int i = DataDrop2.size(); i <= abs(R); ++i)
                    {
                        DataDrop2.push_back({R, i});
                    }
                }
                else
                {
                    for (int i = -DataDrop2.size(); i >= -abs(R); --i)
                    {
                        DataDrop2.push_back({R, i});
                    }
                }
            }
        }
        else
        {
            if (DataDrop1.back().x != R)
            {
                // 將 X軸 相同數值最後的一個作為 關鍵點
                for (auto &i : DataDrop1)
                {
                    DataDrop1[abs(i.x)] = i;
                }
                // 將 X軸 所以數組最後一個保存，其餘都丟棄
                DataDrop1.erase(DataDrop1.begin() + abs(DataDrop1.back().x), DataDrop1.end() - 1);
                // 將 X軸 補全到 R;
                R = DataDrop1.back().y;
                if (Cos > 0)
                {
                    for (int i = DataDrop1.size(); i <= abs(R); ++i)
                    {
                        DataDrop1.push_back({i, R});
                    }
                }
                else
                {
                    for (int i = -DataDrop1.size(); i >= -abs(R); --i)
                    {
                        DataDrop1.push_back({i, R});
                    }
                }
            }
            if (DataDrop2.back().x != R)
            {
                // 將 X軸 相同數值最後的一個作為 關鍵點
                for (auto &i : DataDrop2)
                {
                    DataDrop2[abs(i.x)] = i;
                }
                // 將 X軸 所以數組最後一個保存，其餘都丟棄
                DataDrop2.erase(DataDrop2.begin() + abs(DataDrop2.back().x), DataDrop2.end() - 1);
                // 將 X軸 補全到 R;
                R = DataDrop2.back().y;
                if (Cos > 0)
                {
                    for (int i = DataDrop2.size(); i <= abs(R); ++i)
                    {
                        DataDrop2.push_back({i, R});
                    }
                }
                else
                {
                    for (int i = -DataDrop2.size(); i >= -abs(R); --i)
                    {
                        DataDrop2.push_back({i, R});
                    }
                }
            }
        }
    }

    std::vector<vec2> GetViewBoox(float ViewAngle)
    {
        int R;
        vec2 Line1;
        vec2 Line2;
        std::vector<vec2> Data{};
        float Sin = sin(ViewAngle);
        float Cos = cos(ViewAngle);

        // 算出線段結束的位置
        if (abs(Sin) > abs(Cos))
        {
            R = (Sin < 0 ? -mR : mR);
            Line1 = {int(R / sin(ViewAngle + ViewX / 2) * cos(ViewAngle + ViewX / 2)), R};
            Line2 = {int(R / sin(ViewAngle - ViewX / 2) * cos(ViewAngle - ViewX / 2)), R};
        }
        else
        {
            R = (Cos < 0 ? -mR : mR);
            Line1 = {R, int(R / cos(ViewAngle + ViewX / 2) * sin(ViewAngle + ViewX / 2))};
            Line2 = {R, int(R / cos(ViewAngle - ViewX / 2) * sin(ViewAngle - ViewX / 2))};
        }

        std::vector<vec2> DataDrop1{};
        std::vector<vec2> DataDrop2{};

        // 將線段沿途的點都記錄下來
        Bresenham(0, 0, Line1.x, Line1.y, mR, DataDrop1);
        Bresenham(0, 0, Line2.x, Line2.y, mR, DataDrop2);

        if (abs(Sin) > abs(Cos))
        {
            // 最後點 Y軸 如果位置不等於 R
            if (DataDrop1.back().y != R)
            {
                // 將 Y軸 相同數值最後的一個作為 關鍵點
                for (auto &i : DataDrop1)
                {
                    DataDrop1[abs(i.y)] = i;
                }
                // 將 Y軸 所以數組最後一個保存，其餘都丟棄
                DataDrop1.erase(DataDrop1.begin() + abs(DataDrop1.back().y), DataDrop1.end() - 1);
                // 將 Y軸 補全到 R;
                R = DataDrop1.back().x;
                if (Sin > 0)
                {
                    for (int i = DataDrop1.size(); i <= abs(R); ++i)
                    {
                        DataDrop1.push_back({R, i});
                    }
                }
                else
                {
                    for (int i = -DataDrop1.size(); i >= -abs(R); --i)
                    {
                        DataDrop1.push_back({R, i});
                    }
                }
            }
            if (DataDrop2.back().y != R)
            {
                // 將 Y軸 相同數值最後的一個作為 關鍵點
                for (auto &i : DataDrop2)
                {
                    DataDrop2[abs(i.y)] = i;
                }
                // 將 Y軸 所以數組最後一個保存，其餘都丟棄
                DataDrop2.erase(DataDrop2.begin() + abs(DataDrop2.back().y), DataDrop2.end() - 1);
                // 將 Y軸 補全到 R;
                R = DataDrop2.back().x;
                if (Sin > 0)
                {
                    for (int i = DataDrop2.size(); i <= abs(R); ++i)
                    {
                        DataDrop2.push_back({R, i});
                    }
                }
                else
                {
                    for (int i = -DataDrop2.size(); i >= -abs(R); --i)
                    {
                        DataDrop2.push_back({R, i});
                    }
                }
            }
        }
        else
        {
            if (DataDrop1.back().x != R)
            {
                // 將 X軸 相同數值最後的一個作為 關鍵點
                for (auto &i : DataDrop1)
                {
                    DataDrop1[abs(i.x)] = i;
                }
                // 將 X軸 所以數組最後一個保存，其餘都丟棄
                DataDrop1.erase(DataDrop1.begin() + abs(DataDrop1.back().x), DataDrop1.end() - 1);
                // 將 X軸 補全到 R;
                R = DataDrop1.back().y;
                if (Cos > 0)
                {
                    for (int i = DataDrop1.size(); i <= abs(R); ++i)
                    {
                        DataDrop1.push_back({i, R});
                    }
                }
                else
                {
                    for (int i = -DataDrop1.size(); i >= -abs(R); --i)
                    {
                        DataDrop1.push_back({i, R});
                    }
                }
            }
            if (DataDrop2.back().x != R)
            {
                // 將 X軸 相同數值最後的一個作為 關鍵點
                for (auto &i : DataDrop2)
                {
                    DataDrop2[abs(i.x)] = i;
                }
                // 將 X軸 所以數組最後一個保存，其餘都丟棄
                DataDrop2.erase(DataDrop2.begin() + abs(DataDrop2.back().x), DataDrop2.end() - 1);
                // 將 X軸 補全到 R;
                R = DataDrop2.back().y;
                if (Cos > 0)
                {
                    for (int i = DataDrop2.size(); i <= abs(R); ++i)
                    {
                        DataDrop2.push_back({i, R});
                    }
                }
                else
                {
                    for (int i = -DataDrop2.size(); i >= -abs(R); --i)
                    {
                        DataDrop2.push_back({i, R});
                    }
                }
            }
        }

        if (abs(Sin) > abs(Cos))
        {
            if (Sin > 0)
            {
                for (int i = 0; i < DataDrop1.size(); ++i)
                {
                    for (int ir = DataDrop1[i].x; ir <= DataDrop2[i].x; ++ir)
                    {
                        Data.push_back({ir, i});
                    }
                }
            }
            else
            {
                for (int i = 0; i < DataDrop1.size(); ++i)
                {
                    for (int ir = DataDrop2[i].x; ir <= DataDrop1[i].x; ++ir)
                    {
                        Data.push_back({ir, -i});
                    }
                }
            }
        }
        else
        {
            if (Cos > 0)
            {
                for (int i = 0; i < DataDrop1.size(); ++i)
                {
                    for (int ir = DataDrop2[i].y; ir <= DataDrop1[i].y; ++ir)
                    {
                        Data.push_back({i, ir});
                    }
                }
            }
            else
            {
                for (int i = 0; i < DataDrop1.size(); ++i)
                {
                    for (int ir = DataDrop1[i].y; ir <= DataDrop2[i].y; ++ir)
                    {
                        Data.push_back({-i, ir});
                    }
                }
            }
        }

        for (auto i : DataDrop1)
        {
            std::cout << i.x << " - " << i.y << std::endl;
        }
        std::cout << " ******************** " << std::endl;
        for (auto i : DataDrop2)
        {
            std::cout << i.x << " - " << i.y << std::endl;
        }

        std::cout << " size: " << DataDrop1.size() << " - " << DataDrop2.size() << std::endl;

        return Data;
    }

private:
    inline void Bresenham(int x0, int y0, int x1, int y1, int R, std::vector<vec2> &data)
    {
        int dx = abs(x1 - x0);
        int dy = abs(y1 - y0);
        int sx = (x0 < x1) ? 1 : -1;
        int sy = (y0 < y1) ? 1 : -1;
        int err = dx - dy;
        int e2;
        while (true)
        {
            data.push_back({x0, y0});
            if (abs(x0) >= R || abs(y0) >= R)
                break;
            e2 = 2 * err;
            if (e2 > -dy)
            {
                err -= dy;
                x0 += sx;
            }
            if (e2 < dx)
            {
                err += dx;
                y0 += sy;
            }
        }
    }
};
