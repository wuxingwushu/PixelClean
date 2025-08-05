#pragma once
#include "PhysicsFormwork.hpp"
#include <vector>

namespace PhysicsBlock
{

// 是否采用莫顿编码
#define Morton_define 1

    class GridSearch
    {
    private:
        std::vector<std::vector<PhysicsFormwork *>> Grid; // 网格的 一维展开
        int mStorey = 0;                                  // 四叉网格层数
        Vec2_ mExcursion{0};                                 // 相对世界坐标系的位置偏移量

    public:
        GridSearch() {}
        ~GridSearch() {}

    private:
#if Morton_define
        // 莫顿码
        inline uint_fast32_t Morton2D(uint_fast16_t x, uint_fast16_t y)
        {
            uint_fast64_t m = x | (uint_fast64_t(y) << 32); // put Y in upper 32 bits, X in lower 32 bits
            m = (m | (m << 8)) & 0x00FF00FF00FF00FF;
            m = (m | (m << 4)) & 0x0F0F0F0F0F0F0F0F;
            m = (m | (m << 2)) & 0x3333333333333333;
            m = (m | (m << 1)) & 0x5555555555555555;
            m = m | (m >> 31); // merge X and Y back together
            // hard cut off to 32 bits, because on some systems uint_fast32_t will be a 64-bit type, and we don't want to retain split Y-version in the upper 32 bits.
            m = m & 0x00000000FFFFFFFF;
            return uint_fast32_t(m);
        }
#endif

        // 四叉网格的第几层，需要从偏移多少个开始算
        inline unsigned int Excursion(int storey)
        {
            int Ppos = 0;
            for (int i = 0; i <= (mStorey - storey); ++i)
            {
                Ppos <<= 2;
                ++Ppos;
            }
            return Ppos;
        };

        // 计算在四叉网格的哪一个细分层
        inline int Storey(FLOAT_ R)
        {
            unsigned int r = R * 2 + 0.99;
            //++r;
            unsigned int m = 1;
            while (r >>= 1)
            {
                ++m;
            }
            return m;
        }

        // 世界位置处理为网格坐标位置
        inline glm::ivec2 Pos_ToInt(Vec2_ pos)
        {
            return (pos + mExcursion);
        }

        inline std::vector<PhysicsFormwork *> &at(unsigned int i) { return Grid[i]; }

#if Morton_define
        // 获取对应网格
        inline std::vector<PhysicsFormwork *> &at(unsigned int x, unsigned int y, unsigned int _Excursion, int _storey) { return at(_Excursion + Morton2D(x, y)); }
#else
        // 获取对应网格
        inline std::vector<PhysicsFormwork *> &at(unsigned int x, unsigned int y, unsigned int _Excursion, int _storey) { return at(_Excursion + (x * (1 << (mStorey + 1 - _storey)) + y)); }
#endif

        // 计算对应网格索引
        inline unsigned int atIndex(Vec2_ xy, FLOAT_ R)
        {
            xy += mExcursion;
            glm::ivec2 pos = (xy);
            int _storey = Storey(R);
            pos >>= _storey;

            // 判断是否在网格边缘，是就放在上层较大的网格内
            glm::ivec2 Spos = pos << _storey;
            glm::ivec2 Epos = (pos + 1) << _storey;
            if ((Spos.x > (xy.x - R)) || (Epos.x < (xy.x + R)) || (Spos.y > (xy.y - R)) || (Epos.y < (xy.y + R)))
            {
                ++_storey;
                pos >>= 1;
            }

#if Morton_define
            return Excursion(_storey) + Morton2D(pos.x, pos.y);
#else
            return Excursion(_storey) + (pos.x * (1 << (mStorey + 1 - _storey))) + pos.y;
#endif
        }

        // 获取对应网格
        inline std::vector<PhysicsFormwork *> &at(Vec2_ xy, FLOAT_ R)
        {
            return at(atIndex(xy, R));
        }

    public:
        // 设置大小
        void SetMapRange(unsigned int Size)
        {
            Size *= 4;
            mExcursion = {Size / 2, Size / 2};
            unsigned int GridSize = 1;
            while (Size >>= 1)
            {
                GridSize <<= 2;
                ++mStorey;
                ++GridSize;
            }
            Grid.clear();
            Grid.resize(GridSize);
        }

        // 添加
        void Add(PhysicsFormwork *atocr)
        {
            at(atocr->PFGetPos(), atocr->PFGetCollisionR()).push_back(atocr);
        }

        // 去除
        void Remove(PhysicsFormwork *atocr)
        {
            unsigned int index = atIndex(atocr->PFGetPos(), atocr->PFGetCollisionR());
            for (size_t i = 0; i < Grid[index].size(); ++i)
            {
                if (Grid[index][i] == atocr)
                {
                    Grid[index][i] = Grid[index].back();
                    Grid[index].pop_back();
                }
            }
        }

        // 获取这个范围内所有对象
        std::vector<PhysicsFormwork *> Get(Vec2_ pos, FLOAT_ R)
        {
            std::vector<PhysicsFormwork *> V;
            R += 1; // 范围稍微加大点。
            Vec2_ S = pos + mExcursion - R;
            Vec2_ E = pos + mExcursion + R;
            glm::ivec2 iS = S;
            glm::ivec2 iE = E;

            unsigned int P = Grid.size();

            for (int i = 1; i <= mStorey; ++i)
            {
                iS >>= 1;
                iE >>= 1;
                P >>= 2;
                for (size_t x = iS.x; x <= iE.x; ++x)
                {
                    for (size_t y = iS.y; y <= iE.y; ++y)
                    {
                        for (auto j : at(x, y, P, i))
                        {
                            V.push_back(j);
                        }
                    }
                }
            }
            return V;
        }

        // 更新网格
        void UpData()
        {
            for (size_t i = 0; i < Grid.size(); ++i)
            {
                for (size_t a = 0; a < Grid[i].size(); ++a)
                {
                    PhysicsFormwork *Formwork = Grid[i][a];
                    unsigned int index = atIndex(Formwork->PFGetPos(), Formwork->PFGetCollisionR());
                    if (i != index)
                    {
                        Grid[i][a] = Grid[i].back();
                        Grid[i].pop_back();
                        Grid[index].push_back(Formwork);
                    }
                }
            }
        }
    };

}
