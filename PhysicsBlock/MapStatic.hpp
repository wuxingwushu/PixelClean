#pragma once
#include "MapFormwork.hpp"
#include "BaseGrid.hpp"
#include "BaseCalculate.hpp"
#include <vector>

namespace PhysicsBlock
{

    /**
     * @brief 地图轮廓结构体
     * @details 用于存储地图轮廓点的信息
     */
    struct MapOutline
    {
        Vec2_ pos;  // 点位置
        Vec2_ face; // 朝向（法线方向）
        FLOAT_ F;   // 摩擦力
    };

    /**
     * @brief 静态地图类
     * @details 继承自 BaseGrid（处理网格数据）和 MapFormwork（地图框架接口）
     * 表示一个静态的物理地图，包含障碍物信息和碰撞检测功能
     */
    class MapStatic : public BaseGrid, public MapFormwork
    {
#if PhysicsBlock_Serialization
    public:
        /**
         * @brief 将地图状态序列化为 JSON
         * @param data 输出的 JSON 数据
         */
        virtual void JsonSerialization(nlohmann::json_abi_v3_12_0::basic_json<> &data)
        {
            BaseGrid::JsonSerialization(data);
            SerializationVec2(data, centrality);
        }
        
        /**
         * @brief 从 JSON 数据反序列化地图状态
         * @param data JSON 数据
         */
        virtual void JsonContrarySerialization(const nlohmann::json_abi_v3_12_0::basic_json<> &data)
        {
            BaseGrid::JsonContrarySerialization(data);
            ContrarySerializationVec2(data, centrality);
        }
        
        /**
         * @brief 从 JSON 数据构造地图
         * @param data JSON 数据
         */
        MapStatic(const nlohmann::json_abi_v3_12_0::basic_json<> &data) : BaseGrid(data) { ContrarySerializationVec2(data, centrality); };
#endif
    public:
        /**
         * @brief 中心位置
         * @details 地图的中心位置对应世界坐标系的原点，用于将地图坐标转换为世界坐标
         * @note 地图的中心位置必须在地图范围内，否则设置无效
         * @note 地图的中心位置用于坐标转换
         */
        Vec2_ centrality{0, 0}; 
    
    public:
        /**
         * @brief 构造函数
         * @param Width 地图宽度（单位：格子）
         * @param Height 地图高度（单位：格子）
         */
        MapStatic(const unsigned int Width, const unsigned int Height) : BaseGrid(Width, Height) {};
        
        /**
         * @brief 析构函数
         */
        ~MapStatic() {};

        /**
         * @brief 设置地图的中心位置
         * @param Centrality 中心位置
         * @note 必须在地图范围内，否则设置无效
         */
        void SetCentrality(Vec2_ Centrality);

        /**
         * @brief 获取轻量级轮廓
         * @param x_ 起始X坐标
         * @param y_ 起始Y坐标
         * @param w_ 结束X坐标
         * @param h_ 结束Y坐标
         * @return 轮廓点向量
         * @note 轻量级轮廓，只返回部分关键点
         */
        std::vector<MapOutline> GetLightweightOutline(int x_, int y_, int w_, int h_);

        /**
         * @brief 获取完整轮廓
         * @param x_ 起始X坐标
         * @param y_ 起始Y坐标
         * @param w_ 结束X坐标
         * @param h_ 结束Y坐标
         * @return 轮廓点向量
         * @note 返回所有轮廓点
         */
        std::vector<MapOutline> GetOutline(int x_, int y_, int w_, int h_);

        /*=========MapFormwork=========*/

        /**
         * @brief 获取对象类型
         * @return 物理对象类型，返回 PhysicsObjectEnum::_MapStatic
         * @note 实现了 MapFormwork 中的虚函数
         */
        virtual PhysicsObjectEnum FMGetType() { return PhysicsObjectEnum::_MapStatic; }

        /**
         * @brief 获取地图模拟场大小
         * @return 地图大小（宽度和高度）
         * @note 单位：格子
         * @note 实现了 MapFormwork 中的虚函数
         */
        virtual glm::uvec2 FMGetMapSize() { return glm::uvec2{width, height}; }

        /**
         * @brief 获取网格是否有障碍物（网格坐标）
         * @param start 网格位置
         * @return 是否碰撞（true: 碰撞，false: 无碰撞）
         * @note 实现了 MapFormwork 中的虚函数
         */
        virtual bool FMGetCollide(glm::ivec2 start)
        {
            if ((start.x >= width) || (start.y >= height))
            {
                return false;
            }
            return at(start).Collision;
        }

        /**
         * @brief 获取是否有障碍物（世界坐标）
         * @param start 世界位置
         * @return 是否碰撞（true: 碰撞，false: 无碰撞）
         * @note 将世界坐标转换为网格坐标后进行检测
         * @note 实现了 MapFormwork 中的虚函数
         */
        virtual bool FMGetCollide(Vec2_ start) { return FMGetCollide(ToInt(start + centrality)); }

        /**
         * @brief 获取网格块（不安全）
         * @param start 网格位置
         * @return 网格块的引用
         * @warning 范围不安全，超出地图范围会报错
         * @note 实现了 MapFormwork 中的虚函数
         */
        virtual GridBlock &FMGetGridBlock(glm::ivec2 start)
        {
            return at(start);
        }

        /**
         * @brief 地图线段碰撞检测（整数坐标）
         * @param start 起始位置（网格坐标）
         * @param end 结束位置（网格坐标）
         * @return 第一个碰撞的位置（网格坐标）
         * @warning 位置必须在地图范围内
         * @note 实现了 MapFormwork 中的虚函数
         */
        virtual CollisionInfoI FMBresenhamDetection(glm::ivec2 start, glm::ivec2 end);

        /**
         * @brief 地图线段碰撞检测（浮点数坐标）
         * @param start 起始位置（世界坐标）
         * @param end 结束位置（世界坐标）
         * @return 第一个碰撞的位置（准确位置）
         * @note 会将线段裁剪到地图范围内再进行检测
         * @note 实现了 MapFormwork 中的虚函数
         */
        virtual CollisionInfoD FMBresenhamDetection(Vec2_ start, Vec2_ end);
    };

}