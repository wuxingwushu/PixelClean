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
     *          3. 固体交互 = 逐格细粒度投影（亚像素，防深穿透）+ **速度型**接触反作用
     *             （线性 + 角，见 Update 的速度回代段；旧实现用 Δx/dt 当冲量，量级比
     *              钳位大几十倍，等价于方向逐帧翻转的恒力矩 → 刚体在水里一直摇）
     *             + 阿基米德浮力（圆形=弓形**面积**占比；网格形状=**逐格**
     *             与水线裁剪，力作用于浸没形心 → 稳心效应扶正）+ 液体阻力/角阻尼
     *             + 浮力等大反向施加到液体（按接触环带局部、按质量占比分摊 + 单粒子限幅，动量守恒，防"活塞效应"整池升空）；
     *          4. 液面 = **沿水平轴的局部水面高度场**（不是全局平面）。把液体粒子按
     *             垂直于重力的水平轴分桶（桶宽默认 ≈ max(h, 2×静息间距)，可用
     *             param.surfaceBinWidth 覆盖），每桶取"自由水面"粒子作为该水平
     *             区域的水面高度。自由水面 = 该粒子暴露在空气中（正上方无水体、
     *             无动态刚体）+ 下方有 ≥2 层连续水柱且水柱底部不在刚体上 +
     *             其下方一段竖直范围内没有动态刚体。这样"被浮体压住的水"、
     *             "堆在浮体顶面的水"、"浮体正下方的深水"都不算水面，对应的桶
     *             改由左右最近的有效桶填充 → 浮体不会踩着自己推到身上的水升空。
     *             固体逐格/逐圆取自己所在桶的水面做水线裁剪：悬在池壁外、岸上、
     *             或跨越两个水池的部分各用各的水面，互不干扰；相邻有效桶之间线性
     *             插值（跨到无水桶立即中断），并有坡度限制兜底。接触判定 =
     *             固体环带 [0.8R, R+4h] 内是否有液体粒子。
     *             **水柱层数/根部是否在刚体上必须是每桶状态**（SurfaceBin::chain /
     *             chainBottom / chainOnSolid）：若用全局变量，"根部在刚体上"会由
     *             全池最低的那颗粒子一票决定，整条链连带所有桶一起失效 →
     *             SurfaceAt 处处无水 → 浮力整体归零。
     * @note  已知边界与语义限制（设计取舍，使用前请阅读）：
     *          - 液体粒子跳过引擎仲裁器 ⇒ **与地图/固体之间没有库仑摩擦**，
     *            池壁侧的水只受粘滞，不会"爬壁"；
     *          - 参数（h/stiffness/粘滞/浮力倍率等）按 dt = 0.01s（100Hz）
     *            标定；参数入口已做上下限清洗，但改变步长仍需重新调参；
     *          - 等质量假设：粒子宜相同质量；浮力面密度由 h/restDensity 反解
     *            （见 ApplySolidCoupling）；浮力反作用按"与该固体接触的粒子"的
     *            质量占比分摊（单粒子单帧增量另受 param.maxReactionSpeed 限制），
     *            投影力矩仍按"最近一次添加"的 mParticleMass 估算；
     *          - 只有"够大"的水体参与水面高度场：主水簇 + 规模 ≥ 主簇 1/6 的其他
     *            水簇。因此**很小的孤立水洼（< 主簇 1/6）不提供浮力**，很薄的水膜
     *            （< 2 个粒子层）也不算自由水面——这两条是"浮体不能踩着自己带起的
     *            水悬浮"的代价；
     *          - 水面在桶内是平的、跨桶是折线；需要更细的水面变化时减小
     *            param.surfaceBinWidth（代价：每桶粒子更少、噪声更大）；
     *          - 投影的线反作用默认**开启但按速度计算**（param.reactionDragGain）：
     *            水会以"相对速度 × 增益"的方式拖住刚体（含自转），但它只是阻尼项，
     *            不参与库仑摩擦、也不会把刚体推过 maxReactionSpeed 的单帧限幅；
     *            置 0 可回到"水流无法水平推动刚体，制动只靠浮力与阻力"的旧行为；
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
            FLOAT_ velocitySmoothing = 0.0f;  // 速度平滑（XSPH 风格，0 = 关闭）：
                                              // 对邻居速度做核加权平均，专治"贴壁/贴底
                                              // 的水一直抖"。默认 0 保持原有手感，
                                              // 觉得水面不稳时可以 0.1~0.3 逐档加
            FLOAT_ viscosity = 10.0f;         // 线性粘滞 σ（越大流动越"稠"，静置收敛越快）
            FLOAT_ viscosityQuadratic = 1.0f; // 二次粘滞 β（高速时额外耗散，抑制振荡）
            int    iterations = 3;            // 密度松弛迭代次数
            FLOAT_ maxSpeed = 12.0f;          // 粒子速度上限（稳定性钳制）
            FLOAT_ maxPairDisplacement = 0.25f; // 单对粒子单次迭代最大位移（相对 h 的倍数）
            FLOAT_ maxFrameDisplacement = 0.5f; // 单粒子单帧压力位移上限（相对 h 的倍数；
                                                // 会被自动钳到 mapEscapeRadius 之内，保证推得出来）
            FLOAT_ mapEscapeRadius = 4.0f;    // 地图投影最大推出距离（相对 h 的倍数）。
                                              // 必须大于地图上最厚实体的半宽：否则粒子
                                              // 卡在厚墙/立柱里出不来，会一路穿过地板
                                              // 无限下坠（射程内搜索步数恒定，代价为零）
            FLOAT_ surfaceBinWidth = 0.0f;    // 局部水面高度场的水平分辨率（世界单位）：
                                              // 0 = 自动 = max(h, 2×静息间距)（默认 ≈ 1 个地图格）；
                                              // 调小 → 水面更精细（代价：每桶粒子更少、噪声更大）
            FLOAT_ surfaceSmoothing = 0.25f;  // 水面高度的时间低通系数（0~1，0=关闭）：
                                              // 每帧 top = 新值·s + 旧值·(1−s)。水面逐帧抖动
                                              // 会让浮力跟着抖 → 浮体上下泵水 → 水静不下来；
                                              // 这一层滤波专门切断该正反馈
            FLOAT_ buoyancy = 1.0f;           // 阿基米德浮力倍率（1=物理正确密度比；>1 更容易浮）
            FLOAT_ solidDrag = 2.0f;          // 固体在液体中的速度阻尼（1/秒，按浸没比例缩放）
            FLOAT_ maxRiseSpeed = 2.5f;       // 固体上浮速度上限（防止"活塞效应"把液体一起抬离水域）
            FLOAT_ contactDamping = 0.1f;     // 未浸没但仍有水接触时的最小阻力比例（防角速度穿过水面无阻尼摇摆）
            FLOAT_ angularDampingFactor = 5.0f; // 角速度液体阻尼倍数（相对线阻力）
            FLOAT_ maxAngularSpeed = 3.0f;    // 固体角速度上限（防摇摆自激）
            FLOAT_ reactionTorqueGain = 0.08f; // 接触反作用**角**增益（1.0 = 物理正确的动量交换：
                                               // 水从刚体拿到的角动量等大反向还给它）。
                                               // 注意：反作用按**相对速度**计算，量级与速度同阶，
                                               // 不会像"Δx/dt 冲量"那样恒被 maxTorqueImpulse 钳死
                                               // （钳死的力矩方向逐帧翻转 = 刚体在水里一直摇）
            FLOAT_ reactionDragGain = 0.10f;   // 接触反作用**线**增益（水对刚体运动的阻力）。
                                               // 0 = 关闭（回到"水流无法水平推动刚体"的旧行为）
            FLOAT_ maxTorqueImpulse = 0.04f;  // 单帧角速度增量上限（rad/s）。0 = 抑制角反作用
            FLOAT_ maxReactionSpeed = 4.0f;   // 液体反作用（浮力 + 接触）单帧给**单个**粒子/刚体
                                              // 的速度增量上限（世界单位/秒）：粒子少、刚体轻或
                                              // 接触点很多时，反作用叠加起来若不限幅，一帧就能
                                              // 把速度改掉十几甚至上百 u/s（下一帧直接穿地板）。
                                              // 0 = 完全抑制反作用（动量不守恒，仅调试用）
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
         * @brief 查询某世界坐标处的**局部水面高度**（调试/测试用）
         * @param pos      查询位置（只用其水平分量，y 不影响结果）
         * @param outLevel 输出：该水平位置的水面高度（= dot(pos, -gravity) 坐标系的读数）
         * @return true = 该水平区域有实质水体；false = 无水（空气/仅零星水花）
         * @note 数据来自最近一次 Update 建立的水面高度场；第一次 Update 之前恒为 false */
        bool QuerySurfaceLevel(Vec2_ pos, FLOAT_ &outLevel) const;

        /**
         * @brief 密度 → 颜色（粒子流体着色：深蓝(低压) → 亮白蓝(高压)） */
        static glm::vec4 ColorByDensity(FLOAT_ density, const Params &param);

    private:
        /// 边界采样点（地图/形状的实体格、圆内部采样，按流体间距子采样）
        struct BoundarySample
        {
            Vec2_ pos;     // 世界坐标
            FLOAT_ weight; // ψ_b：等效流体粒子数（体积权重）
        };

        /// 重建指针→索引映射（惰性：仅在粒子增删后重建，避免每帧哈希）
        void BuildIndexMap();
        /// 重建邻居表（扁平存储：mNeighborOffset[i]..mNeighborOffset[i+1] 为粒子 i 的邻居索引）
        void RebuildNeighbors(FLOAT_ h);
        /// 重建每个流体粒子附近的**边界采样点**（固体/地图的实体格按流体间距子采样）。
        /// 它们参与密度求和（消除固体边界处的密度缺失）并在位移阶段把流体推开；
        /// 自身不可移动 → 水不会往固体内挤，也不会被反复"挤入-推出"而弹射。
        void RebuildBoundarySamples(FLOAT_ h, FLOAT_ spacingEff);
        /// 把液体粒子钳制出 固体（形状/圆/线）与 地图，防止压力把粒子推入刚体内部；
        /// 同时把投影的动量反作用（力矩）施加到动态固体上
        void ResolveSolidOverlap(FLOAT_ time);
        /// 固体↔液体耦合：阿基米德浮力（圆=外接圆盘；网格形状=逐格 × 逐区域水面裁剪，
        /// 含扶正扭矩）与液体阻力
        void ApplySolidCoupling(FLOAT_ time);
        /// 桶内稳健顶面（自由水面）；该桶没有可用水面时返回 mNoWater
        FLOAT_ BinTopAt(int bin) const;
        /// 局部水面高度（相邻有水桶之间分段线性插值，跨到无水桶立即中断）；无水返回 mNoWater
        FLOAT_ SurfaceAt(FLOAT_ u) const;
        /// 点是否落在某个**动态刚体**（形状/圆）内部（水面高度场用它排除"被刚体
        /// 压住/带起的水"，否则浮体会踩着自己推到身上的水不断升高）
        bool InsideDynamicSolid(const Vec2_ &pos) const;
        /// 形状逐格实心判定（越界 = 空闲，与 PhysicsShape::DropCollision 的坐标约定一致）
        /// @param cosA/sinA 该形状本帧的 cos(angle)/sin(angle)，由调用方按形状缓存
        ///        （本函数在 NearestFree / InsideDynamicSolid 里每帧被调用上万次，
        ///          不能在内部重算三角函数）
        bool ShapeSolidAt(PhysicsShape *s, const Vec2_ &cand, FLOAT_ cosA, FLOAT_ sinA) const;

        /**
         * @brief 水面高度场的一个水平桶
         * @details top/valid 是结果；其余字段是建场时的中间量 */
        struct SurfaceBin
        {
            FLOAT_ top = 0;            // 自由水面高度（valid 时有效）
            FLOAT_ lastV = 0;          // 降序扫描：该桶上一个（更高的）粒子高度
            FLOAT_ lastVUp = 0;        // 升序扫描：该桶上一个（更低的）粒子高度
            FLOAT_ chainBottom = 0;    // 升序扫描：当前水柱（该桶内）最底部粒子的高度
            unsigned int chain = 0;    // 当前连续水柱层数（升序扫描，含当前粒子）
            bool hasWater = false;     // 该桶含实质水粒子
            bool chainOnSolid = false; // 当前水柱底部是否坐在动态刚体上
            bool valid = false;        // 是否已得到可用水面
            unsigned int topIdx = 0;   // 提供该水面的粒子下标（调试用）
        };

        PhysicsWorld *mWorld = nullptr;
        std::vector<PhysicsParticle *> mParticles;
        std::vector<FLOAT_> mDensity;      // 压缩密度（last iterate）
        std::vector<FLOAT_> mDensityNear;  // 近场密度
        std::vector<Vec2_> mPrevPos;       // 上一步修正后的位置（PBF 速度回代基准）
        std::vector<Vec2_> mShift;         // 单次迭代的位移累加（Jacobi，避免顺序偏差）
        // ── 投影与速度解耦（Ihmsen 2011 的"粒子推出"做法）──
        // 投影只修位置：速度用**投影前**的位置回代，再只去掉"撞进固体"的法向分量。
        // 否则固定步长/深穿透的推出位移会被放大成 v=Δx/dt，每帧给粒子注入假速度。
        std::vector<Vec2_> mPreProjPos;    // 投影前位置（速度回代基准）
        std::vector<Vec2_> mProjNormal;    // 本帧投影方向累加（指向固体外侧）
        std::vector<Vec2_> mProjBodyVel;   // 推开它的刚体速度（地图 = 0）
        std::vector<int> mProjBodyIdx;     // 推开它的刚体在 mReactionBodies 中的下标（-1 = 地图/线/无）
        std::vector<unsigned int> mNeighborIndex;   // 邻居扁平表
        std::vector<unsigned int> mNeighborOffset;  // 每粒子邻居区间起点（size = n+1）
        std::vector<BoundarySample> mBoundary;      // 边界采样点扁平表
        std::vector<unsigned int> mBoundaryOffset;  // 每粒子边界区间起点（size = n+1）
        std::vector<PhysicsFormwork *> mSearchV;    // 网格查询缓冲（复用）
        std::vector<Vec2_> mClipPoly;      // 水线裁剪后的浸没多边形（复用，避免每帧分配）
        // ── 固体↔液体接触反作用（每帧在 ResolveSolidOverlap 里重建、Update 里结算）──
        // 顺序：形状在前、圆在后，mProjBodyIdx 用的就是这个下标。
        std::vector<PhysicsAngle *> mReactionBodies;
        std::vector<Vec2_> mReactionJ;     // 每刚体本帧累计的反作用**线**冲量（原始值，未乘增益）
        std::vector<FLOAT_> mReactionT;    // 每刚体本帧累计的反作用**角**冲量（原始值，未乘增益）
        // ── 局部水面高度场（水平分桶）──
        // 液面不再是"一个全局平面"，而是沿水平轴（垂直于重力）分桶的
        // 高度场：每个桶记录该水平区间内**实质水体**的顶面高度。
        // 固体逐格取自己所在的桶 → 悬在池壁外/岸上的部分自然没有水面 → 不计入浸没。
        std::vector<unsigned int> mOrder;  // 高度降序的粒子索引（分簇/分桶复用）
        std::vector<FLOAT_> mHeightOf;     // 与 mParticles 平行的高度缓存（dot(pos, up)）
        std::vector<unsigned char> mEligible; // 该粒子是否属于"实质水体"簇（剔除小水花）
        std::vector<unsigned char> mExposed;  // 该粒子是否"直接暴露在空气中"（其正上方无水体/刚体）
        std::vector<size_t> mClusterCount;   // 各高度簇的粒子数（建场时用）
        std::vector<SurfaceBin> mBins;
        // 水面高度的时间低通历史。**必须按世界坐标对齐**：桶网格锚定在固定栅格上
        //（mBinOrigin 只做整桶跳变），所以历史数组既能按整桶平移复用，
        // 也能在分辨率/对齐变化时整段作废（mBinTopSmoothValid）。
        // 用独立的 valid 标记而不是"top != 0"，否则水面高度恰好经过 0 时滤波会静默失效。
        std::vector<FLOAT_> mBinTopSmooth;             // 上帧的水面高度（时间低通用）
        std::vector<unsigned char> mBinTopSmoothValid; // 该桶的低通历史是否可用
        FLOAT_ mBinTopSmoothOrigin = 0;                // 历史值对应的桶 0 起点
        FLOAT_ mBinTopSmoothWidth = 0;                 // 历史值对应的桶宽
        FLOAT_ mBinOrigin = 0;             // 桶 0 的水平起点（世界坐标沿水平轴的投影）
        FLOAT_ mBinWidth = 1.0f;           // 桶宽（世界单位）
        int mBinNum = 0;                   // 桶数量
        FLOAT_ mGapTh = 1.0f;              // 高度分簇的间隙阈值（建场时更新）
        FLOAT_ mNoWater = 0;               // "无水"哨兵值（建场时更新）
        // ── 固体↔液体动量反作用（局部环带）──
        std::vector<Vec2_> mReactionForce; // 每粒子的浮力反作用分摊力（本帧，按接触粒子质量占比）
        std::vector<unsigned int> mWetIdx; // 与当前固体接触的本系统粒子下标（复用缓冲）
        std::vector<FLOAT_> mLevelBuf;     // 远场水位（窗口内有效桶水面的中位数）计算缓冲
        std::unordered_map<const PhysicsParticle *, unsigned int> mIndexMap; // 指针→索引
        bool mIndexMapDirty = true;        // 粒子增删后置位，Update 时惰性重建
        FLOAT_ mParticleMass = 1.0f;       // 最近一次添加的粒子质量（浮力估算用）
        FLOAT_ mSpacing = 0.5f;            // 最近一次 AddGrid 的粒子间距（浮力估算兜底）
    };
}
