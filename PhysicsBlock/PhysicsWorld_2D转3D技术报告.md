# PhysicsWorld / PhysicsEmulator 2D→3D 迁移技术报告

> 范围：`PhysicsWorld.cpp`(2163 行) + `PhysicsWorld.hpp`(567 行)，并交叉核对本引擎其余类。
> 目录：`/storage/emulated/0/DSH/PixelClean/PhysicsBlock/`。行号均指该目录下同名文件。
> 本报告只做技术分析，不含新代码；引用代码为原文摘录。命名空间 `PhysicsBlock`。

---

## 0. 结论速览

| 维度 | 2D 现状 | 3D 必须改动 |
|---|---|---|
| 类型系统 | `BaseStruct.hpp:12-26` `#define FLOAT_ float / Vec2_ glm::vec2 / Mat2_ glm::mat2` | 新增 `Vec3_ = glm::vec3`、`Mat3_ = glm::mat3`、`Quat_ = glm::quat` |
| 姿态 | 标量 `FLOAT_ angle`（`PhysicsAngle.hpp:40`） | `glm::quat`（或 mat3），需重新定义 `PFGetAngle/PFSetAngle` 返回类型 |
| 角速度/扭矩 | 标量 `angleSpeed`/`torque`（`PhysicsAngle.hpp:46,52`） | `Vec3_` |
| 惯量 | 标量 `MomentInertia`/`invMomentInertia`（`PhysicsAngle.hpp:28,34`） | 3×3 惯量张量及其逆（对角化可存 vec3） |
| 叉积 | `Cross(Vec2_,Vec2_)→FLOAT_`（`BaseCalculate.hpp:433`） | `glm::cross`→`Vec3_`；标量×向量两种重载语义整体重写 |
| 空间索引 | `GridSearch` 四叉树 + `Morton2D`（`GridSearch.hpp:165,196`） | 八叉树 + `Morton3D` + 3 轴 AABB 包含判定 |
| 接触 | `Contact{Vec2_ normal/r1/r2; FLOAT_ Pt}`（`BaseArbiter.hpp:13-38`） | `normal/r1/r2`→`Vec3_`，切向需 2 个基向量（`Pt2/massTangent2`） |
| 方向枚举 | `CheckDirection{Right,Up,Left,Down}`（`BaseStruct.hpp:90-96`） | 6 面（±X,±Y,±Z），`Direction:2` 位域需 3 bit |
| 地图 | `GridBlock[width*height]`（`BaseGrid.hpp:31,67`） | `width*height*depth` 体素 + 3D DDA |
| 线程 | `ThreadTaskAllot` 纯一维索引切分（`PhysicsWorld.hpp:37-58`） | **无需改**，可原样复用 |

**关键判断**：`PhysicsEmulator` 的九阶段骨架、线程池组织、arbiter 生命周期（`HandleCollideGroup`/`ResolveCollideGroup`/`DeleteArbiter`）、内存池宏全部**与维度无关**，可原样保留；真正需要重写的是 ①`BaseStruct.hpp` 类型别名层 ②`BaseCalculate.hpp` 的 2D 数学核 ③`GridSearch` 空间索引 ④窄相 `PhysicsBaseCollide.cpp` 的法线生成 ⑤arbiter 求解器中的标量角量 ⑥地图/液体的 2D 网格布局。

---

## 1. `PhysicsEmulator(FLOAT_ time)` 精确阶段结构

签名：`void PhysicsWorld::PhysicsEmulator(FLOAT_ time)`，`PhysicsWorld.cpp:330-895`（声明 `PhysicsWorld.hpp:548`）。
文档注释 `PhysicsWorld.cpp:316-329` 声明为 8 个阶段，**实际代码为 9 段**（PreStep 与 ApplyImpulse 之间还有 GPU 分支，且阶段 1 的等待发生在计时起点之后）。

### 阶段 1：计时起点 + 等待上一帧异步碰撞检测（:332-344）
- `:332` `auto tEmulatorStart = std::chrono::high_resolution_clock::now();`
- `:335-336` `const unsigned int xThreadNumHW = std::thread::hardware_concurrency(); const unsigned int xThreadNum = std::min(xThreadNumHW, 8u);`
- `:339-343` `for (auto &tf : xTn) { tf.wait(); } xTn.clear();` —— 等待上一帧末尾（阶段 9）入队、**故意未等待**的 `XT_Fun` 碰撞检测任务。

### 阶段 2：结算碰撞对增删（:345-348）
- `:345` `ResolveCollideGroup();`
- `:347-348` `mCollisionDetectionTimeMS = duration<float,milli>(tCollisionEnd - tEmulatorStart)` —— **注意计时区间从阶段 1 的等待开始算起**，因此该计时实际覆盖「等待上一帧碰撞检测 + 结算」，而非本帧碰撞检测本身。

### 阶段 3：多线程碰撞检测（宽相 + 窄相），:350-555
- `:350` `const auto XT_Fun = [this](int T_Num, int Tx, CollideOutput *output)`
- `:352` `tlCollideOutput = output;`（thread_local 指向本线程输出缓冲）；`:554` `tlCollideOutput = nullptr;`
- `:357` 局部复用 `std::vector<PhysicsFormwork*> SearchV;`（宽相结果，跨对象复用不释放）
- 四个子扫描，每个都以 `ThreadTaskAllot(SizeD, SizeY, <vector>.size(), T_Num, Tx)` 切分：
  1. `:359-425` `PhysicsShapeS`。每对象：`JZ = (StaticNum > 10)`（休眠跳过）；`if (!JZ && mass != FLOAT_MAX) Arbiter(shape, wMapFormwork);`（地形）；`mGridSearch.Get(pos, radius, SearchV)`（宽相）；对候选 `i` 按 `PFGetType()` 分派 circle/particle/shape，含静止对跳过、`SameAssembly` 跳过、`CollideAABB` 早退、`Map_Delete(key)`、`Arbiter(a,b)`。
  2. `:427-483` `PhysicsCircleS`（同构）。
  3. `:485-539` `PhysicsLineS`（对 particle 无 AABB 预筛，:522）。
  4. `:541-552` `PhysicsParticleS`：`if (IsLiquidParticle || StaticNum > 10) continue;` 仅 `Arbiter(particle, wMapFormwork);`，**不参与网格宽相**。

### 阶段 4：逆时间步（:557）
- `:557` `FLOAT_ inv_dt = 1.0 / time;`

### 阶段 5：力/重力积分（速度更新），:559-630
- `:566-595` `const auto PhysicsSpeedXT_Fun = [this, time](int T_Num, int Tx)`：`ThreadTaskAllot` 依次切分 `PhysicsShapeS` / `PhysicsParticleS`（跳过 `IsLiquidParticle`）/ `PhysicsCircleS` / `PhysicsLineS`，调用 `->PhysicsSpeed(time, GravityAcceleration)`。
- `:596-606` 按 `xThreadNum` 入队 + 等待 + `xTn.clear()`；`:607-630` 为 `#else` 串行回退。
- 顺序说明 `:559-564`：本引擎把 `v += dt*(g + F/m)` 放在约束求解**之前**，求解后 `x += dt*v`；旧实现把力加在位置更新之后导致滞后一帧。

### 阶段 6：PreStep（:632-687）
- `:634` `#define Definite 0`
- `:637` `#if (Definite != 1) & ThreadPoolBool & 0` —— **多线程 PreStep 路径被 `& 0` 永久编译掉**；注释 `:636` 理由：PreStep 含冲量副作用（改速度/角速度），多线程无法收敛静止物体。
- `:670-684` 生效的串行路径：`for (const auto i : CollideGroupVector) i->PreStep(inv_dt);` → `for (auto J : PhysicsJointS) J->PreStep(inv_dt);` → `for (auto J : BaseJunctionS) J->PreStep(inv_dt);`
- `:686-687` `mPreStepTimeMS`。

### 阶段 7：ApplyImpulse 迭代（:690-764）
- `:692-696` GPU 分支：`if (mGPU && mGPU->IsReady() && mUseGPUApplyImpulse) { mApplyImpulseCPUTimeMS = 0.0f; mGPU->ExecuteGPUApplyImpulse(inv_dt, ApplyImpulseSize); }`
- `:702-738` CPU 多线程：`const auto ApplyImpulseXT_Fun = [this](int T_Num, int Tx)` 切分 `CollideGroupVector` / `PhysicsJointS` / `BaseJunctionS` 并调用 `->ApplyImpulse()`；外层 `:725` `for (size_t di = 0; di < ApplyImpulseSize; ++di)` **每轮整批重新入队 + 全屏障等待**。
- `:739-759` 串行回退：三层嵌套 for，外层迭代 `ApplyImpulseSize` 次。
- `:763-764` `mApplyImpulseCPUTimeMS`。

### 阶段 8：位置积分 + 运动学（:766-840）
- `:771-801` `const auto PhysicsPosXT_Fun = [this, time](int T_Num, int Tx)`：切分四类向量调用 `->PhysicsPos(time, GravityAcceleration)`（粒子跳过液体）。
- `:802-812` 入队 + 等待；`:813-835` 串行回退。
- `:837` `mKinematic.UpdateKinematicMotion(time);`（位于位置计时区间内）
- `:839-840` `mPositionUpdateTimeMS`。

