# PhysicsBlock 物理引擎

基于像素网格的 2D 刚体物理引擎，支持多种物理对象（网格形状、粒子、圆、线段）、关节约束、绳索连接、静态/动态地图碰撞检测，以及基于四叉树的网格搜索加速。

---

## 类关联图

### 继承关系

```
BaseSerialization
    ├── BaseGrid ───────────────────────── BaseOutline
    │       │                                   │
    │       └─────── MapStatic ──┐              │
    │                            │              │
    │       └─────── MapDynamic ─┤              │
    │                            │              │
    └────────────────────────────┴── MapFormwork (接口)
                                        ▲
                                        │ 实现

PhysicsFormwork (接口)
    │
    └── PhysicsParticle ──── PhysicsAngle ──┬── PhysicsCircle
                                           ├── PhysicsLine
                                           │
                                           └── PhysicsShape (多继承)
                                                   │
                                                   └── 同时继承 BaseOutline

BaseArbiter (碰撞裁决基类)
    │
    ├── PhysicsBaseArbiterAA ── PhysicsArbiterSS, PhysicsArbiterCS, PhysicsArbiterCC, PhysicsArbiterLC, PhysicsArbiterLS
    ├── PhysicsBaseArbiterAD ── PhysicsArbiterSP, PhysicsArbiterCP, PhysicsArbiterLP
    ├── PhysicsBaseArbiterA  ── PhysicsArbiterS, PhysicsArbiterC, PhysicsArbiterL
    └── PhysicsBaseArbiterD  ── PhysicsArbiterP

BaseJunction (连接约束基类)
    ├── PhysicsJunctionSS  (Angle ↔ Angle)
    ├── PhysicsJunctionS   (Angle ↔ 固定点)
    ├── PhysicsJunctionP   (Particle ↔ 固定点)
    └── PhysicsJunctionPP  (Particle ↔ Particle)
```

### 组合关系

```
PhysicsWorld ─┬── 持有 MapFormwork*           (地图)
              ├── 持有 vector<PhysicsShape*>    (网格形状集合)
              ├── 持有 vector<PhysicsParticle*> (粒子集合)
              ├── 持有 vector<PhysicsCircle*>   (圆形集合)
              ├── 持有 vector<PhysicsLine*>     (线段集合)
              ├── 持有 vector<PhysicsJoint*>    (关节集合)
              ├── 持有 vector<BaseJunction*>    (连接约束集合)
              ├── 持有 GridSearch              (四叉网格搜索)
              ├── 持有 CollideGroupS           (碰撞对哈希表)
              └── 持有 MemoryPool<Arbiter*>    (碰撞裁决内存池)

PhysicsJoint ──── 持有 PhysicsAngle* ×2  (两个物理对象)

PhysicsJunctionSS ── 持有 PhysicsAngle* ×2
PhysicsJunctionS  ── 持有 PhysicsAngle* ×1 + 固定点
PhysicsJunctionP  ── 持有 PhysicsParticle* ×1 + 固定点
PhysicsJunctionPP ── 持有 PhysicsParticle* ×2

MapDynamic ──── 持有 MovePlate<BaseGrid*>  (板块移动控制台)

GridSearch ──── 持有 vector<PhysicsFormwork*>  (网格内对象列表)
```

### 碰撞检测与裁决流程

```
PhysicsWorld::PhysicsEmulator()
    │
    ├─ 1. PhysicsSpeed() ─── 更新所有对象速度
    ├─ 2. PhysicsPos()   ─── 更新所有对象位置
    ├─ 3. GridSearch::UpData() ── 更新四叉网格
    ├─ 4. BroadPhase (AABB) ──── 粗筛碰撞对
    │       │
    │       └─ CollideAABB() ── PhysicsBaseCollide
    │
    ├─ 5. NarrowPhase ────────── 精确碰撞检测
    │       │
    │       └─ Collide() ────── PhysicsBaseCollide
    │
    ├─ 6. Arbiter::ComputeCollide() ── 计算碰撞点
    ├─ 7. Arbiter::PreStep() ──────── 预处理冲量
    ├─ 8. Arbiter::ApplyImpulse() ×N ─ 迭代求解冲量
    ├─ 9. Joint::PreStep() ────────── 关节预处理
    ├─10. Joint::ApplyImpulse() ×N ── 关节迭代
    ├─11. Junction::PreStep() ─────── 连接预处理
    └─12. Junction::ApplyImpulse() ×N 连接迭代
```

---

## 基础设施层

### BaseDefine.h

编译期常量定义：

