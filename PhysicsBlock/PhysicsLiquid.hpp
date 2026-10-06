#pragma once
#include "PhysicsWorld.hpp"
#include <vector>
#include <future>

namespace PhysicsBlock
{
    /**
     * @brief 基于位置的流体 Position Based Fluids (PBF)
     * @details **本类的算法与参数完整移植自参考项目 Floaty-main**（Rust/TypeScript）：
     *          - 算法：Position Based Dynamics / Position Based Fluids
     *      （Müller PBD；Macklin PBF；密度约束 C_i = ρ_i/ρ0 - 1 的 Jacobi λ 求解）
     *      —— 见 Floaty `src/lib.rs solve_fluid_jacobi` 与 `solver.ts PBD.solveFluid`；
     *          - 邻居搜索：哈希链表网格（cell = 1.3h，3×3 邻域，每粒子 ≤64 邻居，哈希桶 370111）
     *      —— 见 Floaty `src/lib.rs NeighborLinkedList`；
     *          - 表面张力：Akinci 内聚 + 曲率项（Floaty `lib.rs` 内联常数
     *      cohesion = 0.01*0.01、curvature = 0.001*0.02）；
     *          - 积分与稳定化：半隐式欧拉子步（substeps=10）、位移 CFL 钳制
     *      （maxDisp = cfl·h，流体 0.9×，v = Δx/dt）—— 见 `solver.ts PBD.simulate / enforceCFL`；
     *          - 参数：以 `main.ts SimulationParams` 为基底，并做了两处工程校准：
     *      substeps=6（100Hz 物理步下与 Floaty 同子步步长 1/600）、
     *      restDensity 按静息点阵精确求值（Floaty 的 1.5/d² 近似公式低 ~5%，
     *      会引起出生喷泉）、内聚取 Floaty 的 25%（水滴感而非胶感）、
     *      另加 XSPH 帧级黏性（Macklin PBF 可选黏性项，水的物理黏性）。
     *
     *          固液双向耦合同样照搬 Floaty 的做法（README "Two-Way coupling"：
     *          *Unified Particle Physics for Real-Time Applications* 的质量加权 PBD）——
     *          Floaty 里参与 PBF 的软体 "blob 粒子" 在本引擎对应 **刚体采样点**：
     *          · 动态刚体（网格形状/圆/线）按流体粒子间距布采样点，加入同一 PBF 邻居系统；
     *            采样布局（局部锚点）按"几何指纹"跨帧缓存，几何不变则零重建（R1）；
     *          · 采样点计入密度约束、按 w = 1/m_采样 参与 Δp（刚体质量越大位移越小）；
     *          · Δp 经最小二乘刚体拟合（平移 = 平均位移，转动 = Σ(r×d)/Σr²）投影回刚体，
     *            浮力由压力梯度**自然涌现**（无需任何经验浮力/阻力参数）；
     *          · 静态刚体（invMass=0 && mass>0）**不布采样点**（R2-A，已落地）：
     *            其采样是零回报路径（w=0 的 λ/Δp 纯消耗，投影时又整体丢弃），
     *            挡水与地形统一交给 SolveMapBoundaries 的几何钳制，贴墙手感与地形一致。
     *            静态体因此不进 mPos，N 回落到纯流体规模（静态构件越多降幅越大）；
     *            它仍作为"纯几何钳制体"留在变换缓存 mSolidXf 中，故挡水能力不变；
     *          · 流体粒子陷入动态刚体内部时按 Floaty 的 `inside` 规则跳过压力项
     *      （不再与该刚体作用，自然流出，防"被刚体带起的水悬浮"）。
     *
     *          地形（MapFormwork）对应 Floaty 的容器边界 `solveFluidBoundaries`：
     *          积分后把落入实体格 / 静态刚体内部的粒子钳到最近空位（带 ε·rand 抖动）。
     *
     * @note   与 Floaty 的差异（引擎宿主差异，非算法差异）：
     *          - Floaty 的 blob 是软体（距离/面积约束）；本引擎的固件是刚体，
     *            故其"约束投影"槽位换成上面的刚体拟合投影；
     *          - 刚体自身的重力/碰撞积分由 PhysicsWorld 完成（液体粒子则完全由
     *        本类积分，PhysicsWorld 对 IsLiquidParticle 跳过 PhysicsSpeed/PhysicsPos）；
     *          - 液体粒子质量按 Floaty 的单位质量公式求解（`mass` 字段仅作引擎记账）。
     * @note   λ/Δp 两遍按 i 独立做数据并行分片（结果与串行逐位一致、确定性）；
     *          邻居搜索等其余阶段单线程。
     * @note   未接入 JSON 序列化（反序列化世界不还原液体）。 */
    class PhysicsLiquid
    {
    public:
        /**
         * @brief 液体参数（= Floaty 的 SimulationParams + 其 main.ts 几何常数 + lib.rs 表面张力常数） */
        struct Params
        {
            // —— 几何尺度（Floaty main.ts：particleRadius=0.15 → kernelRadius=3r、initSpacing=0.8·2r）——
            FLOAT_ particleRadius = 0.15f;          ///< 粒子半径 r（Floaty: 0.15）
            FLOAT_ kernelRadius = FLOAT_(0.45);     ///< 核半径 h（Floaty: 3.0*particleRadius = 0.45）
            FLOAT_ restDensity = FLOAT_(16.66667);  ///< 静息密度 ρ0（= 静息点阵实际核密度，Sync 时精确求值）

