#include "PhysicsShape.hpp"
#include "BaseCalculate.hpp"

namespace PhysicsBlock
{

    /**
     * @brief 构造函数
     * @param Pos 形状的初始位置
     * @param Size 网格大小
     * @details 初始化物理形状的位置和网格大小
     * 调用 PhysicsAngle 构造函数初始化位置，调用 BaseOutline 构造函数初始化网格
     */
    PhysicsShape::PhysicsShape(Vec2_ Pos, glm::ivec2 Size) : PhysicsAngle(Pos), BaseOutline(Size.x, Size.y)
    {
    }

    /**
     * @brief 析构函数
     */
    PhysicsShape::~PhysicsShape()
    {
    }

    /**
     * @brief 检测点是否点击到网格
     * @param Pos 点的世界位置
     * @return 碰撞结果信息，包含是否碰撞和碰撞的网格位置
     * @details 1. 将世界坐标转换为形状局部坐标
     * 2. 应用旋转变换，将点转换到未旋转的坐标系
     * 3. 计算点在网格中的位置
     * 4. 检查点是否在网格范围内
     * 5. 检测点是否碰撞到实体网格
     */
    CollisionInfoI PhysicsShape::DropCollision(Vec2_ Pos)
    {
        Pos -= pos; // 转换为局部坐标
        Pos = vec2angle(Pos, -angle); // 应用反向旋转
        CollisionInfoI info;
        info.pos = ToInt(Pos + CentreMass); // 计算网格位置
        
        // 处理边界情况
        if (Pos.x == width)
        {
            info.pos.x = width - 1;
        }
        if (Pos.y == height)
        {
            info.pos.y = height - 1;
        }
        
        info.Collision = GetCollision(info.pos); // 检测碰撞
        return info;
    }

    /**
     * @brief 更新形状的物理信息
     * @details 更新以下信息：
     * 1. 总质量
     * 2. 几何中心
     * 3. 质心
     * 4. 转动惯量
     * @note 当质量为 FLOAT_MAX 时，形状被视为不可移动
     */
    void PhysicsShape::UpdateInfo()
    {
        Vec2_ UsedCentreMass = CentreMass; // 旧质心
        mass = 0;
        unsigned int Size = 0;    // 实体格子数
        unsigned int i;           // 存储网格索引
        CentreMass = {0.0, 0.0};  // 清空位置
        CentreShape = {0.0, 0.0}; // 清空位置
        
        // 计算质量、几何中心和质心
        for (int x = 0; x < width; ++x)
        {
            for (int y = 0; y < height; ++y)
            {
                i = x * height + y;
                // 不为空
                if (at(i).type & GridBlockType::Entity)
                {
                    mass += at(i).mass;
                    ++Size;
                    CentreShape += glm::ivec2{x, y};
                    CentreMass += (Vec2_{x, y} * at(i).mass);
                }
            }
        }
        
        CentreShape /= Size;
        invMass = 1.0 / mass;
        
        // 处理质量为零的情况
        if (invMass == 0)
        {
            mass = FLOAT_MAX;
            CentreMass = CentreShape;
        }
        else
        {
            CentreMass /= mass;
        }
        
        CentreShape += Vec2_{0.5, 0.5}; // 移动到格子的中心
        CentreMass += Vec2_{0.5, 0.5};  // 移动到格子的中心

        // 因为位置是指质心在世界坐标的位置，质心改了，位置也需要进行偏移
        pos += vec2angle(CentreMass - UsedCentreMass, angle);
        OldPos = pos;

        // 计算转动惯量
        MomentInertia = 0; // 清空转动惯量
        Vec2_ lpos;        // 存储临时位置
        FLOAT_ Lmass;      // 用于存储切割出的小正方形的质量
        
        for (FLOAT_ x = 0; x < width; ++x)
        {
            for (FLOAT_ y = 0; y < height; ++y)
            {
                // 不存在跳过
                i = x * height + y;
                if ((at(i).Entity) == 0)
                {
                    continue;
                }
                
                // 获取格子切分的小格子质量
                Lmass = at(x, y).mass / (MomentInertiaSamplingSize * MomentInertiaSamplingSize);
                
                // 采样计算转动惯量
                for (int cx = 0; cx < MomentInertiaSamplingSize; ++cx)
                {
                    for (int cy = 0; cy < MomentInertiaSamplingSize; ++cy)
                    {
                        // 得到采样点的位置
                        lpos = {x + ((1.0 / MomentInertiaSamplingSize) * cx + (0.5 / MomentInertiaSamplingSize)), 
                                y + ((1.0 / MomentInertiaSamplingSize) * cy + (0.5 / MomentInertiaSamplingSize))};
                        // 获取位置偏移量（方向，距离）
                        lpos -= CentreMass;
                        // 累加转动惯量
                        MomentInertia += Lmass * (lpos.x * lpos.x + lpos.y * lpos.y);
                    }
                }
            }
        }
        
        invMomentInertia = 1.0 / MomentInertia;
        if (invMomentInertia == 0)
        {
            MomentInertia = FLOAT_MAX;
        }
    }

    /**
     * @brief 计算碰撞半径
     * @details 根据形状的轮廓计算碰撞半径
     * @note 碰撞半径是从质心到轮廓上最远点的距离
     */
    void PhysicsShape::UpdateCollisionR()
    {
        radius = 0;
        FLOAT_ R;
        for (size_t i = 0; i < OutlineSize; i++)
        {
            R = ModulusLength(OutlineSet[i]); // 计算轮廓点到质心的距离平方
            if (radius < R)
            {
                radius = R;
            }
        }
        radius = SQRT_(radius); // 开方得到实际距离
    }

