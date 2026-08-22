#pragma once
#include "PhysicsFormwork.hpp"
#include <vector>
#include <algorithm>

namespace PhysicsBlock
{

// 是否采用莫顿编码优化网格索引
// 启用后使用 Z-order 曲线进行空间索引，提高缓存局部性
#define Morton_define 1

// 是否预计算每层网格的偏移量
// 启用后将偏移量存储在数组中，避免运行时重复计算
#define CurerExcursionVector 1

    /**
     * @brief   四叉网格搜索类（内存优化版：索引化链表）
     * @details 用于物理引擎中的空间索引，支持多层四叉网格和莫顿编码。
     *
     * 内存优化说明（方案 C：稠密索引数组 + 槽位链表）：
     * - 旧实现 Grid 为 std::vector<std::vector<PhysicsFormwork*>>：
     *   每格固定 24 字节（x64 三指针）+ 每占用格一个独立堆块（16B 头 + 容量），
     *   且对象搬移后旧格容量永不回落（高水位内存），
     *   1024 地图时 Grid 空载即约 537MB。
     * - 新实现：每格仅 4 字节链头（mCellHead，整块连续分配，零逐格 malloc），
     *   对象通过槽位（mSlots 下标）以链表串接（mNext），
     *   1024 地图时索引区仅约 21.4MB；且 mGridDim 取整为 2 的幂后，
     *   紧凑层偏移使索引区仅为旧布局的 1/4（对 2 的幂尺寸）或不超过旧布局（任意尺寸）。
     * - 每帧 UpData 改为 O(对象数) 的整树重建：只清"脏格"（上帧写入过的格），
     *   稳态零分配；多线程路径（UpDaraWorkeTask/End）保持"并行算索引 + 串行应用"结构。
     */
    class GridSearch
    {
    private:
        /**
         * @brief   每格链表头（4 字节/格）
         * @details 存储对象槽位下标（mSlots 的下标），UINT_MAX 表示该格为空。
         *          取代旧实现"每格一个 std::vector"的 24 字节固定开销 + 逐格堆分配。
         */
        std::vector<unsigned int> mCellHead;

        /**
         * @brief   对象槽位容器（8 字节/对象）
         * @details 存储网格内所有物理对象指针；槽位采用"删除回收进 mFreeSlots 栈"
         *          的方式复用，槽位内容从不被 swap/搬移，因此链表里的槽位下标全程稳定。
         */
        std::vector<PhysicsFormwork *> mSlots;

        /**
         * @brief   槽位在格内链表中的下一槽位（4 字节/对象）
         * @details mSlots[i] 与 mNext[i] 平行；遍历一格 = 从链头顺 mNext 走到底。
         */
        std::vector<unsigned int> mNext;

        /**
         * @brief   每帧多线程 UpData 计算的"新格号"暂存区（4 字节/对象）
         * @details 各线程只写自己分到的槽位区间（写不相交，无数据竞争），
         *          主线程 UpDaraWorkeTaskEnd 统一按此重建整树。
         */
        std::vector<unsigned int> mNewIndex;

        /**
         * @brief   空闲槽位栈
         * @details Remove() 把槽位入栈，Add() 优先复用，对象数量波动时内存不增长。
         */
        std::vector<unsigned int> mFreeSlots;

        /**
         * @brief   上帧写入过的格子（脏格集合，无重复元素）
         * @details 整树重建时只需把这些格的头清零（成本 ∝ 对象数），
         *          而不是 memset 整张网格表（成本 ∝ 格数）。
         */
        std::vector<unsigned int> mDirtyCells;

        /**
         * @brief   网格外对象容器
         * @details 存储超出网格范围的物理对象（例如超大型对象）
         *          这些对象在碰撞检测时需要特殊处理
         */
        std::vector<PhysicsFormwork *> GridExtrovert;

        /**
         * @brief   网格总格数（头部数组长度）
         * @details 等于 SetMapRange 中紧凑布局的 (4^mStorey - 1) / 3
         */
        unsigned int mGridSize = 0;

        /**
         * @brief   四叉网格层数
         * @details 表示网格的最大细分层数，层数越多网格越精细
         *          第 mStorey 层为根节点（整个空间，1 格），每往下一层空间四等分
         */
        int mStorey = 0;