### 阶段 9：后处理（:842-894）
- `:844` `mCollision.ProcessCollisions();`（碰撞回调 OnStay/Enter/Exit）
- `:847` `mTrigger.ProcessTriggers(mGridSearch);`
- `:851-855` `if (mGridPositionDirty) { mGridSearch.UpdateGridOffset(mPendingGridCenter); mGridPositionDirty = false; }`
- `:856-866` `xTn.reserve(xThreadNum);` 然后 `for i in [0,xThreadNum)` `mThreadPool.enqueue(&GridSearch::UpDaraWorkeTask, &mGridSearch, xThreadNum, i)` → 等待 → `mGridSearch.UpDaraWorkeTaskEnd();`
- `:871-874` `if (mLiquid != nullptr) mLiquid->Update(time);`
- `:876-891` 重建 `mCollideOutputs`（`clear()` + `resize(xThreadNum)`），逐线程 `mCollideOutputs[i].mThreadIndex = (unsigned char)i;` 后 `xTn.push_back(mThreadPool.enqueue(XT_Fun, i, xThreadNum, &mCollideOutputs[i]));` —— **故意不等待**，留到下一帧阶段 1；`#else` 分支 `:890` `XT_Fun(1, 1, nullptr);`
- `:893-894` `mPostProcessTimeMS`。

### 附：`PhysicsInformationUpdate()`（:902-1157）差异
同构但**无**力积分/冲量/位置三段；碰撞检测 `XT_Fun` **去掉休眠跳过**（`StaticNum`/`JZ` 判定移除，地形 arbiter 无条件调用，见 :928/:987/:1035）；PreStep 走**无参** `i->PreStep()`（:1106/:1124）；末尾同样异步入队 `XT_Fun`。

---

## 2. 全部 2D 特定假设（含行号）

### 2.1 类型别名层（改动的总闸门）
- `BaseStruct.hpp:12-26`：`#if 1` 分支 `#define FLOAT_ float` / `#define Vec2_ glm::vec2` / `#define Mat2_ glm::mat2` / `#define FLOAT_MAX FLT_MAX` / `#define SQRT_ sqrtf`；`#else` 分支为 double/dvec2/dmat2。**全引擎仅此一处决定维度类型**，但 `Mat2_` 被直接用于 `PhysicsJoint.cpp` 的 K 矩阵（2×2）。
- `BaseDefine.h:6` `#define PixelBlockEdgeSize 16`（像素块边长，须为 2 的幂）；`:34` `PhysicsContactMaxSize 20`；`:36` `PhysicsApplyImpulseSize 10`；`:38` `PhysicsSleepThreshold 10`；`:57` `PhysicsGPUBool 1`。
- `PhysicsWorld.hpp:24` `#define MemoryPoolBool 1`；`:27` `#define ThreadPoolBool 1`。

### 2.2 `Vec2_` 位置/速度/力（基础物体接口）
- `PhysicsFormwork.hpp:27` `virtual void PhysicsSpeed(FLOAT_ time, Vec2_ Ga) {};`；`:33` `virtual void PhysicsPos(FLOAT_ time, Vec2_ Ga) {};`
- `PhysicsFormwork.hpp:47` `virtual Vec2_ PFGetPos(){ return {0,0}; }`；`:73` `virtual Vec2_& PFSpeed() = 0;`；`:83` `virtual void AddForce(Vec2_ Force)`；`:90` `virtual void AddForce(Vec2_ Pos, Vec2_ Force)`；`:96` `virtual void ApplyImpulse(const Vec2_& impulse)`；`:103` `virtual void ApplyImpulse(const Vec2_& impulse, const Vec2_& worldPoint)`。
- `PhysicsParticle.hpp:50,62,68,74` `Vec2_ OldPos/pos/speed/force`；`:108,115` 构造 `(Vec2_ Pos, ...)`。
- `PhysicsWorld.hpp:226` `Vec2_ GravityAcceleration;`；`:228` `Vec2_ Wind{0};`；`:229` `Vec2_ *GridWind = nullptr;`；`:230` `glm::uvec2 GridWindSize{0};`；`:233` `Vec2_ mGridCenter{0};`；`:235` `Vec2_ mPendingGridCenter{0};`

### 2.3 标量角 / 标量角速度 / 标量惯量 / 标量扭矩（最深的耦合）
- `PhysicsAngle.hpp:28` `FLOAT_ MomentInertia = 1;`；`:34` `FLOAT_ invMomentInertia = 1;`；`:40` `FLOAT_ angle = 0;`；`:46` `FLOAT_ angleSpeed = 0;`；`:52` `FLOAT_ torque = 0;`
- `PhysicsAngle.hpp:171` `virtual FLOAT_ PFGetInvI() { return invMomentInertia; }`；`:177` `virtual FLOAT_ PFGetAngle() override { return angle; }`；`:183` `virtual void PFSetAngle(FLOAT_ a) override { angle = a; }`；`:190` `virtual FLOAT_ &PFAngleSpeed() { return angleSpeed; }`
- `PhysicsFormwork.hpp:77` `virtual FLOAT_& PFAngleSpeed() = 0;`；`:111` `virtual void ApplyTorqueImpulse(FLOAT_ torqueImpulse)`。
- `PhysicsAngle.cpp:54` `torque += Arm.x * Force.y - Arm.y * Force.x;`（标量叉积 = 2D 扭矩）
- `PhysicsAngle.cpp:70-73` `void PhysicsAngle::AddTorque(FLOAT_ ArmForce, FLOAT_ Force) { torque += ArmForce * Force; }`
- `PhysicsAngle.cpp:105` `angleSpeed += invMomentInertia * (r.x * impulse.y - r.y * impulse.x); // Cross(r, impulse)`
- `PhysicsAngle.cpp:119` `angleSpeed += invMomentInertia * torqueImpulse;`
- `PhysicsAngle.cpp:139-141` `angleSpeed *= MinSpoilageFactor(time);` … `angleSpeed += time * invMomentInertia * torque; torque = 0;`
- `PhysicsAngle.cpp:158` `angle += time * angleSpeed;`（标量角积分，**无四元数归一化、无陀螺项**）
- `PhysicsWorld.cpp:1690,1703` `Energy += i->MomentInertia * i->angleSpeed * i->angleSpeed;`（`GetWorldEnergy`，2D 转动动能 ½Iω²）
- `PhysicsCircle.cpp:25` `MomentInertia = FLOAT_(0.5) * mass * radius * radius;`（圆盘 2D 惯量）
- `PhysicsLine.cpp:16` `MomentInertia = radius * radius * mass_ / 12;`（细杆 2D 惯量）
- `PhysicsShape.cpp:144` `MomentInertia += Lmass * (lpos.x * lpos.x + lpos.y * lpos.y);`（逐像素点质量算 2D 惯量，采样 `MomentInertiaSamplingSize`）
- `PhysicsShape.cpp:115` `MomentInertia = 0;`；`:150-153` `invMomentInertia = 1.0 / MomentInertia;` + 除零置 `FLOAT_MAX`
- `PhysicsKinematic.cpp:181` `static_cast<PhysicsAngle *>(object)->invMomentInertia = 0.0f;`；`:204-206` `if (pa->MomentInertia > 0.0f) pa->invMomentInertia = 1.0f / pa->MomentInertia;`
- `PhysicsLiquid.cpp:1119` `body->angle += dTheta;`；`:1227,1231` `FLOAT_ dw = st.accumR / time; body->angleSpeed += dw;`

### 2.4 标量叉积 / 垂直向量（`x*y-y*x`）
- `BaseCalculate.hpp:433-436` `FLOAT_ inline Cross(const Vec2_ &a, const Vec2_ &b) { return a.x*b.y - a.y*b.x; }`
- `BaseCalculate.hpp:444-447` `Vec2_ inline Cross(const Vec2_ &a, FLOAT_ s) { return Vec2_(s*a.y, -s*a.x); }`
- `BaseCalculate.hpp:455-458` `Vec2_ inline Cross(FLOAT_ s, const Vec2_ &a) { return Vec2_(-s*a.y, s*a.x); }`
- `BaseCalculate.hpp:421-424` `FLOAT_ inline Dot(const Vec2_ &a, const Vec2_ &b) { return a.x*b.x + a.y*b.y; }`
- `BaseCalculate.hpp:81-84` `ModulusLength` = `x*x + y*y`；`:91-94` `Modulus` = `sqrt(x*x + y*y)`（均缺 z²）
- `BaseCalculate.hpp:343` `FLOAT_ TorqueCalculate(Vec2_ Barycenter, Vec2_ Spot, Vec2_ Force);`（2D 标量扭矩）
- `PhysicsBaseArbiter.cpp:88` `Vec2_ tangent = Cross(c->normal, 1.0); // 垂直于法向量的切向量`（2D 旋转 90°，3D 需切向基）
- `PhysicsBaseArbiter.cpp:103,109,143,148,173,178,274,313,342,444,479,500` `angleSpeed -= object->invMomentInertia * Cross(c->r1, P);`（全文件 12 处标量角冲量）
- `PhysicsBaseArbiter.cpp:126-127` `Vec2_ dv = object2->speed + Cross(object2->angleSpeed, c->r2) - object1->speed - Cross(object1->angleSpeed, c->r1);`（标量 ω × r 得 2D 线速度）
- `PhysicsJoint.cpp:66-69` `K2[0][0] = body1->invMomentInertia * r1.y * r1.y; K2[0][1] = -body1->invMomentInertia * r1.x * r1.y; K2[1][0] = -...; K2[1][1] = ... * r1.x * r1.x;`（`Mat2_` 有效质量矩阵）
- `PhysicsJoint.cpp:77` `Mat2_ K = K1 + K2 + K3;`；`:84,86,97,99` `body->angleSpeed ±= body->invMomentInertia * Cross(r, P);`；`:93` `Vec2_ dv = body2->speed + Cross(body2->angleSpeed, r2) - ...`
- `PhysicsJunction.cpp:52-53` `k += mParticle1->invMomentInertia * r1CrossN * r1CrossN;`（标量叉积平方作有效质量）；`:57,59,72,80,83,123,128,143,151` 同类标量角更新
- `PhysicsLiquid.cpp:1107` `rot += r.x * d.y - r.y * d.x;`（刚体拟合的 2D 标量转动量）；`:1111` `FLOAT_ dTheta = rot / den;`
- `shaders/physics_arbiter.comp:43` `float cross2D(vec2 a, vec2 b) { return a.x * b.y - a.y * b.x; }`；`:90,96,100-101,119,125,175,183,201,252,255,272` 全部 `cross2D` 用法；`:106,188,260,318` `vec2 tangent = vec2(normalY, -normalX);`

