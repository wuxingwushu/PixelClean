#pragma once
#include "BaseStruct3D.hpp"
#include "BaseCalculate3D.hpp"
// 接口需要的具体类型：粒子（AddParticle/Particles 按值返回指针）、
// 刚体基类 PhysicsAngle3D（SolidSample/SolidTrack/SolidXf 成员）、
// PhysicsShape3D（SolidContains 调用 shape->InBounds/GridAt，需完整定义）、
// PhysicsLine3D（BuildSolidSamples 区分线段体）。
#include "PhysicsParticle3D.hpp"
#include "PhysicsShape3D.hpp"
#include "PhysicsLine3D.hpp"
#include <vector>
#include <algorithm>
#include <cmath>

// 3D 液体子系统：PBF（Position Based Fluids）
//
// 完整移植自 PixelClean/PhysicsBlock/PhysicsLiquid.{hpp,cpp}（2D PBF），
// 维度改写要点：
//   · Vec2_ → Vec3_；2D 标量角 → 四元数 Quat_（PhysicsAngle3D::rotation）；
//   · poly6 核幅度 4/(π·h⁸)    → 315/(64·π·h⁹)
//     spiky 梯度幅度 10/(π·h⁵) → 45/(π·h⁶)      （3D 归一化常数）
//   · 邻居邻域 3×3 → 3×3×3；kMaxNeighbors 64 → 160（3D 球内 ≈60.6 个点阵粒子，
//     2D 圆内仅 ≈18.7；160 ≈ 2.6×，留液面/压缩区余量）
//   · restDensity 标定：3D 点阵求和（7×7×7、间距 1.6r、d² < h²，跳过 (0,0,0)）；
//     r=0.15 时精确值 71.9514618（2D 的 16.66667 只对二维体积元成立，不可沿用）
//   · 标定幂次：eps ∝ k⁻²、cohesion ∝ k¹ 与 2D 相同；curvature ∝ k^(d+2)，
//     2D 的 k⁴ 在 3D 应为 **k⁵**（核量纲推导见 SyncFromParticleRadius 文档注释）
//   · 矩形填充 → 立方体填充（AddGrid 三轴）
//   · 刚体拟合：2D 标量叉积 → L = Σ r×d 与 3×3 求逆（SymmetricInverse）
//
// ✅ 阶段 A + 阶段 B 均已完成：
//   阶段 A —— 类接口、参数标定（3D 点阵 restDensity）、粒子/生命周期管理、着色、
//             采样点权重、刚体包含判定与变换缓存；
//   阶段 B —— 14 个重求解阶段的函数体（逐式移植自 2D 参考 PhysicsLiquid.cpp，
//             维度改写要点见 PhysicsLiquid3D.cpp 各阶段上方注释）。
//   阶段 B 唯一新增成员：`SolverConst mConst`（Update 开头求值一次，替代 2D
//   在各函数内重算 param.*；数学等价）。
//
// 宿主契约（PhysicsWorld3D，见 PhysicsWorld3D.hpp:197-201）：
//   #if PhysicsLiquid3DBool3D（BaseDefine3D.h:56 默认 0）
//     mLiquid->Update(time);                    // 后处理阶段
//     mLiquid->NotifyParticleRemoved(particle);  // RemoveObject 转发
//     delete mLiquid;                            // 世界析构
//     SetLiquid() / GetLiquid()

namespace PhysicsBlock3D
{
    class PhysicsWorld3D;

    // 本头文件独立可编译所需的类型：粒子（mParticles）、刚体基类
    // PhysicsAngle3D（SolidSample/SolidTrack/SolidXf）、PhysicsShape3D
    // （SolidContains 的逐体素判定）。三者由下列引擎头提供，按依赖顺序包含。

