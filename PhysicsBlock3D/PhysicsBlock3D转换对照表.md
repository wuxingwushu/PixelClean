# PhysicsBlock → PhysicsBlock3D 逐类改写对照表

本文档记录 **`PixelClean/PhysicsBlock/`（2D 像素网格刚体物理引擎）逐类改写为
`PixelClean/PhysicsBlock3D/`（3D 体素网格刚体物理引擎）** 的完整对照关系、
每一类的维度映射、以及改写过程中修掉的 2D 既有缺陷。

- 源（2D）：63 个文件 / 20194 行
- 目标（3D）：54 个文件（含 CMakeLists.txt 与本表）
- 全部符号位于 `namespace PhysicsBlock3D`，全部宏加 `3D` 后缀 —— **两个版本可在同一个
  可执行文件中共存而不冲突**。

---

## 0. 三个总体约定

| 维度 | 2D | 3D |
|---|---|---|
| 平移 | `Vec2_` (`glm::vec2`) | `Vec3_` (`glm::vec3`) |
| 旋转 | `FLOAT_ angle`（标量弧度） | `Quat_ rotation`（`glm::quat`，**左乘世界系增量**） |
| 角速度 | `FLOAT_ angleSpeed` | `Vec3_ angleSpeed` |
| 惯量 | `FLOAT_ MomentInertia`（与朝向无关） | `Mat3_ MomentInertia`（**局部系** + `WorldInvInertia()` 共轭变换） |
| 力矩 | `FLOAT_ torque` | `Vec3_ torque` |
| 网格索引 | `glm::ivec2` | `IVec3_` (`glm::ivec3`) |
| 索引布局 | `at(x,y) = at(x*height+y)` | `at(x,y,z) = at((x*height+y)*depth+z)` |
| 射线步进 | Bresenham（4 向） | Amanatides-Woo 3D DDA（6 面） |
| 方向枚举 | `CheckDirection` 4 值 / 2 bit | `CheckDirection` 6 值 / **3 bit** |
| 空间索引 | 四叉树 `(4^k-1)/3` | 八叉树 `(8^k-1)/7` |
| 空间编码 | `Morton2D`（32 位） | `Morton3D`（**64 位**） |
| 接触点 | `Contact{Vec2_ position, normal; FLOAT_ Pn, Pt}` | `Contact3D{Vec3_ position, normal; FLOAT_ Pn, Pt1, Pt2}` |
| 摩擦 | 1D 区间钳制 | **摩擦圆盘投影**（`(Pt1,Pt2)` 径向投影） |
| 约束自由度 | 2 DOF（x/y 平移） | 3 DOF（xyz 平移） |

**三条必须理解的 3D 语义变更**（不是机械替换）：

1. **惯量张量随朝向变化**。2D 的 `Izz` 与朝向无关，3D 必须把 `MomentInertia` /
   `invMomentInertia` 存**局部系**，任何"用 `invI` 乘世界系向量"的地方都要走
   `PhysicsAngle3D::WorldInvInertia()`
   （= `R * invMomentInertia * transpose(R)`），否则旋转体行为错误。
2. **`glm::angleAxis(angle, axis)` 内部已取半角**（`w=cos(a/2)`）。
   所以 `QuatFromAngularVelocity(ω, dt)` 必须传**完整转角** `|ω|*dt`，不能写
   `speed*time*0.5`。
3. **接触法线方向约定**：`contacts[i].normal` 一律**由 object1 指向 object2**，
   与求解器 `dv = v2 - v1`、`dPn = massNormal*(-vn + bias)` 自洽。
   2D 各 `Collide` 重载的符号并不统一（既有缺陷），3D 统一了。

---

## 1. 基础类型层