### 2.5 `vec2angle` / 标量角旋转
- `BaseCalculate.hpp:101-104` `Vec2_ inline AngleFloatToAngleVec(FLOAT_ angle) { return {cos(angle), sin(angle)}; }`
- `BaseCalculate.hpp:133` `Vec2_ vec2angle(Vec2_ pos, FLOAT_ angle);`；`:142-145` `Vec2_ inline vec2angle(Vec2_ pos, Vec2_ angle) { return Vec2_((pos.x*angle.x)-(pos.y*angle.y), (pos.x*angle.y)+(pos.y*angle.x)); }`（角打包成 `{cos,sin}`）
- `BaseCalculate.hpp:112-124` `FLOAT_ inline EdgeVecToCosAngleFloat(Vec2_ XYedge)`（acos 求与 +X 轴夹角，依赖参考轴）
- `BaseCalculate.hpp:155-158` `Vec2_ inline vec2PosAngle(Vec2_ pos, Vec2_ lingdian, FLOAT_ angle)`；`:168-171` 重载
- `BaseCalculate.hpp:180-183` `glm::ivec2 inline ToIntPos(Vec2_ Pos, Vec2_ xPos, FLOAT_ angle)`；`:192-195` 重载
- `BaseCalculate.hpp:229-275` `struct AngleMat { FLOAT_ Cos; FLOAT_ Sin; AngleMat(FLOAT_ angle); void SetAngle(FLOAT_ angle); Vec2_ Rotary(Vec2_ pos); Vec2_ Anticlockwise(Vec2_ pos); }`（缓存 cos/sin 的 2D 旋转器，全引擎高频使用）
- `BaseCalculate.hpp:205-209` `LineXToPos(Vec2_, Vec2_, FLOAT_ x)`；`:219-223` `LineYToPos(Vec2_, Vec2_, FLOAT_ y)`（轴对齐线插值）
- `BaseCalculate.cpp:47-49` `Vec2_ vec2angle(Vec2_ pos, FLOAT_ angle) { return vec2angle(pos, Vec2_{cos(angle), sin(angle)}); }`
- `PhysicsShape.cpp:39` `Pos = vec2angle(Pos, -angle);`；`:111` `pos += vec2angle(CentreMass - UsedCentreMass, angle);`；`:214` `Vec2_ OutlineDrop = vec2angle({radius, 0}, EdgeVecToCosAngleFloat(drop - pos));`；`:231` `Vec2_ drop = vec2angle({radius, 0}, Angle);`
- `PhysicsBaseCollide.cpp:123,144,737,842,874,919` `Vec2_ pR = vec2angle({Line->radius, 0}, Line->angle);`
- `PhysicsJunction.cpp:45-46` `mR1 = vec2angle(mArm1, mParticle1->angle); mR2 = vec2angle(mArm2, mParticle2->angle);`；`:123` `R = vec2angle(Arm, mParticle->angle);`
- `PhysicsWorld.cpp:1671` `Drop = DropUptoLineShortesIntersect(i->pos + vec2angle({i->radius, 0}, i->angle), i->pos - vec2angle({i->radius, 0}, i->angle), pos);`
- `PhysicsLiquid.cpp:266` `return vec2angle(p, Vec2_{cs.x, -cs.y});`；`:529,1106,1128` 采样点世界化 `body->pos + vec2angle(local, cs)`

### 2.6 4 方向 / 2 轴 AABB
- `BaseStruct.hpp:90-96` `enum CheckDirection { Right = 0U, Up = 1U, Left = 2U, Down = 3U };`（**恰好 4 面**）
- `BaseStruct.hpp:111-126` `struct CollisionInfoD { union { unsigned char Flag; struct { bool Collision:1; unsigned char Direction:2; }; }; Vec2_ pos; glm::ivec2 GridPos; FLOAT_ Friction; }`（`Direction:2` 仅 2 bit）
- `BaseStruct.hpp:101-106` `struct CollisionInfoI { bool Collision; glm::ivec2 pos; FLOAT_ Friction; }`
- `BaseStruct.hpp:130-140` `struct DecompositionForce { Vec2_ Vartical; Vec2_ Parallel; }` / `DecompositionForceVal { FLOAT_ Vartical; FLOAT_ Parallel; }`（2D 法/切分解）
- `BaseStruct.hpp:144-149` `struct SquareFocus { bool Focus; Vec2_ start; Vec2_ end; }`
- `BaseStruct.hpp:153-162` `enum PhysicsObjectEnum { Null, shape, particle, circle, line, _MapStatic, _MapDynamic }`（`circle`/`line` 是 2D 语义，3D 需 sphere/capsule/box）
- `BaseStruct.hpp:39-87` `struct GridBlock { ... union { struct { unsigned char DistanceField; unsigned char DirectionField; }; ... } }`（标量距离场 + 标量方向场，只编码 4 向之一）
- `PhysicsBaseCollide.cpp:183-190` 分离深度按轴向二选一：`if (info.Direction & 0x1) contacts->separation = info.pos.y - DropPos.y; else contacts->separation = info.pos.x - DropPos.x;`（**2 轴特化**，3D 需 3 选一）
- `PhysicsBaseCollide.cpp:196,226,267,318,360,405,456,593,774,797,821,966,989,1013` 全部法线生成：`contacts->normal = vec2angle({-1, 0}, (info.Direction * (M_PI / 2)) + Shape->angle);`（**4 个轴对齐方向之一再叠标量角**）
- `PhysicsBlockTypes.hpp:159-163` `inline unsigned char DirectionFromNormal(Vec2_ normal) { float ax = std::abs(normal.x), ay = std::abs(normal.y); if (ax > ay) return (normal.x > 0) ? 2 : 0; else return (normal.y > 0) ? 1 : 3; }`（2 分量反推 4 向）
- `PhysicsBlockTypes.hpp:142-147` `struct BulletHitInfo { Vec2_ WorldPos; glm::ivec2 GridPos; unsigned char Direction; Vec2_ Normal; }`
- `PhysicsBlockTypes.hpp:71-121` `struct Bounds { Vec2_ Center; Vec2_ HalfSize; bool Contains(const Vec2_&) const; bool Overlaps(const Bounds&) const; }`（2 轴区间）
- `PhysicsCollision.hpp:184` `void OnMapCollisionChanged(MapFormwork *map, glm::ivec2 pos, bool newState);`；`PhysicsCollision.cpp:399` 定义
- `PhysicsCollision.cpp:240` `hitInfo.Direction = DirectionFromNormal(arbiterKey->contacts[0].normal);`
- `PhysicsTrigger.cpp:157-158` 触发区 minPos/maxPos 为 `Vec2_`（2 轴 AABB）
- `PhysicsWorld.cpp:1202-1205` 地图通知桥 lambda `[this, MapFormwork_](glm::ivec2 pos, bool state)`；`MapFormwork.hpp:136` `SetCollisionChangeNotifier(std::function<void(glm::ivec2, bool)>)`；`:153` 成员
- `MapFormwork.hpp:44` `virtual glm::uvec2 FMGetMapSize()`；`:55,67,94,103,111` 均 `glm::ivec2`；`:119,127` `SetFriction/GetFriction(int x, int y, ...)`

### 2.7 Bresenham / 2D 像素网格行进
- `BaseGrid.hpp:31` `Grid = new GridBlock[width * height];`；`:67` `virtual inline GridBlock &at(int x, int y) { return at(x * height + y); }`（**x*height+y 二维行主序**）；`:75` `at(glm::ivec2 pos) { return at(pos.x * height + pos.y); }`
- `BaseGrid.hpp:111` `virtual inline bool GetCollision(glm::uvec2 Pos)`；`:147` `CollisionInfoI BresenhamDetection(glm::ivec2 start, glm::ivec2 end);`；`:156` `CollisionInfoD BresenhamDetection(Vec2_ start, Vec2_ end);`
- `BaseGrid.cpp:102-178` `CollisionInfoD BaseGrid::BresenhamDetection(Vec2_ start, Vec2_ end)`：`CollisionInfoI info = BresenhamDetection(ToInt(start), ToInt(end));`（:105），`FLOAT_ Difference = (end.x - start.x) / (start.y - end.y);`（:120），`calcFromX`/`calcFromY` 分别判定 Left/Right 与 Down/Up（:127-149），并按「垂直/水平相邻格」二次校正（:152-168）。
- `PhysicsShape.hpp:132,140` `CollisionInfoI PsBresenhamDetection(glm::ivec2 start, glm::ivec2 end);` / `CollisionInfoD PsBresenhamDetection(Vec2_ start, Vec2_ end);`
- `PhysicsShape.cpp:242-244` `CollisionInfoI PhysicsShape::PsBresenhamDetection(glm::ivec2 start, glm::ivec2 end) { return BresenhamDetection(start, end); }`；`:258` `Vec2_` 重载
- `PhysicsShape.hpp:124` `CollisionInfoD RayCollide(Vec2_ Pos, FLOAT_ Angle);`（标量射线角）
- `MapFormwork.hpp:94,101,103` `FMBresenhamDetection(glm::ivec2,glm::ivec2)` / `(Vec2_,Vec2_)` / `FMSafeBresenhamDetection`
- `MapDynamic.hpp:205,212,220` 同上三个覆写；`MapDynamic.hpp:17` `typedef void (*_MapDynamicGenerateCallback)(BaseGrid** mT, int x, int y, void* Data);`（**2D 生成回调**）
- `MapDynamic.hpp:45-48` `atGrid(int x, int y)` / `atGrid(glm::ivec2)` / `atGrid(Vec2_)`；`:127` `GetPlatePos()` 返回 `glm::ivec2`
- `MovePlate.h`（404 行）：2D plate/chunk 分块与 `MovePlateX/Y` 轴特化
- `BaseOutline.hpp:122` `Vec2_ MaxOutlineCentre(int x, int y);`；`:19,22` `Vec2_ *OutlineSet; Vec2_ *MaxOutlineCentreMass;`

