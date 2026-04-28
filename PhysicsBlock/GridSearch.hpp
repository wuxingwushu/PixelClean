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
         */
        std::vector<std::vector<PhysicsFormwork *>> Grid;
        
        /**
         * @brief   网格外对象容器
         * @details 存储超出网格范围的物理对象
         */
        std::vector<PhysicsFormwork *> GridExtrovert;
        
        /**
         * @brief   四叉网格层数
         * @details 表示网格的最大细分层数
         */
        int mStorey = 0;
        
        /**
         * @brief   相对世界坐标系的位置偏移量
         * @details 将世界坐标转换为网格坐标的偏移量，使负坐标也能正确映射
         */
        Vec2_ mExcursion{0};
        
#if CurerExcursionVector
        /**
         * @brief   预计算的每层偏移量数组
         * @details 存储每层网格在一维数组中的起始索引偏移
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
        ~GridSearch() { if(ExcursionVector != nullptr) { delete ExcursionVector; } }

    private:
#if Morton_define
        /**
         * @brief   莫顿码编码（Z-order曲线）
         * @param   x x 坐标
         * @param   y y 坐标
         * @return  返回一维索引值
         * @details 使用位交错算法，将二维坐标映射到一维索引
         *          让二维空间中相邻的点在一维数组中也尽可能相邻
         */
        inline uint_fast32_t Morton2D(uint_fast16_t x, uint_fast16_t y)
        {
            uint_fast64_t m = x | (uint_fast64_t(y) << 32);
            m = (m | (m << 8)) & 0x00FF00FF00FF00FF;
            m = (m | (m << 4)) & 0x0F0F0F0F0F0F0F0F;
            m = (m | (m << 2)) & 0x3333333333333333;
            m = (m | (m << 1)) & 0x5555555555555555;
            m = m | (m >> 31);
            m = m & 0x00000000FFFFFFFF;
            return uint_fast32_t(m);
        }
#endif

        /**
         * @brief   计算四叉网格某层的起始偏移量
         * @param   storey 网格层数（从0开始）
         * @return  返回该层在一维数组中的起始索引
         * @details 使用公式 (4^(k+1) - 1) / 3 计算偏移量
         *          其中 k = mStorey - storey
         */
        inline unsigned int Excursion(int storey)
        {
            int k = mStorey - storey;
            if (k < 0) return 0;
            unsigned int n = k + 1;
            unsigned int result = (1U << (2 * n)) - 1;
            result /= 3;
            return result;
        }

        /**
         * @brief   计算对象应该放入的网格细分层
         * @param   R 对象的碰撞半径
         * @return  返回网格层数
         * @details 根据对象大小确定合适的网格层级
         *          大对象放入上层粗网格，小对象放入下层细网格
         */
        inline int Storey(FLOAT_ R)
        {
            // 搜索四叉网格范围大小
            unsigned int r = R * 2 + 0.99;
            // 处理0的情况
            if (r == 0) return 1;
            // 找到最高有效位的位数
            unsigned long index;
            #if defined(_MSC_VER)
                _BitScanReverse(&index, r);
            #else
                index = 31 - __builtin_clz(r);
            #endif
            return index + 1;
        }

        /**
         * @brief   世界位置转换为网格坐标
         * @param   pos 世界坐标位置
         * @return  返回网格坐标（整数）
         * @details 通过加上偏移量将负坐标转换为正坐标
         */
        inline glm::ivec2 Pos_ToInt(Vec2_ pos)
        {
            return (pos + mExcursion);
        }

        /**
         * @brief   获取指定索引的网格
         * @param   i 网格索引
         * @return  返回网格内的对象列表引用
         */
        inline std::vector<PhysicsFormwork *> &at(unsigned int i) { return Grid[i]; }

#if Morton_define
        /**
         * @brief   获取指定位置和层级的网格（莫顿编码版本）
         * @param   x x 坐标
         * @param   y y 坐标
         * @param   _Excursion 该层的起始偏移量
         * @param   _storey 网格层数
         * @return  返回网格内的对象列表引用
         */
        inline std::vector<PhysicsFormwork *> &at(unsigned int x, unsigned int y, unsigned int _Excursion, int _storey) 
        { 
            return at(_Excursion + Morton2D(x, y)); 
        }