            // —— SimulationParams（Floaty main.ts 默认值）——
            // substeps=10 是配合 Floaty 60Hz 帧的（子步步长 1/600 秒）；
            // 本引擎 PhysicsEmulator 为 100Hz 固定步 → 等价子步数 = 6（同为
            // 1/600 子步步长，还省 40% 开销）。子步步长才是稳定性关键，不是个数。
            int substeps = 6;                       ///< 每次 Update 的子步数（子步步长 ≈ Floaty 的 1/600）
            FLOAT_ eps = FLOAT_(50);                ///< λ 松弛项（Floaty: 50）
            FLOAT_ cfl = FLOAT_(0.12);              ///< 位移 CFL 系数：maxDisp = cfl·h（Floaty: 0.12）

            // —— 表面张力（Floaty src/lib.rs solve_fluid_jacobi 内联常数，按用途配强度）——
            // · 内聚：把液体"黏连成团"的力——**史莱姆/胶质感的来源**。Floaty 原始系数
            //   （0.01²·k ≈ 2.08e-4）是黏液手感；**水预设取 25%（默认）**：水滴可聚拢、
            //   液体不成胶。调到 0 则完全无黏连（连浮力都会变弱：静水压力场太软，
            //   内聚膜承担一部分支撑）。
            // · 曲率：梯度差分的平滑/扩散项（抗毛刺），无黏连力。
            FLOAT_ cohesion = FLOAT_(0.000025);     ///< 内聚（水预设 = Floaty 0.01²×25%；史莱姆 = 0.0001·k）
            FLOAT_ curvature = FLOAT_(0.00002);     ///< 曲率系数（Floaty 硬编码: 0.001*0.02）
            bool surfaceTension = true;             ///< 表面张力项总开关（内聚/曲率强度由系数控制）

            // —— 水的黏性（XSPH 速度平滑，Macklin PBF 论文的可选黏性项）——
            // v_i += viscosity · Σ_j (v_j − v_i)·W(r_ij) / Σ_j W(r_ij)
            // 帧级黏性（每次 Update 平滑**一次**）：抹平速度高频噪声、晃动自然衰减。
            // 数值上它还是 Jacobi 单次迭代能量泵的主要稳定器（去掉它水体会喷发）。
            // 0=无黏性；水≈0.05~0.15；>0.3 开始变糖浆。
            FLOAT_ viscosity = FLOAT_(0.1);         ///< XSPH 系数（帧级，每 Update 一次）

