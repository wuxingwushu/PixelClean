#pragma once
#include "PhysicsWorld.hpp"
#include <vector>

namespace PhysicsBlock
{
    /**
     * @brief 基于粒子的 2D 液体模拟（Clavet 双密度松弛风格 / PBF 变体）
     * @details 引擎目前的物理对象集合中没有任何液体/流体模拟：
     *          - PhysicsParticle 是独立粒子，粒子之间没有碰撞裁决器（不会互推）；
     *          - PhysicsCircle/Shape 是刚体，无法松散流动；
     *          - 地图只有静态碰撞格子，没有流体网格。
     *          本类补齐该缺口，实现轻量粒子液体：
     *          1. 液体粒子就是普通 PhysicsParticle，正常注册到物理世界
     *             （重力、摩擦力、与地图/形状/圆的碰撞全部复用既有管线）；
     *          2. 粒子间不参与碰撞裁决，改为本类在每步执行的"双密度松弛"：
     *             压缩压力 P = k·max(ρ−ρ0, 0) + 近场压力 P_near = k_near·ρ_near，
     *             成对按位移 D = dt²·(P·(1−r/h) + P_near·(1−r/h)²) 推开粒子；
     *          3. 位移回代速度（v = Δx/dt）并做线性+二次粘滞，保持流动感；
     *          4. 邻居查找复用物理世界的 GridSearch，无需额外空间结构。
     *          通过 PhysicsWorld::SetLiquid 挂载后，PhysicsEmulator 每步自动调用
     *          Update()，渲染可复用 DrawPhysicsWorld（内部自动绘制液体粒子）。
     * @note  等质量假设：AddParticle/AddGrid 生成的粒子宜使用相同质量，
     *         位移按质量倒数加权，质量差异过大时表现会异常。 */
    class PhysicsLiquid
    {
    public:
        /**
         * @brief 液体参数
         * @details 全部可运行时调整（ImGuiPhysics::PhysicsUI 提供滑杆） */
        struct Params
        {
            FLOAT_ h = 1.0f;                  // 相互作用半径（世界单位），约为粒子间距的 1.5~2 倍
            FLOAT_ restDensity = 1.35f;       // 静息密度（与核函数同单位：Σ(1−r/h)²）
            FLOAT_ stiffness = 2500.0f;       // 压缩压力刚度 k（越大越"硬"）
            FLOAT_ stiffnessNear = 1000.0f;   // 近场压力刚度 k_near（抗粒子重叠/防聚集）
            FLOAT_ viscosity = 10.0f;         // 线性粘滞 σ（越大流动越"稠"，静置收敛越快）
            FLOAT_ viscosityQuadratic = 1.0f; // 二次粘滞 β（高速时额外耗散，抑制振荡）
            int    iterations = 3;            // 密度松弛迭代次数
            FLOAT_ maxSpeed = 12.0f;          // 速度上限（稳定性钳制）
            FLOAT_ maxPairDisplacement = 0.25f; // 单对粒子单次迭代最大位移（相对 h 的倍数）
            FLOAT_ buoyancy = 1.0f;           // 阿基米德浮力倍率（1=物理正确密度比；>1 更容易浮）
            FLOAT_ solidDrag = 2.0f;          // 固体在液体中的速度阻尼（1/秒，按浸没比例缩放）
            FLOAT_ maxRiseSpeed = 2.5f;       // 固体上浮速度上限（防止"活塞效应"把液体一起抬离水域）
        };

        /**
         * @brief 构造液体
         * @param World  物理世界（必须已 SetMapFormwork；粒子通过 World->AddObject 注册）
         * @param P      初始参数 */
        PhysicsLiquid(PhysicsWorld *World, const Params &P = {});
        ~PhysicsLiquid();

        /**
         * @brief 添加单个液体粒子（注册到物理世界并加入网格搜索）
         * @param pos      初始位置（世界坐标）
         * @param mass     质量（建议与其他粒子一致）
         * @param friction 摩擦因数（与地形/固体接触时使用） */
        PhysicsParticle *AddParticle(Vec2_ pos, FLOAT_ mass = 1.0f, FLOAT_ friction = 0.2f);

        /**
         * @brief 生成一片矩形网格状的液体（常用于模拟"水团下落/注水"）
         * @param center  中心位置（世界坐标）
         * @param numX    横向粒子数
         * @param numY    纵向粒子数
         * @param spacing 粒子间距（推荐 0.4~0.6）
         * @param mass    单粒子质量 */
        void AddGrid(Vec2_ center, int numX, int numY, FLOAT_ spacing, FLOAT_ mass = 1.0f, FLOAT_ friction = 0.2f);

        /**
         * @brief 清空液体：从物理世界移除并删除所有液体粒子 */
        void Clear();

        /**
         * @brief 每物理步执行液体更新（PhysicsEmulator 内部自动调用）
         * @param time 时间步长（秒） */
        void Update(FLOAT_ time);

        /// 最近一次求解得到的密度（与 Particles() 一一对应，用于着色/调试）
        const std::vector<FLOAT_> &Density() const { return mDensity; }
        /// 液体粒子列表
        const std::vector<PhysicsParticle *> &Particles() const { return mParticles; }
        /// 所属物理世界
        PhysicsWorld *World() const { return mWorld; }

        /// 运行时参数
        Params param;

        /**
         * @brief 密度 → 颜色（粒子流体着色：深蓝(低压) → 亮白蓝(高压)） */
        static glm::vec4 ColorByDensity(FLOAT_ density, const Params &param);

    private:
        /// 重建邻居表（扁平存储：mNeighborOffset[i]..mNeighborOffset[i+1] 为粒子 i 的邻居索引）
        void RebuildNeighbors();
        /// 把液体粒子钳制出 固体（形状/圆）与 地图，防止压力把粒子推入刚体内部；
        /// 同时把投影的动量反作用施加到动态固体上（液体对固体的制动/支撑）
        void ResolveSolidOverlap(FLOAT_ time);
        /// 固体↔液体耦合：阿基米德浮力（圆=外接圆盘；网格形状=真实矩形水线裁剪，含扶正扭矩）与液体阻力
        void ApplySolidCoupling(FLOAT_ time);

        PhysicsWorld *mWorld = nullptr;
        std::vector<PhysicsParticle *> mParticles;
        std::vector<FLOAT_> mDensity;      // 压缩密度（last iterate）
        std::vector<FLOAT_> mDensityNear;  // 近场密度
        std::vector<Vec2_> mPrevPos;       // 上一步修正后的位置（PBF 速度回代基准）
        std::vector<unsigned int> mNeighborIndex;   // 邻居扁平表
        std::vector<unsigned int> mNeighborOffset;  // 每粒子邻居区间起点（size = n+1）
        std::vector<PhysicsFormwork *> mSearchV;    // 网格查询缓冲（复用）
        std::vector<Vec2_> mClipPoly;      // 水线裁剪后的浸没多边形（复用，避免每帧分配）
        std::vector<FLOAT_> mHeightBuf;    // 粒子高度排序缓冲（液面估计复用，避免每帧分配）
        FLOAT_ mParticleMass = 1.0f;       // 最近一次添加的粒子质量（浮力估算用）
        FLOAT_ mSpacing = 0.5f;            // 最近一次 AddGrid 的粒子间距（液体面密度估算用）
    };
}