    /**
     * @brief 基于位置的流体 Position Based Fluids (PBF)，3D 版
     * @details 算法与参数完整移植自参考项目 Floaty-main（Rust/TypeScript），
     *          与 2D 版本 PhysicsLiquid 同源；详见 2D 头文件的长注释。
     *
     *          固液双向耦合：动态刚体（shape/circle/line）按流体粒子间距布采样点，
     *          加入同一 PBF 邻居系统（对应 Floaty 的软体 blob 粒子）；Δp 经最小二乘
     *          刚体拟合投影回刚体，浮力由压力梯度自然涌现。静态刚体（invMass=0 &&
     *          mass>0）不布采样点，只作纯几何钳制体留在 mSolidXf 中。
     *
     *          地形（MapFormwork3D）对应 Floaty 的容器边界：积分后把落入实体体素 /
     *          静态刚体内部的粒子钳到最近空位。
     */
    class PhysicsLiquid3D
    {
    public:
        /**
         * @brief 液体参数（= Floaty SimulationParams + 几何常数 + 表面张力常数） */
        struct Params
        {
            // —— 几何尺度（Floaty main.ts：particleRadius=0.15 → kernelRadius=3r、initSpacing=0.8·2r）——
            FLOAT_ particleRadius = 0.15f;          ///< 粒子半径 r（Floaty: 0.15）
            FLOAT_ kernelRadius = FLOAT_(0.45);     ///< 核半径 h（Floaty: 3.0*particleRadius = 0.45）
            // 静息密度 3D 默认值 = r=0.15（s=1.6r）三维点阵的**精确**核密度，由下面的
            // SyncFromParticleRadius 用同一 7×7×7 求和得到（实测 71.9514618）。
            // ⚠ 不再是 2D 的 16.66667：2D 值只对二维体积元成立，3D 沿用会让 C=ρ/ρ0−1≈3.3
            //   直接爆开。任何未走 AddGrid 的场景都应显式调用 SyncFromParticleRadius()。
            FLOAT_ restDensity = FLOAT_(71.9514618); ///< 静息密度 ρ0（SyncFromParticleRadius 按 3D 点阵精确求值）

            // —— SimulationParams（Floaty main.ts 默认值）——
            // 子步步长才是稳定性关键：Floaty 用 60Hz×10 = 1/600；本引擎 100Hz 固定步
            // → 等价子步数 6（同 1/600 子步步长，还省 40% 开销）。
            int substeps = 6;                       ///< 每次 Update 的子步数
            FLOAT_ eps = FLOAT_(50);                ///< λ 松弛项（Floaty: 50，按尺度不变性重标定）
            FLOAT_ cfl = FLOAT_(0.12);              ///< 位移 CFL 系数：maxDisp = cfl·h

            // —— 表面张力（Floaty src/lib.rs solve_fluid_jacobi 内联常数）——
            // · 内聚：把液体"黏连成团"的力（史莱姆/胶质感来源）。水预设取 25%。
            // · 曲率：梯度差分的平滑/扩散项（抗毛刺），无黏连力。
            FLOAT_ cohesion = FLOAT_(0.000025);     ///< 内聚（水预设 = Floaty 0.01²×25%）
            FLOAT_ curvature = FLOAT_(0.00002);     ///< 曲率系数（Floaty 硬编码 0.001*0.02）
            bool surfaceTension = true;             ///< 表面张力项总开关

            // —— 水的黏性（XSPH 速度平滑，Macklin PBF 可选黏性项）——
            // 帧级黏性（每次 Update 平滑一次）；水≈0.05~0.15，>0.3 开始变糖浆。
            FLOAT_ viscosity = FLOAT_(0.1);         ///< XSPH 系数（帧级）

            /**
             * @brief 默认构造（空体即可，必须由用户写出：
             *        构造函数默认实参 `const Params &P = {}` 位于类体内部，
             *        嵌套类的默认成员初始化器在外层类结束前不可见） */
            Params() {}