        /**
         * @brief   网格维度（最细层级网格坐标上限）
         * @details 等于 SetMapRange 输入 Size 的 4 倍
         *          有效网格坐标范围为 [0, mGridDim - 1]
         */
        unsigned int mGridDim = 0;

        /**
         * @brief   相对世界坐标系的位置偏移量
         * @details 将世界坐标转换为网格坐标的偏移量，使负坐标也能正确映射
         *          例如世界范围是 [-Size/2, Size/2]，加上偏移后变成 [0, Size]
         */
        Vec2_ mExcursion{0};

        /**
         * @brief   线程数量
         * @details 用于多线程 UpData 的线程数，默认为 0（单线程模式）
         */
        unsigned int mThreadCount = 0;

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
         * @details 释放预计算的偏移量数组内存（其余容器为 RAII 自动释放）
         */
        ~GridSearch()
        {
            if (ExcursionVector != nullptr)
            {
                delete[] ExcursionVector;
            }
        }

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
            m = (m | (m << 8)) & 0x00FF00FF00FF00FF; // 第一步：分离奇偶位
            m = (m | (m << 4)) & 0x0F0F0F0F0F0F0F0F; // 第二步：进一步分离
            m = (m | (m << 2)) & 0x3333333333333333; // 第三步：位交错
            m = (m | (m << 1)) & 0x5555555555555555; // 第四步：完成位交错
            m = m | (m >> 31);                       // 处理符号位扩展问题
            m = m & 0x00000000FFFFFFFF;              // 取低32位
            return uint_fast32_t(m);
        }
#endif

        /**
         * @brief   计算四叉网格某层的起始偏移量（紧凑布局）
         * @param   storey 网格层数（1 = 最细层，mStorey = 根层/整个空间）
         * @return  返回该层在一维数组中的起始索引
         * @details 公式：off(storey) = (4^(mStorey - storey) - 1) / 3
         *
         * 紧凑布局原理（层 storey 含 4^(mStorey-storey) 个格子）：
         *   storey = mStorey   （根，1 格）    -> 偏移 0
         *   storey = mStorey-1 （4 格）        -> 偏移 1
         *   storey = mStorey-2 （16 格）       -> 偏移 1+4 = 5
         *   ...
         *   偏移 = 4^0 + 4^1 + ... + 4^(mStorey-storey-1) = (4^(mStorey-storey) - 1)/3
         *
         * 注意：旧实现公式为 (4^(mStorey-storey+1)-1)/3，为兼容任意非 2 的幂 mGridDim
         * 每层预留了 4 倍容量（层坐标多 1 bit）；本实现将 mGridDim 取整为 2 的幂，
         * 因此该公式即为精确紧凑布局：所有格子全部可达且数组长度仅为旧实现的 1/4
         * （对 2 的幂尺寸）或 ≤ 旧实现（对任意尺寸）。
         */
        inline unsigned int Excursion(int storey)
        {
            int k = mStorey - storey;
            if (k <= 0)
                return 0; // 根层（或超出范围）偏移为 0
            return (unsigned int)(((1ULL << (2 * k)) - 1) / 3);
        }