### 2.8 `GridSearch`（唯一的空间索引，四叉树）
- `GridSearch.hpp:11` `#define Morton_define 1`；`:165-175` `inline uint_fast32_t Morton2D(uint_fast16_t x, uint_fast16_t y)`（2 路位交错，`x | (y<<32)`，掩码 `0x5555555555555555`）
- `GridSearch.hpp:196-202` `Excursion(int storey)`：`unsigned int k = mStorey - storey; if (k<=0) return 0; return (unsigned int)(((1ULL << (2*k)) - 1) / 3);`（`2*k` 与 `/3` 是**四叉**层数学）
- `GridSearch.hpp:108` `Vec2_ mExcursion{0};`；`:220-235` `Storey(FLOAT_ R)`（半径→最高位定层）
- `GridSearch.hpp:244-247` `inline glm::ivec2 Pos_ToInt(Vec2_ pos) { return (pos + mExcursion); }`
- `GridSearch.hpp:260-315` `atIndex(Vec2_ xy, FLOAT_ R)`：`:273` `glm::ivec2 pos = (xy);`；`:287-288` `glm::ivec2 Spos = pos << _storey; glm::ivec2 Epos = (pos + 1) << _storey;`；`:291-292` 跨界判定 `if ((Spos.x > (xy.x - R)) || (Epos.x < (xy.x + R)) || (Spos.y > (xy.y - R)) || (Epos.y < (xy.y + R)))`（**2 轴显式展开**）；`:304` `return ExcursionVector[_storey] + Morton2D((uint_fast16_t)pos.x, (uint_fast16_t)pos.y);`
- `GridSearch.hpp:425-483` `SetMapRange(unsigned int Size)`：`:434` `Size *= 4;`（2D 覆盖 4 倍）；`:444-447` `while (gridDim < Size) gridDim <<= 1;`；`:448` `mExcursion = {gridDim / 2.0f, gridDim / 2.0f};`；`:456-461` `while (t >>= 1) { GridSize <<= 2; ++storey; ++GridSize; }`（**每层 ×4**）；`:467` `GridSize = (GridSize - 1) / 4;`（四叉紧凑计数）；`:472` `ExcursionVector = new unsigned int[mStorey + 1];`；`:478-483` `mCellHead.resize(GridSize, UINT_MAX);`（文档 :421-423：Size=1024 时约 559 万格 ×4B ≈ 21.4MB）
- `GridSearch.hpp:491-496` `MoveGridTo(Vec2_ newCenter)`；`:510-514` `UpdateGridOffset(Vec2_ newCenter)` `mExcursion = Vec2_{halfRange, halfRange} - newCenter;`；`:522-526` `GetGridCenter()` 返回 `Vec2_`
- `GridSearch.hpp:539` `void Add(PhysicsFormwork *atocr)`（用 `atIndex(PFGetPos(), PFGetCollisionR())`）；`:566` `void Remove(PhysicsFormwork *atocr)`
- `GridSearch.hpp:609-613` `void Get(Vec2_ pos, FLOAT_ R, std::vector<PhysicsFormwork*> &Out)`：`R *= 2; Get(pos - R, pos + R, Out);`（**把圆查询退化成 2D 方框查询**，调用方需二次筛除）
- `GridSearch.hpp:630-690` `void Get(Vec2_ Spos, Vec2_ Epos, std::vector<PhysicsFormwork*> &Out)`：`:652-653` `glm::uvec2 iS = glm::uvec2(S); glm::uvec2 iE = glm::uvec2(E);`；`:678` `unsigned int cellIdx = off + Morton2D((uint_fast16_t)x, (uint_fast16_t)y);`（**双重 x/y 循环遍历格子**，3D 需三重）
- `GridSearch.hpp:704-755` `std::vector<Vec2_> GetDividedVision(PhysicsFormwork *atocr)`（每层返回 2 个 `Vec2_` 表示矩形对角；`:740-741,750-751` push `Spos/Epos - mExcursion`）
- `GridSearch.hpp:778` `void UpData()`；`:800-829` `UpDaraWorkeTask(unsigned int ThreadSize, unsigned int ThreadID)`（**未用 `ThreadTaskAllot`**，自带 `BlockSize/BlockRemain` 切分，写不相交 `mNewIndex[slot]`）；`:836-839` `UpDaraWorkeTaskEnd()` → `ApplyRebuild()`
- 调用点：`PhysicsWorld.cpp:859`（主循环）、`:1132`（PhysicsInformationUpdate）

### 2.9 `ThreadTaskAllot`（**唯一维度无关的并行宏**）
- `PhysicsWorld.hpp:37-58`：
```
#define ThreadTaskAllot(S, E, Size, Tn, TSize) \
    if (Tn == TSize) { S = 0; E = Size; } \
    else { S = Size / TSize; E = Size % TSize; \
        if (Tn < E) { E = S * Tn + Tn + S + 1; S = S * Tn + Tn; } \
        else { int Y = E; E = S * Tn + Y + S; S = S * Tn + Y; } }
```
纯按**线性索引**切分连续区间，与物体是 2D 还是 3D 无关，**3D 移植无需改动**。使用点：`PhysicsWorld.cpp:359,427,485,541,570,575,585,590,644,649,654,709,714,719,776,781,791,796,925,984,1032,1079,1103`；`PhysicsGPU.cpp:151,162,173,184,527,536,545,553,580,821,823,825`。

### 2.10 `Get(pos)` 2D 点查询
- `PhysicsWorld.hpp:511` `PhysicsFormwork *Get(Vec2_ pos);`；`PhysicsWorld.cpp:1643-1677` 实现：`mGridSearch.Get(pos, 1, SearchV);` 后按类型——circle `if (Modulus(((PhysicsCircle*)i)->pos - pos) < radius)`；particle `if (Modulus(...) < 0.25)`；shape `if (Modulus(...) < PFGetCollisionR() && DropCollision(pos).Collision)`；line 线性扫 `DropUptoLineShortesIntersect(i->pos + vec2angle({i->radius,0}, i->angle), i->pos - vec2angle({i->radius,0}, i->angle), pos)` 且 `< 0.25`。

### 2.11 `SameAssembly`（**维度无关**）
- `PhysicsWorld.hpp:518-521` `inline static bool SameAssembly(const PhysicsFormwork *a, const PhysicsFormwork *b) { return a->assembly && a->assembly == b->assembly; }`（纯指针同一性，3D 无需改）
- 使用点：`PhysicsWorld.cpp:385,396,411`（及 Circle/Line 段同类）

### 2.12 `ApplyImpulseAdd` 迭代次数公式（**维度无关**）
- `PhysicsWorld.hpp:354-367`：
```
void ApplyImpulseAdd() {
    ++ObjectSize;
    if (ObjectSize < (PhysicsApplyImpulseSize * PhysicsApplyImpulseSize * 5)) ApplyImpulseSize = PhysicsApplyImpulseSize;
    else ApplyImpulseSize = sqrt(ObjectSize / 5);
}
```
`PhysicsApplyImpulseSize=10`（`BaseDefine.h:36`）→ 阈值 500；纯计数启发式，与维度无关。

### 2.13 网格风 `GridWind`（2D 数组，且为死代码）
- `PhysicsWorld.hpp:227-230` `const bool WindBool; Vec2_ Wind{0}; Vec2_ *GridWind = nullptr; glm::uvec2 GridWindSize{0};`
- `PhysicsWorld.cpp:1208` `GridWindSize = MapFormwork_->FMGetMapSize();`；`:1209` `mGridSearch.SetMapRange(std::max(GridWindSize.x, GridWindSize.y));`；`:1220` `GridWind = new Vec2_[GridWindSize.x * GridWindSize.y];`；`:1221-1224` 逐元素清零
- 序列化：`:1718` `SerializationVec2(data, GridWindSize);`；`:1724-1726` 按 `GridWindSize.x * GridWindSize.y` 展开；`:1892-1898` 反序列化
- 析构：`:124-126` `if (GridWind != nullptr) { delete GridWind; }`（**`new[]` 配 `delete`，UB**）；`:1216-1218` 同样
- **注意**：全项目搜索确认 `GridWind`/`Wind`/`WindBool` 除声明、分配、序列化、ImGui 显示外**无任何物理消费者**（力的唯一入口 `PhysicsSpeed(time, GravityAcceleration)` 只传重力）——是死代码，但按地图尺寸占内存。3D 移植时可直接删除，或改为 `Vec3_*` 且体积变为 `x*y*z`。

### 2.14 序列化中的 2D 布局
- `PhysicsWorld.cpp:1713-1885` `JsonSerialization`：`Wind`/`GridWindSize` 走 `SerializationVec2`；`GridWind` 按 `GridWindSize.x*GridWindSize.y` 平铺；各类型向量、关节（body 指针索引）、连接体、`CollideGroupVector`（含 `GetArbiterType()`）逐个写出。
- `:1887-2103` `JsonContrarySerialization`：`:1895` `GridWind = new Vec2_[data["GridWind"].size()];`；`:1913` `mGridSearch.SetMapRange(std::max(GridWindSize.x, GridWindSize.y));`；`:2020/:2031` 宏从 `Pool##Arbiter_[kMainThreadPoolIndex].newElement(...)` 分配并 push 到 `NewCollideGroup`；`:2057-2101` 按 12 个 `PhysicsArbiterType` 分派。
- `:2105-2135` `GetPtrIndex`（各类型向量线性查找，返回 `UINT_MAX`）；`:2136-2161` `GetIndexPtr`。

---

## 3. 多线程组织（及 3D 影响）