| 2D | 3D | 改写要点 |
|---|---|---|
| `BaseDefine.h` | `BaseDefine3D.h` | 宏全加 `3D` 后缀；新增 `PhysicsContactManifoldMaxSize3D 4`（face-clipped 流形上限）；**删除 `PhysicsGPUBool`**（3D 明确不含 GPU） |
| `BaseStruct.hpp` | `BaseStruct3D.hpp` | `Vec3_/Vec4_/DVec3_/Mat3_/Mat4_/Quat_/IVec3_/UVec3_`；`GridBlock`→`GridBlock3D`；`CollisionInfoI`→`CollisionInfoI3D`（`IVec3_ pos`）；`CollisionInfoD`→`CollisionInfoD3D`（`Direction` 由 2 bit 扩到 **3 bit**，新增 `GridPos`）；`DecompositionForce`→`DecompositionForce3D`（新增 `Normal` 参考法向）；`SquareFocus`→`BoxFocus`；新增 `DirectionNormal(dir)` 六面单位法向表与 `QuatRotate/QuatInverseRotate/QuatFromAngularVelocity/IdentityQuat` |
| `PhysicsBlockTypes.hpp` | `PhysicsBlockTypes3D.hpp` | `Bounds`→`Bounds3D`（补 Z，新增 `ClosestPoint`）；`BulletHitInfo`→`BulletHitInfo3D`；`DirectionFromNormal` 由"2 bit 方向"改为**取绝对值最大分量轴** |
| `BaseSerialization.hpp` | `BaseSerialization3D.hpp` | 新增 `SerializationVec3/IVec3/Quat/Mat3` 及各自 Contrary 版本。**`TOSIRING3D`（`#str`）只对裸标识符成立**：传 member 表达式（如 `OutlineSet[i].LocalPos`）会把整个拼写当键名，必须手写字面键 |

## 2. 数学核

| 2D | 3D | 改写要点 |
|---|---|---|
| `BaseCalculate.hpp/.cpp` | `BaseCalculate3D.hpp/.cpp` | `AngleMat{Cos,Sin}` **删除**，旋转走四元数；`CalculateDecompositionForce` 由 acos+cos/sin 角度链改为**直接向量投影**；`Cross(Vec2_,Vec2_)→FLOAT_` 改为 `Cross(Vec3_,Vec3_)→Vec3_`；`Morton2D`→`Morton3D`（64 位，libmorton `SplitBy3` 掩码），**新增 `Morton3DInverse`**；`LineSquareFocus`→`LineBoxFocus`（slab 法）；`SquareToSquare`→`BoxToBox`；`SquareToDrop`→`BoxToDrop`；**新增** `SATTest`（15 轴）/`BuildSATAxes`/`ClipPolygonToPlane`（Sutherland-Hodgman）/`PolygonArea`/`BuildTangentBasis`/`ProjectFrictionDisk`/`SymmetricInverse`/`SafeNormalize`/`AABBIntersect`/`RayAABB`；修 2D 用**平方长度**做投影分母的缺陷（`t = Dot(d,p)/len²` 并夹到 `[0,1]`） |

## 3. 网格 / 轮廓层