            /**
             * @brief 默认构造（空体即可，必须由用户写出：
             *        构造函数默认实参 `const Params &P = {}` 位于类体内部，
             *        嵌套类的默认成员初始化器在外层类结束前不可见，GCC 会拒绝转换） */
            Params() {}

            /**
             * @brief 按 Floaty 公式由 particleRadius 重算派生参数
             * @details kernelRadius = 3r；restDensity = 静息间距(1.6r)点阵的实际核密度
             *      （Floaty 的 1.5/(2r)² 是近似公式、低 ~5%，会引起出生喷泉，
             *        这里按 SolveFluidJacobi 同一核函数精确求和消除）；
             *      有量纲常数 eps / 内聚 / 曲率按 Floaty 参考尺度 r0=0.15 不变量化
             *      （eps·s²、内聚/s、曲率/s⁴ 保持 Floaty 数值），保证任意场景
             *      间距下与 Floaty 无量纲等价。 */
            void SyncFromParticleRadius();

            /**
             * @brief 非法值清洗（h/ρ0 ≤ 0 会让 1/h、1/ρ0 发散；substeps/eps/cfl 取合法域） */
            void Sanitize();
        };

        /**
         * @brief 构造液体
         * @param World 物理世界（必须已 SetMapFormwork；粒子通过 World->AddObject 注册）
         * @param P     初始参数（Floaty 默认值） */
        PhysicsLiquid(PhysicsWorld *World, const Params &P = {});
        ~PhysicsLiquid();

        /**
         * @brief 添加单个液体粒子（注册到物理世界）
         * @param pos      初始位置（世界坐标）
         * @param mass     质量（引擎记账用；PBF 求解按 Floaty 单位质量公式）
         * @param friction 摩擦因数 */
        PhysicsParticle *AddParticle(Vec2_ pos, FLOAT_ mass = 1.0f, FLOAT_ friction = 0.2f);

        /**
         * @brief 生成一片矩形网格状的液体
         * @param center  中心位置（世界坐标）
         * @param numX    横向粒子数
         * @param numY    纵向粒子数
         * @param spacing 粒子间距（Floaty 建议 initSpacing = 0.8·粒径 = 1.6·r）
         * @param mass    单粒子质量
         * @param friction 摩擦因数
         * @details 依 Floaty 的 `initSpacing = 0.8 * particleDiameter` 反推粒子尺度
         *      r = spacing/1.6 并按 Floaty 公式同步 kernelRadius / restDensity，
         *      使场景无论用什么间距都保持 Floaty 的无量纲参数组。
         *      参数同步只在液体为空（首次填充）时自动执行（R15）；之后追加水块
         *      不再静默重写已调过的参数组，需要换尺度请显式调用
         *      param.SyncFromParticleRadius()。 */
        void AddGrid(Vec2_ center, int numX, int numY, FLOAT_ spacing, FLOAT_ mass = 1.0f, FLOAT_ friction = 0.2f);

        /**
         * @brief 清空液体：从物理世界移除并删除所有液体粒子
         * @note 析构函数也会调用本函数，因此手动 delete 液体不会留下
         *       "跳过全部仲裁器"的孤儿粒子 */
        void Clear();

        /**
         * @brief 通知液体：某粒子即将被物理世界删除
         * @param p 待删除的粒子（必须已在本液体内，否则静默忽略）
         * @details 由 PhysicsWorld::RemoveObject(PhysicsParticle*) 转发。 */
        void NotifyParticleRemoved(PhysicsParticle *p);

        /**
         * @brief 每物理步执行液体更新（PhysicsEmulator 内部自动调用）
         * @param time 时间步长（秒，= Floaty 的 dtFrame）
         * @details 完整的 Floaty PBF 流程：
         *      刚体布点（动态体布局；静态体只刷新变换缓存）→ 组合位置
         *      → 邻居搜索(1.3h) → inside 标记 → **近边界候选标记**（帧级；子步内
         *      边界探测只对候选执行，深水粒子全程免探测）
         *      → substeps × { 半隐式欧拉积分 → λ/Δp Jacobi 求解（内聚+曲率）
         *        → Δp 应用 + 刚体拟合投影 → 边界钳制（地形/静态/动态刚体，位移不入速度）
         *        → CFL 位移钳制（v = Δx/dt）} → XSPH 黏性 → 速度回代。 */
        void Update(FLOAT_ time);