### 3.1 线程池与任务向量
- `PhysicsWorld.hpp:336-337` `ThreadPool mThreadPool; std::vector<std::future<void>> xTn;`
- `PhysicsWorld.hpp:27` `#define ThreadPoolBool 1`
- 构造 `PhysicsWorld.cpp:90-92`：`xThreadNumHW = hardware_concurrency(); xThreadNum = min(xThreadNumHW, 8u); xTn.reserve(xThreadNum);`
- `Tool/ThreadPool.h:17-137`：经典 `std::queue<std::function<void()>>` + `mutex`/`condition_variable`，`enqueue` 返回 `std::future`。`PhysicsWorld` 用的是无参构造（`ThreadPool()`，`Tool/ThreadPool.h:43-69`，按 `hardware_concurrency()` 起线程），**但只向池中投递 `xThreadNum ≤ 8` 个任务**。

### 3.2 `thread_local CollideOutput` 无锁输出
- `PhysicsWorld.hpp:340` `static thread_local CollideOutput *tlCollideOutput;`；定义 `PhysicsWorld.cpp:19`
- `PhysicsWorld.hpp:192-204` `struct CollideOutput { unsigned char mThreadIndex = kMainThreadPoolIndex; std::vector<BaseArbiter*> newGroup; std::vector<ArbiterKey> deleteGroup; void clear(); }`
- `PhysicsWorld.hpp:343` `std::vector<CollideOutput> mCollideOutputs;`（每帧 `resize(xThreadNum)`，`PhysicsWorld.cpp:878-879`）
- 工作线程在 `XT_Fun` 入口设 `tlCollideOutput = output;`（`PhysicsWorld.cpp:352`），出口置空（`:554`）。`AuxiliaryArbiter` 宏（`PhysicsWorld.hpp:94-104`）读 `tlCollideOutput->mThreadIndex` 作为内存池下标，`Map_Delete` 宏（`PhysicsWorld.cpp:23-30`）据此把删除键写入本线程 `deleteGroup`。**每个线程只写自己的 `CollideOutput`，无锁、无竞争。**
- **注意**：`PhysicsWorld.hpp:318` 声明了 `void MergeCollideOutputs(std::vector<CollideOutput> &outputs);`，但**全项目无任何定义**（已 grep 确认）——是死声明；per-thread 输出实际由 `ResolveCollideGroup` 直接遍历 `mCollideOutputs` 消费。

### 3.3 每线程 `MemoryPool` 数组
- `PhysicsWorld.hpp:72-73` `constexpr unsigned kMaxPoolThreads = 9;` / `constexpr unsigned kMainThreadPoolIndex = kMaxPoolThreads - 1;`（8 worker + 1 main）
- `PhysicsWorld.hpp:82-84` `AuxiliaryMemoryPool` 展开为 `MemoryPool<Arbiter_, 1000 * sizeof(Arbiter_)> Pool##Arbiter_[kMaxPoolThreads];`（每类型 9 个池）
- `PhysicsWorld.hpp:94-104` `AuxiliaryArbiter`：`const unsigned char tId = tlCollideOutput ? tlCollideOutput->mThreadIndex : (unsigned char)kMainThreadPoolIndex;` → `BaseArbiter *ptr = Pool##Arbiter_[tId].newElement(Name1, Name2); ptr->key.PoolID = (char)ID; ptr->mAllocThreadIndex = tId; HandleCollideGroup(ptr);`
- `PhysicsWorld.hpp:114-119` `AuxiliaryDelete`：`case ID: { Pool##Arbiter_[BA->mAllocThreadIndex].deleteElement((Arbiter_*)BA); } break;`
- `PhysicsWorld.cpp:41-52` 12 个 `AuxiliaryArbiter(...)` 实例化（ID 0..11：CP,SP,SS,S,P,C,CS,CC,LC,LS,LP,L）；`:54-78` `DeleteArbiter` 按 `BA->key.PoolID` 分派 `AuxiliaryDelete`，`default` 走 `delete BA`。
- `PhysicsWorld.hpp:320-331` 12 个 `AuxiliaryMemoryPool(...)` 声明。

### 3.4 各阶段并行度（含串行例外）
| 阶段 | 并行 | 备注 |
|---|---|---|
| 碰撞检测 `XT_Fun` | ✅ 4 类向量各按 `ThreadTaskAllot` | 每线程独立 `CollideOutput` + 独立内存池 |
| `PhysicsSpeed` | ✅ 4 类向量 | 无副作用冲突 |
| **PreStep** | ❌ **强制串行** | `PhysicsWorld.cpp:637` `#if (Definite != 1) & ThreadPoolBool & 0` 恒假；注释 :636 说明 PreStep 含冲量副作用 |
| `ApplyImpulse` | ✅ 但每轮全屏障 | `:725` 外层迭代 × `:728-736` 每轮等待 |
| `PhysicsPos` | ✅ 4 类向量 | 注释 :768「这个多线程不影响结果」 |
| `GridSearch::UpDaraWorkeTask` | ✅ 自带块切分 | `:856-866`；写 `mNewIndex` 不相交 |
| 液体 `PhysicsLiquid::Update` | 部分（λ/Δp 两遍分片） | `PhysicsLiquid.cpp:797` `T = min(hardware_concurrency(), 8u)`；邻居搜索等阶段单线程 |
| `ResolveCollideGroup` | 单线程 | 遍历 `mCollideOutputs` |

### 3.5 3D 需要改什么
1. **不需要改**：`ThreadTaskAllot`、`ThreadPool`、`xTn`、`CollideOutput`/`tlCollideOutput`、per-thread `MemoryPool` 数组、`kMaxPoolThreads=9`、`AuxiliaryArbiter/Delete` 宏、阶段划分与屏障结构。这些全部只按线性索引与指针工作。
2. **需要改**：`mCollideOutputs` 的容量仍等于 `xThreadNum`（与维度无关）；但**每线程宽相 `SearchV` 的候选量在 3D 会显著增大**（球查询体积 vs 面积），需评估 `GridSearch::Get` 的重叠候选膨胀与内存池 `1000*sizeof(Arbiter_)` 是否够用——3D arbiter 的 `Contact` 变大（`Vec2_`→`Vec3_`，新增第二切向），`sizeof(Arbiter_)` 增大，池总量随之上升。
3. **需注意**：`PhysicsWorld.cpp:856` / `:882` 在 `#if ThreadPoolBool` 关闭时引用未定义的 `xThreadNum`（`XT_Fun(1,1,nullptr)` 分支 `:890` 之外仍用到），是既有的编译期脆弱点，3D 重构时应一并清理。
4. **确定性**：注释 `:634` `#define Definite 0` 与 `:701` 明确放弃确定性；3D 若要求确定性需改为按 `ArbiterKey` 排序后求解。

---

## 4. Arbiter 生命周期

### 4.1 创建：`HandleCollideGroup(BaseArbiter *Ba)`
- 声明 `PhysicsWorld.hpp:307`；定义 `PhysicsWorld.cpp:184-217`（`inline`）。逐字逻辑：
```
inline void PhysicsWorld::HandleCollideGroup(BaseArbiter *Ba)
{
    Ba->ComputeCollide();
    auto it = CollideGroupS.find(Ba->key);
    if (Ba->numContacts > 0) {
        if (it == CollideGroupS.end()) {
            if (tlCollideOutput) tlCollideOutput->newGroup.push_back(Ba);
            else NewCollideGroup.push_back(Ba);
            return;                                  // 保留 Ba，等待 Resolve 阶段入表
        }
        it->second.arbiter->Update(Ba->contacts, Ba->numContacts);   // 复用已有 arbiter
    }
    else if (it != CollideGroupS.end()) {
        if (tlCollideOutput) tlCollideOutput->deleteGroup.push_back(Ba->key);
        else DeleteCollideGroup.push_back(Ba->key);
    }
    DeleteArbiter(Ba);                               // 新 Ba 归还内存池
}
```
**判定规则**：
- `numContacts > 0` 且表中**无**该 key → 新碰撞对，`Ba` 被推入 `newGroup`（**不**归还内存池），`ResolveCollideGroup` 会把它装进 `CollideGroupS`+`CollideGroupVector`。
- `numContacts > 0` 且表中**有**该 key → 调用已有 arbiter 的 `Update(contacts, numContacts)` 刷新接触点，新 `Ba` 归还内存池。
- `numContacts == 0` 且表中有 key → 记入 `deleteGroup`，归还新 `Ba`。
- `numContacts == 0` 且表中无 key → 直接归还（无对可删）。
- 即：**keep = (numContacts > 0)，delete = (numContacts == 0 && 已在表中)**。`numContacts` 由 `ComputeCollide()` 通过窄相 `Collide(...)` 写入（`BaseArbiter.hpp:199` `int numContacts = 0;`，上限 `PhysicsContactMaxSize=20`）。

### 4.2 结算：`ResolveCollideGroup()`
- 声明 `PhysicsWorld.hpp:313`；定义 `PhysicsWorld.cpp:224-313`（`inline`）。
- **线程路径（`:224-267`）**：遍历 `mCollideOutputs`：
  - **先删**（每线程 `deleteGroup`）：`find` → 取 `arb = it->second.arbiter; idx = it->second.vectorIndex;` → `CollideGroupS.erase(it);` → `mCollision.RemoveCollisionPair(arb);` → `DeleteArbiter(arb);` → swap-and-pop：
    `if (idx != (int)CollideGroupVector.size()-1) { CollideGroupVector[idx] = CollideGroupVector.back(); CollideGroupS[CollideGroupVector[idx]->key].vectorIndex = idx; } CollideGroupVector.pop_back();`
  - **后加**（每线程 `newGroup`）：`CollideGroupVector.reserve(...)`；`auto result = CollideGroupS.emplace(J->key, CollideGroupEntry{J, 0}); if (result.second) { CollideGroupVector.push_back(J); result.first->second.vectorIndex = (int)CollideGroupVector.size()-1; mCollision.AddCollisionPair(J); } else { DeleteArbiter(J); }`（key 冲突时归还，防御重复）
  - **先删后加的顺序保证**：同一 key 若本帧既删又加，删除先执行、再重新 emplace。