| 2D | 3D | 改写要点 |
|---|---|---|
| `BaseGrid.hpp/.cpp` | `BaseGrid3D.hpp/.cpp` | 新增 `depth`；新增 `InBounds`/`GridAt`（const 访问路径）；`BresenhamDetection`→**`DDADetection`**（Amanatides-Woo，整数版转发浮点版**统一成一条遍历路径**）；新增 `FaceFromStep(axis,step)`；**入射面由几何直接得出**，不再像 2D 用 calcFromX/calcFromY 回切试探 |
| `BaseOutline.hpp/.cpp` | `BaseOutline3D.hpp/.cpp` | 三条平行裸数组（`OutlineSet`/`MaxOutlineCentreMass`/`FrictionSet`）→ **`std::vector<OutlinePoint3D{Vec3_ LocalPos; Vec3_ Normal; FLOAT_ Friction; Vec3_ SeedPos; IVec3_ Voxel; unsigned char Corner;}`**；`MaxOutlineCentre` 的 O(row²) 贪婪矩形枚举 → `SeedField3D` 的**最大内接长方体**搜索（实心前缀和 O(1) 判整块、六方向贪心扩展 + 二分，逐体素缓存）；**轮廓点取方形面的 4 个顶点（`Corner` 0..3 沿面环绕，`(p1-p0)×(p2-p1)` 与法线同向），不再取面中心**（面中心会让 16×16 大平面只剩 1 个采样点、流形退化）；同一顶点被相邻面重复输出时共享同一个 `SeedPos`（取离顶点最近的质心，一个外骨骼点只连一条射线）；**新增 `EdgeSet`（`OutlineEdge3D{A,B,Friction}`）与 `BuildEdges`**：面片 4 点按 `Corner` 重排取 4 条边 → 顶点去重 → 只留折棱（相邻面片法线不同）→ 沿边网格采样剔除跨凹口的弦（否则最小模式的支撑四点是极值顶点，连线可能是空中弦）；**根治 2D 的 `W*H*3` 分配越界**（实需最坏 `4*W*H`）与 JSON 分支漏分配 `MaxOutlineCentreMass` |
| `BaseArbiter.hpp` | `BaseArbiter3D.hpp` | `Contact`→`Contact3D`（**单 `Pt` → `Pt1`/`Pt2` 两个正交切向**，`tangent1/tangent2` 由 PreStep 缓存）；`ArbiterKey`→`ArbiterKey3D`；新增 `kMaxPoolThreads3D=9`/`kMainThreadPoolIndex3D=8`/`CollideOutput3D`/`CollideGroupEntry3D`（并补 `class BaseArbiter3D;` 前向声明） |
| `GridSearch.hpp` | `GridSearch3D.hpp` | `Morton2D`→`Morton3D`；层基数 4→8、偏移 `(4^k-1)/3`→`(8^k-1)/7`；**稠密 `vector<unsigned int> mCellHead` 改为 `unordered_map`**（2D 的 `SetMapRange(1024)` 是 5.59e6 格 ≈21MB，同参 3D 是 9.8e9 格 ≈**39GB**）；新增 `kMaxStorey3D = 11`（保 `mGridIndex` 32 位不溢出）；`Excursion`/总格数用 **64 位中间量**；`Storey()` 保持 2D 语义（**不夹到 `mStorey`**，超大对象靠返回值判网格外）；修 2D `GetDividedVision` 循环体内又自增 `_storey` 的双重计数缺陷；新增 `GetGridDim/GetStorey/GetGridSize/GetOccupiedCellCount` 观测接口 |

## 4. 对象层

| 2D | 3D | 改写要点 |
|---|---|---|
| `PhysicsFormwork.hpp` | `PhysicsFormwork3D.hpp` | `PFGetAngle/PFSetAngle(FLOAT_)`→`PFGetRotation/PFSetRotation(Quat_)`；`PFGetInvI()->FLOAT_`→`Mat3_`；`PFAngleSpeed()->FLOAT_&`→`Vec3_&`；`AddTorque(FLOAT_,FLOAT_)`→`ApplyTorqueImpulse(const Vec3_&)`；**析构改虚**（2D 非虚析构是缺陷） |
| `PhysicsParticle.hpp/.cpp` | `PhysicsParticle3D.hpp/.cpp` | 语义逐字沿用；**修 2D `PFAngleSpeed()` 断言后 `return invMass`**（NDEBUG 下会写坏质量倒数）→ 返回独立 static 零向量 |
| `PhysicsAngle.hpp/.cpp` | `PhysicsAngle3D.hpp/.cpp` | 见 §0 的三条语义变更；新增 `GetYaw()`；`PhysicsSpeed` 的 `angleSpeed += time*(WorldInvInertia()*torque)`；`PhysicsPos` 用 `QuatFromAngularVelocity` 左乘 |
| `PhysicsShape.hpp/.cpp` | `PhysicsShape3D.hpp/.cpp` | 转动惯量采样 `5×5`→`5×5×5`，`I += m*((r·r)E − r⊗r)`；`RayCollide(Vec2_, FLOAT_ Angle)`→`RayCollide(Vec3_ Pos, Vec3_ Dir)`（三维方向需方位+俯仰，传向量无歧义）；`DropCollision` **上下界都 clamp**（2D 只夹上界）；`PsBresenhamDetection`→`PsDDADetection` 并**把法线旋转到世界系**（2D 未旋转，缺陷）；`ApproachDrop` 修正 2D 未考虑旋转 |
| `PhysicsCircle.hpp/.cpp` | `PhysicsCircle3D.hpp/.cpp` | 惯量 `0.5mr²`（圆盘）→ **`0.4mr²`（实心球，2/5）**，各向同性故 `MomentInertia = Mat3_(I)` |
| `PhysicsLine.hpp/.cpp` | `PhysicsLine3D.hpp/.cpp` | 新增 `Thickness`；拆出 `HalfLength`；`GetSegment(begin,end)`；`radius` 仍存半长以兼容宽相；朝向用**最短弧四元数**（含反向 π 退化分支）；惯量 **细杆 `mL²/12` → 胶囊公式**（3D 细杆会让 `Ixx=0` 致张量奇异） |