        /**
         * @brief   计算对象应该放入的网格细分层
         * @param   R 对象的碰撞半径（世界坐标系单位）
         * @return  返回网格层数（整数，1 = 最细层，mStorey = 根层）
         * @details 根据对象大小确定合适的网格层级
         *          大对象放入上层粗网格，小对象放入下层细网格
         *
         * 原则：对象占据的空间应该能被当前层的单个网格单元格容纳
         *
         * 算法：
         * 1. 计算对象直径（2*R）并上取整
         * 2. 找到这个数值的最高有效位的位置
         * 3. 该位置 + 1 即为合适的网格层级
         *
         * 示例：R = 3.5，直径 = 8，取最高位得 index=3，所以 _storey=4
         */
        inline int Storey(FLOAT_ R)
        {
            // 计算对象占据的网格数量（直径 / 1，加上一点余量向上取整）
            unsigned int r = (unsigned int)(R * 2 + 0.99);
            if (r == 0)
                return 1; // 零半径对象默认放在第1层

            // 找到最高有效位的位置（从0开始计数）
            unsigned long index;
#if defined(_MSC_VER)
            _BitScanReverse(&index, r); // MSVC intrinsics
#else
            index = 31 - __builtin_clz(r); // GCC/Clang intrinsics
#endif
            return (int)index + 1; // +1因为index是从0开始的，而第0层是根节点
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
         * @brief   计算对象所在的网格索引
         * @param   xy 对象位置（世界坐标）
         * @param   R 对象碰撞半径
         * @return  返回网格索引（一维数组中的位置）；≥ mGridSize 表示网格外
         * @details 根据对象位置和大小计算应该放入的网格
         *          如果对象跨网格边界（不在任何子网格内），则放入上层更大的网格
         *          超大型对象（_storey > mStorey，比整个网格还大）直接返回无效索引，
         *          由 Add/UpData 放入 GridExtrovert —— 修复了旧实现
         *          "ExcursionVector[_storey] 越界读" 的内存安全问题
         */
        inline unsigned int atIndex(Vec2_ xy, FLOAT_ R)
        {
            // 转换到网格坐标系（处理负坐标）
            xy += mExcursion;

            // 将网格坐标 clamp 到有效范围，防止后续位运算溢出
            // 超出网格范围的对象会被 Add/UpData 放入 GridExtrovert
            FLOAT_ maxCoord = (FLOAT_)(mGridDim - 1);
            if (xy.x < 0) xy.x = 0;
            if (xy.y < 0) xy.y = 0;
            if (xy.x > maxCoord) xy.x = maxCoord;
            if (xy.y > maxCoord) xy.y = maxCoord;

            glm::ivec2 pos = (xy);

            // 确定应该放在哪一层
            int _storey = Storey(R);

            // 对象比整张网格还大（/ 直径超过 mGridDim），直接判为网格外
            // （旧实现在此会读到 ExcursionVector[_storey] 越界内存）
            if (_storey > mStorey)
                return mGridSize;

            // 右移相当于除以2^_storey，即计算在当前层的网格坐标
            pos >>= _storey;

            // 计算这个网格单元格的边界
            glm::ivec2 Spos = pos << _storey;       // 网格起始点（世界坐标）
            glm::ivec2 Epos = (pos + 1) << _storey; // 网格结束点（世界坐标）

            // 判断对象是否跨网格边界
            if ((Spos.x > (xy.x - R)) || (Epos.x < (xy.x + R)) ||
                (Spos.y > (xy.y - R)) || (Epos.y < (xy.y + R)))
            {
                ++_storey; // 升级到上一层（更大的网格）
                pos >>= 1; // 在上一层中的位置也要重新计算

                // 升级后超过根层：比整个网格还大，判为网格外
                if (_storey > mStorey)
                    return mGridSize;
            }

#if CurerExcursionVector
#if Morton_define
            return ExcursionVector[_storey] + Morton2D((uint_fast16_t)pos.x, (uint_fast16_t)pos.y);
#else
            return ExcursionVector[_storey] + (pos.x * (1U << (mStorey - _storey))) + pos.y;
#endif
#else
#if Morton_define
            return Excursion(_storey) + Morton2D((uint_fast16_t)pos.x, (uint_fast16_t)pos.y);
#else
            return Excursion(_storey) + (pos.x * (1U << (mStorey - _storey))) + pos.y;
#endif
#endif
        }

        /**
         * @brief   分配/复用对象槽位
         * @param   atocr 物理对象指针
         * @return  槽位下标（在 mSlots/mNext/mNewIndex 中平行）
         * @details 优先复用 mFreeSlots 中的空槽位，否则追加新槽位。
         *          槽位内容在生命周期内不会被搬移，保证链表中下标稳定。
         */
        inline unsigned int NewSlot(PhysicsFormwork *atocr)
        {
            unsigned int id;
            if (mFreeSlots.empty())
            {
                id = (unsigned int)mSlots.size();
                mSlots.push_back(atocr);
                mNext.push_back(UINT_MAX);
                mNewIndex.push_back(UINT_MAX);
            }
            else
            {
                id = mFreeSlots.back();
                mFreeSlots.pop_back();
                mSlots[id] = atocr;
                mNext[id] = UINT_MAX;
            }
            return id;
        }

        /**
         * @brief   把槽位头插到指定格子
         * @param   cellIdx 格子索引
         * @param   slot 对象槽位
         * @details 头插 O(1)；若该格此前为空则记录为脏格，
         *          供下一次整树重建时定点清零（避免 memset 整表）。
         */
        inline void LinkCell(unsigned int cellIdx, unsigned int slot)
        {
            bool wasEmpty = (mCellHead[cellIdx] == UINT_MAX);
            mNext[slot] = mCellHead[cellIdx];
            mCellHead[cellIdx] = slot;
            if (wasEmpty)
                mDirtyCells.push_back(cellIdx);
        }

        /**
         * @brief   整树重建（供单线程 UpData 与多线程 UpDaraWorkeTaskEnd 共用）
         * @details 前提：mNewIndex[i] 已填好（每槽位的新格号）。
         *          1. 只清上帧写入过的脏格（成本 ∝ 对象数，而非格数）
         *          2. 清空并重建网格外对象列表
         *          3. 遍历所有槽位，把对象按新格号头插回网格
         *          全程无任何堆分配（稳态零分配）
         */
        void ApplyRebuild()
        {
            // 1. 清零上帧写入过的格子
            for (size_t i = 0; i < mDirtyCells.size(); ++i)
                mCellHead[mDirtyCells[i]] = UINT_MAX;
            mDirtyCells.clear();

            // 2. 网格外列表整体重建（成员关系以本次计算结果为准）
            GridExtrovert.clear();

            // 3. 重建所有链接
            for (size_t slot = 0; slot < mSlots.size(); ++slot)
            {
                PhysicsFormwork *Formwork = mSlots[slot];
                if (Formwork == nullptr)
                    continue; // 已删除的空槽位

                unsigned int newIndex = mNewIndex[slot];
                if (newIndex < mGridSize)
                {
                    LinkCell(newIndex, (unsigned int)slot);
                    Formwork->mGridIndex = newIndex;
                }
                else
                {
                    GridExtrovert.push_back(Formwork);
                    Formwork->mGridIndex = UINT_MAX;
                }
            }
        }

    public:
        /**
         * @brief   设置多线程 UpData 的线程数
         * @param   threadCount 线程数量（设为 0 使用单线程模式）
         * @details 方案 C 的并行路径采用了"槽位数组写不相交"的零拷贝设计，
         *          不再需要每线程独立暂存容器。
         */
        void SetThreadCount(unsigned int threadCount) {
            mThreadCount = threadCount;
        }

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
         *
         * 内存：（紧凑布局）
         *   总格数 = (4^(mStorey) - 1) / 3，每格仅 4 字节（方案 C）
         *   例如 Size=1024 时约 559 万格 × 4B ≈ 21.4MB（旧实现约 537MB）
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
            Size *= 4; // 实际空间是输入的4倍

            // 网格维度向上取整到 2 的幂（紧凑布局的关键前提）：
            // 紧凑偏移 + 莫顿编码要求"层 storey 每维格子数 = 2^(mStorey-storey)"，
            // 即 mGridDim 必须恰为 2^mStorey。旧实现 mGridDim=4*Size 可为任意值
            // （如 SetMapRange(10) -> mGridDim=40, mStorey=floor(log2 40)=5），
            // 此时层内坐标可超出 2^(mStorey-storey)，莫顿码会越过该层区域导致数组越界
            // （旧实现靠每层 4 倍容量兜底，实测仅勉强容错、语义错乱）。
            // 取整后世界覆盖 [-mGridDim/2, +mGridDim/2) 包含原范围 [-2*Size, 2*Size]，
            // 空间语义不缩水，仅索引映射更紧凑。
            unsigned int gridDim = 1;
            while (gridDim < Size)
                gridDim <<= 1;
            mGridDim = gridDim;
            mExcursion = {gridDim / 2.0f, gridDim / 2.0f}; // 偏移量，使负坐标变为正坐标

            // 计算网格层数和总大小（完整四叉树计数）
            // 每次循环：空间尺寸缩小一半，节点数增加4倍加1
            // 循环结束时，GridSize = (4^0 + 4^1 + ... + 4^mStorey) = (4^(mStorey+1) - 1) / 3
            unsigned int GridSize = 1;
            int storey = 0;
            unsigned int t = gridDim;
            while (t >>= 1)
            {
                GridSize <<= 2; // GridSize = GridSize * 4
                ++storey;       // 层数加1
                ++GridSize;     // 总节点数加上这一层的节点数
            }
            mStorey = storey;

            // 紧凑布局：总格数 = (4^mStorey - 1) / 3 = (完整计数 - 1) / 4
            // 旧布局为兼容任意非 2 的幂 mGridDim，每层预留了 4 倍容量；
            // mGridDim 取整为 2 的幂后此处即最紧凑且严格够用的尺寸。
            GridSize = (GridSize - 1) / 4;
            mGridSize = GridSize;

#if CurerExcursionVector
            // 预计算每层的偏移量，避免运行时重复计算
            ExcursionVector = new unsigned int[mStorey + 1];
            for (size_t i = 0; i <= (size_t)mStorey; i++)
            {
                ExcursionVector[i] = Excursion((int)i);
            }
#endif
            // 重建索引表：全部置空。已有对象仍在槽位中，下次 UpData 会重新归位
            // （旧实现 resize 会保留旧容量不释放内存；这里用 swap 确保释放）
            std::vector<unsigned int>().swap(mCellHead);
            mCellHead.resize(GridSize, UINT_MAX);
            mDirtyCells.clear();
        }