| 宏名 | 值 | 说明 |
|------|----|------|
| `MomentInertiaSamplingSize` | 5 | 转动惯量采样密度 |
| `PixelBlockEdgeSize` | 16 | 像素块边长（必须是 2 的幂） |
| `Define_MinSpoilage` | 0.9994 | 能量转换率（模拟阻力） |
| `PhysicsApplyImpulseSize` | 10 | 最低物理迭代次数 |

### BaseStruct.hpp

核心类型定义与数据结构：

- **`FLOAT_`** / **`Vec2_`** / **`Mat2_`**：可通过编译开关切换 `float`/`double` 精度
- **`GridBlockType`** 枚举：`Entity`(实体)、`Collision`(碰撞)、`Event`(碰撞事件)
- **`GridBlock`** 结构体：单个格子的完整信息（摩擦因数、质量/血量、类型位域）
- **`CollisionInfoI`** / **`CollisionInfoD`**：整数/浮点数碰撞结果
- **`DecompositionForce`**：受力分解结果（垂直分量 + 平行分量）
- **`PhysicsObjectEnum`**：物理对象类型枚举（shape、particle、circle、line、MapStatic、MapDynamic）

### BaseCalculate.hpp / .cpp

数学工具函数集：

| 函数 | 说明 |
|------|------|
| `ToInt()` | 浮点转整数，向下取整 |
| `Modulus()` / `ModulusLength()` | 向量模长（开方/不开方） |
| `AngleFloatToAngleVec()` | 角度值 → (cos, sin) 向量 |
| `vec2angle()` | 向量绕原点旋转 |
| `AngleMat` | 可复用的旋转矩阵，提供 `Rotary()`(正转) 和 `Anticlockwise()`(逆转) |
| `LineSquareFocus()` | 线段被矩形裁剪 |
| `SquareToSquare()` | 正方形与正方形碰撞检测 |
| `SquareToDrop()` | 正方形与点碰撞检测 |
| `TorqueCalculate()` | 扭矩计算 |
| `CalculateDecompositionForce()` | 力分解（垂直/平行于力臂） |
| `Morton2D()` | 二维莫顿码（空间索引优化） |
| `DropUptoLineShortes()` | 点到线段最短距离 |
| `Dot()` / `Cross()` | 点乘 / 叉乘 |

### BaseSerialization.hpp

JSON 序列化基础设施：

- **`BaseSerialization`** 类：提供 `JsonSerialization()` / `JsonContrarySerialization()` 虚函数
- **`SerializationInherit_`** 宏：条件编译时自动继承 `BaseSerialization`
- **`SerializationVec2`** / **`ContrarySerializationVec2`** 宏：Vec2 序列化/反序列化快捷方式
- 通过 `PhysicsBlock_Serialization` 宏控制是否启用序列化功能

---

## 网格与地图层

### BaseGrid

基础网格类，管理 `GridBlock` 二维数组：

- **构造**：可内部分配网格或接收外部指针
- **`at(x, y)`**：获取格子引用（不安全，无边界检查）
- **`GetCollision(x, y)`**：安全获取碰撞状态（越界返回 false）
- **`BresenhamDetection()`**：Bresenham 线段碰撞检测，返回整数/浮点碰撞信息

### BaseOutline : public BaseGrid

外轮廓点集合，继承自 BaseGrid：

- **`OutlineSet[]`**：轮廓点坐标数组（相对于质心）
- **`FrictionSet[]`**：轮廓点对应摩擦力数组
- **`UpdateOutline()`**：全量计算外轮廓点
- **`UpdateLightweightOutline()`**：轻量版轮廓计算（减少冗余角点）

### MapFormwork (接口)

地图模板接口，定义所有地图必须实现的功能：

- `FMGetType()`：返回地图类型
- `FMGetMapSize()`：返回地图尺寸
- `FMGetCollide()`：查询碰撞
- `FMGetGridBlock()`：获取格子引用
- `FMBresenhamDetection()`：线段碰撞检测

### MapStatic : public BaseGrid, public MapFormwork

静态地图，不可变地形：

- **`centrality`**：中心偏移量（世界坐标 → 网格坐标的偏移）
- **`SetCentrality()`**：设置中心位置
- **`GetOutline()`** / **`GetLightweightOutline()`**：获取指定区域的外轮廓点
- **`FMBresenhamDetection()`**：带线段裁剪的碰撞检测

### MapDynamic : public MapFormwork, public BaseGrid

动态地图，支持无限滚动：

