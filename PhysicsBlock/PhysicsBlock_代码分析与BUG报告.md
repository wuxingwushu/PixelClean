# PhysicsBlock 物理引擎 —— 代码分析与 BUG 报告

> 分析范围：`/workspace/PhysicsBlock/` 全部 48 个源文件（基础层 / 网格地图层 / 物理对象层 / 碰撞检测层 / 约束求解层 / 世界与外围系统）。
> 报告内容：引擎工作原理综述 → BUG 与算法问题逐条分析（含定位、成因、后果、修正方案）→ 设计层面的改进建议。

---

## 目录

1. [引擎总体架构与工作原理](#1-引擎总体架构与工作原理)
2. [每帧物理流水线详解](#2-每帧物理流水线详解)
3. [BUG 与问题总览表](#3-bug-与问题总览表)
4. [P0 级问题（严重：内存越界 / 未定义行为 / 数据竞争）](#4-p0-级问题严重内存越界--未定义行为--数据竞争)
5. [P1 级问题（功能缺陷 / 数值错误）](#5-p1-级问题功能缺陷--数值错误)
6. [P2 级问题（隐患 / 可维护性 / 性能）](#6-p2-级问题隐患--可维护性--性能)
7. [经核实为"非 BUG"的易误解点](#7-经核实为非-bug-的易误解点)
8. [修复优先级与验证建议](#8-修复优先级与验证建议)

---

## 1. 引擎总体架构与工作原理

PhysicsBlock 是一个 **基于像素网格（Pixel Grid）的 2D 刚体物理引擎**，与 Box2D 等"解析几何"引擎不同，它的形状（`PhysicsShape`）由像素网格定义，碰撞检测大量使用 **Bresenham 直线射线检测** 在网格上完成，地形（`MapStatic` / `MapDynamic`）本身就是一张碰撞网格。

### 1.1 分层结构

```
┌─────────────────────────────────────────────────────────┐
│ 世界与外围层  PhysicsWorld / PhysicsGPU / PhysicsCollision │
│              PhysicsTrigger / PhysicsKinematic / Assembly │
├─────────────────────────────────────────────────────────┤
│ 约束求解层    PhysicsJoint（关节-旋转约束）                │
│              PhysicsJunction（绳/弹簧/杠杆，4 种连接）     │
│              PhysicsBaseArbiter*（接触约束-序贯冲量求解）  │
├─────────────────────────────────────────────────────────┤
│ 碰撞检测层    GridSearch（四叉网格+莫顿码 粗检测）          │
│              PhysicsBaseCollide（Bresenham 精检测）        │
│              PhysicsArbiter*（12 种组合的仲裁器）          │
├─────────────────────────────────────────────────────────┤
│ 物理对象层    PhysicsFormwork（接口）→ Particle → Angle    │
│              → Shape / Circle / Line / Kinematic          │
├─────────────────────────────────────────────────────────┤
│ 网格与地图层  BaseGrid / BaseOutline / MapStatic          │
│              MapDynamic + MovePlate（无限滚动地形）        │
├─────────────────────────────────────────────────────────┤
│ 基础层        BaseDefine / BaseStruct / BaseCalculate     │
│              BaseSerialization / MemoryPool / ThreadPool  │
└─────────────────────────────────────────────────────────┘
```

### 1.2 对象模型与继承链

```
PhysicsFormwork (接口: 位置/质量/摩擦/类型/冲量虚函数)
 └── PhysicsParticle        （质点: pos/speed/mass/StaticNum 休眠计数）
      └── PhysicsAngle      （刚体基类: angle/angleSpeed/torque/MomentInertia）
           ├── PhysicsCircle（圆, I = 0.5·m·r²）
           ├── PhysicsLine  （线段, I = L²·m/12）
           └── PhysicsShape （像素网格形状, 多继承 BaseOutline, I = Σ mᵢ·rᵢ²）
```

- 静止物体用 `mass == FLOAT_MAX`（invMass=0）表示；休眠用 `StaticNum > 10` 表示。
- `PhysicsShape` 的质量/转动惯量由网格逐像素累加：`I = Σ mᵢ·(xᵢ²+yᵢ²)`（平行轴定理）。

### 1.3 碰撞对管理（核心数据结构）

- `CollideGroupS`：`unordered_map<ArbiterKey, CollideGroupEntry>`，`ArbiterKey` 内部**按指针大小排序**保证 (A,B) 与 (B,A) 是同一个键。
- `CollideGroupVector`：线性数组，配合 entry 中的 `vectorIndex` 实现 O(1) 的 swap-and-pop 删除。
- `Arbiter` 对象从**每线程独立的 MemoryPool** 分配（无锁），碰撞检测结果先写入 per-thread 的 `CollideOutput` 缓冲，帧首统一 `ResolveCollideGroup()` 合并——这是很好的无锁设计。

### 1.4 约束求解模型

接触约束采用 **Box2D 风格的序贯冲量（Sequential Impulse）+ 热启动（Warm Starting）+ 偏移修正（Baumgarte bias）**：

```cpp
// PhysicsBaseArbiterAA::ApplyImpulse（每接触点、每迭代）
dv   = v2 + ω2×r2 − v1 − ω1×r1;          // 接触点相对速度
dPn  = massNormal * (−vn + bias);          // 法向冲量增量
Pn   = max(Pn0 + dPn, 0);                  // 累积冲量钳位（不可拉伸约束）
dPt  = massTangent * (−vt);
Pt   = Clamp(Pt0 + dPt, −μ·Pn, μ·Pn);      // 摩擦锥钳位
```

迭代次数 `ApplyImpulseSize` 随物体数自适应：`sqrt(ObjectSize/5)`，下限为 `PhysicsApplyImpulseSize`。

### 1.5 多线程与 GPU

- 碰撞检测（XT_Fun）与位置积分（PhysicsPosXT_Fun）多线程分区执行（`ThreadTaskAllot` 宏均分区间，已验证宏数学正确）。
- 冲量迭代有 CPU 多线程版（**默认启用，存在数据竞争，见 BUG-2**）与 Vulkan compute shader 版（`PhysicsGPU`，打包 body/arbiter/joint/junction 缓冲到 GPU 求解后回写）。

### 1.6 动态地图（无限滚动）

`MapDynamic` 将碰撞网格划分为 `width×height` 个板块（`BaseGrid`），由模板类 `MovePlate` 管理板块环形移动；板块内存布局用 **莫顿码（Z-order）** 重排以提高缓存局部性；焦点移动超过 `mGridRebuildThreshold` 时重建 `GridSearch` 网格偏移。

---

## 2. 每帧物理流水线详解

`PhysicsWorld::PhysicsEmulator(time)`（PhysicsWorld.cpp:318）按以下顺序执行：

| 阶段 | 内容 | 线程 | 耗时统计 |
|---|---|---|---|
| 1. 同步 | 等待上一帧异步碰撞检测任务结束，合并 per-thread 碰撞输出 | 主线程 | mCollisionDetectionTimeMS |
| 2. PreStep | 为每个 arbiter/joint/junction 计算有效质量、bias，并施加热启动冲量 | 单线程（多线程版已被 `& 0` 禁用） | mPreStepTimeMS |
| 3. ApplyImpulse | 序贯冲量迭代 `ApplyImpulseSize` 次（CPU 多线程或 GPU） | 多线程/GPU | mApplyImpulseCPUTimeMS |
| 4. 积分 | `PhysicsPos`（位置+休眠计数）→ `PhysicsSpeed`（重力/风力/阻力）| 多线程（按物体分区，安全） | mPositionUpdateTimeMS |
| 5. 后处理 | 运动学物体、碰撞回调（延迟到帧末统一分发）、触发器检测 | 主线程 | mPostProcessTimeMS |
| 6. 网格更新 | GridSearch 重建/物体网格归属更新 | 多线程 | — |
| 7. 异步碰撞检测 | 启动下一帧的 XT_Fun（AABB 粗检+Bresenham 精检），**不等待完成** | 线程池 | — |

粗检测流程：`GridSearch::Get(pos, radius)` 按物体半径选层查询 → `CollideAABB` 包围盒过滤 → 对应 `Arbiter(X, Y)` 精检。AABB 不再重叠时通过 `Map_Delete` 宏登记删除旧碰撞对；双方均休眠（`StaticNum>10`）时直接跳过（注意：见问题 P2-4）。

地形碰撞特殊处理：形状与粒子碰撞时关闭粒子的旧位置更新（`OldPosUpDataBool=false`），因为形状可能旋转，粒子的"旧位置"参考系失效。

---

## 3. BUG 与问题总览表

| 编号 | 严重度 | 位置 | 问题摘要 |
|---|---|---|---|
| BUG-1 | **P0** | PhysicsBaseCollide.cpp 多处 | 地形类 Collide 接触点数量无上限 → `contacts[PhysicsContactMaxSize]` 数组越界写 |
| BUG-2 | **P0** | PhysicsWorld.cpp:611-647 | 冲量迭代多线程版（默认启用）对不同 arbiter 共享的物体并发读写速度 → 数据竞争（C++ UB） |
| BUG-3 | **P0** | MapDynamic.cpp 析构 | `new[]` / `delete`（非 `delete[]`）不匹配 ×2 → UB |
| BUG-4 | **P0** | PhysicsShape.cpp:94,150 | `mass==0` 或 `MomentInertia==0` 时 `invMass=1/0=inf`，NaN 传播 |
| BUG-5 | P1 | BaseCalculate.cpp:17-38 | `q_sqrt` 实际实现是**快速倒数平方根**（Quake III 1/√x），命名与注释全部错误 |
| BUG-6 | P1 | PhysicsWorld.cpp:1569 | `GetWorldEnergy()` 遗漏 `PhysicsLineS` 的动能 |
| BUG-7 | P1 | PhysicsWorld.cpp:396,440 等 | shape-shape / circle-circle 去重条件混入 `radius` 比较 → 同一对物体重复精检，且可能两线程并发 `Update` 同一 arbiter |
| BUG-8 | P1 | EnergyConservation.hpp:85-222 | 形状间"动能守恒"算法物理不成立（动量一般不守恒）；且**全库无调用**（死代码）。粒子版依赖"双重符号抵消"才正确，极脆弱 |
| BUG-9 | P1 | PhysicsBaseCollide.cpp:483-487 | 圆心恰好与地形轮廓点重合（L=0）→ 除零 → NaN 法线 |
| BUG-10 | P2 | PhysicsWorld.cpp:686-708 | 积分顺序 `PhysicsPos→PhysicsSpeed` 为显式欧拉，重力延迟一帧，稳定性差于半隐式 |
| BUG-11 | P2 | BaseArbiter.cpp:76-96 | `ArbiterKey::operator<` 实际语义是 `>`（逆序比较器） |
| BUG-12 | P2 | PhysicsWorld.cpp:351,365 等 | 双方休眠跳过检测时不删除旧 arbiter → 陈旧接触持续参与求解 |
| BUG-13 | P2 | PhysicsGPU.cpp:139-200 | 每帧 `std::async` 按硬件线程数 spawn 线程做打包；GPU/CPU 双路径数值一致性风险 |
| BUG-14 | P2 | PhysicsWorld.cpp:817-1046 | `PhysicsInformationUpdate` 与 `PhysicsEmulator` 约 230 行碰撞检测代码复制粘贴 |
| BUG-15 | P2 | PhysicsWorld.hpp:343-356 | 迭代次数只随物体增加而升，`RemoveObject` 只减 `ObjectSize` 不回调 `ApplyImpulseSize` |

---

## 4. P0 级问题（严重：内存越界 / 未定义行为 / 数据竞争）

### BUG-1 地形碰撞检测接触点无上限，数组越界写 ⚠️ 最高优先级

**位置**：[PhysicsBaseCollide.cpp](PhysicsBaseCollide.cpp)
- `Collide(Contact*, PhysicsShape*, MapFormwork*)` 第二轮（约 337-365 行）
- `Collide(Contact*, PhysicsCircle*, MapFormwork*)` 两轮（约 440-461、476-493 行）
- 其余地形式（Line/Particle vs Map）同样模式

**成因**：物体-物体形式的 `Collide(ShapeA, ShapeB)` 在循环条件里有 `ContactSize < PhysicsContactMaxSize` 保护（174、206 行），但**物体-地形形式全部没有**：

```cpp
// 337 行：遍历地形轮廓点，无上限保护
for (size_t i = 0; i < Outline.size(); ++i)
{
    ...
    contacts->w_side = ContactSize;
    ++contacts;          // contacts 是 BaseArbiter::contacts[PhysicsContactMaxSize]
    ++ContactSize;       // 可以无限增长！
}
```

**后果**：`BaseArbiter::contacts` 是定长数组（`PhysicsContactMaxSize`）。一个较大/较扁的形状或大圆骑在锯齿地形上时，包围盒内的地形轮廓点很容易超过上限，直接越界写坏 `BaseArbiter` 对象之后的内存（对象池中相邻 arbiter、`numContacts`、`key` 等被破坏），引发随机崩溃或碰撞对错乱——且因内存池复用，症状极难复现。

**修正**（所有地形式 Collide 的每个写入循环统一加边界）：

```cpp
for (size_t i = 0; i < Outline.size() && ContactSize < PhysicsContactMaxSize; ++i)
{
    ...
}
```

同时在 `Update()`（PhysicsBaseArbiter.cpp:192-213 等）加防御性钳位：

```cpp
void PhysicsBaseArbiterAD::Update(Contact *NewContacts, int numNewContacts)
{
    if (numNewContacts > PhysicsContactMaxSize)
        numNewContacts = PhysicsContactMaxSize;   // 防御：钳位防止越界拷贝
    ...
}
```

建议顺手做一次全局 grep：所有 `++contacts` 的循环必须以 `ContactSize < PhysicsContactMaxSize` 为循环条件之一。

---

### BUG-2 冲量迭代多线程分区，共享物体速度数据竞争（默认启用）⚠️

**位置**：[PhysicsWorld.cpp:611-647](PhysicsWorld.cpp#L611-L647)

```cpp
#if (Definite != 1) & ThreadPoolBool     // Definite=0, ThreadPoolBool=1 → 默认启用！
    const auto ApplyImpulseXT_Fun = [this](int T_Num, int Tx)
    {
        // 把 CollideGroupVector 按区间分给不同线程
        ThreadTaskAllot(SizeD, SizeY, CollideGroupVector.size(), T_Num, Tx);
        for (; SizeD < SizeY; ++SizeD)
            CollideGroupVector[SizeD]->ApplyImpulse();   // 写两个物体的 speed/angleSpeed
        ...
    };
```

**成因**：按 **arbiter（碰撞对）** 分区，但一个物体同时处于多个碰撞对中（例如叠在地面上的箱子：与地面一个 arbiter、与相邻箱子各一个 arbiter）。这些 arbiter 被分到不同线程后，会**并发读-改-写同一个物体的 `speed` / `angleSpeed`**（`speed -= invMass * Pn` 是非原子的复合赋值）。注释中写"会增加不确定性"，但这不是不确定性问题，而是标准的**数据竞争（C++ UB）**：丢失更新会让冲量随机丢失或放大。

**与其它阶段的对比**：
- `PhysicsPosXT_Fun`（位置积分）按**物体**分区，每个物体只被一个线程触碰 → 安全。
- `PreStep` 多线程版已被作者用 `& 0` 禁用并注释了原因（"存在冲量影响，无法收敛"）——说明作者意识到了此类问题，但 ApplyImpulse 处漏掉了。

**修正方案（按推荐顺序）**：

1. **最简单（推荐）**：像 PreStep 一样禁用多线程迭代，改为单线程顺序迭代（即启用现有的 `#else` 分支）。序贯冲量法本身要求"序贯"，单线程版本不仅正确，收敛性也更好。

```cpp
// 将条件改为禁用（与 PreStep 保持一致）
#if (Definite != 1) & ThreadPoolBool & 0
```

2. **保留并行的正确做法**：改为"每迭代两阶段"——
   - 阶段 A（并行）：每个 arbiter 根据当前速度计算冲量增量，写到 per-arbiter 缓冲，**不直接写物体**；
   - 阶段 B（并行，按物体分区）：把该物体收到的所有冲量增量累加后一次性应用。
   这与 Jacobi 迭代等价，收敛比 Gauss-Seidel 慢，需适当增加迭代次数。

3. **按连通分量分区**：把通过接触/关节相连的物体聚类到同一线程（图着色/并查集），分量内顺序求解。实现复杂，收益最大。

---

### BUG-3 MapDynamic 析构 `delete` / `delete[]` 不匹配

**位置**：[MapDynamic.cpp](MapDynamic.cpp) 构造/析构

构造时：

```cpp
BaseGridBuffer = (BaseGrid *)new char[width * height * sizeof(BaseGrid)];  // char[]
GridBuffer     = new GridBlock[width * height * PixelBlockEdgeSize * PixelBlockEdgeSize]; // GridBlock[]
```

析构时（当前代码）：

```cpp
for (size_t i = 0; i < width*height; ++i) BaseGridBuffer[i].~BaseGrid();  // ✓ 手工调析构
delete (char *)BaseGridBuffer;   // ✗ new[] 配 delete（应为 delete[]）
delete GridBuffer;               // ✗ new[] 配 delete（GridBlock 是数组，应为 delete[]）
```

**后果**：`operator new[]` / `operator delete`（非数组）不匹配是 UB；在主流实现上若 `GridBlock` 是平凡可析构类型可能"侥幸"工作，但一旦类型带析构/对齐要求（C++17 后 over-aligned）就会崩溃或泄漏。

**修正**：

```cpp
delete[] (char *)BaseGridBuffer;   // 与 new char[] 匹配
delete[] GridBuffer;               // 与 new GridBlock[] 匹配
```

更简单的方案是直接用 `std::vector<BaseGrid>` / `std::unique_ptr<BaseGrid[]>`，彻底消除手工配对。

---

### BUG-4 PhysicsShape 零质量 / 零转动惯量除零

**位置**：[PhysicsShape.cpp:94](PhysicsShape.cpp#L94)、[PhysicsShape.cpp:150](PhysicsShape.cpp#L150)

```cpp
invMass = 1.0 / mass;              // mass==0 → +inf
...
invMomentInertia = 1.0 / MomentInertia;   // 单像素形状时 I 可能为 0 → +inf
```

**成因**：`PhysicsShape::UpdateInfo()` 由网格内容统计质量。空网格 / 全挖空 / 单像素形状会出现 `mass==0` 或 `MomentInertia==0`。`inf` 传入求解器后：`speed -= inf * Pn` → NaN → 一个 NaN 物体可以把整个世界的速度污染成 NaN（NaN 通过碰撞对传播）。

**修正**：

```cpp
invMass = (mass > 0.0) ? 1.0 / mass : 0.0;                 // 0 质量 → 视为无限质量（静态）
invMomentInertia = (MomentInertia > 0.0) ? 1.0 / MomentInertia : 0.0;
```

或约定 `mass==0` 一律映射为 `FLOAT_MAX` 静态物体，并在 `UpdateInfo` 入口统一处理。`PhysicsCircle`（r=0）与 `PhysicsLine`（L=0）构造路径建议做同样防御。

---

## 5. P1 级问题（功能缺陷 / 数值错误）

### BUG-5 `q_sqrt` 命名与实现相反（快速**倒数**平方根）

**位置**：[BaseCalculate.cpp:17-38](BaseCalculate.cpp#L17-L38)

```cpp
float q_sqrt(float S)
{
    int i = *reinterpret_cast<int*>(&S);
    i = 0x5f3759df - (i >> 1);                 // Quake III 魔数 → 得到 1/√S 的近似
    float y = *reinterpret_cast<float*>(&i);
    return y * (1.5f - 0.5f * S * y * y);      // 牛顿迭代后仍返回 ≈ 1/√S
}
```

**问题**：实现是经典的 **fast inverse square root（返回 ≈ 1/√S）**，但函数名 `q_sqrt`、头文件注释"快速平方根"都声称返回 √S。目前库内无人调用（死代码），但它出现在公共头文件里，一旦被外部或日后当作 `sqrt` 使用，结果会差一个 `S` 倍因子且毫无警告。

**修正**（二选一）：

```cpp
// 方案 A：改名，语义与实现一致
float q_rsqrt(float S);   // 快速倒数平方根 ≈ 1/√S

// 方案 B：保留名字，修实现，使其真正返回 √S
float q_sqrt(float S)
{
    float y = q_rsqrt(S);
    return S * y;          // √S = S · (1/√S)
}
```

同时修正头文件注释。注意 `double` 版牛顿迭代里 `(1.5f - 0.5f * ...)` 用了 `float` 字面量，精度受损，应改为 `1.5 - 0.5 *`。

---

### BUG-6 `GetWorldEnergy()` 遗漏线段动能

**位置**：[PhysicsWorld.cpp:1569-1587](PhysicsWorld.cpp#L1569-L1587)

统计了 `PhysicsShapeS`、`PhysicsParticleS`、`PhysicsCircleS` 的平动+转动动能，但**漏掉 `PhysicsLineS`**——线段有 `mass/speed/MomentInertia/angleSpeed`，通常还是场景中的主要动力学对象（绳、杆）。

**修正**：

```cpp
for (auto i : PhysicsLineS)
{
    Energy += i->mass * ModulusLength(i->speed);
    Energy += i->MomentInertia * i->angleSpeed * i->angleSpeed;
}
```

（顺带说明：`ModulusLength` 返回模的平方，所以 `Σ m|v|²` 最后统一 `/2` 的写法是正确的 ½mv² + ½Iω²。）

---

### BUG-7 同类型碰撞对去重条件混入 `radius`，导致重复检测与并发 Update

**位置**：[PhysicsWorld.cpp:396-397](PhysicsWorld.cpp#L396-L397)（shape-shape）、440-441（circle-circle）、864-865/898-899（`PhysicsInformationUpdate` 同样代码）

```cpp
case PhysicsObjectEnum::shape:
    if ((PhysicsShape *)PhysicsShapeS[SizeD] <= (PhysicsShape *)i &&
        PhysicsShapeS[SizeD]->radius <= ((PhysicsShape *)i)->radius) break;   // 试图"只处理一次"
    ...
    Arbiter(((PhysicsShape *)i), PhysicsShapeS[SizeD]);
```

**成因**：设两物体指针 X < Y。当 X 侧遍历到 Y 时，仅当 `X->radius <= Y->radius` 才 break；若 `X->radius > Y->radius`，X 侧不 break，而 Y 侧遍历到 X 时因 `Y <= X` 为假也不 break → **同一对物体从两侧各做一次完整 Bresenham 精检**。

**后果**：
1. 性能：大物体旁的小物体场景，精检（最贵的阶段）成倍浪费。
2. 更严重的是**线程安全**：两侧可能运行在不同线程，两个 `HandleCollideGroup` 都查到已存在的 arbiter，并发调用 `it->second.arbiter->Update(...)`，对同一 `contacts` 数组并发写（Pn/Pt 热启动数据竞争）。

**修正**：去重只需指针序（ArbiterKey 本身就按指针规范化）：

```cpp
if ((PhysicsShape *)PhysicsShapeS[SizeD] <= (PhysicsShape *)i) break;  // 只由指针大的一侧处理
```

或对称写法 `if (std::less<>()(i, PhysicsShapeS[SizeD])) ...`，删掉 radius 条件。

---

### BUG-8 `EnergyConservation` 形状间算法物理不成立（死代码）+ 粒子版脆弱写法

**位置**：[EnergyConservation.hpp](EnergyConservation.hpp)

**(a) 形状版 `EnergyConservation(Shape*, Shape*, ...)`（85-222 行）**

算法把一次碰撞拆成两段互相独立的"一维弹性碰撞"：先以 B 的质量为参考系解 A 的速度变化，再反过来解 B。两段各自满足一维守恒，但**合起来的动量矢量一般不守恒**（两段的冲量方向分别沿 `dfA.Parallel` 和 `dfB.Parallel` 两个不同方向，并非同一约束的反作用力对）。转动部分把切向线速度能量直接开方换算成角速度（`sqrt(v²m/I)`），也是启发式而非力学推导。该函数在全部代码中**没有任何调用点**（已 grep 确认，仅文档提及）。

**建议**：删除，或注明"实验性/未启用"。若需要弹性碰撞效果，正确做法是在接触约束中引入恢复系数 `e`：`dPn = massNormal * (−vn + e·vn_init + bias)`，由现有 arbiter 框架统一处理，天然保证动量守恒。

**(b) 粒子版（21-72 行）——结论正确但依赖"双重符号抵消"**

标准推导中二次方程两根之和应为 `+2P/(ma+mb)`，代码里 B 系数符号写反得到 `-2P/(ma+mb)`，随后又用 `-(Va + sum)` 再取一次负，两处错误恰好抵消，最终 `vA' = 2P/(ma+mb) − Va` 数值正确。两个版本（Optimum 0/1）都靠这组抵消成立。

**风险**：任何人"修正"其中一处符号都会引入真正的错误。

**修正**（改写为教科书公式，消除抵消依赖）：

```cpp
// 一维对心弹性碰撞标准解
FLOAT_ total = a->mass + b->mass;
FLOAT_ va2 = ((a->mass - b->mass) * Va + 2.0 * b->mass * Vb) / total;
FLOAT_ vb2 = ((b->mass - a->mass) * Vb + 2.0 * a->mass * Va) / total;
```

另外建议在函数入口加 `if (Dot(a->pos - b->pos, a->speed - b->speed) >= 0) return;`——两粒子正在分离时直接返回，避免重复求解造成的能量注入（工程上比判别式检查更有效）。

---

### BUG-9 圆与地形碰撞 L=0 除零

**位置**：[PhysicsBaseCollide.cpp:478-487](PhysicsBaseCollide.cpp#L478-L487)

```cpp
L = ModulusLength(d.pos);            // 距离平方
if ((Circle->radius * Circle->radius) > L)
{
    L = SQRT_(L);
    ...
    contacts->normal = d.pos / L;    // L==0（圆心恰在轮廓点上）→ 0/0 = NaN
```

**后果**：NaN 法线进入求解器污染整个仿真（NaN 会沿碰撞图传播）。

**修正**：

```cpp
if (L <= 1e-12) {                    // 退化：圆心与轮廓点重合
    contacts->normal = Vec2_{0, -1}; // 任取一致方向（如竖直向上推出）
    L = 1e-12;
}
```

---

## 6. P2 级问题（隐患 / 可维护性 / 性能）

### BUG-10 积分顺序：显式欧拉

**位置**：[PhysicsWorld.cpp:686-708](PhysicsWorld.cpp#L686-L708)（多线程版与单线程版相同顺序）

```cpp
PhysicsShapeS[SizeD]->PhysicsPos(time, GravityAcceleration);    // 先动位置
PhysicsShapeS[SizeD]->PhysicsSpeed(time, GravityAcceleration);  // 后更新速度（重力在此生效）
```

本帧的重力到下一帧才位移 → 显式欧拉。半隐式（辛）欧拉（先 `PhysicsSpeed` 后 `PhysicsPos`）在同样成本下阻尼更合理、堆叠更稳定，是刚体引擎标准做法。**建议交换两行调用顺序**并回归测试堆叠场景。

### BUG-11 `ArbiterKey::operator<` 是反向比较器

**位置**：[BaseArbiter.cpp（BaseArbiter.hpp 内联）76-96 行](BaseArbiter.hpp#L76-L96)

```cpp
inline bool operator<(const ArbiterKey &a1) const
{
    if (a1.object1 < object1) return true;   // a1 < this 才返回 true → 实际是 operator>
    ...
}
```

当前 `CollideGroupS` 用 unordered_map + 自定义 hash，未触发排序问题；但只要日后放进 `std::map` / `std::sort`，得到的是**逆序**且语义颠倒，极难排查。建议改为正常语义：

```cpp
if (object1 < a1.object1) return true;
if (object1 == a1.object1 && object2 < a1.object2) return true;
return false;
```

### BUG-12 双方休眠跳过时不清除旧碰撞对

**位置**：[PhysicsWorld.cpp:351,365 / 418,432 / 475,488](PhysicsWorld.cpp#L351)

```cpp
JZ = (PhysicsShapeS[SizeD]->StaticNum > 10);
...
if (JZ && (((PhysicsParticle *)i)->StaticNum > 10)) continue;   // 直接跳过，不 Map_Delete
```

若两物体在接触状态下先后入睡，其 arbiter 留在 `CollideGroupVector` 中，每帧仍参与 PreStep/ApplyImpulse（用陈旧的 separation 计算 bias）。多数情况会因分离后 bias→0 而自行收敛，但与 BUG-2 的竞争叠加时可能产生持续微抖、物体"永不深睡"。建议跳过前先 `Map_Delete(key)` 或在物体入睡时统一清理其名下 arbiter。

### BUG-13 GPU 路径风险

**位置**：[PhysicsGPU.cpp](PhysicsGPU.cpp)

1. `PackBodyDynamic` 每帧用 `std::async` 按硬件线程数 spawn 线程（139-200 行），线程创建开销可能吃掉 GPU 并行收益，建议复用引擎自带 `ThreadPool`。
2. CPU 与 GPU 两条求解路径并存（[PhysicsWorld.cpp:603-670](PhysicsWorld.cpp#L603-L670)），shader 侧（VulkanTool/Calculate）不在本目录内、无法审计；两者浮点顺序不同必然产生轨迹分歧——存档/回放/联机场景必须锁定单一路径。建议在文档中明确"GPU 路径不保证与 CPU 位相等"。
3. `PackBodyStatic` 对 map 槽位只写 `[3..7]` 不写 `[0..2]`（速度），静态无害但依赖隐式约定，建议补 `dst[offset+0..2]=0` 并加注释。

### BUG-14 `PhysicsInformationUpdate` 与 `PhysicsEmulator` 约 230 行重复

**位置**：[PhysicsWorld.cpp:817-1046](PhysicsWorld.cpp#L817-L1046) vs 338-539

两份几乎相同的 XT_Fun lambda（后者多了休眠跳过与 `OldPosUpDataBool` 处理细节差异）。任何碰撞流程修改都要改两处，极易漂移（历史上 `PhysicsEmulator` 有 JZ 判断而 `PhysicsInformationUpdate` 没有，已经是漂移证据）。建议提取私有成员函数 `RunCollisionDetection(bool skipSleeping)`。

### BUG-15 迭代次数只升不降

**位置**：[PhysicsWorld.hpp:343-356](PhysicsWorld.hpp#L343-L356) + [PhysicsWorld.cpp:1495-1496](PhysicsWorld.cpp#L1495)

`AddObject → ApplyImpulseAdd()` 会重算 `ApplyImpulseSize = sqrt(ObjectSize/5)`；`RemoveObject` 只 `--ObjectSize` 不重算。大量删除物体后迭代数仍维持峰值，浪费性能。建议把重算逻辑提取成 `UpdateApplyImpulseSize()`，增删都调用。

---

## 7. 经核实为"非 BUG"的易误解点

阅读与交叉验证后确认以下几处**虽然写法可疑，但实际正确**，记录在此避免误改：

| 位置 | 现象 | 核实结论 |
|---|---|---|
| EnergyConservation 粒子版 | `-2P/(ma+mb)` 似乎是符号错误 | 与后续 `-(Va+sum)` 的取负**恰好抵消**，最终公式正确（但见 BUG-8b 建议重写） |
| PhysicsBaseArbiterA::ApplyImpulse（485 行）| 切向量用 `Cross(normal, -1.0)`，其余 7 处用 `+1.0` | 切向翻转使 `vt`、`dPt` 同时反号，应用时物理冲量方向与 AA 版一致；且 PreStep 热启动 `P = Pn·n − Pt·t`（442 行）与之配套。**正确但约定混乱**，建议统一为 `+1.0` 并同步修改热启动符号，仅作重构不改行为 |
| `GetWorldEnergy` 用 `ModulusLength`（模的平方）| 看似漏开方 | `Σ m·|v|²` 后统一 `/2` 正是 ½mv²，正确（真正问题是漏了 Line，见 BUG-6） |
| `ThreadTaskAllot` 宏 | 手写区间分配 | 数学验证正确：前 `rem` 个线程各 `base+1` 个，其余 `base` 个，无缝覆盖 |
| `((PhysicsParticle *)i)->StaticNum` 跨类型强转 | 似有多继承偏移风险 | C 风格基类→派生类转换等价 static_cast，编译器会做指针调整；继承链上 `StaticNum` 位于公共基类，安全 |
| ArbiterKey 与 Map_Delete 参数顺序 | LC 对创建 `(Line,Circle)`、删除 `(Circle,Line)` 顺序不同 | ArbiterKey 构造函数内部按指针大小规范化，两个顺序生成同一键，删除有效 |

---

## 8. 修复优先级与验证建议

### 建议修复顺序

1. **BUG-1（接触点越界）** —— 一行循环条件的修复，消除随机内存损坏；修完可用 ASan 跑"大形状压锯齿地形"场景验证。
2. **BUG-2（冲量迭代数据竞争）** —— 改回单线程迭代一行搞定；或实现两阶段 Jacobi。用 TSan 验证。
3. **BUG-3 / BUG-4** —— delete[] 与零质量防御，各两行。
4. **BUG-7（去重条件）** —— 删除 radius 比较，消除重复精检与并发 Update。
5. 其余 P1/P2 按迭代节奏处理。

### 验证手段

- **AddressSanitizer**：覆盖"形状-地形""圆-地形"高频碰撞场景（针对 BUG-1/3）。
- **ThreadSanitizer**：开启 `ThreadPoolBool` 跑多物体堆叠（针对 BUG-2/7）。
- **能量监控**：修复 BUG-6 后，用 `GetWorldEnergy()` 做回归基线——封闭系统弹性场景能量漂移应小于 1%/千帧。
- **休眠测试**：堆叠场景静置 10 秒，观察是否全部入睡（验证 BUG-12）。
- **数值 NaN 注入测试**：构造空形状、单像素形状、圆心贴地的圆（针对 BUG-4/9）。

### 总体评价

引擎整体架构相当扎实：像素网格 + Bresenham 检测的选型统一；序贯冲量 + 热启动 + Baumgarte 的求解器实现标准；无锁的 per-thread 碰撞输出、per-thread 内存池、O(1) swap-and-pop 碰撞对管理都是亮点；莫顿码优化缓存、无限滚动地图、GPU 求解通路等体现了完整的工程考量。主要风险集中在**边界条件防御**（接触点数量、零质量、除零）与**一处被默认启用的多线程数据竞争**；算法层面（求解器、约束、积分器）没有原理性错误。

---

*报告生成日期：2026-08-17 · 基于 /workspace/PhysicsBlock 全量源码静态分析（含关键路径交叉验证）*