    /**
     * @brief 更新全部信息
     * @details 1. 如果质量为 FLOAT_MAX（不可移动），则设置相关参数
     * 2. 否则调用 UpdateInfo() 更新物理信息
     * 3. 调用 UpdateOutline() 更新轮廓
     * 4. 调用 UpdateCollisionR() 更新碰撞半径
     */
    void PhysicsShape::UpdateAll()
    {
        if (mass == FLOAT_MAX)
        {
            invMass = 0;
            MomentInertia = FLOAT_MAX;
            invMomentInertia = 0;
            CentreMass = { FLOAT_(width) / 2, FLOAT_(height) / 2 };
            CentreShape = CentreMass;
            OldPos = pos;
        }
        else
        {
            UpdateInfo();
        }
        UpdateOutline(CentreMass);
        UpdateCollisionR();
    }

    /**
     * @brief 几何形状靠近指定点
     * @param drop 目标点的位置
     * @details 1. 计算从形状中心到目标点的方向
     * 2. 沿该方向从轮廓的一侧到另一侧发射一条线段
     * 3. 检测线段与形状的碰撞
     * 4. 移动形状使碰撞点与目标点重合
     */
    void PhysicsShape::ApproachDrop(Vec2_ drop)
    {
        // 计算从形状中心到目标点的方向
        Vec2_ OutlineDrop = vec2angle({radius, 0}, EdgeVecToCosAngleFloat(drop - pos));
        // 检测线段与形状的碰撞
        CollisionInfoD info = BresenhamDetection(CentreMass + OutlineDrop, CentreMass - OutlineDrop);
        // 移动形状
        pos += drop - (info.pos + pos);
    }

    /**
     * @brief 射线碰撞检测
     * @param Pos 射线经过的点
     * @param Angle 射线角度
     * @return 碰撞结果信息，包含是否碰撞和碰撞的精确位置
     * @details 1. 计算射线方向上的线段
     * 2. 调用 PsBresenhamDetection 检测线段与形状的碰撞
     */
    CollisionInfoD PhysicsShape::RayCollide(Vec2_ Pos, FLOAT_ Angle)
    {
        Vec2_ drop = vec2angle({radius, 0}, Angle);
        return PsBresenhamDetection(Pos - drop, Pos + drop);
    }

    /**
     * @brief 线段碰撞检测（整数坐标）
     * @param start 线段的起始位置
     * @param end 线段的结束位置
     * @return 碰撞结果信息，包含是否碰撞和碰撞的网格位置
     * @details 调用 BresenhamDetection 进行线段碰撞检测
     */
    CollisionInfoI PhysicsShape::PsBresenhamDetection(glm::ivec2 start, glm::ivec2 end)
    {
        return BresenhamDetection(start, end);
    }

    /**
     * @brief 线段碰撞检测（浮点数坐标）
     * @param start 线段的起始位置
     * @param end 线段的结束位置
     * @return 碰撞结果信息，包含是否碰撞和碰撞的精确位置
     * @details 1. 将线段转换为形状局部坐标
     * 2. 应用旋转变换，将线段转换到未旋转的坐标系
     * 3. 裁剪线段，使其在形状的矩形范围内
     * 4. 检测线段与形状的碰撞
     * 5. 如果碰撞，将碰撞点转换回世界坐标系
     */
    CollisionInfoD PhysicsShape::PsBresenhamDetection(Vec2_ start, Vec2_ end)
    {
        // 偏移中心位置，对其网格坐标系
        start -= pos;
        end -= pos;

        AngleMat Mat(angle);
        start = Mat.Anticlockwise(start);
        end = Mat.Anticlockwise(end);

        // 裁剪线段 让线段都在矩形内
        PhysicsBlock::SquareFocus data = PhysicsBlock::LineSquareFocus(start + CentreMass, end + CentreMass, width, height);
        if (data.Focus)
        {
            // 线段碰撞检测
            CollisionInfoD info = BresenhamDetection(data.start, data.end);
            if (info.Collision)
            {
                // 返回物理坐标系
                info.pos = Mat.Rotary(info.pos - CentreMass) + pos;
                return info;
            }
        }
        return {false};
    }

#if PhysicsBlock_Serialization
    /**
     * @brief 将形状状态序列化为 JSON
     * @param data 输出的 JSON 数据
     * @details 调用父类的序列化方法，将 PhysicsAngle 和 BaseGrid 的状态序列化到 JSON 中
     */
    void PhysicsShape::JsonSerialization(nlohmann::json_abi_v3_12_0::basic_json<> &data)
    {
        PhysicsAngle::JsonSerialization(data);
        BaseGrid::JsonSerialization(data);
    }

    /**
     * @brief 从 JSON 数据反序列化形状状态
     * @param data JSON 数据
     * @details 1. 调用父类的反序列化方法，从 JSON 数据中恢复 PhysicsAngle 和 BaseGrid 的状态
     * 2. 调用 UpdateAll() 更新所有信息
     * 3. 从 JSON 数据中恢复位置和旧位置
     */
    void PhysicsShape::JsonContrarySerialization(const nlohmann::json_abi_v3_12_0::basic_json<> &data)
    {
        PhysicsAngle::JsonContrarySerialization(data);
        BaseGrid::JsonContrarySerialization(data);
        UpdateAll();
        ContrarySerializationVec2(data, pos);
        ContrarySerializationVec2(data, OldPos);
    }
#endif

}