- 使用 **`MovePlate<BaseGrid*>`** 控制板块移动
- 通过莫顿码优化板块内存布局，提高缓存命中率
- **`Updata(pos)`**：更新玩家位置，触发板块滚动
- **`at(x, y)`**：通过板块偏移计算实际网格位置

### MovePlate\<PlateT\>

板块移动控制台（模板工具类）：

- 管理二维板块数组，支持按位置偏移滚动
- **`UpData(x, y)`**：更新监测位置，返回板块移动信息
- **`ExcursionGetPlate()`**：偏移获取板块
- 支持生成/销毁回调函数，板块移动时自动触发

---

## 物理对象层

### PhysicsFormwork (接口)

动态物体的统一接口：

| 虚函数 | 说明 |
|--------|------|
| `PhysicsSpeed(time, Ga)` | 外力 → 速度 |
| `PhysicsPos(time, Ga)` | 速度 → 位置 |
| `PFGetType()` | 返回对象类型枚举 |
| `PFGetCollisionR()` | 返回碰撞半径 |
| `PFGetPos()` | 返回位置 |
| `PFGetInvMass()` | 返回质量倒数 |
| `PFGetMass()` | 返回质量 |
| `PFGetInvI()` | 返回转动惯量倒数 |
| `PFSpeed()` | 速度引用 |
| `PFAngleSpeed()` | 角速度引用 |

### PhysicsParticle : public PhysicsFormwork

物理粒子，最基础的动态对象：

- **`pos`**：世界位置（在 Shape 中代表质心位置）
- **`speed`**：线速度
- **`force`**：合外力
- **`mass`** / **`invMass`**：质量 / 质量倒数（`invMass=0` 表示不可移动）
- **`friction`**：摩擦因数
- **`OldPos`**：上一帧位置（用于碰撞检测，保证旧位置不在碰撞体内）
- **`StaticNum`**：静止计数器（连续静止帧数，≥200 视为静止）
- **`AddForce()`**：累加外力
- **`PhysicsSpeed()`**：`v += t * (g + F/m)`，应用最小损耗系数
- **`PhysicsPos()`**：`p += t * v`，更新位置并记录旧位置

### PhysicsAngle : public PhysicsParticle

物理角度类，在粒子基础上增加旋转能力：

- **`MomentInertia`** / **`invMomentInertia`**：转动惯量 / 倒数
- **`angle`**：旋转角度（弧度）
- **`angleSpeed`**：角速度
- **`torque`**：扭矩
- **`radius`**：碰撞半径
- **`AddForce(Pos, Force)`**：在指定位置施加力，同时影响移动和旋转
- **`AddForce(Force)`**：仅影响移动
- **`AddTorque(ArmForce, Force)`**：仅影响旋转
- **`PhysicsSpeed()`**：更新线速度 + 角速度
- **`PhysicsPos()`**：更新位置 + 角度

### PhysicsShape : public PhysicsAngle, public BaseOutline

网格形状，核心物理对象，多继承自角度类和轮廓类：

- **`CentreMass`**：质心（质量加权中心）
- **`CentreShape`**：几何中心
- **`UpdateInfo()`**：重新计算质量、质心、几何中心、转动惯量
- **`UpdateCollisionR()`**：根据轮廓点计算碰撞半径
- **`UpdateAll()`**：全部更新（Info + Outline + CollisionR）
- **`DropCollision(pos)`**：检测点是否点击到网格
- **`RayCollide(Pos, Angle)`**：射线碰撞检测
- **`PsBresenhamDetection()`**：线段碰撞检测（支持旋转变换）

### PhysicsCircle : public PhysicsAngle

物理圆形：

- 构造时自动计算转动惯量：`I = 0.5 * m * r²`
- 碰撞半径即为圆的半径

### PhysicsLine : public PhysicsLine

物理线段：

- 位置为线段中点
- 角度为线段方向
- 碰撞半径为线段半长
- 转动惯量：`I = L² * m / 12`（细杆公式）

---

## 碰撞裁决层

### BaseArbiter

碰撞裁决基类：

- **`Contact contacts[20]`**：碰撞点集合（最多 20 个）
- **`numContacts`**：碰撞点数量
- **`ArbiterKey key`**：碰撞对键（两个对象指针排序组合）
- **`ComputeCollide()`**：计算碰撞点（纯虚函数）
- **`PreStep()`**：预处理冲量分母
- **`ApplyImpulse()`**：迭代求解冲量

### Contact 结构体

单个碰撞点的完整信息：