- **非线程路径（`:269-311`）**：对 `DeleteCollideGroup`/`NewCollideGroup` 做完全相同的操作，入口有 `if (DeleteCollideGroup.empty() && NewCollideGroup.empty()) return;` 早退。
- 表结构：`PhysicsWorld.hpp:246` `std::unordered_map<ArbiterKey, CollideGroupEntry, ArbiterKeyHash> CollideGroupS;`；`:206-210` `struct CollideGroupEntry { BaseArbiter *arbiter = nullptr; int vectorIndex = -1; }`；`:252` `std::vector<BaseArbiter*> CollideGroupVector;`。`vectorIndex` + swap-and-pop 使删除 O(1)，`CollideGroupVector` 提供 O(1) 线性遍历（供 PreStep/ApplyImpulse/GPU 打包）。

### 4.3 销毁：`DeleteArbiter(BaseArbiter *BA)`
- 声明 `PhysicsWorld.hpp:333`；定义 `PhysicsWorld.cpp:54-78`。`switch (BA->key.PoolID)` 12 个 case 各自 `Pool##Arbiter_[BA->mAllocThreadIndex].deleteElement((Arbiter_*)BA);`，`default: break;`；`#else` 分支 `delete BA;`。**跨线程归还安全**：按下标 `mAllocThreadIndex` 找回原分配线程的池。

### 4.4 相关结构
- `BaseArbiter.hpp:44-97` `struct ArbiterKey { void *object1; void *object2; char PoolID; bool operator<(...); bool operator==(...); }`（维度无关）；`:103-122` `ArbiterKeyHash`
- `BaseArbiter.hpp:198-206` `Contact contacts[PhysicsContactMaxSize]; int numContacts = 0; ArbiterKey key; void *mOriginalObject1; void *mOriginalObject2; unsigned char mAllocThreadIndex = 0; PhysicsArbiterType mArbiterType;`
- `BaseArbiter.hpp:124-137` `enum PhysicsArbiterType { ArbiterS, ArbiterP, ArbiterC, ArbiterL, ArbiterSS, ArbiterSP, ArbiterCS, ArbiterCP, ArbiterCC, ArbiterLS, ArbiterLC, ArbiterLP }`（12 种，与 `PhysicsWorld.cpp:41-52` 的 PoolID 0..11 一一对应）
- `BaseArbiter.hpp:227,235,242,248,254` 纯虚接口：`virtual void ComputeCollide() = 0;` / `virtual void Update(Contact *NewContacts, int numNewContacts) = 0;` / `virtual void PreStep(FLOAT_ inv_dt) = 0;` / `virtual void PreStep() = 0;` / `virtual void ApplyImpulse() = 0;`
- 4 个具体求解器：`PhysicsBaseArbiter.hpp:15` `PhysicsBaseArbiterAA`、`:65` `PhysicsBaseArbiterAD`、`:117` `PhysicsBaseArbiterA`、`:169` `PhysicsBaseArbiterD`（按「两体是否可动 / 是否地图」组合特化，避免分支）。

### 4.5 物体移除时的 arbiter 清理
- `PhysicsWorld.cpp:1250-1612` `RemoveObject(PhysicsFormwork *Object)`：按类型 `mGridSearch.Remove(obj)` → swap-and-pop 出类型向量 → 删除依附的 `PhysicsJointS`/`BaseJunctionS` → 遍历 `CollideGroupS` 擦除所有含该对象的 arbiter（同样 swap-and-pop 修正 `vectorIndex`）→ 清 `NewCollideGroup`/`DeleteCollideGroup` 中残留键 → 粒子分支额外 `mCollision.RemoveTerrainHitListener(particle)` 与 `mLiquid->NotifyParticleRemoved(particle)` → `delete`；`:1610-1611` `if (ObjectSize > 0) --ObjectSize;`
- `:1619-1632` `RemoveObject(PhysicsAssembly *Object)`。

---

## 5. 其余类级 2D 耦合（按文件）

### 5.1 `PhysicsBaseArbiter.cpp`（647 行，4 个求解器）
- `:76-79` `FLOAT_ R1 = Dot(c->r1,c->r1); FLOAT_ R2 = Dot(c->r2,c->r2);`
- `:80-81` `FLOAT_ kNormal = object1->invMass + object2->invMass; FLOAT_ kTangent = kNormal;`
- `:84` `kNormal += object1->invMomentInertia * (R1 - rn1 * rn1) + object2->invMomentInertia * (R2 - rn2 * rn2);`
- `:91` `kTangent += object1->invMomentInertia * (R1 - rt1 * rt1) + object2->invMomentInertia * (R2 - rt2 * rt2);`
- `:96-107` `Vec2_ P = c->Pn * c->normal + c->Pt * tangent;` + 两侧 `speed`/`angleSpeed` 更新
- **3D 改动**：`kNormal` 需变为 `1/(invM1+invM2) + n·((invI1*(r1×n))×r1) + ...`（矩阵形式）；切向需 2 个正交基向量（`massTangent2`/`Pt2`）或直接构造 `invMassMatrix`；`w_side` 需重新语义化。

### 5.2 `PhysicsBaseCollide.cpp`（1023 行，13 个 `Collide` 重载 + 8 个 `CollideAABB`）
- 签名（`PhysicsBaseCollide.hpp:18-180`）：`bool CollideAABB(PhysicsShape*, PhysicsShape*)` :18、`(PhysicsShape*, PhysicsParticle*)` :27、`(PhysicsShape*, PhysicsCircle*)` :36、`(PhysicsCircle*, PhysicsCircle*)` :45、`(PhysicsCircle*, PhysicsParticle*)` :54、`(PhysicsLine*, PhysicsShape*)` :63、`(PhysicsLine*, PhysicsCircle*)` :72；`unsigned int Collide(Contact*, ...)` :81,90,99,108,117,126,135,144,153,162,171,180。
- 所有法线均出自 4 向 + 标量角（见 §2.6 行号清单）。
- `:123,144,737,842,874,919` 线段端点由 `vec2angle({Line->radius,0}, Line->angle)` 旋转得到。
- **3D 改动**：13 个重载的组合结构可保留，但内部几何全部重写——圆→球、线→胶囊/杆、形状→体素/盒；`CollideAABB` 由 2 轴区间重叠扩到 3 轴；法线由「4 向枚举」改为「6 面枚举」或直接用接触面法向；Bresenham 窄相改为 3D DDA / 体素射线。

### 5.3 `BaseCalculate.hpp/.cpp`（2D 数学核，482 行）
- 2D 专属助手（签名 + 行号）：`ToInt(glm::vec2/dvec2)→glm::ivec2` :43,57；`ModulusLength` :81；`Modulus` :91；`AngleFloatToAngleVec` :101；`EdgeVecToCosAngleFloat` :112；`vec2angle(Vec2_,FLOAT_)` :133；`vec2angle(Vec2_,Vec2_)` :142；`vec2PosAngle` :155,168；`ToIntPos` :180,192；`LineXToPos` :205；`LineYToPos` :219；`AngleMat` :229-275；`LineSquareFocus` :287；`SquareToSquare` :299；`SquareToDrop` :311,321；`SquareToRadial` :333；`TorqueCalculate` :343；`CalculateIncludedAngle` :351；`CalculateDecompositionForce` :359,367；`CalculateDecompositionForceVal` :375；`Morton2D` :384；`DropUptoLineShortes` :393；`DropLineShortesIntersect` :403；`DropUptoLineShortesIntersect` :413；`Dot` :421；`Cross(Vec2_,Vec2_)` :433；`Cross(Vec2_,FLOAT_)` :444；`Cross(FLOAT_,Vec2_)` :455；`Clamp` :467；`Random` :475。
- **3D 可保留**：`Dot`（加 z）、`Modulus/ModulusLength`（加 z²）、`Drop*Line*` 系列（点到线段最近点公式自然升维）、`Clamp`、`Random`、`q_sqrt`。
- **3D 必须重写**：全部 `vec2angle`/`AngleMat`/`AngleFloatToAngleVec`（→四元数或 mat3）、`Cross` 三个重载（→`glm::cross` 与 `mat3*vec3`）、`SquareToSquare/Drop/Radial`（2D 矩形→3D 盒/体素）、`TorqueCalculate`（标量→`Vec3_`）、`CalculateDecompositionForce`（→3D 法/双切分解）、`Morton2D`（→`Morton3D`）、`EdgeVecToCosAngleFloat`（无单一参考轴）、`LineX/YToPos`、`ToIntPos`、`LineSquareFocus`。

### 5.4 `PhysicsJoint.cpp`（136 行）—— 最典型的 2D 耦合
- `:59,65,71,77` `Mat2_ K1; Mat2_ K2; Mat2_ K3; Mat2_ K = K1 + K2 + K3;`（**2×2 有效质量矩阵**）
- `:66-69` `K2[0][0] = body1->invMomentInertia * r1.y * r1.y; K2[0][1] = -body1->invMomentInertia * r1.x * r1.y; K2[1][0] = -...; K2[1][1] = ... * r1.x * r1.x;`（2D `-skew(r)·I·skew(r)` 的标量展开）
- `:72-75` 同构 K3
- `:84,86,97,99` `body->angleSpeed ±= body->invMomentInertia * Cross(r, P);`
- `:93` `Vec2_ dv = body2->speed + Cross(body2->angleSpeed, r2) - body1->speed - Cross(body1->angleSpeed, r1);`
- 类型：`PhysicsJoint.hpp:50` 行，`Set(PhysicsAngle*, PhysicsAngle*, const Vec2_ anchor)`、`PreStep(FLOAT_ inv_dt)`、`ApplyImpulse()`；成员 `r1/r2` 为 `Vec2_`、`M` 为 `Mat2_`。
- **3D 改动**：`Mat2_`→`Mat3_`，`M` 变为 3×3；`Cross(r,P)`→`glm::cross(r,P)`（`Vec3_`）；`Cross(ω,r)`→`glm::cross(ω,r)`；关节由 2 DOF 变 3 DOF（或保留球铰 3 平移约束 + 3 转动自由度）。

