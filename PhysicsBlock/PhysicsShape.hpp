#pragma once
#include "PhysicsAngle.hpp"
#include "BaseOutline.hpp"
#include "BaseDefine.h"
#include "PhysicsFormwork.hpp"

namespace PhysicsBlock
{

    /**
     * @brief 物理形状类
     * @details 表示一个具有质量、位置、旋转和碰撞检测能力的物理形状
     * 继承自 PhysicsAngle（处理物理角度和位置）和 BaseOutline（处理网格和轮廓）
     * 
     * @note 包含以下主要属性：
     * - 质心 (CentreMass)：形状的质量中心
     * - 几何中心 (CentreShape)：形状的几何中心
     * - 继承自 PhysicsAngle 的属性：位置、速度、角速度、质量等
     * - 继承自 BaseOutline 的属性：网格、轮廓等
     */
    class PhysicsShape : public PhysicsAngle, public BaseOutline
    {
#if PhysicsBlock_Serialization
    public:
        /**
         * @brief 序列化虚函数
         */
        SerializationVirtualFunction;
        
        /**
         * @brief 从 JSON 数据构造物理形状
         * @param data JSON 数据
         * @details 通过反序列化从 JSON 数据中恢复物理形状的状态
         */
        PhysicsShape(const nlohmann::json_abi_v3_12_0::basic_json<> &data) : PhysicsAngle(data), BaseOutline(data)
        {
            UpdateAll();
            ContrarySerializationVec2(data, pos);
            ContrarySerializationVec2(data, OldPos);
        };
#endif
    public:
        /**
         * @brief 质心
         * @details 形状的质量中心，用于物理计算
         */
        Vec2_ CentreMass{0};  
        
        /**
         * @brief 几何中心
         * @details 形状的几何中心，用于碰撞检测和渲染
         */
        Vec2_ CentreShape{0}; 
    
    public:
        /**
         * @brief 构造函数
         * @param Pos 形状的初始位置
         * @param Size 网格大小
         * @details 初始化物理形状的位置和网格大小
         */
        PhysicsShape(Vec2_ Pos, glm::ivec2 Size);
        
        /**
         * @brief 析构函数
         */
        ~PhysicsShape();

        /**
         * @brief 获取指定索引的网格块
         * @param i 网格索引
         * @return 网格块的引用
         */
        GridBlock &Gridat(unsigned int i) { return at(i); }

        /**
         * @brief 检测点是否点击到网格
         * @param pos 点的世界位置
         * @return 碰撞结果信息，包含是否碰撞和碰撞的网格位置
         */
        CollisionInfoI DropCollision(Vec2_ pos);

        /**
         * @brief 更新形状的物理信息
         * @details 更新以下信息：
         * - 总重量（质量）
         * - 几何中心
         * - 质心
         * - 转动惯量
         * @note 当质量为 FLOAT_MAX 时，形状被视为不可移动
         */
        void UpdateInfo();

        /**
         * @brief 计算碰撞半径
         * @details 根据形状的轮廓计算碰撞半径
         * @note 更新：半径
         * @warning 必须先计算 Outline（轮廓）
         */
        void UpdateCollisionR();

        /**
         * @brief 更新全部信息
         * @details 调用 UpdateInfo()、UpdateOutline() 和 UpdateCollisionR() 更新所有信息
         */
        void UpdateAll();

        /**
         * @brief 几何形状靠近指定点
         * @param drop 目标点的位置
         * @details 使形状的轮廓靠近指定点
         */
        void ApproachDrop(Vec2_ drop);

        /*=========BaseGrid=========*/

        /**
         * @brief 射线碰撞检测
         * @param Pos 射线经过的点
         * @param Angle 射线角度
         * @return 碰撞结果信息，包含是否碰撞和碰撞的精确位置
         */
        CollisionInfoD RayCollide(Vec2_ Pos, FLOAT_ Angle);

        /**
         * @brief 线段碰撞检测（整数坐标）
         * @param start 线段的起始位置
         * @param end 线段的结束位置
         * @return 碰撞结果信息，包含是否碰撞和碰撞的网格位置
         */
        CollisionInfoI PsBresenhamDetection(glm::ivec2 start, glm::ivec2 end);

        /**
         * @brief 线段碰撞检测（浮点数坐标）
         * @param start 线段的起始位置
         * @param end 线段的结束位置
         * @return 碰撞结果信息，包含是否碰撞和碰撞的精确位置
         */
        CollisionInfoD PsBresenhamDetection(Vec2_ start, Vec2_ end);

        /*=========PhysicsFormwork=========*/

        /**
         * @brief 获取对象类型
         * @return 物理对象类型，返回 PhysicsObjectEnum::shape
         * @note 实现了 PhysicsFormwork 中的虚函数 GetType
         */
        virtual PhysicsObjectEnum PFGetType() { return PhysicsObjectEnum::shape; }
    };

}