            /**
             * @brief 按 Floaty 公式由 particleRadius 重算派生参数
             * @details kernelRadius = 3r；restDensity = 静息间距(1.6r) **三维**点阵
             *          （7×7×7，跳过 (0,0,0)，d² < h²）的实际核密度（自贡献 + Σ 邻居
             *          poly6），消除 Floaty 近似公式引起的"出生喷泉"；
             *
             *          —— 标定参数的 k 次幂（k = r/0.15，由核量纲推导）——
             *          要求整体尺度缩放 k 倍时轨迹同步缩放 k 倍，即 Δp 的每一项量纲
             *          必须 = L。记 ρ0 量纲 L^(−d)、∇W 量纲 L^(−d−1)（d = 2/3）：
             *          · |∇C|²：∇C = ∇W/ρ0 量纲 L^(−1)（**与 d 无关**）⇒ eps 量纲 L^(−2)
             *            ⇒ **eps ∝ k^(−2)（3D 与 2D 相同，不变）**；
             *          · 内聚项 u·cohesion：u 无量纲 ⇒ **[cohesion] = L ⇒ ∝ k^(1)（不变）**；
             *          · 曲率项 (∇C_i−∇C_j)·[spikyScale·w³]·curvature：∇C 量纲 L^(−1)，
             *            spikyScale 是**梯度**幅度 L^(−(d+3))、w³ 量纲 L³ ⇒ 方括号量纲
             *            L^(−d)，故 [curvature] = L^(1+1)·L^(+d) = **L^(d+2)**。
             *            2D：d+2 = 4，与 2D 代码的 k⁴ **精确吻合**（交叉验证成立）；
             *            **3D：d+2 = 5 ⇒ curvature ∝ k⁵（相对 2D 的指数 4 改变）**。
             *
             *          基值（50 / 0.000025 / 0.00002）是 Floaty 在参考尺度 r0=0.15 上的
             *          经验锚点，不动；故 k=1 时三个参数与 2D 完全一致。 */
            void SyncFromParticleRadius();

            /**
             * @brief 非法值清洗（h/ρ0 ≤ 0 会让 1/h、1/ρ0 发散；substeps/eps/cfl 取合法域） */
            void Sanitize();
        };

        /**
         * @brief 构造液体
         * @param World 物理世界（必须已 SetMapFormwork；粒子通过 World->AddObject 注册）
         * @param P     初始参数（Floaty 默认值） */
        PhysicsLiquid3D(PhysicsWorld3D *World, const Params &P = {});
        ~PhysicsLiquid3D();

        /**
         * @brief 添加单个液体粒子（注册到物理世界）
         * @param pos      初始位置（世界坐标）
         * @param mass     质量（引擎记账用；PBF 求解按 Floaty 单位质量公式）
         * @param friction 摩擦因数 */
        PhysicsParticle3D *AddParticle(Vec3_ pos, FLOAT_ mass = 1.0f, FLOAT_ friction = 0.2f);

        /**
         * @brief 生成一片立方体网格状的液体
         * @param center  中心位置（世界坐标）
         * @param numX    X 向粒子数
         * @param numY    Y 向粒子数
         * @param numZ    Z 向粒子数
         * @param spacing 粒子间距（Floaty 建议 initSpacing = 0.8·粒径 = 1.6·r）
         * @param mass    单粒子质量
         * @param friction 摩擦因数
         * @details 2D 矩形填充（AddGrid(numX,numY)）的三维版：由间距反推粒子尺度
         *      r = spacing/1.6 并按 Floaty 公式同步 kernelRadius / restDensity。
         *      参数同步只在液体为空（首次填充）时自动执行（R15）。 */
        void AddGrid(Vec3_ center, int numX, int numY, int numZ, FLOAT_ spacing,
                     FLOAT_ mass = 1.0f, FLOAT_ friction = 0.2f);

        /**
         * @brief 清空液体：从物理世界移除并删除所有液体粒子
         * @note 析构函数也会调用本函数，因此手动 delete 液体不会留下孤儿粒子 */
        void Clear();

        /**
         * @brief 通知液体：某粒子即将被物理世界删除
         * @param p 待删除的粒子（必须已在本液体内，否则静默忽略）
         * @details 由 PhysicsWorld3D::RemoveObject(PhysicsParticle3D*) 转发。 */
        void NotifyParticleRemoved(PhysicsParticle3D *p);

        /**
         * @brief 每物理步执行液体更新（PhysicsWorld3D 后处理阶段自动调用）
         * @param time 时间步长（秒，= Floaty 的 dtFrame）
         * @details 完整流程（Floaty solver.ts PBD.simulate）：
         *      刚体布点（动态体布局；静态体只刷新变换缓存）→ 组合位置快照
         *      → 邻居搜索(1.3h) → inside 标记 → 近边界候选标记（帧级）
         *      → substeps × { 半隐式欧拉积分 → λ/Δp Jacobi 求解（内聚+曲率）
         *        → Δp 应用 + 刚体拟合投影 → 边界钳制（地形/静态/动态刚体）
         *        → CFL 位移钳制（v = Δx/dt）} → XSPH 黏性 → 速度回代。 */
        void Update(FLOAT_ time);