### 5.5 `PhysicsJunction.hpp/.cpp`（4 种绳/连接约束）
- `PhysicsJunction.hpp:180-181`（约）`BaseJunction` 成员 `FLOAT_ bias{0}; FLOAT_ k{0}; Vec2_ Normal; Vec2_ P; CordType Type;`
- `PhysicsJunction.cpp:45-46` `mR1 = vec2angle(mArm1, mParticle1->angle); mR2 = vec2angle(mArm2, mParticle2->angle);`
- `:52-53` `k += mParticle1->invMomentInertia * r1CrossN * r1CrossN; k += mParticle2->invMomentInertia * r2CrossN * r2CrossN;`
- `:57,59,72,80,83,123,128,143,151` 标量 `Cross` 与标量 `angleSpeed` 更新
- 4 个类型 `JunctionAA/JunctionA/JunctionP/JunctionPP`（`CordObjectType`），序列化分派见 `PhysicsWorld.cpp:2057-2101`。
- **3D 改动**：`Normal`/`P`→`Vec3_`；`r1CrossN` 标量→向量叉积模长或矩阵形式；距离约束本身可升维，但所有有效质量项需重推。

### 5.6 `PhysicsCollision.cpp/.hpp`（事件分发，几乎无 2D 数学）
- 纯回调管理（Enter/Stay/Exit、层级、优先级）。唯一 2D 耦合：`:184` `OnMapCollisionChanged(MapFormwork*, glm::ivec2, bool)`；`:240` `DirectionFromNormal(...)`；`AddCollisionPair` :174、`ProcessCollisions` :269、`RemoveCollisionPair` :293、`RemoveTerrainHitListener` :346。
- `Priority` 字段被读写校验但**无排序消费点**。
- **3D 改动**：`glm::ivec2`→`glm::ivec3`，`DirectionFromNormal` 重写为 6 面；其余不变。

### 5.7 `PhysicsTrigger.cpp/.hpp`
- `:157-158` 触发区由 minPos/maxPos 两个 `Vec2_` 定义的 2D AABB。
- **3D 改动**：`Bounds`→3 轴；触发判定加 z 区间。

### 5.8 `PhysicsKinematic.cpp/.hpp`
- 运动学插值：位置按 `Vec2_` 分量 lerp，旋转按**标量角** lerp（无最短弧/四元数 slerp）。
- `:181` `static_cast<PhysicsAngle *>(object)->invMomentInertia = 0.0f;`；`:204-206` 恢复 `1.0f / MomentInertia`。
- **3D 改动**：位置→`Vec3_`；旋转→`glm::slerp`（或对欧拉角插值，但需注意万向锁）。

### 5.9 `PhysicsShape.hpp/.cpp` + `BaseGrid` + `BaseOutline` + `MapStatic/MapDynamic` + `MovePlate.h`
- `PhysicsShape.hpp:48,54` `Vec2_ CentreMass/CentreShape`；`:63` `PhysicsShape(Vec2_ Pos, glm::ivec2 Size);`（**网格尺寸是 2D**）；`:82` `CollisionInfoI DropCollision(Vec2_ pos);`；`:124` `CollisionInfoD RayCollide(Vec2_ Pos, FLOAT_ Angle);`
- `PhysicsShape.cpp:36` `DropCollision(Vec2_ Pos)`；`:144` 2D 惯量逐点累加；`:216` `BresenhamDetection(CentreMass + OutlineDrop, CentreMass - OutlineDrop)`；`:242,258` `PsBresenhamDetection` 两个重载
- `BaseGrid.hpp:31` `new GridBlock[width*height]`；`:67` `at(x*height+y)`；`:111` `GetCollision(glm::uvec2)`；`:147,156` Bresenham
- `BaseGrid.cpp:102-178` 2D Bresenham + 4 向边界精确求交
- `BaseOutline.hpp:19,22` `Vec2_ *OutlineSet/*MaxOutlineCentreMass`；`:122` `Vec2_ MaxOutlineCentre(int x, int y);`（2D 轮廓，4 邻域）
- `MapStatic.cpp` / `MapDynamic.cpp`：`_MapDynamicGenerateCallback(BaseGrid** mT, int x, int y, void* Data)`（`MapDynamic.hpp:17`）；`atGrid(int,int)`/`atGrid(glm::ivec2)`/`atGrid(Vec2_)`（:45-48）；`GetPlatePos()`→`glm::ivec2`（:127）
- `MovePlate.h`（404 行）：2D plate/chunk 分块与 X/Y 轴特化
- **3D 改动**：`GridBlock[width*height]`→`[width*height*depth]`，索引 `x*height*depth + y*depth + z`；Bresenham→Amanatides-Woo 3D DDA；轮廓 4 角→8 体素角（2×2×2 邻域）；plate 分块由 2 轴变 3 轴；生成回调签名加 `int z` 或 `glm::ivec3`。

### 5.10 `PhysicsLiquid.hpp/.cpp`（PBF 流体，1330 行）
- 头文件 2D 面：`AddParticle(Vec2_ pos, ...)`、`AddGrid(Vec2_ center, int numX, int numY, FLOAT_ spacing, ...)`；`struct SolidSample { PhysicsAngle* body; Vec2_ local; }`；`struct SolidTrack { ... Vec2_ accumT{0}; FLOAT_ accumR = 0; }`；`struct SolidXf { Vec2_ pos{0}; Vec2_ cs{0}; FLOAT_ radius; FLOAT_ radius2; bool dynamic; }`；工作数组 `std::vector<Vec2_> mPos/mVel/mPrevPos/mGrad/mDelta/mBoundaryDisp`、`std::vector<Vec2_> mPairDir`。
- `PhysicsLiquid.cpp:561-567` `int PhysicsLiquid::HashCell(int gx, int gy)`：`(unsigned int)gx * 92837111u ^ (unsigned int)gy * 689287499u`，`% kHashSize`（`kHashSize=370111`）——**2D 整数哈希**
- `:594-606` 网格坐标 `gx = floor(mPos[i].x * inv); gy = floor(mPos[i].y * inv);`（2 分量）
- `:617-638` **3×3 邻域双重循环** `for (ix = gx-1; ix <= gx+1; ++ix) for (iy = gy-1; iy <= gy+1; ++iy)`，`r2 = dx*dx + dy*dy`
- `:831-832` 核归一化 `poly6Scale`、`spikyScale = 10.0 / (kPi * h2 * h2 * h)`（**3D poly6/spiky 归一化常数不同**：3D poly6 = 315/(64πh⁹)，spiky = −45/(πh⁶)）
- `:1107` `rot += r.x * d.y - r.y * d.x;`（2D 标量转动拟合）；`:1108` `den += r.x*r.x + r.y*r.y;`；`:1111` `dTheta = rot / den;`
- `:1114-1116` `maxDisp` 钳制；`:1119` `body->angle += dTheta;`；`:1128` `mPos[n+s] = body->pos + vec2angle(mSamples[s].local, csNew);`
- `:1147` `EnforceCFL` 用 `ModulusLength`（2D）；`:1155-1163` 边界位移与速度回代
- `:1199-1201` XSPH `dx/dy/r2 = dx*dx+dy*dy`
- `:1226-1231` `Vec2_ dv = st.accumT / time; FLOAT_ dw = st.accumR / time; ... body->speed += dv; body->angleSpeed += dw;`
- `:797` `const unsigned int T = std::min(std::thread::hardware_concurrency(), 8u);`
- `SolidContains` 说明（hpp）：`AABB 快筛 + 纯乘加`，圆判定 `(0.9r)²` —— 2D AABB + 2D 圆。
- **3D 改动**：哈希键改 3 元（或 Morton3D）；邻域 3×3→3×3×3（27 桶）；核函数换 3D 归一化；刚体拟合的转动量由标量 `Σ(r×d)/Σr²` 改为 3×3 最小二乘（`Σ(r×d)` 变 `Vec3_`，`Σ r rᵀ` 变 `mat3`）并解 3 元方程组；`mPairDir`→`Vec3_`；`SolidXf.cs` 由 `{cos,sin}` 改为四元数/旋转矩阵；`SolidContains` 的 AABB 快筛加 z。