## 5. 碰撞 / 求解器层

| 2D | 3D | 改写要点 |
|---|---|---|
| `PhysicsBaseCollide.hpp/.cpp` | `PhysicsBaseCollide3D.hpp/.cpp` | 7 个 `CollideAABB` 补 Z 轴判据；12 个 `Collide` 重载全部改为三维几何；`segmentIntersection`（2D 线段相交）在 3D 无对应物 → 改用 `ClosestSegmentSegment`（Ericson §5.1.9，含平行/退化分支）；**新增 `IncidentFaceNormal(dir, rayDir)`**（点积自校正外向法线）与 **`PushContact(...)`**（带容量保护，根治 2D 除 SS 外 11 处无保护自增）；**新增 `ReduceManifold`**（按深度排序 + 最远点采样，上限 4）；**新增 `FeatureId`**（Knuth 哈希，供热启动按特征匹配）；**新增棱线段补测 `CollideEdges` + `MinExitDirection`**（形状-形状顶点接触 < 4 点时，用 `EdgeSet` 的棱做线段 DDA，深度取"最短逃逸轴向"，根治棱-棱 45° 穿模） |
| `PhysicsBaseArbiter.hpp/.cpp` | `PhysicsBaseArbiter3D.hpp/.cpp` | `kNormal = invM1+invM2 + n·((I1⁻¹(r1×n))×r1) + n·((I2⁻¹(r2×n))×r2)`（2D `invI(R−rn²)` 的三维推广）；切向两条同理；摩擦用 `ProjectFrictionDisk` 做**径向投影**（**刻意不逐分量钳制** —— 那会把摩擦圆盘变成正方形）；统一 2D 的 `Pn*n − Pt*t` 与 `Cross(n,-1)` 符号不一致；`WarmStartByFeature` + `CommitContacts` 取代 2D 的"上一帧数组下标"热启动（3D 的 `w_side` 语义是几何 FeatureId） |
| `PhysicsArbiter.hpp/.cpp` | `PhysicsArbiter3D.hpp/.cpp` | 12 个极薄壳逐字沿用（构造设 `mArbiterType` + `ComputeCollide()` 调对应 `Collide`）；SS/CC 保留指针重排；**删掉 2D 把 `w_side` 重编号为下标的 `FinalizeContacts`**（会抹掉 FeatureId） |

## 6. 地图层

| 2D | 3D | 改写要点 |
|---|---|---|
| `MapFormwork.hpp` | `MapFormwork3D.hpp/.cpp` | `MapOutline{Vec2_ pos, face(±1,±1), F}`→`MapOutline3D{Vec3_ pos, Vec3_ face, F}`，**`face` 语义由"2×2 角点方向"改为"外向面法线"**；`FMGetMapSize()->glm::uvec2`→`UVec3_`；`FMBresenhamDetection`→`FMDDADetection`；三个 Outline 函数补 Z 与深度参数 |
| `MapStatic.hpp/.cpp` | `MapStatic3D.hpp/.cpp` | 三个 Outline 函数改为"遍历实体体素 6 个暴露面"（mode 0=全暴露 / 1=轻量 / 2=最小凸面）；**区域参数是世界坐标**，函数内 `+= centrality` |
| `MovePlate.h` | `MovePlate3D.h` | `MovePlateInfo` 补 `Z`；回调补 z 参数；板块索引 `(x*N_y+y)*N_z+z`；滚动顺序固定 **Z→Y→X**（跳过集合逐级收窄，2D 单级跳过的三维推广）；**修 2D `new char[]`/`delete` 不匹配** → `::operator new` + placement new / `~T()` + `::operator delete` |
| `MapDynamic.hpp/.cpp` | `MapDynamic3D.hpp/.cpp` | 板块 16×16→16×16×16；**贪心正方形分配 → 贪心立方体分配**（`PlatePower` 取 2 的幂 + `Morton3D` 落位）；**两份 Bresenham 判据不一致的缺陷消除** —— 3D 直接转发 `BaseGrid3D::DDADetection`，其内部 `at()` 是虚函数会自动派发到板块查找；**修 2D 析构 `delete`/`new[]` 不匹配** |