        /// 最近一次求解得到的密度（与 Particles() 一一对应，用于着色/调试）
        const std::vector<FLOAT_> &Density() const { return mDensity; }
        /// 液体粒子列表
        const std::vector<PhysicsParticle3D *> &Particles() const { return mParticles; }
        /// 所属物理世界
        PhysicsWorld3D *World() const { return mWorld; }

        /// 运行时参数（= Floaty 参数组）
        Params param;

        /**
         * @brief 密度 → 颜色（深蓝(稀疏) → 饱和水蓝(静息) → 亮青白(压缩/泡沫)） */
        static Vec4_ ColorByDensity(FLOAT_ density, const Params &param);

    private:
        // ── Floaty 常数（common.ts / lib.rs；2D 版 PhysicsLiquid.hpp:182-184） ──
        // kMaxNeighbors 2D 64 → 3D 160：邻域半径 1.3h = 2.4375·s（s 为点阵间距）的**球**
        // 内含 (4/3)π(2.4375)³ ≈ 60.6 个点阵粒子（2D 圆内仅 ≈18.7），64 会截断邻居表；
        // 160 ≈ 2.6×60.6，给液面/压缩区留余量。
        static const int kMaxNeighbors = 160;
        // kHashSize 保持 370111：哈希桶只作链表头，桶数与**正确性无关**（只影响探测长度），
        // 桶数远大于典型粒子数即可，无需随维度放大。
        static const int kHashSize = 370111;
        // kMaxSolidSamples 2D 4096 → 3D 16384：2D 每实体体素采样 per² = 4² = 16 点
        // （per = round(1/spacing)，r=0.15 时 spacing=0.24 ⇒ per=4），4096 恰好覆盖
        // 256 个实体体素；3D 每体素 per³ = 64 点（**4×** 的 2D 采样密度），同样覆盖
        // 256 个体素需要 16384。**不**按"16³ 体素全覆盖"放大（那要 262144）：邻居对
        // 缓存是 N×kMaxNeighbors 的扁平表（mNeighbors 4B + mPairDir 12B + mPairW 4B
        // = 20B/项），262144×160×20B ≈ 800MB 不可行；16384 时最坏 ≈55MB，是可接受折中。
        // 超限时与 2D 一样由 PushSample 优雅截断（不崩、不越界）。
        static const int kMaxSolidSamples = 16384;

        // ── 3D 核常数（仅此一处；标定与求解共用） ──
        /// poly6 核幅度 315/(64·π·h⁹)（2D 是 4/(π·h⁸)）
        static FLOAT_ Poly6Scale(FLOAT_ h);
        /// spiky 梯度幅度 45/(π·h⁶)（2D 是 10/(π·h⁵)）
        static FLOAT_ SpikyScale(FLOAT_ h);
        /// poly6 核权重 (h²−r²)³（自贡献时 r2 取 0）
        static FLOAT_ Poly6Weight(FLOAT_ h2, FLOAT_ r2);

        /**
         * @brief 刚体采样点（Floaty 的 blob 粒子角色）
         * @details 世界坐标 = body->pos + (body->rotation * local) */
        struct SolidSample
        {
            PhysicsAngle3D *body; ///< 所属刚体（shape / circle / line 均为 PhysicsAngle3D）
            Vec3_ local;          ///< 刚体系锚点（未旋转）
        };

        /**
         * @brief 参与密度采样的刚体（仅动态体；静态体不布采样点，见 mStaticBodies）
         * @details 采样区间对应 mSamples；xfIndex 是其变换缓存在 mSolidXf 中的下标
         *      （恒等于本条目在 mSolids 中的下标——RefreshSolidXform 先排动态体） */
        struct SolidTrack
        {
            PhysicsAngle3D *body = nullptr;
            int begin = 0, end = 0; ///< 采样点在 mSamples 的区间 [begin, end)
            int xfIndex = 0;        ///< 在 mSolidXf 中的下标（= mSolids 下标）
            Vec3_ accumT{0};        ///< 本帧累计平移修正
            Vec3_ accumR{0};        ///< 本帧累计转动修正（3D 是轴-角形式向量）