        /**
         * @brief   移动网格中心到新位置
         * @param   newCenter 新的网格中心（世界坐标）
         * @details 更新偏移量并重建所有对象索引
         *          网格范围大小保持不变，仅改变在世界空间中的位置
         */
        void MoveGridTo(Vec2_ newCenter)
        {
            FLOAT_ halfRange = mGridDim / 2.0f;
            mExcursion = Vec2_{halfRange, halfRange} - newCenter;
            UpData();
        }

        /**
         * @brief   更新网格偏移量（仅更新偏移，不触发索引重建）
         * @param   newCenter 新的网格中心（世界坐标）
         * @details 仅更新内部偏移量 mExcursion，不调用 UpData() 重建对象索引。
         *          与 MoveGridTo() 的区别：
         *          - MoveGridTo()：更新偏移量后立即调用 UpData() 重建所有对象索引，
         *            适合单次移动后立即需要正确查询的场景
         *          - UpdateGridOffset()：仅更新偏移量，不重建索引，
         *            适合连续移动中分步操作，在合适时机再统一调用 UpData()
         *          典型用法：多帧连续移动时，每帧先调用 UpdateGridOffset()，
         *          再在移动结束后统一调用一次 UpData()
         */
        void UpdateGridOffset(Vec2_ newCenter)
        {
            FLOAT_ halfRange = mGridDim / 2.0f;
            mExcursion = Vec2_{halfRange, halfRange} - newCenter;
        }