## 7. 约束层

| 2D | 3D | 改写要点 |
|---|---|---|
| `PhysicsJoint.hpp/.cpp` | `PhysicsJoint3D.hpp/.cpp` | 2 DOF→3 DOF；`Mat2_ M`→`Mat3_ M`；`glm::inverse(K)`→`SymmetricInverse`（双静态体 K 为零矩阵时返零而非 NaN）；`AngularMassTerm(r, I⁻¹) = Skew(r)·I⁻¹·Skew(r)ᵀ`（2D `invI(R−r⊗r)` 的三维推广，天然支持各向异性）；去掉 `friend class PhysicsGPU` |
| `PhysicsJunction.hpp/.cpp` | `PhysicsJunction3D.hpp/.cpp` | `CordType`/`CordObjectType` 加 `3D`（**避免与 2D 枚举名冲突**）；`biasType`/`ElasticityType` 语义逐字沿用；转动项 `EffectiveAngularMass(r,n,I⁻¹) = (r×n)·(I⁻¹(r×n))`（3D 张量必须夹在两侧）；**`SafeDirection` 修 2D 的 `L≈0` → `dp/0` NaN 缺陷**；`ApplyImpulse` 统一补 `if (k == 0) return;` |
| `EnergyConservation.hpp` | `EnergyConservation3D.hpp` | ① 粒子重载：2D 用 acos 角度分解 + 一维对心弹性碰撞 → 3D 改为**向量点积/投影**，数学等价、无三角函数、零速度不产生未定义角；② 刚体重载：2D 是"三次链式角度分解 + 一元二次方程求根"且自承"假定碰撞点在质心连线上" → 3D 改为**标准刚体弹性冲量解**，有效质量 `K = invMa+invMb + n·((Ia⁻¹(rA×n))×rA) + n·((Ib⁻¹(rB×n))×rB)`（与 `PhysicsBaseArbiter3D` 同源），支持各向异性惯量张量、不要求接触点在三心线上；`Vertical` 由 `Vec2_` 角度改为 `Vec3_` 法向量。2D 的 `EnergyConservationOptimum` 宏在 3D 无对应物（冲量解法不需要判别式） |

## 8. 子系统层

| 2D | 3D | 改写要点 |
|---|---|---|
| `PhysicsCollision.hpp/.cpp` | `PhysicsCollision3D.hpp/.cpp` | 纯事件分发，维度无关；**修 2D `AddCollisionPair` 中 B 侧 OnEnter 误用 `Ait->first`**（`Ait == end()` 时取 `->first` 属 UB）→ 统一用 `mOriginalObject1/2` 解析 |
| `PhysicsKinematic.hpp/.cpp` | `PhysicsKinematic3D.hpp/.cpp` | `RotateTo(FLOAT_)`→`RotateTo(const Quat_&)`；角插值由**纯线性**改为 **`glm::slerp`**（最短弧、角速度均匀）；`invMomentInertia = Mat3_(0)` / `SymmetricInverse(MomentInertia)`；平移用整体 `Lerp` |
| `PhysicsTrigger.hpp/.cpp` | `PhysicsTrigger3D.hpp/.cpp` | `Bounds`→`Bounds3D`；**新增 `SetLayerProvider`** 让 2D 的死字段 `TriggerLayers` 真正生效；精确检测由"仅点包含"改为"**点包含 ∨ 球盒重叠**"；**修 2D `nearbyObjects` 跨触发器不清空的污染** |
| `PhysicsAssembly.hpp/.cpp` | `PhysicsAssembly3D.hpp/.cpp` | 纯指针身份比较，维度无关；**修 2D三处缺陷**：`Add(nullptr)`/`RemoveChild(nullptr)` 崩溃、`BuildChildDescriptors` 不跳 null |
| `PhysicsLiquid.hpp/.cpp` | *（未移植）* | 2D 是 PBF（Position Based Fluids）。3D 化需换核常数（`4/(πh⁸)`→`315/(64πh⁹)`、`10/(πh⁵)`→`45/(πh⁶)`）、重算 `restDensity`/`eps`/`cohesion`/`curvature` 标定、邻域 3×3→**3×3×3**、`kMaxNeighbors` 64→**128~160**（3D 球内 ≈60.6 点会贴死上限）、边界 8 方向→**26 方向**、刚体拟合的标量叉积→`L=Σr×d` 与 3×3 求逆。整段包在 `#if PhysicsLiquid3DBool3D`（默认 **0**），`PhysicsWorld3D` 已预留 `mLiquid` 挂点与 `NotifyParticleRemoved` 调用位 |
| `PhysicsGPU.hpp/.cpp` + 3 个 shader | *（明确排除）* | 三个 compute shader 全部硬编码 2D（`BODY_STRIDE 8`、`cross2D`、`vec2 tangent = vec2(normalY,-normalX)`）。3D 化须把 body stride 从 8 扩到 18~21 并重写 `cross3` 与 3×3 逆惯量。代价仅失去冲量迭代的 GPU 加速；CPU 多线程路径完整保留 |

