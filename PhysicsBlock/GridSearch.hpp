#pragma once
#include "PhysicsFormwork.hpp"
#include <vector>

namespace PhysicsBlock
{

// 是否采用莫顿编码优化网格索引
// 启用后使用 Z-order 曲线进行空间索引，提高缓存局部性
#define Morton_define 1

// 是否预计算每层网格的偏移量
// 启用后将偏移量存储在数组中，避免运行时重复计算
#define CurerExcursionVector 1

    /**
     * @brief   四叉网格搜索类
     * @details 用于物理引擎中的空间索引，支持多层四叉网格和莫顿编码
     *          通过分层网格实现高效的碰撞检测和对象查询
     */
    class GridSearch
    {
    private:
        /**
         * @brief   网格容器（一维展开）
         * @details 存储所有网格层，每层包含多个物理对象指针列表
         *          使用一维数组存储四叉树结构，通过偏移量计算每层的实际位置
         */
        std::vector<std::vector<PhysicsFormwork *>> Grid;

        /**
         * @brief   网格外对象容器
         * @details 存储超出网格范围的物理对象（例如超大型对象）
         *          这些对象在碰撞检测时需要特殊处理
         */
        std::vector<PhysicsFormwork *> GridExtrovert;

        /**
         * @brief   四叉网格层数
         * @details 表示网格的最大细分层数，层数越多网格越精细
         *          第0层为根节点（整个空间），每增一层将空间四等分
         */
        int mStorey = 0;

        /**
         * @brief   相对世界坐标系的位置偏移量
         * @details 将世界坐标转换为网格坐标的偏移量，使负坐标也能正确映射
         *          例如世界范围是 [-Size/2, Size/2]，加上偏移后变成 [0, Size]
         */
        Vec2_ mExcursion{0};

#if CurerExcursionVector
        /**
         * @brief   预计算的每层偏移量数组
         * @details 存储每层网格在一维数组中的起始索引偏移
         *          避免每次查询时重复计算，提升性能
         */
        unsigned int *ExcursionVector = nullptr;
#endif

    public:
        /**
         * @brief 默认构造函数
         */
        GridSearch() {}

        /**
         * @brief 析构函数
         * @details 释放预计算的偏移量数组内存
         */
        ~GridSearch() { if(ExcursionVector != nullptr) { delete[] ExcursionVector; } }

    private:
#if Morton_define
        /**
         * @brief   莫顿码编码（Z-order曲线）
         * @param   x x 坐标（已转换为无符号整数）
         * @param   y y 坐标（已转换为无符号整数）
         * @return  返回一维索引值（32位无符号整数）
         * @details 使用位交错算法，将二维坐标映射到一维索引
         *          让二维空间中相邻的点在一维数组中也尽可能相邻
         *          这大大提高了CPU缓存的命中率，因为访问相邻空间的对象时
         *          它们的莫顿码也相邻，在内存中也是相邻的
         *
         * 算法原理：
         * 1. 将x和y扩展到64位并拼接：xxxxxxx....yyyyyyy....
         * 2. 通过一系列位掩码和移位操作，将相邻的位交错在一起
         * 3. 最终结果的位模式为：x0y0x1y1x2y2x3y3...（交替排列）
         *
         * 示例：点(3, 5)的莫顿码计算
         *   3 的二进制：  0011
         *   5 的二进制：  0101
         *   交错后：   0-0-1-1-0-1-0-1 -> 00010101 -> 21
         */
        inline uint_fast32_t Morton2D(uint_fast16_t x, uint_fast16_t y)
        {
            uint_fast64_t m = x | (uint_fast64_t(y) << 32);
            m = (m | (m << 8)) & 0x00FF00FF00FF00FF;  // 第一步：分离奇偶位
            m = (m | (m << 4)) & 0x0F0F0F0F0F0F0F0F;  // 第二步：进一步分离
            m = (m | (m << 2)) & 0x3333333333333333;  // 第三步：位交错
            m = (m | (m << 1)) & 0x5555555555555555;  // 第四步：完成位交错
            m = m | (m >> 31);                         // 处理符号位扩展问题
            m = m & 0x00000000FFFFFFFF;                // 取低32位
            return uint_fast32_t(m);
        }
#endif