        /// 最近一次求解得到的密度（与 Particles() 一一对应，用于着色/调试）
        const std::vector<FLOAT_> &Density() const { return mDensity; }
        /// 液体粒子列表
        const std::vector<PhysicsParticle *> &Particles() const { return mParticles; }
        /// 所属物理世界
        PhysicsWorld *World() const { return mWorld; }

        /// 运行时参数（= Floaty 参数组）
        Params param;

        /**
         * @brief 密度 → 颜色（深蓝(稀疏) → 亮蓝白(压缩)） */
        static glm::vec4 ColorByDensity(FLOAT_ density, const Params &param);

    private:
        // ── Floaty 常数（common.ts / lib.rs） ──
        static const int kMaxNeighbors = 64;      ///< 每粒子最大邻居数（Floaty MAX_NEIGHBOR_PARTICLES）
        static const int kHashSize = 370111;      ///< 邻居哈希桶数（Floaty HASH_SIZE）
        static const int kMaxSolidSamples = 4096; ///< 刚体采样点总量上限（防护）

        /**
         * @brief 刚体采样点（Floaty 的 blob 粒子角色）
         * @details 世界坐标 = body->pos + vec2angle(local, body->angle) */
        struct SolidSample
        {
            PhysicsAngle *body; ///< 所属刚体（shape / circle / line 均为 PhysicsAngle）
            Vec2_ local;        ///< 刚体系锚点（未旋转）
        };

        /**
         * @brief 参与密度采样的刚体（仅动态体；静态体不布采样点，见 mStaticBodies）
         * @details 采样区间对应 mSamples；xfIndex 是其变换缓存在 mSolidXf 中的下标
         *      （恒等于本条目在 mSolids 中的下标——RefreshSolidXform 先排动态体） */
        struct SolidTrack
        {
            PhysicsAngle *body = nullptr;
            int begin = 0, end = 0; ///< 采样点在 mSamples 的区间 [begin, end)
            int xfIndex = 0;        ///< 在 mSolidXf 中的下标（= mSolids 下标）
            Vec2_ accumT{0};        ///< 本帧累计平移修正
            FLOAT_ accumR = 0;      ///< 本帧累计转动修正

            SolidTrack() = default;
            SolidTrack(PhysicsAngle *b, int b0, int e0, int xf0)
                : body(b), begin(b0), end(e0), xfIndex(xf0) {}
        };

        /**
         * @brief 子步循环不变量（每 Update 求值一次，各子步阶段共享，R7）
         * @details h/ρ0/核归一化/CFL 上限等在整个 Update 内不随子步变化，
         *      旧版每子步重算属纯冗余。 */
        struct SolverConst
        {
            FLOAT_ h = 0, h2 = 0;         ///< 核半径及其平方
            FLOAT_ poly6Scale = 0;        ///< poly6 核幅度 4/(π·h⁸)
            FLOAT_ spikyScale = 0;        ///< spiky 梯度幅度 10/(π·h⁵)
            FLOAT_ restInv = 0;           ///< 1/ρ0
            FLOAT_ selfRho = 0;           ///< 密度自贡献
            FLOAT_ cflDisp = 0;           ///< cfl·h（边界推出/刚体拟合钳制上限）
            FLOAT_ cflDispVel = 0;       ///< 0.9·cfl·h（速度回代位移上限）
            FLOAT_ boundaryStep = 0;     ///< 边界搜索步长 max(0.05h, 0.02)
            FLOAT_ boundaryJitter = 0;   ///< 深陷逃逸抖动 0.01h
            FLOAT_ gridSpacing = 0;      ///< 邻居格宽 1.3h（候选标记复用同一哈希网格）
            int fastSteps = 2;           ///< 边界近程搜索步数
        };