        /**
         * @brief   获取当前网格中心（世界坐标）
         * @return  网格中心在世界空间中的位置
         * @details 通过逆运算从偏移量反推网格中心位置：
         *          网格中心 = (halfRange, halfRange) - mExcursion
         */
        Vec2_ GetGridCenter() const
        {
            FLOAT_ halfRange = mGridDim / 2.0f;
            return Vec2_{halfRange, halfRange} - mExcursion;
        }

        /**
         * @brief   添加物理对象到网格
         * @param   atocr 要添加的物理对象指针
         * @details 根据对象位置和碰撞半径计算网格索引，将对象头插到对应网格
         *          如果索引超出范围，添加到网格外容器
         *
         * 添加策略：
         *   1. 计算对象的网格索引（atIndex）
         *   2. 如果索引在有效范围内，头插对应网格（O(1)）
         *   3. 否则（超大型对象），加入网格外容器
         */
        void Add(PhysicsFormwork *atocr)
        {
            unsigned int index = atIndex(atocr->PFGetPos(), atocr->PFGetCollisionR());
            unsigned int slot = NewSlot(atocr);
            if (index < mGridSize)
            {
                LinkCell(index, slot);
                atocr->mGridIndex = index;
            }
            else
            {
                GridExtrovert.push_back(atocr);
                atocr->mGridIndex = UINT_MAX;
            }
        }