#else
        /**
         * @brief   获取指定位置和层级的网格（线性版本）
         * @param   x x 坐标
         * @param   y y 坐标
         * @param   _Excursion 该层的起始偏移量
         * @param   _storey 网格层数
         * @return  返回网格内的对象列表引用
         */
        inline std::vector<PhysicsFormwork *> &at(unsigned int x, unsigned int y, unsigned int _Excursion, int _storey) 
        { 
            return at(_Excursion + (x * (1 << (mStorey + 1 - _storey)) + y)); 
        }
#endif

        /**
         * @brief   计算对象所在的网格索引
         * @param   xy 对象位置
         * @param   R 对象碰撞半径
         * @return  返回网格索引
         * @details 根据对象位置和大小计算应该放入的网格
         *          如果对象跨网格边界，则放入上层更大的网格
         */
        inline unsigned int atIndex(Vec2_ xy, FLOAT_ R)
        {
            xy += mExcursion;
            glm::ivec2 pos = (xy);
            int _storey = Storey(R);
            pos >>= _storey;

            // 判断是否在网格边缘，是就放在上层较大的网格内
            glm::ivec2 Spos = pos << _storey;
            glm::ivec2 Epos = (pos + 1) << _storey;
            
            // 判断对象是否跨网格边界
            if ((Spos.x > (xy.x - R)) || (Epos.x < (xy.x + R)) || 
                (Spos.y > (xy.y - R)) || (Epos.y < (xy.y + R)))
            {
                ++_storey;
                pos >>= 1;
            }

#if CurerExcursionVector
#if Morton_define
            return ExcursionVector[_storey] + Morton2D(pos.x, pos.y);
#else
            return ExcursionVector[_storey] + (pos.x * (1 << (mStorey + 1 - _storey))) + pos.y;
#endif
#else
#if Morton_define
            return Excursion(_storey) + Morton2D(pos.x, pos.y);
#else
            return Excursion(_storey) + (pos.x * (1 << (mStorey + 1 - _storey))) + pos.y;
#endif
#endif
        }

        /**
         * @brief   获取对象所在的网格
         * @param   xy 对象位置
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
         * @param   Size 网格的单边大小
         * @details 初始化网格容器，计算网格层数和偏移量
         *          网格实际大小为 Size * 4，支持负坐标区域
         */
        void SetMapRange(unsigned int Size)
        {
#if CurerExcursionVector
            if (ExcursionVector != nullptr)
            {
                delete ExcursionVector;
                ExcursionVector = nullptr;
            }
#endif
            Size *= 4;
            mExcursion = {Size / 2, Size / 2};
            unsigned int GridSize = 1;
            
            // 计算网格层数和总大小
            while (Size >>= 1)
            {
                GridSize <<= 2;
                ++mStorey;
                ++GridSize;
            }

#if CurerExcursionVector
            // 预计算每层的偏移量
            ExcursionVector = new unsigned int[mStorey];
            for (size_t i = 0; i < mStorey; i++)
            {
                ExcursionVector[i] = Excursion(i);
            }
#endif
            
            Grid.clear();
            Grid.resize(GridSize);
        }

        /**
         * @brief   添加物理对象到网格
         * @param   atocr 要添加的物理对象指针
         * @details 根据对象位置和碰撞半径计算网格索引，将对象添加到对应网格
         *          如果索引超出范围，添加到网格外容器
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
         */
        void Remove(PhysicsFormwork *atocr)
        {
            unsigned int index = atIndex(atocr->PFGetPos(), atocr->PFGetCollisionR());
            
            // 在网格中查找并移除
            for (size_t i = 0; i < Grid[index].size(); ++i)
            {
                if (Grid[index][i] == atocr)
                {
                    Grid[index][i] = Grid[index].back();
                    Grid[index].pop_back();
                    return;
                }
            }
            
            // 在网格外容器中查找并移除
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
         * @brief   获取指定范围内的所有对象（圆形范围）
         * @param   pos 范围中心位置
         * @param   R 范围半径
         * @return  返回范围内的物理对象列表
         * @details 将圆形范围转换为正方形范围，然后调用 Get(Spos, Epos)
         */
        std::vector<PhysicsFormwork *> Get(Vec2_ pos, FLOAT_ R)
        {
            R *= 2; // 范围稍微加大，确保覆盖
            return Get(pos - R, pos + R);
        }

        /**
         * @brief   获取指定矩形范围内的所有对象
         * @param   Spos 矩形起始位置（最小坐标）
         * @param   Epos 矩形结束位置（最大坐标）
         * @return  返回范围内的物理对象列表
         * @details 遍历所有层级的网格，收集范围内的对象
         *          如果范围超出网格边界，也会包含网格外的对象
         */
        std::vector<PhysicsFormwork *> Get(Vec2_ Spos, Vec2_ Epos)
        {
            std::vector<PhysicsFormwork *> V;
            Vec2_ S = Spos + mExcursion;
            Vec2_ E = Epos + mExcursion;
            glm::uvec2 iS = S;
            glm::uvec2 iE = E;

            unsigned int w = mExcursion.x + mExcursion.x - 1;
            bool Extrovert = false;

            // 检查是否超出网格边界
            if (iS.x > w) { Extrovert = true; iS.x = w; }
            if (iS.y > w) { Extrovert = true; iS.y = w; }
            if (iE.x > w) { Extrovert = true; iE.x = w; }
            if (iE.y > w) { Extrovert = true; iE.y = w; }

            // 如果超出边界，包含网格外的对象
            if (Extrovert)
            {
                for (auto i : GridExtrovert)
                {
                    V.push_back(i);
                }
            }

            unsigned int P = Grid.size();

            // 遍历所有层级
            for (int i = 1; i <= mStorey; ++i)
            {
                iS >>= 1;
                iE >>= 1;
                P >>= 2;
                
                // 遍历该层的所有网格
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

        /**
         * @brief   获取物体所在的所有细分网格范围
         * @param   atocr 物理对象指针
         * @return  返回网格范围列表（每对为一个矩形的对角点）
         * @details 返回对象所在的所有层级网格的边界坐标
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

            // 判断是否跨网格边界
            glm::ivec2 Spos = pos << _storey;
            glm::ivec2 Epos = (pos + 1) << _storey;
            if ((Spos.x > (atocr_pos.x - R)) || (Epos.x < (atocr_pos.x + R)) || 
                (Spos.y > (atocr_pos.y - R)) || (Epos.y < (atocr_pos.y + R)))
            {
                ++_storey;
                pos >>= 1;
                Spos = pos << _storey;
                Epos = (pos + 1) << _storey;
            }
            
            Vision.push_back(Vec2_(Spos) - mExcursion);
            Vision.push_back(Vec2_(Epos) - mExcursion);
            
            // 添加所有上层网格范围
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
         */
        std::vector<PhysicsFormwork *> &GetGridExtrovert()
        {
            return GridExtrovert;
        }

        /**
         * @brief   更新所有对象的网格位置
         * @details 遍历所有网格和网格外对象，重新计算它们的网格索引
         *          如果索引发生变化，将对象移动到新的网格
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
                        Grid[i][a] = Grid[i].back();
                        Grid[i].pop_back();
                        
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
            
            // 更新网格外的对象
            for (size_t i = 0; i < GridExtrovert.size(); ++i)
            {
                PhysicsFormwork *Formwork = GridExtrovert[i];
                unsigned int index = atIndex(Formwork->PFGetPos(), Formwork->PFGetCollisionR());
                
                if (index < Grid.size())
                {
                    GridExtrovert[i] = GridExtrovert.back();
                    GridExtrovert.pop_back();
                    Grid[index].push_back(Formwork);
                }
            }
        }
    };

}