        /**
         * @brief 动态刚体采样布局缓存条目（R1）
         * @details 局部锚点只取决于刚体几何，按几何指纹跨帧复用；几何变更
         *      （图案/半径）指纹不符自动重建。条目仅以指针作键值比较、悬垂键
         *      从不解引用，地址复用时由指纹校验兜底；连续 64 帧未命中即回收。 */
        struct SolidLayout
        {
            PhysicsAngle *body = nullptr;
            unsigned long long fingerprint = 0; ///< 几何指纹（类型种子 + 图案哈希/半径位型）
            std::vector<Vec2_> locals;         ///< 局部锚点布局
            size_t lastUsed = 0;                ///< 最近命中帧号（清扫用）

            SolidLayout() = default;
            SolidLayout(PhysicsAngle *b, unsigned long long fp, size_t used)
                : body(b), fingerprint(fp), lastUsed(used) {}
        };

        // —— 流程各阶段（对应 Floaty solver.ts / lib.rs 同名函数） ——
        void BuildSolidSamples();               ///< 刚体布采样点（Floaty 的 blob 粒子）
        void SyncPositions();                   ///< 组合位置快照（流体 + 采样点）
        void SyncSamplePositions();             ///< 采样点重新锚定到刚体位姿
        void FindNeighbors(FLOAT_ gridSpacing); ///< 哈希链表邻居搜索（Floaty NeighborLinkedList）
        static int HashCell(int gx, int gy);    ///< 网格坐标 → 哈希桶（Floaty 哈希链表）
        void MarkInside();                      ///< 流体粒子在动态刚体内部标记（Floaty makeIsInsideForFluid）
        void MarkNearSolid();                   ///< 帧级近边界候选标记（R3；子步内边界探测只对候选执行）
        void IntegrateFluid(FLOAT_ dt);         ///< 半隐式欧拉（Floaty simulate 的积分段）
        void SolveFluidJacobi();                ///< λ + Δp（Floaty solve_fluid_jacobi）
        void SolveMapBoundaries();              ///< 地形/静态刚体钳制（Floaty solveFluidBoundaries）
        void ApplyDeltas();                     ///< pos += Δp + 刚体拟合投影（Floaty 约束投影槽位）
        void ProjectSolids();                   ///< Δp 最小二乘刚体拟合 → 位姿（顺带刷新变换缓存）
        void EnforceCFL(FLOAT_ dt);             ///< 位移钳制 + v = Δx/dt（Floaty enforceCFL）
        void ApplyXSPH();                       ///< XSPH 速度平滑黏性（Macklin PBF 可选黏性项）
        void CommitSolids(FLOAT_ time);         ///< 刚体速度回代（修正按帧时间折算）
        void CommitFluid();                     ///< 回写粒子 pos/speed/OldPos/StaticNum
        FLOAT_ SampleWeight(size_t combinedIndex) const; ///< 采样点 w = 1/m_采样（静态体 0）

        // ── 查询/求值缓存（性能优化，数学结果不变） ──

        /**
         * @brief 刚体点包含判定的查询缓存
         * @details 旧实现每次判定都现场构造 AngleMat（一对三角函数）：MarkInside 与
         *      边界投影每帧做数万次判定，三角函数是最大单项浪费。缓存 cos/sin 与
         *      平方半径后：AABB 快筛 + 纯乘加，绝大多数查询一步退出。 */
        struct SolidXf
        {
            PhysicsAngle *body = nullptr;
            PhysicsShape *shape = nullptr; ///< 形状体专用（逐格判定）；其余为空
            Vec2_ pos{0};                  ///< 刚体质心（缓存，随位姿刷新）
            Vec2_ cs{0};                   ///< {cos(angle), sin(angle)}（缓存，随位姿刷新）
            FLOAT_ radius = 0;             ///< AABB 半宽（= 碰撞半径）
            FLOAT_ radius2 = 0;            ///< 圆判定：(0.9r)²（非圆 = 0）
            bool dynamic = false;          ///< 动态刚体（inside 标记 / 几何挤出的对象）
        };