### 5.11 `PhysicsGPU.hpp/.cpp` + shaders
- `PhysicsGPU.hpp:32-46` 缓冲与打包 API：`PackBodyBuffer/PackBodyStatic/PackBodyDynamic/PackArbiterBuffer/PackJointBuffer/PackJunctionBuffer`、`PackSingleArbiter/Joint/Junction`、`UnpackBodyBuffer/UnpackArbiterPnPt`、`Ensure*Capacity`、`RecreateCalculate*`
- `PhysicsGPU.hpp:93` `uint32_t mLocalSizeX = 64;`；`:94` `uint32_t mSMCount = 32;`
- `PhysicsGPU.cpp:19-29` 布局常量：`BODY_FLOATS = 8`、`MAX_CONTACTS = 20`、`CONTACT_FLOATS = 12`、`ARBITER_META_BYTES = 32`、`ARBITER_STRIDE_BYTES = 32 + 20*12*4`、`JOINT_FLOATS = 20`、`JUNCTION_FLOATS = 20`
- `PhysicsGPU.cpp:88-95` 静态体写入：`dst[offset+3]=invMass; dst[offset+4]=invMomentInertia;`（**标量惯量**）
- `PhysicsGPU.cpp:156-170` 动态体写入：`dst[off+0]=speed.x; dst[off+1]=speed.y; dst[off+2]=angleSpeed;`（**vec2 + 标量 ω**）
- `PhysicsGPU.cpp:224-249` arbiter 打包：`ct[0]=normal.x; ct[1]=normal.y; ct[2]=r1.x; ct[3]=r1.y; ct[4]=r2.x; ct[5]=r2.y; ct[6]=massNormal; ct[7]=massTangent; ct[8]=bias; ct[9]=friction; ct[10]=Pn; ct[11]=Pt;`
- `PhysicsGPU.cpp:280-287` joint 打包：`slot[2..5]=r1.xy/r2.xy; slot[6..7]=bias.xy; slot[8]=M[0][0]; slot[9]=M[1][0]; slot[10]=M[0][1]; slot[11]=M[1][1];`
- `PhysicsGPU.cpp:533,542,559` 回读 `obj->angleSpeed = mapped[off + 2];`
- shaders：`shaders/physics_body.glsl` 文档化 `BODY_STRIDE=8` 布局（`offset 0: speedX, 1: speedY, 2: angleSpeed, 3: invMass, 4: invMomentInertia, ...`）；`shaders/physics_arbiter.comp:43` `float cross2D(vec2 a, vec2 b) { return a.x*b.y - a.y*b.x; }`，`:76-77` `vec2 dv = vec2(spdBX,spdBY) + vec2(-angB*r2Y, angB*r2X) - vec2(spdAX,spdAY) - vec2(-angA*r1Y, angA*r1X);`，`:90` `float torqueA = -invIA * cross2D(vec2(r1X,r1Y), impulseN);`，`:106` `vec2 tangent = vec2(normalY, -normalX);`；`physics_joint.comp`/`physics_junction.comp` 同构。
- **3D 改动**：`BODY_FLOATS` 需扩到容纳 `speed.xyz + ω.xyz + quat(4)`（约 13+）；`CONTACT_FLOATS` 加第二切向与 `Vec3_` 分量；joint 的 `M` 由 4 个 float 变 9 个；shader 内 `cross2D`→`cross`、`vec2`→`vec3`、标量 `angX`→`vec3 ω`；`mLocalSizeX` 与 workgroup 划分需重新评估。注意 `PhysicsGPUBool`（`BaseDefine.h:57`）当前为 1，但注释 `:50-56` 说明仓库缺 VulkanSDK 时须为 0。

### 5.12 `PhysicsAssembly`
- 组装体仅做成员指针记账与相对变换（2D `Vec2_`），`AddToWorld(PhysicsWorld*)` 委托世界注册；无独立 2D 数学。**3D 改动**：`Vec2_`→`Vec3_`，相对变换的标量角→四元数。

---

## 6. 移植风险与既有缺陷清单（3D 重构时一并处理）

1. **`GridWind` 数组析构不匹配**：`PhysicsWorld.cpp:1220` `new Vec2_[...]` 对 `:126`/`:1218` `delete GridWind;`（应 `delete[]`）→ UB。3D 直接删除该死代码或改 `delete[]`。
2. **`GridWind`/`Wind`/`WindBool` 无消费者**：全项目搜索确认除声明/分配/序列化/ImGui 外无任何物理读取，力的唯一入口是 `PhysicsSpeed(time, GravityAcceleration)`。3D 可整体移除，或改为 `Vec3_*` 体积 `x*y*z`（内存按立方增长）。
3. **`XT_Fun` 中的未检查向下转型**：`PhysicsWorld.cpp:377` `if (JZ && (((PhysicsParticle *)i)->StaticNum > 10))` 与 `:445` 同型——`i` 可能是 `PhysicsShape`/`PhysicsCircle`/`PhysicsLine`，却按 `PhysicsParticle*` 读 `StaticNum`（偏移错位/UB）。3D 重构应改用虚接口（如 `PFGetStaticNum()`）或先判类型。
4. **`MergeCollideOutputs` 声明无定义**：`PhysicsWorld.hpp:318` 声明，全项目无定义——死声明，可删。
5. **`PhysicsWorld.cpp:856`/`:882` 在非线程编译下引用 `xThreadNum`**（该变量仅在 `#if ThreadPoolBool` 内定义）——既有的编译期脆弱点。
6. **`GridSearch` 内存随维度爆炸**：2D `Size=1024` 时约 559 万格 ×4B ≈ 21.4MB（`GridSearch.hpp:421-423`）。3D 八叉树同世界范围格数约 ×N 倍（每层 ×8 而非 ×4，层数同），需重新评估容量或改用哈希网格。
7. **`GridSearch::Get(Vec2_ pos, FLOAT_ R, ...)` 把圆查询退化为方框查询**（`GridSearch.hpp:609-613` `R *= 2`），调用方必须二次筛除；3D 中球→立方体查询的**过取率**（立方体/球体积比 = 6/π ≈ 1.91）比 2D（方/圆面积比 = 4/π ≈ 1.27）更高，宽相候选量与窄相负担显著上升。
8. **`PhysicsShape` 逐像素惯量与轮廓**：`PhysicsShape.cpp:131-150` 按 `MomentInertiaSamplingSize` 采样算 2D 惯量；3D 需算 3×3 张量（或对角近似），采样量从 `S²` 变 `S³`。
9. **`Contact::w_side`**（`BaseArbiter.hpp`）：2D 小矩形边分离标志，3D 语义需重新定义（6 面索引或 3D 边/角特征码）。
10. **`Direction:2` 位域**（`BaseStruct.hpp:111-126`）只有 2 bit，3D 需 3 bit（6 面）；`CheckDirection` 枚举扩到 6 值后所有 `switch`/位运算（`info.Direction & 0x1`，`PhysicsBaseCollide.cpp:183`）都要重写。
11. **PreStep 串行**（`PhysicsWorld.cpp:637` `& 0`）：3D 场景物体更多时该段可能成为瓶颈；若要并行需先消除 PreStep 的冲量副作用或改为两阶段（累积+应用）。
12. **无确定性**：`Definite 0`（`PhysicsWorld.cpp:634`），`ApplyImpulse` 每轮跨线程无序累加；3D 若需可复现需按 key 排序。
13. **`PhysicsJoint` 仅 2 DOF**：`Mat2_` K 矩阵；3D 球铰需 3×3，若还要限制转动自由度需额外角约束。
14. **液体核常数是 2D 的**：`PhysicsLiquid.cpp:832` `spikyScale = 10.0/(kPi*h2*h2*h)` 与 `poly6Scale` 均为 2D 归一化；3D 必须换 `315/(64π h⁹)` 与 `-45/(π h⁶)`，否则密度/压力尺度全错。
15. **液体邻居哈希 2 元**：`HashCell(int gx, int gy)` 与 3×3 邻域；3D 需 3 元哈希 + 3×3×3 邻域，`kMaxNeighbors=64` 在 3D 下可能不足（3D 球内邻居数约 ×1.5）。
16. **GPU 路径布局硬编码**：`BODY_FLOATS=8`（vec2+标量ω）与 `CONTACT_FLOATS=12`（vec2 normal/r1/r2 + 6 标量）；3D 需重排并同步改 `shaders/physics_*.comp` 与 `physics_body.glsl`。

---

## 7. 移植工作分层建议（按依赖顺序）

1. **类型层**：`BaseStruct.hpp:12-26` 增 `Vec3_/Mat3_/Quat_`；`FLOAT_` 不变。同步 `BaseDefine.h` 常量与 `PhysicsObjectEnum`（`circle`→`sphere`，`line`→`capsule`）。
2. **数学核**：重写 `BaseCalculate.hpp` 的旋转/叉积/矩形容器系列，保留 `Dot`/`Modulus`/`Drop*Line*`/`Clamp`。
3. **物体基类**：`PhysicsFormwork.hpp` 虚接口 `Vec2_`→`Vec3_`、`PFGetAngle/PFSetAngle` 换四元数、`PFAngleSpeed` 换 `Vec3_&`；`PhysicsAngle.hpp/.cpp` 的标量角/角速度/惯量/扭矩换四元数 + `Vec3_` + `mat3`。
4. **接触与求解器**：`BaseArbiter.hpp` `Contact` 升维（含第二切向）；`PhysicsBaseArbiter.cpp` 四个求解器的有效质量与角冲量矩阵化。
5. **窄相**：`PhysicsBaseCollide.cpp` 13 个重载几何重写；法线由 4 向枚举改 6 面；Bresenham→3D DDA。
6. **空间索引**：`GridSearch` 四叉→八叉，`Morton2D`→`Morton3D`，`Excursion` 的 `/3`→`/7`、`2*k`→`3*k`，`SetMapRange` 的 `*4`→`*8`、`<<=2`→`<<=3`、`/4`→`/8`，`Get` 的双重循环→三重循环，`GetDividedVision` 返回 `Vec3_`。
7. **地图/网格**：`BaseGrid` `x*height+y`→`x*height*depth+y*depth+z`，Bresenham→3D DDA；`BaseOutline` 4 角→8 体素角；`MapStatic/MapDynamic/MovePlate` 3 轴化。
8. **约束**：`PhysicsJoint`（`Mat2_`→`Mat3_`）、`PhysicsJunction`（`Vec3_ Normal/P`）。
9. **子系统**：`PhysicsKinematic`（slerp）、`PhysicsTrigger`（3 轴 AABB）、`PhysicsCollision`（`ivec3` + 6 面 `DirectionFromNormal`）、`PhysicsLiquid`（3D 核 + 3 元哈希 + 3×3 拟合）。
10. **GPU**：`PhysicsGPU.cpp` 打包布局 + 4 个 shader。
11. **不改**：`PhysicsWorld` 主循环九阶段骨架、`ThreadTaskAllot`、`ThreadPool`/`xTn`、`CollideOutput`/`tlCollideOutput`、per-thread `MemoryPool` 数组与 `AuxiliaryArbiter/Delete`、`HandleCollideGroup`/`ResolveCollideGroup`/`DeleteArbiter`、`SameAssembly`、`ApplyImpulseAdd`、`ArbiterKey`/`ArbiterKeyHash`、序列化框架（仅改向量序列化分量数）。
