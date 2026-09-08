#pragma once
#include "PhysicsWorld.hpp"
#include <vector>
#include <unordered_map>

namespace PhysicsBlock
{
    /**
     * @brief 基于粒子的 2D 液体模拟（Clavet 双密度松弛风格 / PBF 变体）
     * @details 引擎的物理对象集合（刚体粒子/形状/圆/线 + 地图碰撞）没有液体能力：
     *          - PhysicsParticle 之间没有碰撞裁决器（粒子不会互推，无法成流体）；
     *          - PhysicsCircle/Shape 是刚体，不会流动；
     *          - 地图只有静态碰撞格子，没有流体网格。
     *          本类补齐该缺口，实现轻量粒子液体，结构与职责如下：
     *          1. 液体粒子 = 普通 PhysicsParticle，注册到物理世界
     *             （重力/位置积分/网格搜索/渲染复用），但通过
     *             PhysicsParticle::IsLiquidParticle 标记**跳过全部引擎仲裁器**——
     *             液体与地图/固体的交互全部由本类自理（固液交互是专用路径，
     *             引擎的仲裁器 bias 机制对深穿透会猛踢刚体，不适用）；
     *          2. 粒子间相互作用 = 双密度松弛：压缩压力
     *             P = k·max(ρ−ρ0,0) + 近场压力 P_near = k_near·ρ_near
     *             + 内聚力（表面张力）P_ten = k·σ·min(ρ−ρ0,0)（欠密吸引，限幅），
     *             成对位移 D = dt²·(P·(1−r/h) + (P_near+P_ten)·(1−r/h)²)，
     *             位移以 **Jacobi 累加**方式统一施加（与粒子索引顺序无关），
     *             随后 v = Δx/dt 回代速度并做线性+二次粘滞；
     *          3. 固体交互 = 逐格细粒度投影（亚像素，防深穿透）+ 接触点反作用
     *             力矩（撬转）+ 阿基米德浮力（圆形=外接圆盘；网格形状=**逐格**
     *             与水线裁剪，力作用于浸没形心 → 稳心效应扶正）+ 液体阻力/角阻尼
     *             + 浮力等大反向施加到液体（动量守恒，防"活塞效应"整池升空）；
     *          4. 液面锚定"固体接触到的最大高度簇"顶面（按高度分段聚类，
     *             若接触到的簇过小——例如被固体带起来的水花——回落到全局主簇，
     *             避免"固体踩着自己带起的水悬浮"）；接触判定=固体环带
     *             [0.8R, R+4h] 内是否有**液体**粒子。
     * @note  已知边界与语义限制（设计取舍，使用前请阅读）：
     *          - 液体粒子跳过引擎仲裁器 ⇒ **与地图/固体之间没有库仑摩擦**，
     *            池壁侧的水只受粘滞，不会"爬壁"；
     *          - 参数（h/stiffness/粘滞/浮力倍率等）按 dt = 0.01s（100Hz）
     *            标定；参数入口已做上下限清洗，但改变步长仍需重新调参；
     *          - 等质量假设：粒子宜相同质量；浮力面密度由 h/restDensity 反解
     *            （见 ApplySolidCoupling），多质量混合时仍以"最近一次添加"的
     *            mParticleMass 为准；
     *          - 浮力反作用按全部液体粒子均分（动量守恒近似，非环带局部）；
     *          - 投影的**线**反作用被有意关闭（只保留力矩，且增益为人工标定值）：
     *            水流无法水平推动刚体，制动只靠浮力与阻力；
     *          - 未接入 JSON 序列化（反序列化世界不还原液体；
     *            IsLiquidParticle 标记不会被序列化）；
     *          - 投影路径 O(液体粒子数 × 固体数)，上千粒子+数十固体前
     *            需按半径网格预筛。 */
    class PhysicsLiquid
    {
    public:
        /**
         * @brief 液体参数
         * @details 全部可运行时调整（ImGuiPhysics::PhysicsUI 提供滑杆，
         *          Update 内部对非法值做上下限清洗，不会产生 NaN） */
        struct Params
        {
            FLOAT_ h = 1.0f;                  // 相互作用半径（世界单位），约为粒子间距的 1.5~2 倍
            FLOAT_ restDensity = 1.35f;       // 静息密度（与核函数同单位：Σ(1−r/h)²）
            FLOAT_ stiffness = 2500.0f;       // 压缩压力刚度 k（越大越"硬"）
            FLOAT_ stiffnessNear = 1000.0f;   // 近场压力刚度 k_near（抗粒子重叠/防聚集）
            FLOAT_ surfaceTension = 0.1f;     // 内聚力（表面张力）系数 σ_t：欠密时吸引，0=关闭（只压不吸）。
                                              // 0.1 ≈ 与默认近场压力同量级（孤立水滴/水花会缓慢并拢，
                                              // 自由液面不会被拉塌）；调大更有"黏稠成团"感
            FLOAT_ maxDensityDeficit = 1.0f;  // 内聚项允许的最大欠密量（防止无限吸引塌缩）
            FLOAT_ viscosity = 10.0f;         // 线性粘滞 σ（越大流动越"稠"，静置收敛越快）
            FLOAT_ viscosityQuadratic = 1.0f; // 二次粘滞 β（高速时额外耗散，抑制振荡）
            int    iterations = 3;            // 密度松弛迭代次数
            FLOAT_ maxSpeed = 12.0f;          // 粒子速度上限（稳定性钳制）
            FLOAT_ maxPairDisplacement = 0.25f; // 单对粒子单次迭代最大位移（相对 h 的倍数）
            FLOAT_ maxFrameDisplacement = 0.5f; // 单粒子单帧压力位移上限（相对 h 的倍数；
                                                // 会被自动钳到 mapEscapeRadius 之内，保证推得出来）
            FLOAT_ mapEscapeRadius = 0.75f;   // 地图投影最大推出距离（相对 h 的倍数）
            FLOAT_ buoyancy = 1.0f;           // 阿基米德浮力倍率（1=物理正确密度比；>1 更容易浮）
            FLOAT_ solidDrag = 2.0f;          // 固体在液体中的速度阻尼（1/秒，按浸没比例缩放）
            FLOAT_ maxRiseSpeed = 2.5f;       // 固体上浮速度上限（防止"活塞效应"把液体一起抬离水域）
            FLOAT_ contactDamping = 0.1f;     // 未浸没但仍有水接触时的最小阻力比例（防角速度穿过水面无阻尼摇摆）
            FLOAT_ angularDampingFactor = 5.0f; // 角速度液体阻尼倍数（相对线阻力）
            FLOAT_ maxAngularSpeed = 3.0f;    // 固体角速度上限（防摇摆自激）
            FLOAT_ reactionTorqueGain = 0.08f; // 接触点反作用力矩增益（1.0 = 物理正确冲量 τ=r×m·Δv/dt；
                                               // 0.08 ≈ 原实现 100Hz 下的手感，调大可增强撬转）
            FLOAT_ maxTorqueImpulse = 0.04f;  // 单帧角冲量上限（归一约束：防数值过冲）
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
         * @brief 清空液体：从物理世界移除并删除所有液体粒子
         * @note 析构函数也会调用本函数，因此手动 delete 液体不会留下
         *       "跳过全部仲裁器"的孤儿粒子 */
        void Clear();

