#pragma once
#include "BaseStruct.hpp"
#include <vector>

namespace PhysicsBlock
{

    /**
     * @brief 地图轮廓结构体
     * @details 用于存储地图轮廓点的信息 */
    struct MapOutline
    {
        Vec2_ pos;
        Vec2_ face;
        FLOAT_ F;
    };

    /**
     * @brief 地图模板
     * @note 地图模板提示每个地图都应该有的函数功能 */
    class MapFormwork
    {
    public:
        PhysicsObjectEnum type = PhysicsObjectEnum::Null;

        MapFormwork() {};
        virtual ~MapFormwork() {};

        /**
         * @brief 获取对象类型
         * @return PhysicsObjectEnum 类型 */
        PhysicsObjectEnum FMGetType() { return type; }

        /**
         * @brief 获取距离场
         * @return 最小碰撞半径 */
        virtual unsigned char FMGetDistanceField() { return 0; }

        /**
         * @brief 获取地图模拟场大小
         * @return 地图大小
         * @note 单位：格子 */
        virtual glm::uvec2 FMGetMapSize() { return glm::uvec2{0}; }

        /**
         * @brief 获取地图中心位置
         * @return 中心位置（用于坐标转换） */
        virtual Vec2_ FMGetCentrality() { return {0, 0}; }

        /**
         * @brief 获取网格是否有障碍物
         * @param start 网格位置
         * @return 是否碰撞（true: 碰撞） */
        virtual bool FMGetCollide(glm::ivec2 start) { return false; }

        /**
         * @brief 获取是否有障碍物
         * @param start 世界位置
         * @return 是否碰撞（true: 碰撞） */
        virtual bool FMGetCollide(Vec2_ start) { return false; }

        /**
         * @brief 获取网格(不安全)
         * @param start 网格位置
         * @warning 范围不安全，超出会报错 */
        virtual GridBlock& FMGetGridBlock(glm::ivec2 start) = 0;

        /**
         * @brief 获取轻量级轮廓
         * @param x_ 起始X坐标（世界坐标）
         * @param y_ 起始Y坐标（世界坐标）
         * @param w_ 结束X坐标（世界坐标）
         * @param h_ 结束Y坐标（世界坐标）
         * @return 轮廓点向量 */
        virtual std::vector<MapOutline> FMGetLightweightOutline(int x_, int y_, int w_, int h_) { return {}; }

        /**
         * @brief 获取完整轮廓
         * @param x_ 起始X坐标（世界坐标）
         * @param y_ 起始Y坐标（世界坐标）
         * @param w_ 结束X坐标（世界坐标）
         * @param h_ 结束Y坐标（世界坐标）
         * @return 轮廓点向量 */
        virtual std::vector<MapOutline> FMGetOutline(int x_, int y_, int w_, int h_) { return {}; }

        /**
         * @brief 地图 线段(Bresenham) 检测
         * @param start 起始位置
         * @param end 结束位置
         * @return 第一个碰撞的位置(网格位置) */
        virtual CollisionInfoI FMBresenhamDetection(glm::ivec2 start, glm::ivec2 end) { return CollisionInfoI{}; }

        /**
         * @brief 地图 线段(Bresenham) 检测
         * @param start 起始位置
         * @param end 结束位置
         * @return 第一个碰撞的位置(准确位置) */
        virtual CollisionInfoD FMBresenhamDetection(Vec2_ start, Vec2_ end) { return CollisionInfoD{}; }

        virtual CollisionInfoI FMSafeBresenhamDetection(glm::ivec2 start, glm::ivec2 end) { return CollisionInfoI{}; }
    };

}