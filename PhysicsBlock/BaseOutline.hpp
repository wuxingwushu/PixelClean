#pragma once
#include "BaseGrid.hpp"

namespace PhysicsBlock
{

    /**
     * @brief 外轮廓类
     * @details 继承自 BaseGrid，用于管理和计算网格的外轮廓点
     * 提供完整版本和轻量版本的轮廓计算
     */
    class BaseOutline : public BaseGrid
    {
    public:
        /**
         * @brief 轮廓点集合
         * @details 存储轮廓点的坐标
         */
        Vec2_ *OutlineSet;       
        
        /**
         * @brief 轮廓点对应摩擦力集合
         * @details 存储每个轮廓点对应的摩擦因数
         */
        FLOAT_ *FrictionSet;      
        
        /**
         * @brief 轮廓点数量
         * @details 当前轮廓点集合中的有效点数
         */
        unsigned int OutlineSize; 

#if PhysicsBlock_Serialization
        /**
         * @brief 从 JSON 数据构造外轮廓对象
         * @param data JSON 数据
         * @details 从 JSON 数据中恢复轮廓信息
         */
        BaseOutline(const nlohmann::json_abi_v3_12_0::basic_json<> &data) : BaseGrid(data)
        {
            int size = width * height * 3;
            if (size < 4)
            {
                size = 4;
            }
            FrictionSet = new FLOAT_[size];
            OutlineSet = new Vec2_[size];
            JsonContrarySerialization(data);
        }
#endif

        /**
         * @brief 构造函数
         * @param Width 网格宽度
         * @param Height 网格高度
         * @details 创建一个指定大小的外轮廓对象，初始化轮廓点集合
         */
        BaseOutline(const unsigned int Width, const unsigned int Height);
        
        /**
         * @brief 析构函数
         * @details 释放轮廓点集合和摩擦力集合的内存
         */
        ~BaseOutline();

        /**
         * @brief 更新外轮廓点集合（完整版）
         * @param CentreMass_ 质心位置
         * @details 遍历所有网格，计算完整的外轮廓点，并将轮廓点坐标转换为相对于质心的坐标
         */
        void UpdateOutline(Vec2_ CentreMass_);

        /**
         * @brief 更新外轮廓点集合（轻量版）
         * @param CentreMass_ 质心位置
         * @details 遍历所有网格，计算轻量版的外轮廓点（减少点数），并将轮廓点坐标转换为相对于质心的坐标
         */
        void UpdateLightweightOutline(Vec2_ CentreMass_);

    private:
        /**
         * @brief 外轮廓处理单元（完整版）
         * @param x 格子坐标x
         * @param y 格子坐标y
         * @details 检查指定格子的四个角是否为轮廓点，如果是则添加到轮廓集合中
         */
        void OutlineUnit(int x, int y);

        /**
         * @brief 外轮廓处理单元（轻量版）
         * @param x 格子坐标x
         * @param y 格子坐标y
         * @details 检查指定格子的四个角是否为轮廓点（更严格的条件），如果是则添加到轮廓集合中
         */
        void LightweightOutlineUnit(int x, int y);
    };

}