            SolidTrack() = default;
            SolidTrack(PhysicsAngle3D *b, int b0, int e0, int xf0)
                : body(b), begin(b0), end(e0), xfIndex(xf0) {}
        };

        /**
         * @brief 子步循环不变量（每 Update 求值一次，各子步阶段共享）
         * @details h/ρ0/核归一化/CFL 上限等在整个 Update 内不随子步变化。 */
        struct SolverConst
        {
            FLOAT_ h = 0, h2 = 0;         ///< 核半径及其平方
            FLOAT_ poly6Scale = 0;        ///< poly6 核幅度 315/(64π·h⁹)
            FLOAT_ spikyScale = 0;        ///< spiky 梯度幅度 45/(π·h⁶)
            FLOAT_ restInv = 0;           ///< 1/ρ0
            FLOAT_ selfRho = 0;           ///< 密度自贡献
            FLOAT_ cflDisp = 0;           ///< cfl·h（边界推出/刚体拟合钳制上限）
            FLOAT_ cflDispVel = 0;        ///< 0.9·cfl·h（速度回代位移上限）
            FLOAT_ boundaryStep = 0;      ///< 边界搜索步长 max(0.05h, 0.02)
            FLOAT_ boundaryJitter = 0;    ///< 深陷逃逸抖动 0.01h
            FLOAT_ gridSpacing = 0;       ///< 邻居格宽 1.3h（候选标记复用同一哈希网格）
            int fastSteps = 2;            ///< 边界近程搜索步数
        };

        /**
         * @brief 动态刚体采样布局缓存条目（R1）
         * @details 局部锚点只取决于刚体几何，按几何指纹跨帧复用；几何变更
         *      （图案/半径）指纹不符自动重建。条目仅以指针作键值比较、悬垂键
         *      从不解引用，地址复用时由指纹校验兜底；连续 64 帧未命中即回收。 */
        struct SolidLayout
        {
            PhysicsAngle3D *body = nullptr;
            unsigned long long fingerprint = 0; ///< 几何指纹（类型种子 + 图案哈希/半径位型）
            std::vector<Vec3_> locals;          ///< 局部锚点布局
            size_t lastUsed = 0;                ///< 最近命中帧号（清扫用）

            SolidLayout() = default;
            SolidLayout(PhysicsAngle3D *b, unsigned long long fp, size_t used)
                : body(b), fingerprint(fp), lastUsed(used) {}
        };

        /**
         * @brief 子步循环不变量实例（阶段 B）
         * @details Update 开头求值一次（param.Sanitize() 之后、BuildSolidSamples 之前），
         *      整个 Update 内各子步阶段共享，避免每子步重复求核归一化/CFL 上限。
         *      数学上与 2D 版在各函数内重算 `param.*` 完全等价。 */
        SolverConst mConst{};

        // —— 流程各阶段（对应 Floaty solver.ts / lib.rs 同名函数） ——
        void BuildSolidSamples();               ///< 刚体布采样点（Floaty 的 blob 粒子）
        void SyncPositions();                   ///< 组合位置快照（流体 + 采样点）
        void SyncSamplePositions();             ///< 采样点重新锚定到刚体位姿
        void FindNeighbors(FLOAT_ gridSpacing); ///< 哈希链表邻居搜索（Floaty NeighborLinkedList）
        static int HashCell(int gx, int gy, int gz); ///< 网格坐标 → 哈希桶（3D 三轴）
        void MarkInside();                      ///< 流体粒子在动态刚体内部标记
        void MarkNearSolid();                   ///< 帧级近边界候选标记（R3）
        void IntegrateFluid(FLOAT_ dt);         ///< 半隐式欧拉（Floaty simulate 的积分段）
        void SolveFluidJacobi();                ///< λ + Δp（Floaty solve_fluid_jacobi）
        void SolveMapBoundaries();              ///< 地形/静态刚体钳制（Floaty solveFluidBoundaries）
        void ApplyDeltas();                     ///< pos += Δp + 刚体拟合投影
        void ProjectSolids();                   ///< Δp 最小二乘刚体拟合 → 位姿
        void EnforceCFL(FLOAT_ dt);             ///< 位移钳制 + v = Δx/dt（Floaty enforceCFL）
        void ApplyXSPH();                       ///< XSPH 速度平滑黏性
        void CommitSolids(FLOAT_ time);         ///< 刚体速度回代（修正按帧时间折算）
        void CommitFluid();                     ///< 回写粒子 pos/speed/OldPos/StaticNum
        FLOAT_ SampleWeight(size_t combinedIndex) const; ///< 采样点 w = 1/m_采样（静态体 0）