        /**
         * @brief   计算四叉网格某层的起始偏移量
         * @param   storey 网格层数（从0开始，0表示最高层/根节点）
         * @return  返回该层在一维数组中的起始索引
         * @details 使用公式 (4^(k+1) - 1) / 3 计算偏移量
         *          其中 k = mStorey - storey，表示从该层到最底层的层数差
         *
         * 四叉树一维展开原理：
         *   第0层（根）：1个节点                 -> 偏移0
         *   第1层：     4个节点                 -> 偏移1
         *   第2层：     16个节点                -> 偏移1+4=5
         *   第3层：     64个节点                -> 偏移1+4+16=21
         *   ...
         *   偏移量 = (4^1 - 1)/3 + (4^2 - 1)/3 + ... + (4^k - 1)/3 = (4^(k+1) - 1)/3
         */
        inline unsigned int Excursion(int storey)
        {
            int k = mStorey - storey;
            if (k < 0) return 0;
            unsigned int n = k + 1;
            unsigned int result = (1U << (2 * n)) - 1;  // 2n位全1，即4^n - 1
            result /= 3;                                // (4^n - 1) / 3
            return result;
        }

        /**
         * @brief   计算对象应该放入的网格细分层
         * @param   R 对象的碰撞半径（世界坐标系单位）
         * @return  返回网格层数（整数，0表示最高层）
         * @details 根据对象大小确定合适的网格层级
         *          大对象放入上层粗网格，小对象放入下层细网格
         *
         * 原则：对象占据的空间应该能被当前层的单个网格单元格容纳
         *
         * 算法：
         * 1. 计算对象直径（2*R）并上取整
         * 2. 找到这个数值的最高有效位的位置
         * 3. 该位置即为合适的网格层级
         *
         * 示例：R = 3.5，直径 = 8，取最高位得 index=3，所以 _storey=3
         */
        inline int Storey(FLOAT_ R)
        {
            // 计算对象占据的网格数量（直径 / 1，加上一点余量向上取整）
            unsigned int r = (unsigned int)(R * 2 + 0.99);
            if (r == 0) return 1;  // 零半径对象默认放在第1层

            // 找到最高有效位的位置（从0开始计数）
            unsigned long index;
            #if defined(_MSC_VER)
                _BitScanReverse(&index, r);  // MSVC intrinsics
            #else
                index = 31 - __builtin_clz(r);  // GCC/Clang intrinsics
            #endif
            return (int)index + 1;  // +1因为index是从0开始的，而第0层是根节点
        }

        /**
         * @brief   世界位置转换为网格坐标
         * @param   pos 世界坐标位置（可以是负数）
         * @return  返回网格坐标（整数，始终为正数）
         * @details 通过加上偏移量将负坐标转换为正坐标
         *          这样可以使用无符号整数进行网格索引计算
         */
        inline glm::ivec2 Pos_ToInt(Vec2_ pos)
        {
            return (pos + mExcursion);
        }

        /**
         * @brief   获取指定索引的网格
         * @param   i 网格索引（一维数组中的位置）
         * @return  返回网格内的对象列表引用
         */
        inline std::vector<PhysicsFormwork *> &at(unsigned int i) { return Grid[i]; }

#if Morton_define
        /**
         * @brief   获取指定位置和层级的网格（莫顿编码版本）
         * @param   x x 坐标（网格坐标，已转换为无符号）
         * @param   y y 坐标（网格坐标，已转换为无符号）
         * @param   _Excursion 该层的起始偏移量
         * @param   _storey 网格层数
         * @return  返回网格内的对象列表引用
         * @details 通过莫顿码直接计算一维索引，比线性计算更快
         */
        inline std::vector<PhysicsFormwork *> &at(unsigned int x, unsigned int y, unsigned int _Excursion, int _storey)
        {
            return at(_Excursion + Morton2D(x, y));
        }
#else
        /**
         * @brief   获取指定位置和层级的网格（线性版本）
         * @param   x x 坐标（网格坐标）
         * @param   y y 坐标（网格坐标）
         * @param   _Excursion 该层的起始偏移量
         * @param   _storey 网格层数
         * @return  返回网格内的对象列表引用
         * @details 使用线性计算：index = offset + x * width + y
         *          其中 width = 2^(mStorey + 1 - _storey)
         */
        inline std::vector<PhysicsFormwork *> &at(unsigned int x, unsigned int y, unsigned int _Excursion, int _storey)
        {
            return at(_Excursion + (x * (1U << (mStorey + 1 - _storey)) + y));
        }
#endif