## 9. 世界层

| 2D | 3D | 改写要点 |
|---|---|---|
| `PhysicsWorld.hpp/.cpp` | `PhysicsWorld3D.hpp/.cpp` + `PhysicsWorld3DSerialization.cpp` | **九阶段骨架、`ThreadTaskAllot`、线程池、内存池宏、arbiter 生命周期全部维度无关，逐字保留**；类型全部换 3D；`Definite` 分支与"多线程 PreStep 永久编译掉"的决定沿用；**新增 `MergeCollideOutputs` 实现**（2D 只有声明没有定义）；`SameAssembly` 补 null 检查；`Map_Delete3D` 先 find 再入队（修 2D 不过滤的浪费）；`GridWind` 用 `new Vec3_[]`/`delete[]`（**修 2D `new[]`/`delete` 不匹配**）；`XT_Fun` 里的 `(PhysicsParticle*)i` 强转改为 `static_cast<PhysicsParticle3D*>`（3D 所有物体都继承它） |

## 10. 调试 / 渲染层

| 2D | 3D | 改写要点 |
|---|---|---|
| `ImGuiPhysics.hpp/.cpp` | `ImGuiPhysics3D.hpp/.cpp` | 去掉 `PhysicsUI(PhysicsLiquid*)` 重载；`.ini` 改 `PhysicsBlock3D.ini`；惯性张量编辑器由 1 个标量改为 3×3 |
| `PhysicsAuxiliaryVision.hpp/.cpp` | `PhysicsAuxiliaryVision3D.hpp/.cpp` | `VulkanTool/AuxiliaryVision.h` 的 API **本来就是 `glm::dvec3`**（2D 代码写 `{obj->pos, 0}` 把画面压到 z=0 平面），3D 直接传真三维坐标；去掉 `DrawLiquid`；`OutlineSet` 由三条平行数组改为结构体向量 |

---

## 11. 顺带修掉的 2D 既有缺陷（清单）

除第 1 条外，其余各项都只在 3D 版中修正、**2D 源码未改动**：

1. `Tool/MemoryPool.h:138,161` —— `freeSlots_ = memoryPool.freeSlots;` 成员名笔误（实为 `freeSlots_`）。
   → 3D 版 `PhysicsBlock3D/MemoryPool3D.h` 修正了该笔误，并**把类放进 `namespace PhysicsBlock3D`**：
   2D 版在**全局作用域**定义同名 `class MemoryPool`，而 Vulkan 侧
   （`VulkanTool/AuxiliaryVision.h` 的 include 链会经 `Vulkan/description.h →
   VulKanTool/texture.h → base.h → Tool/Tool.h` 拉进 2D 的 `Tool/MemoryPool.h`），
   两者同为全局名会直接重定义冲突。命名空间隔离后两个副本可在同一翻译单元中共存。
   → **同时把 `PixelClean/Tool/MemoryPool.h` 的这处笔误也修掉了**（改动前原文件备份为
   构建目录下的 `MemoryPool.h.2d-backup`）。这是整个工程**唯一一处对 2D 侧的改动**，
   属于纯 bug 修复：修改前该文件根本编译不过，因此不可能改变任何既有行为。
   **副产品：2D 引擎由此恢复可编译** —— `PhysicsBlock/PhysicsWorld.cpp`、
   `PhysicsLiquid.cpp`、`ImGuiPhysics.cpp`、`PhysicsAuxiliaryVision.cpp` 这四个
   此前被该笔误阻断的翻译单元现已全部编译通过。