| 字段 | 说明 |
|------|------|
| `position` | 碰撞点世界位置 |
| `normal` | 最佳分离轴法向量 |
| `separation` | 最小分离距离 |
| `friction` | 摩擦系数 |
| `r1` / `r2` | 质心指向碰撞点的向量 |
| `Pn` | 位移冲量累计 |
| `Pt` | 旋转冲量累计 |

### PhysicsBaseArbiterAA / AD / A / D

四种裁决中间类，按碰撞对象类型分类：

| 类 | 对象1 | 对象2 | 子类 |
|----|-------|-------|------|
| `PhysicsBaseArbiterAA` | PhysicsAngle | PhysicsAngle | SS, CS, CC, LC, LS |
| `PhysicsBaseArbiterAD` | PhysicsAngle | PhysicsParticle | SP, CP, LP |
| `PhysicsBaseArbiterA` | PhysicsAngle | MapFormwork | S, C, L |
| `PhysicsBaseArbiterD` | PhysicsParticle | MapFormwork | P |

### PhysicsArbiter (具体裁决类)

12 种碰撞组合的具体实现：

| 类 | 碰撞组合 |
|----|---------|
| `PhysicsArbiterSS` | Shape ↔ Shape |
| `PhysicsArbiterSP` | Shape ↔ Particle |
| `PhysicsArbiterS`  | Shape ↔ 地图 |
| `PhysicsArbiterP`  | Particle ↔ 地图 |
| `PhysicsArbiterC`  | Circle ↔ 地图 |
| `PhysicsArbiterCS` | Circle ↔ Shape |
| `PhysicsArbiterCP` | Circle ↔ Particle |
| `PhysicsArbiterCC` | Circle ↔ Circle |
| `PhysicsArbiterLC` | Line ↔ Circle |
| `PhysicsArbiterLS` | Line ↔ Shape |
| `PhysicsArbiterLP` | Line ↔ Particle |
| `PhysicsArbiterL`  | Line ↔ 地图 |

### PhysicsBaseCollide

碰撞检测函数集（无类，纯函数）：

- **`CollideAABB()`**：AABB 粗筛（Broad Phase）
- **`Collide()`**：精确碰撞检测（Narrow Phase），返回碰撞点数量和 Contact 数组

---

## 约束层

### PhysicsJoint

物理关节（铰接约束），连接两个 PhysicsAngle 对象：

- **`Set(Body1, Body2, anchor)`**：设置关节，anchor 为铰接点世界位置
- **`PreStep()`**：计算有效质量矩阵 M 和位置偏差 bias
- **`ApplyImpulse()`**：迭代求解冲量，维持两物体在铰接点的相对位置

### BaseJunction

连接约束基类，支持多种绳索类型：

| `CordType` | 说明 | 行为 |
|-------------|------|------|
| `cord` | 绳子 | 仅约束拉伸（bias > 0 时归零） |
| `rubber` | 橡皮筋 | 弱约束拉伸（bias 缩小 100 倍） |
| `spring` | 弹簧 | 双向约束（bias 缩小 50 倍） |
| `lever` | 杠杆 | 刚性约束 |

四种具体连接：

| 类 | 连接对象 | 说明 |
|----|---------|------|
| `PhysicsJunctionSS` | Angle ↔ Angle | 两个可旋转物体连接 |
| `PhysicsJunctionS` | Angle ↔ 固定点 | 可旋转物体连接到世界固定点 |
| `PhysicsJunctionP` | Particle ↔ 固定点 | 粒子连接到世界固定点 |
| `PhysicsJunctionPP` | Particle ↔ Particle | 两个粒子连接 |

---

## 搜索与优化层

### GridSearch

四叉网格搜索，加速碰撞对筛选：

- 按对象碰撞半径分配到不同层级的网格
- 使用 **莫顿码** 优化一维索引的空间局部性
- **`SetMapRange(Size)`**：设置搜索范围
- **`Add()`** / **`Remove()`**：添加/移除对象
- **`Get(pos, R)`**：获取指定范围内的所有对象
- **`UpData()`**：更新所有对象的网格位置
- **`GetDividedVision()`**：获取对象所在的网格可视化信息

### EnergyConservation.hpp

动能守恒定律计算（头文件内联实现）：

- **`EnergyConservation(Particle*, Particle*)`**：粒子间理想碰撞
- **`EnergyConservation(Shape*, Shape*, CollisionDrop, Vertical)`**：形状间碰撞，同时计算线速度和角速度变化

---

## 世界层

### PhysicsWorld

物理世界，管理所有物理对象和仿真流程：