        /**
         * @brief   计算对象所在的网格索引
         * @param   xy 对象位置（世界坐标）
         * @param   R 对象碰撞半径
         * @return  返回网格索引（一维数组中的位置）
         * @details 根据对象位置和大小计算应该放入的网格
         *          如果对象跨网格边界（不在任何子网格内），则放入上层更大的网格
         *
         * 跨边界判断：
         *   对于一个对象，我们计算它所在的"理想"细网格
         *   然后检查对象是否完全在该网格内（没有越界）
         *   如果越界了，说明对象太大，放到上一层更粗的网格更合适
         */
        inline unsigned int atIndex(Vec2_ xy, FLOAT_ R)
        {
            // 转换到网格坐标系（处理负坐标）
            xy += mExcursion;
            glm::ivec2 pos = (xy);

            // 确定应该放在哪一层
            int _storey = Storey(R);

            // 右移相当于除以2^_storey，即计算在当前层的网格坐标
            pos >>= _storey;

            // 计算这个网格单元格的边界
            glm::ivec2 Spos = pos << _storey;      // 网格起始点（世界坐标）
            glm::ivec2 Epos = (pos + 1) << _storey; // 网格结束点（世界坐标）

            // 判断对象是否跨网格边界
            if ((Spos.x > (xy.x - R)) || (Epos.x < (xy.x + R)) || 
                (Spos.y > (xy.y - R)) || (Epos.y < (xy.y + R)))
            {
                ++_storey;    // 升级到上一层（更大的网格）
                pos >>= 1;    // 在上一层中的位置也要重新计算
            }

#if CurerExcursionVector
#if Morton_define
            return ExcursionVector[_storey] + Morton2D((uint_fast16_t)pos.x, (uint_fast16_t)pos.y);
#else
            return ExcursionVector[_storey] + (pos.x * (1 << (mStorey + 1 - _storey))) + pos.y;
#endif
#else
#if Morton_define
            return Excursion(_storey) + Morton2D((uint_fast16_t)pos.x, (uint_fast16_t)pos.y);
#else
            return Excursion(_storey) + (pos.x * (1 << (mStorey + 1 - _storey))) + pos.y;
#endif
#endif
        }

        /**
         * @brief   获取对象所在的网格
         * @param   xy 对象位置（世界坐标）
         * @param   R 对象碰撞半径
         * @return  返回网格内的对象列表引用
         */
        inline std::vector<PhysicsFormwork *> &at(Vec2_ xy, FLOAT_ R)
        {
            return at(atIndex(xy, R));
        }

    public:
        /**
         * @brief   设置网格范围大小
         * @param   Size 网格的单边大小（世界坐标单位）
         * @details 初始化网格容器，计算网格层数和偏移量
         *          网格实际大小为 Size * 4，支持负坐标区域
         *
         * 空间划分：
         *   输入Size后，实际网格空间是[-2*Size, 2*Size]（即4*Size宽高）
         *   这是因为偏移量设置为Size/2，使得原本[-Size/2, Size/2]的区域
         *   被偏移到[0, Size]，再乘以4得到最终空间
         */
        void SetMapRange(unsigned int Size)
        {
#if CurerExcursionVector
            if (ExcursionVector != nullptr)
            {
                delete[] ExcursionVector;
                ExcursionVector = nullptr;
            }
#endif
            Size *= 4;                                // 实际空间是输入的4倍
            mExcursion = {Size / 2.0f, Size / 2.0f};  // 偏移量，使负坐标变为正坐标
            unsigned int GridSize = 1;                // 第0层的节点数

            // 计算网格层数和总大小
            // 每次循环：空间尺寸缩小一半，节点数增加4倍加1
            // 循环结束时，GridSize = (4^0 + 4^1 + ... + 4^mStorey) = (4^(mStorey+1) - 1) / 3
            while (Size >>= 1)
            {
                GridSize <<= 2;   // GridSize = GridSize * 4
                ++mStorey;        // 层数加1
                ++GridSize;       // 总节点数加上这一层的节点数
            }

#if CurerExcursionVector
            // 预计算每层的偏移量，避免运行时重复计算
            ExcursionVector = new unsigned int[mStorey];
            for (size_t i = 0; i < mStorey; i++)
            {
                ExcursionVector[i] = Excursion((int)i);
            }
#endif

            Grid.clear();
            Grid.resize(GridSize);  // 调整网格容器大小
        }

        /**
         * @brief   添加物理对象到网格
         * @param   atocr 要添加的物理对象指针
         * @details 根据对象位置和碰撞半径计算网格索引，将对象添加到对应网格
         *          如果索引超出范围，添加到网格外容器
         *
         * 添加策略：
         *   1. 计算对象的网格索引（atIndex）
         *   2. 如果索引在有效范围内，加入对应网格
         *   3. 否则（超大型对象），加入网格外容器
         */
        void Add(PhysicsFormwork *atocr)
        {
            unsigned int index = atIndex(atocr->PFGetPos(), atocr->PFGetCollisionR());
            if (index < Grid.size())
            {
                at(index).push_back(atocr);
            }
            else
            {
                GridExtrovert.push_back(atocr);
            }
        }