        // ── 查询/求值缓存（性能优化，数学结果不变） ──

        /**
         * @brief 刚体点包含判定的查询缓存
         * @details 2D 版缓存 {cos,sin}；3D 直接缓存朝向四元数，判定时做一次
         *      `inverse(q) * (p - pos)`（四元数乘向量，无三角函数）。
         *      AABB 快筛 + 纯乘加，绝大多数查询一步退出。 */
        struct SolidXf
        {
            PhysicsAngle3D *body = nullptr;
            PhysicsShape3D *shape = nullptr; ///< 形状体专用（逐体素判定）；其余为空
            Vec3_ pos{0};                   ///< 刚体质心（缓存，随位姿刷新）
            Quat_ rotation{1.0f, 0.0f, 0.0f, 0.0f}; ///< 朝向四元数（缓存，随位姿刷新）
            FLOAT_ radius = 0;              ///< AABB 半宽（= 碰撞半径）
            FLOAT_ radius2 = 0;             ///< 球判定：(0.9r)²（非球 = 0）
            bool dynamic = false;           ///< 动态刚体（inside 标记 / 几何挤出的对象）
        };

        /// AABB 快筛 + 精确判定（形状逐体素、球平方距离、线段恒 false）
        static bool SolidContains(const SolidXf &xf, const Vec3_ &p);

        void RefreshSolidXform(); ///< 重建刚体变换缓存（BuildSolidSamples 末尾调用）

        std::vector<SolidXf> mSolidXf; ///< 变换缓存：动态体 [0,mSolids.size()) + 静态体
        std::vector<Vec3_> mPairDir;   ///< 邻居对单位方向 dx/r（λ/Δp 两遍复用）
        std::vector<FLOAT_> mPairW;    ///< 邻居对核距 w = h−r（<0 = 无效对）

        PhysicsWorld3D *mWorld = nullptr;
        std::vector<PhysicsParticle3D *> mParticles; ///< 液体粒子（拥有者）
        std::vector<FLOAT_> mDensity;                ///< 密度（着色/调试，与 mParticles 对齐）

        // —— 组合工作数组（流体 [0,n) + 采样点 [n,N)） ——
        size_t mFluidCount = 0;
        std::vector<Vec3_> mPos;     ///< 位置（子步工作区）
        std::vector<Vec3_> mVel;     ///< 速度（仅流体使用）
        std::vector<Vec3_> mPrevPos; ///< 子步积分前位置（Floaty prevPos）
        std::vector<Vec3_> mGrad;    ///< −Σ∇W 累计（Floaty grads）
        std::vector<Vec3_> mDelta;   ///< 位置修正（Floaty fluidDelta）
        std::vector<Vec3_> mBoundaryDisp; ///< 本子步边界推出位移（位置级约束，不计入速度）
        std::vector<FLOAT_> mLambda; ///< 约束乘子（Floaty lambdas）
        std::vector<unsigned char> mInside; ///< 在动态刚体内部（Floaty inside）
        /**
         * @brief 帧级"近边界候选"标记（R3，仅流体）
         * @details 帧内位移有严格上界（每子步 ≤ 0.9·cfl·h，子步数 substeps），
         *      故"帧初离一切固体表面 > 漂移量 + 每步位移"的粒子整个 Update 内
         *      不可能触边——该判定每帧只需一次。SolveMapBoundaries 只对本标记
         *      为 1 的粒子跑边界探测，深水粒子 6 个子步全部免探测。 */
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
         *      几何挤出（挡水与地形统一走这一条路径）。 */
        std::vector<SolidTrack> mStaticBodies;
    };

}

// 写在命名空间之外、类定义之后：避免 PhysicsWorld3D.hpp 反向引用本类造成循环包含
#include "PhysicsWorld3D.hpp"
