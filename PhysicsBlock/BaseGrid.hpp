#pragma once
#include "BaseStruct.hpp"
#include "BaseSerialization.hpp"

namespace PhysicsBlock
{

    /**
     * @brief 基础网格类
     * @details 提供二维网格数据的管理功能，包括网格的创建、访问和碰撞检测
     * 实现了序列化和反序列化功能
     */
    class BaseGrid SerializationInherit_
    {
#if PhysicsBlock_Serialization
    public:
        /**
         * @brief 序列化虚函数
         */
        SerializationVirtualFunction;

        /**
         * @brief 从 JSON 数据构造基础网格
         * @param data JSON 数据
         * @details 从 JSON 数据中恢复网格的宽度、高度、网格数据等信息
         */
        BaseGrid(const nlohmann::json_abi_v3_12_0::basic_json<> &data) : width(data["width"]), height(data["height"])
        {
            NewBool = data["NewBool"];
            if (NewBool) {
                Grid = new GridBlock[width * height];
            }
            JsonContrarySerialization(data);
        }
#endif
    public:
        /**
         * @brief 网格宽度
         */
        const unsigned int width;

        /**
         * @brief 网格高度
         */
        const unsigned int height;

        /**
         * @brief 网格是否在内部生成
         * @details 如果为 true，表示网格由 BaseGrid 内部创建，需要在析构时释放
         * 如果为 false，表示网格由外部提供，不需要释放
         */
        bool NewBool = false;

        /**
         * @brief 网格数据指针
         * @details 指向 GridBlock 数组，数组大小为 width * height
         */
        GridBlock *Grid;

        /**
         * @brief 获取网格块（不安全）
         * @param x 网格X坐标
         * @param y 网格Y坐标
         * @return 网格块的引用
         * @warning 坐标范围不安全，超出范围会导致未定义行为
         */
        virtual inline GridBlock &at(int x, int y) { return at(x * height + y); }

        /**
         * @brief 获取网格块（不安全）
         * @param pos 网格坐标
         * @return 网格块的引用
         * @warning 坐标范围不安全，超出范围会导致未定义行为
         */
        virtual inline GridBlock &at(glm::ivec2 pos) { return at(pos.x * height + pos.y); }

        /**
         * @brief 获取网格块（不安全）
         * @param i 数组索引
         * @return 网格块的引用
         * @warning 索引范围不安全，超出范围会导致未定义行为
         */
        virtual inline GridBlock &at(unsigned int i) { return Grid[i]; }

        /**
         * @brief 获取是否有碰撞（安全）
         * @param x 网格X坐标
         * @param y 网格Y坐标
         * @return 是否碰撞
         * @retval true 有障碍物
         * @retval false 没障碍物
         * @note 坐标范围安全，范围外返回 false
         */
        virtual inline bool GetCollision(unsigned int x, unsigned int y)
        {
            if ((x >= width) || (y >= height))
            {
                return false;
            }
            return at(x, y).Collision;
        }

        /**
         * @brief 获取是否有碰撞（安全）
         * @param Pos 网格坐标
         * @return 是否碰撞
         * @retval true 有障碍物
         * @retval false 没障碍物
         * @note 坐标范围安全，范围外返回 false
         */
        virtual inline bool GetCollision(glm::uvec2 Pos)
        {
            return GetCollision(Pos.x, Pos.y);
        }

    public:
        /**
         * @brief 构造函数（内部创建网格）
         * @param Width 网格宽度
         * @param Height 网格高度
         * @details 创建一个指定大小的网格，并在内部分配内存
         */
        BaseGrid(const unsigned int Width, const unsigned int Height);

        /**
         * @brief 构造函数（外部提供网格）
         * @param Width 网格宽度
         * @param Height 网格高度
         * @param Gridptr 外部提供的网格数据指针
         * @warning 外部提供的网格需要使用者自行管理生命周期
         */
        BaseGrid(const unsigned int Width, const unsigned int Height, GridBlock *Gridptr);

        /**
         * @brief 析构函数
         * @details 如果网格是内部创建的（NewBool 为 true），则释放网格内存
         */
        ~BaseGrid();

        /**
         * @brief 线段碰撞检测（整数坐标）
         * @param start 起始位置（网格坐标）
         * @param end 结束位置（网格坐标）
         * @return 碰撞结果信息（包含碰撞位置和摩擦因数）
         * @details 使用 Bresenham 算法检测线段经过的网格是否有碰撞
         */
        CollisionInfoI BresenhamDetection(glm::ivec2 start, glm::ivec2 end);

        /**
         * @brief 线段碰撞检测（浮点数坐标）
         * @param start 起始位置（世界坐标）
         * @param end 结束位置（世界坐标）
         * @return 碰撞结果信息（包含精确碰撞位置和方向）
         * @details 使用 Bresenham 算法检测线段经过的网格是否有碰撞，并计算精确的碰撞位置
         */
        CollisionInfoD BresenhamDetection(Vec2_ start, Vec2_ end);
    };

}