        /// AABB 快筛 + 精确判定（等价旧 BodySolidAt：形状逐格、圆平方距离、线段恒 false）
        static bool SolidContains(const SolidXf &xf, const Vec2_ &p);

        void RefreshSolidXform(); ///< 重建刚体变换缓存（BuildSolidSamples 末尾调用）

        std::vector<SolidXf> mSolidXf; ///< 变换缓存：动态体 [0,mSolids.size()) + 静态体（纯几何钳制）
        std::vector<Vec2_> mPairDir;   ///< 邻居对单位方向 dx/r（λ/Δp 两遍复用，省一遍核求值/开方）
        std::vector<FLOAT_> mPairW;    ///< 邻居对核距 w = h−r（<0 = 无效对）

        PhysicsWorld *mWorld = nullptr;
        std::vector<PhysicsParticle *> mParticles; ///< 液体粒子（拥有者）
        std::vector<FLOAT_> mDensity;              ///< 密度（着色/调试，与 mParticles 对齐）

        // —— 组合工作数组（流体 [0,n) + 采样点 [n,N)） ——
        size_t mFluidCount = 0;
        std::vector<Vec2_> mPos;     ///< 位置（子步工作区）
        std::vector<Vec2_> mVel;     ///< 速度（仅流体使用）
        std::vector<Vec2_> mPrevPos; ///< 子步积分前位置（Floaty prevPos）
        std::vector<Vec2_> mGrad;    ///< −Σ∇W 累计（Floaty grads）
        std::vector<Vec2_> mDelta;   ///< 位置修正（Floaty fluidDelta）
        std::vector<Vec2_> mBoundaryDisp; ///< 本子步边界推出位移（位置级约束，不计入速度）
        std::vector<FLOAT_> mLambda; ///< 约束乘子（Floaty lambdas）
        std::vector<unsigned char> mInside; ///< 在动态刚体内部（Floaty inside）
        /**
         * @brief 帧级"近边界候选"标记（R3，仅流体）
         * @details 帧内位移有严格上界（每子步 ≤ 0.9·cfl·h，子步数 substeps），
         *      故"帧初离一切固体表面 > 漂移量 + 每步位移"的粒子整个 Update 内
         *      不可能触边——该判定每帧只需一次。SolveMapBoundaries 只对本标记
         *      为 1 的粒子跑 PointSolid / 逃逸搜索，深水粒子 6 个子步全部免探测。 */
        std::vector<unsigned char> mNearSolid;

        // —— 邻居（Floaty NeighborLinkedList） ——
        std::vector<int> mNext;                 ///< 哈希链表 next
        std::vector<int> mHashFirst;            ///< 哈希桶头
        std::vector<int> mHashMark;             ///< 哈希桶 epoch 标记
        int mEpoch = 0;                         ///< 当前 epoch
        std::vector<int> mFirst;                ///< 扁平邻居表偏移 [N+1]（Floaty first_neighbors）
        std::vector<int> mNeighbors;            ///< 扁平邻居表 [N * kMaxNeighbors]（Floaty neighbors）

        // —— 刚体双向耦合 ——
        std::vector<SolidSample> mSamples;   ///< 刚体采样点
        std::vector<FLOAT_> mSampleWeight;   ///< 采样点权重 w（与 mSamples 对齐）
        std::vector<SolidTrack> mSolids;     ///< 参与密度采样的刚体（仅动态体，R2-A）
        /**
         * @brief 纯几何钳制体（静态刚体，invMass=0 && mass>0，R2-A）
         * @details 不布采样点、不进 mPos/密度系统，但保留在 mSolidXf 中供
         *      PointSolid 几何挤出（挡水与地形统一走这一条路径）。 */
        std::vector<SolidTrack> mStaticBodies;
    };

}