        /**
         * @brief   从网格中移除物理对象
         * @param   atocr 要移除的物理对象指针
         * @details 先在网格中查找并移除，如果不在网格中则从网格外容器中移除
         *
         * 移除策略（使用swap-and-pop优化）：
         *   1. 不使用erase删除元素（可能触发大量元素移动）
         *   2. 用最后一个元素覆盖要删除的元素
         *   3. pop_back删除最后一个元素
         *   时间复杂度从O(n)降低到O(1)
         */
        void Remove(PhysicsFormwork *atocr)
        {
            unsigned int index = atIndex(atocr->PFGetPos(), atocr->PFGetCollisionR());

            // 在网格中查找并移除
            for (size_t i = 0; i < Grid[index].size(); ++i)
            {
                if (Grid[index][i] == atocr)
                {
                    Grid[index][i] = Grid[index].back();  // swap-and-pop
                    Grid[index].pop_back();
                    return;
                }
            }

            // 在网格外容器中查找并移除
            for (size_t i = 0; i < GridExtrovert.size(); ++i)
            {
                if (GridExtrovert[i] == atocr)
                {
                    GridExtrovert[i] = GridExtrovert.back();  // swap-and-pop
                    GridExtrovert.pop_back();
                    return;
                }
            }
        }

        /**
         * @brief   获取指定圆形范围内的所有对象
         * @param   pos 范围中心位置（世界坐标）
         * @param   R 范围半径
         * @param   Out 输出参数，接收查询到的对象列表
         * @details 将圆形范围转换为正方形范围（外接矩形），然后调用Get(Spos, Epos)
         *          注意：返回的对象可能包含在圆形范围之外的对象，调用方需要二次筛选
         */
        void Get(Vec2_ pos, FLOAT_ R, std::vector<PhysicsFormwork *> &Out)
        {
            R *= 2;  // 半径转直径，得到正方形的外接矩形
            Get(pos - R, pos + R, Out);
        }

        /**
         * @brief   获取指定矩形范围内的所有对象
         * @param   Spos 矩形起始点（左下角，世界坐标）
         * @param   Epos 矩形结束点（右上角，世界坐标）
         * @param   Out 输出参数，接收查询到的对象列表
         * @details 遍历所有与给定矩形相交的网格层和单元格
         *          将所有找到的对象添加到Out中（使用insert批量插入）
         *
         * 算法：
         *   1. 将世界坐标转换为网格坐标（加偏移量）
         *   2. 从最底层（第mStorey层）开始向上遍历
         *   3. 每上一层，坐标右移1位（相当于坐标除以2）
         *   4. 遍历所有在查询范围内的网格单元格
         *   5. 将所有单元格中的对象合并到Out中
         */
        void Get(Vec2_ Spos, Vec2_ Epos, std::vector<PhysicsFormwork *> &Out)
        {
            Out.clear();

            // 坐标转换到网格坐标系
            Vec2_ S = Spos + mExcursion;
            Vec2_ E = Epos + mExcursion;
            glm::uvec2 iS = glm::uvec2(S);  // 转换为无符号整数网格坐标
            glm::uvec2 iE = glm::uvec2(E);

            // 计算坐标上限（用于边界检查）
            unsigned int w = (unsigned int)(mExcursion.x + mExcursion.x - 1);
            bool Extrovert = false;  // 标记查询范围是否超出网格边界

            // 检查并处理超出边界的情况
            if (iS.x > w) { Extrovert = true; iS.x = w; }  // 左边界超出
            if (iS.y > w) { Extrovert = true; iS.y = w; }  // 下边界超出
            if (iE.x > w) { Extrovert = true; iE.x = w; }  // 右边界超出
            if (iE.y > w) { Extrovert = true; iE.y = w; }  // 上边界超出

            // 如果查询范围超出边界，需要包含网格外容器中的所有对象
            if (Extrovert)
            {
                Out.insert(Out.end(), GridExtrovert.begin(), GridExtrovert.end());
            }

            // 从最底层开始向上遍历所有层
            unsigned int P = (unsigned int)Grid.size();  // P初始为总网格数的近似值

            for (int i = 1; i <= mStorey; ++i)
            {
                iS >>= 1;  // 上一层中的网格坐标
                iE >>= 1;
                P >>= 2;  // 上一层每维的网格数量

                // 遍历所有在查询范围内的网格单元格
                for (size_t x = iS.x; x <= iE.x; ++x)
                {
                    for (size_t y = iS.y; y <= iE.y; ++y)
                    {
                        auto &cell = at(x, y, P, i);
                        Out.insert(Out.end(), cell.begin(), cell.end());
                    }
                }
            }
        }