        /**
         * @brief   从网格中移除物理对象
         * @param   atocr 要移除的物理对象指针
         * @details 先在格内链表中查找并摘下（O(k)，k = 该格对象数），
         *          不在网格中则从网格外容器中移除（swap-and-pop）。
         *
         * 移除策略：
         *   1. 利用 mGridIndex 定位到格子，沿链表找到对象前驱并摘下
         *   2. 槽位回收进 mFreeSlots 供后续 Add 复用
         *   3. 若不在网格内，从 GridExtrovert 用 swap-and-pop 移除
         */
        void Remove(PhysicsFormwork *atocr)
        {
            unsigned int index = atocr->mGridIndex;

            if (index != UINT_MAX && index < mGridSize)
            {
                // 沿格内链表查找并摘下
                unsigned int prev = UINT_MAX;
                for (unsigned int slot = mCellHead[index]; slot != UINT_MAX; prev = slot, slot = mNext[slot])
                {
                    if (mSlots[slot] == atocr)
                    {
                        if (prev == UINT_MAX)
                            mCellHead[index] = mNext[slot]; // 摘的是链头
                        else
                            mNext[prev] = mNext[slot];      // 摘的是中间/尾部节点
                        mSlots[slot] = nullptr;
                        mFreeSlots.push_back(slot);
                        return;
                    }
                }
                // 未在该格找到：说明 mGridIndex 已过期，回退到网格外容器查找
            }

            for (size_t i = 0; i < GridExtrovert.size(); ++i)
            {
                if (GridExtrovert[i] == atocr)
                {
                    GridExtrovert[i] = GridExtrovert.back();
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
            R *= 2; // 半径转直径，得到正方形的外接矩形
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
         *   2. 从最底层（第1层）开始向上遍历到根层（第mStorey层）
         *   3. 每上一层，坐标右移1位（相当于坐标除以2）
         *   4. 逐格沿槽位链表收集对象（链头 0? mNext: 遍历）
         *   5. 查询范围超出网格边界时，额外包含网格外容器
         */
        void Get(Vec2_ Spos, Vec2_ Epos, std::vector<PhysicsFormwork *> &Out)
        {
            Out.clear();

            // 坐标转换到网格坐标系
            Vec2_ S = Spos + mExcursion;
            Vec2_ E = Epos + mExcursion;

            // 使用真实网格维度作为边界（而非从 mExcursion 推导）
            unsigned int w = mGridDim - 1;
            bool Extrovert = false; // 标记查询范围是否超出网格边界

            // Clamp 到有效网格坐标范围，防止负数转 unsigned 回绕
            if (S.x < 0) { Extrovert = true; S.x = 0; }
            if (S.y < 0) { Extrovert = true; S.y = 0; }
            if (E.x < 0) { Extrovert = true; E.x = 0; }
            if (E.y < 0) { Extrovert = true; E.y = 0; }
            if (S.x > w) { Extrovert = true; S.x = (FLOAT_)w; }
            if (S.y > w) { Extrovert = true; S.y = (FLOAT_)w; }
            if (E.x > w) { Extrovert = true; E.x = (FLOAT_)w; }
            if (E.y > w) { Extrovert = true; E.y = (FLOAT_)w; }

            glm::uvec2 iS = glm::uvec2(S); // 转换为无符号整数网格坐标
            glm::uvec2 iE = glm::uvec2(E);

            // 如果查询范围超出边界，需要包含网格外容器中的所有对象
            if (Extrovert)
            {
                Out.insert(Out.end(), GridExtrovert.begin(), GridExtrovert.end());
            }

            // 从最底层开始向上遍历所有层
            for (int i = 1; i <= mStorey; ++i)
            {
                iS >>= 1; // 上一层中的网格坐标
                iE >>= 1;

#if CurerExcursionVector
                unsigned int off = ExcursionVector[i];
#else
                unsigned int off = Excursion(i);
#endif

                // 遍历所有在查询范围内的网格单元格，逐格沿槽位链表收集对象
                for (size_t x = iS.x; x <= iE.x; ++x)
                {
                    for (size_t y = iS.y; y <= iE.y; ++y)
                    {
                        unsigned int cellIdx = off + Morton2D((uint_fast16_t)x, (uint_fast16_t)y);
                        // 防御性边界检查：mGridDim 取整后公式恒在界内，
                        // 此检查仅为莫顿码截断/异常输入的兜底，几乎不产生开销
                        if (cellIdx >= mGridSize)
                            continue;
                        for (unsigned int slot = mCellHead[cellIdx]; slot != UINT_MAX; slot = mNext[slot])
                        {
                            Out.push_back(mSlots[slot]);
                        }
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

            // Clamp 到有效网格坐标范围
            FLOAT_ maxCoord = (FLOAT_)(mGridDim - 1);
            if (atocr_pos.x < 0) atocr_pos.x = 0;
            if (atocr_pos.y < 0) atocr_pos.y = 0;
            if (atocr_pos.x > maxCoord) atocr_pos.x = maxCoord;
            if (atocr_pos.y > maxCoord) atocr_pos.y = maxCoord;

            glm::ivec2 pos = (atocr_pos);
            int _storey = Storey(R);
            if (_storey > mStorey)
                _storey = mStorey; // 超大对象取根层视野
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
         * @brief   更新所有对象的网格位置（整树重建）
         * @details 遍历所有对象槽位，重新计算网格索引并重建整棵网格树。
         *          与旧的"增量搬移"不同，这里直接重建链接：
         *          由于只清"脏格"（上帧写入过的格），复杂度为 O(对象数)，
         *          代价远低于旧实现中逐格的 vector 增删/分配。
         *
         * 注意：这是一个O(n)复杂度的操作，应该尽量减少调用频率
         *       典型用法是每帧调用一次，而不是每帧为每个对象调用
         */
        void UpData()
        {
            // 计算每个对象的新格号（单线程版；多线程请用 UpDaraWorkeTask/End）
            for (size_t slot = 0; slot < mSlots.size(); ++slot)
            {
                PhysicsFormwork *Formwork = mSlots[slot];
                if (Formwork == nullptr)
                    continue;
                mNewIndex[slot] = atIndex(Formwork->PFGetPos(), Formwork->PFGetCollisionR());
            }
            ApplyRebuild();
        }

        /**
         * @brief   多线程 UpData 工作函数
         * @param   ThreadSize 总线程数
         * @param   ThreadID 当前线程 ID（从 0 开始）
         * @details 将 mSlots 按块均匀分配给各线程并行处理。
         *          每个线程遍历自己负责的槽位区间，计算新的网格索引，
         *          直接写入 mNewIndex[slot]（各线程写不相交区间，无数据竞争）。
         *          此函数不修改网格结构本身，真正的重建在 UpDaraWorkeTaskEnd 中完成。
         */
        void UpDaraWorkeTask(unsigned int ThreadSize, unsigned int ThreadID)
        {
            if (ThreadSize == 0)
                return;

            unsigned int Count = (unsigned int)mSlots.size();
            unsigned int BlockSize = Count / ThreadSize;
            unsigned int BlockRemain = Count % ThreadSize;

            unsigned int Start;
            unsigned int End;
            if (ThreadID < BlockRemain)
            {
                Start = ThreadID * (BlockSize + 1);
                End = Start + BlockSize + 1;
            }
            else
            {
                Start = BlockRemain * (BlockSize + 1) + (ThreadID - BlockRemain) * BlockSize;
                End = Start + BlockSize;
            }

            for (unsigned int i = Start; i < End; ++i)
            {
                PhysicsFormwork *Formwork = mSlots[i];
                if (Formwork == nullptr)
                    continue;
                mNewIndex[i] = atIndex(Formwork->PFGetPos(), Formwork->PFGetCollisionR());
            }
        }

        /**
         * @brief   多线程 UpData 收尾函数（仅主线程调用）
         * @details 在所有工作线程完成 UpDaraWorkeTask 后，由主线程调用。
         *          根据各线程写入的 mNewIndex 重建整棵网格树（O(对象数)，零分配）。
         */
        void UpDaraWorkeTaskEnd()
        {
            ApplyRebuild();
        }
    };

}