        /**
         * @brief 通知液体：某粒子即将被物理世界删除
         * @param p 待删除的粒子（必须已在本液体内，否则静默忽略）
         * @details 由 PhysicsWorld::RemoveObject(PhysicsParticle*) 转发。
         *          若外部直接删除液体粒子而不通知，本类的指针列表会悬空。 */
        void NotifyParticleRemoved(PhysicsParticle *p);

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
        /// 重建指针→索引映射（惰性：仅在粒子增删后重建，避免每帧哈希）
        void BuildIndexMap();
        /// 重建邻居表（扁平存储：mNeighborOffset[i]..mNeighborOffset[i+1] 为粒子 i 的邻居索引）
        void RebuildNeighbors(FLOAT_ h);
        /// 把液体粒子钳制出 固体（形状/圆/线）与 地图，防止压力把粒子推入刚体内部；
        /// 同时把投影的动量反作用（力矩）施加到动态固体上
        void ResolveSolidOverlap(FLOAT_ time);
        /// 固体↔液体耦合：阿基米德浮力（圆=外接圆盘；网格形状=逐格水线裁剪，含扶正扭矩）
        /// 与液体阻力
        void ApplySolidCoupling(FLOAT_ time);

        PhysicsWorld *mWorld = nullptr;
        std::vector<PhysicsParticle *> mParticles;
        std::vector<FLOAT_> mDensity;      // 压缩密度（last iterate）
        std::vector<FLOAT_> mDensityNear;  // 近场密度
        std::vector<Vec2_> mPrevPos;       // 上一步修正后的位置（PBF 速度回代基准）
        std::vector<Vec2_> mShift;         // 单次迭代的位移累加（Jacobi，避免顺序偏差）
        std::vector<unsigned int> mNeighborIndex;   // 邻居扁平表
        std::vector<unsigned int> mNeighborOffset;  // 每粒子邻居区间起点（size = n+1）
        std::vector<PhysicsFormwork *> mSearchV;    // 网格查询缓冲（复用）
        std::vector<Vec2_> mClipPoly;      // 水线裁剪后的浸没多边形（复用，避免每帧分配）
        std::vector<FLOAT_> mHeightBuf;    // 粒子高度排序缓冲（液面估计复用，避免每帧分配）
        std::vector<FLOAT_> mTorqueBuf;    // 每形状的接触点力矩冲量累积（复用）
        std::vector<FLOAT_> mClusterMin;   // 高度簇：下界
        std::vector<FLOAT_> mClusterMax;   // 高度簇：上界
        std::vector<FLOAT_> mClusterTop;   // 高度簇：顶面（=上界，单独存便于阅读）
        std::vector<size_t> mClusterCount; // 高度簇：粒子数
        std::vector<size_t> mRingCount;    // 环带内各簇的粒子数（每固体复用）
        std::unordered_map<const PhysicsParticle *, unsigned int> mIndexMap; // 指针→索引
        bool mIndexMapDirty = true;        // 粒子增删后置位，Update 时惰性重建
        FLOAT_ mParticleMass = 1.0f;       // 最近一次添加的粒子质量（浮力估算用）
        FLOAT_ mSpacing = 0.5f;            // 最近一次 AddGrid 的粒子间距（浮力估算兜底）
    };
}