        /**
         * @brief   获取物体所在的所有细分网格范围
         * @param   atocr 物理对象指针
         * @return  返回网格范围列表（每两个Vec2_构成一个矩形对角点）
         * @details 返回对象所在的所有层级网格的边界坐标
         *          第一个矩形对应对象直接所在的网格，后续是对应的所有上级网格
         *
         * 用途：
         *   用于碰撞检测时的"视野"计算
         *   当一个对象移动时，只需要检查它所在的所有网格范围内的其他对象
         *   这样可以大大减少碰撞检测的比较次数
         */
        std::vector<Vec2_> GetDividedVision(PhysicsFormwork *atocr)
        {
            std::vector<Vec2_> Vision;

            Vec2_ atocr_pos = atocr->PFGetPos();
            FLOAT_ R = atocr->PFGetCollisionR();
            atocr_pos += mExcursion;
            glm::ivec2 pos = (atocr_pos);
            int _storey = Storey(R);
            pos >>= _storey;

            // 计算网格边界
            glm::ivec2 Spos = pos << _storey;
            glm::ivec2 Epos = (pos + 1) << _storey;

            // 检查是否跨边界，如果跨了则放到上一层
            if ((Spos.x > (atocr_pos.x - R)) || (Epos.x < (atocr_pos.x + R)) ||
                (Spos.y > (atocr_pos.y - R)) || (Epos.y < (atocr_pos.y + R)))
            {
                ++_storey;
                pos >>= 1;
                Spos = pos << _storey;
                Epos = (pos + 1) << _storey;
            }

            // 添加对象直接所在的网格范围（转换回世界坐标）
            Vision.push_back(Vec2_(Spos) - mExcursion);
            Vision.push_back(Vec2_(Epos) - mExcursion);

            // 添加所有上层网格的范围
            for (int i = _storey; i <= mStorey; ++i)
            {
                ++_storey;
                pos >>= 1;
                Spos = pos << _storey;
                Epos = (pos + 1) << _storey;
                Vision.push_back(Vec2_(Spos) - mExcursion);
                Vision.push_back(Vec2_(Epos) - mExcursion);
            }

            return Vision;
        }

        /**
         * @brief   获取所有网格外的对象
         * @return  返回网格外对象容器的引用
         * @details 返回那些因为体积太大而无法放入任何网格的对象
         *          这些对象在进行碰撞检测时需要与所有其他对象进行比较
         */
        std::vector<PhysicsFormwork *> &GetGridExtrovert()
        {
            return GridExtrovert;
        }

        /**
         * @brief   更新所有对象的网格位置
         * @details 遍历所有网格和网格外对象，重新计算它们的网格索引
         *          如果对象移动后索引发生变化，将对象移动到新的网格
         *
         * 注意：这是一个O(n)复杂度的操作，应该尽量减少调用频率
         *       典型用法是每帧调用一次，而不是每帧为每个对象调用
         */
        void UpData()
        {
            // 更新网格内的对象
            for (size_t i = 0; i < Grid.size(); ++i)
            {
                for (size_t a = 0; a < Grid[i].size(); ++a)
                {
                    PhysicsFormwork *Formwork = Grid[i][a];
                    unsigned int index = atIndex(Formwork->PFGetPos(), Formwork->PFGetCollisionR());

                    if (i != index)
                    {
                        // 对象移动到了不同的网格
                        Grid[i][a] = Grid[i].back();  // swap-and-pop
                        Grid[i].pop_back();
                        --a;  // 因为最后一个元素移到了当前位置，所以要重新检查当前位置

                        if (index < Grid.size())
                        {
                            Grid[index].push_back(Formwork);
                        }
                        else
                        {
                            GridExtrovert.push_back(Formwork);
                        }
                    }
                }
            }

            // 更新网格外的对象（检查它们是否回到了网格范围内）
            for (size_t i = 0; i < GridExtrovert.size(); ++i)
            {
                PhysicsFormwork *Formwork = GridExtrovert[i];
                unsigned int index = atIndex(Formwork->PFGetPos(), Formwork->PFGetCollisionR());

                if (index < Grid.size())
                {
                    // 对象回到了网格范围内
                    GridExtrovert[i] = GridExtrovert.back();  // swap-and-pop
                    GridExtrovert.pop_back();
                    --i;
                    Grid[index].push_back(Formwork);
                }
            }
        }
    };

}