**属性**：
- `GravityAcceleration`：重力加速度
- `Wind` / `GridWind`：风 / 网格风
- `wMapFormwork`：地图指针
- `mGridSearch`：四叉网格搜索
- 五种对象容器：`PhysicsShapeS`、`PhysicsParticleS`、`PhysicsCircleS`、`PhysicsLineS`、`PhysicsJointS`、`BaseJunctionS`
- 碰撞对容器：`CollideGroupS`(哈希表)、`CollideGroupVector`(数组)
- 12 个碰撞裁决内存池（`MemoryPool<Arbiter>`）
- 线程池 `mThreadPool`（多线程碰撞检测）

**核心方法**：
- **`AddObject()`**：添加物理对象（自动注册到网格搜索）
- **`SetMapFormwork()`**：设置地图
- **`PhysicsEmulator(time)`**：物理仿真主循环
- **`Get(pos)`**：获取指定位置上的对象
- **`GetWorldEnergy()`**：获取世界总动能

**仿真流程** (`PhysicsEmulator`)：
1. 更新所有对象的力（重力 + 风）
2. `PhysicsSpeed()` → 更新速度
3. `PhysicsPos()` → 更新位置
4. `GridSearch::UpData()` → 更新搜索网格
5. Broad Phase → AABB 粗筛
6. Narrow Phase → 精确碰撞检测
7. 创建/更新/删除 Arbiter
8. `Arbiter::PreStep()` → 预处理
9. `Arbiter::ApplyImpulse()` × N → 迭代求解
10. `Joint::PreStep()` + `ApplyImpulse()` × N
11. `Junction::PreStep()` + `ApplyImpulse()` × N

---

## 调试与 UI 层

### ImGuiPhysics.hpp / .cpp

ImGui 物理调试界面：

- 每种物理对象（Particle、Angle、Shape、Circle、Line、World）的属性编辑器
- 辅助视觉参数控制（位置、速度、受力、质心、外骨骼、碰撞点等开关与颜色）
- 通过 INI 文件持久化辅助视觉设置
- **`PhysicsUI()`**：各类型对象的 ImGui 属性面板
- **`PhysicsAuxiliaryColorUI()`**：辅助视觉参数控制面板
- **`AuxiliaryInfoRead()`** / **`AuxiliaryInfoStorage()`**：INI 读写

---

## 文件索引

| 文件 | 类型 | 说明 |
|------|------|------|
| `BaseDefine.h` | 常量 | 编译期常量定义 |
| `BaseStruct.hpp` | 类型 | 核心类型与数据结构 |
| `BaseCalculate.hpp/.cpp` | 工具 | 数学计算函数集 |
| `BaseSerialization.hpp` | 基础 | JSON 序列化基础设施 |
| `BaseGrid.hpp/.cpp` | 网格 | 基础网格类 |
| `BaseOutline.hpp/.cpp` | 网格 | 外轮廓点集合 |
| `MapFormwork.hpp` | 接口 | 地图模板接口 |
| `MapStatic.hpp/.cpp` | 地图 | 静态地图 |
| `MapDynamic.hpp/.cpp` | 地图 | 动态地图（无限滚动） |
| `MovePlate.h` | 工具 | 板块移动控制台模板 |
| `PhysicsFormwork.hpp` | 接口 | 动态物体接口 |
| `PhysicsParticle.hpp/.cpp` | 对象 | 物理粒子 |
| `PhysicsAngle.hpp/.cpp` | 对象 | 物理角度（可旋转粒子） |
| `PhysicsShape.hpp/.cpp` | 对象 | 网格形状 |
| `PhysicsCircle.hpp/.cpp` | 对象 | 物理圆形 |
| `PhysicsLine.hpp/.cpp` | 对象 | 物理线段 |
| `BaseArbiter.hpp` | 碰撞 | 碰撞裁决基类 |
| `PhysicsBaseArbiter.hpp/.cpp` | 碰撞 | 裁决中间类 |
| `PhysicsArbiter.hpp/.cpp` | 碰撞 | 具体裁决类（12 种组合） |
| `PhysicsBaseCollide.hpp/.cpp` | 碰撞 | 碰撞检测函数集 |
| `PhysicsJoint.hpp/.cpp` | 约束 | 物理关节 |
| `PhysicsJunction.hpp/.cpp` | 约束 | 连接约束（绳/弹簧/橡皮筋） |
| `EnergyConservation.hpp` | 工具 | 动能守恒计算 |
| `GridSearch.hpp` | 搜索 | 四叉网格搜索 |
| `PhysicsWorld.hpp/.cpp` | 世界 | 物理世界（仿真主循环） |
| `ImGuiPhysics.hpp/.cpp` | 调试 | ImGui 调试界面 |