2. `BaseOutline.cpp:14-23` —— 按 `W*H*3` 分配但单格最多推 4 角点，最坏 `4*W*H` 越界 → 3D 改动态 `std::vector`。
3. `BaseOutline` 的 JSON 构造分支只 new 了 `FrictionSet` 与 `OutlineSet`，漏 `MaxOutlineCentreMass` → 3D 由结构体向量天然免除。
4. `PhysicsParticle::PFAngleSpeed()` 断言后 `return invMass`（NDEBUG 下静默写坏质量倒数）→ 3D 返回独立零向量。
5. `PhysicsBaseCollide.cpp` 除 SS 外 11 处 `contacts[ContactSize++]` 无上限保护 → 3D 统一走带保护的 `PushContact`。
6. `PhysicsBaseCollide.cpp:745,885,949` 用 `ModulusLength`（平方长度）做投影分母 → 3D 用 `len²`。
7. `PhysicsBaseCollide` 12 处法线符号不统一（`A` 分支 PreStep 与 ApplyImpulse 互相矛盾）→ 3D 统一"object1→object2"。
8. `BaseGrid` 与 `MapDynamic` 两份 Bresenham 判据不一致（一处含等号一处不含）→ 3D 统一到一条 DDA 路径。
9. `GridSearch::GetDividedVision` 循环体自增 `_storey` 导致只输出约一半层 → 3D 单步推进。
10. `PhysicsCollision.cpp:204-206` B 侧 OnEnter 误用 `Ait->first`（UB）→ 3D 用 `mOriginalObject1/2`。
11. `PhysicsTrigger` 的 `TriggerLayers` 是死字段、精确检测只有点包含、`nearbyObjects` 跨触发器污染 → 3D 三处全修。
12. `PhysicsJunction` 各 `PreStep` 缺 `L≈0` 保护（两端重合产生 NaN）→ 3D 加 `SafeDirection`。
13. `PhysicsKinematic` 角度纯线性插值（3D 会绕远路）→ 3D 用 `slerp`。
14. `PhysicsAssembly::Add(nullptr)` / `RemoveChild(nullptr)` 崩溃 → 3D 加 null 检查。
15. `MovePlate.h` `new char[]`/`delete` 与 `MapDynamic.cpp` `new[]`/`delete` 不匹配（UB）→ 3D 全部配对。
16. `PhysicsWorld.hpp:318` `MergeCollideOutputs` 只有声明没有定义 → 3D 补实现。
17. `PhysicsWorld` 析构 `delete GridWind`（配 `new Vec2_[]`）→ 3D 用 `delete[]`。
18. `PhysicsWorld::XT_Fun:377,445` 把任意类型强转 `PhysicsParticle*` 读 `StaticNum` → 3D 用 `static_cast<PhysicsParticle3D*>`（所有 3D 物体都继承它，安全）。
19. `PhysicsFormwork` 非虚析构 → 3D 改虚析构。
20. `PhysicsWorld3D` 序列化：`TOSIRING3D` 宏对 member 表达式失效导致键名两侧不一致（在 3D 实现过程中发现的新缺陷，已修并加 `@warning` 文档）。
21. `PhysicsKinematic::SetIsKinematic` 对**任意**物体无条件调用 `PFAngleSpeed()`，对裸粒子会触发 `assert(0 && "[Error]: 粒子不存在角速度!")`（2D 里的潜伏缺陷，只有把粒子设为运动学才会踩到）→ 3D 加 `HasRotation3D()` 判据，只对 shape/circle/line 操作角速度。

**刻意保留**（与 2D 行为一致，不视为缺陷）：
- 物体↔地图碰撞走 `AddMapCollision*Listener`，物体↔物体走 `AddCollision*Listener`。
- `GridSearch3D::Storey()` 不夹到 `mStorey`（超大对象靠返回值判网格外，进 `GridExtrovert`）。
- 多线程 PreStep 永久编译掉（PreStep 含冲量副作用，多线程无法收敛）。
- PhysicsJunction 的 `lever` 类型走 `ElasticityType()` 的 default 分支（返回 1）。
