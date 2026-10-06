# PhysicsBlock 模块问题分析报告

> 分析对象：`E:\Physics\PixelClean-main\PhysicsBlock`（像素网格 2D 刚体物理引擎，约 60 个文件 / 500 KB C++）
> 触发请求：`分析@PhysicsBlock/问题`（仓库内不存在名为「问题」的文件或目录，故按「分析 PhysicsBlock 模块存在的问题」执行）
> 方法：8 个只读子代理分模块审计（基础层／地图与空间索引／物理对象／碰撞检测／裁决器与约束／PhysicsWorld 主循环／PhysicsLiquid／GPU 与工具层）+ 父代理对全部高严重度条目的第一手复核
> 说明：本报告只读审计，**未修改任何引擎代码**。带 `[推测]` 的条目为静态分析推断、未经运行时验证。

---

## 0. 结论速览

| 级别 | 数量 | 性质 |
|---|---|---|
| **P0 致命** | 11 | 释放后使用（UAF）、确定性堆越界、死循环、NaN 永久污染求解器、多线程数据竞争 |
| **P1 严重** | 19 | 并发窗口、生命周期泄漏、物理量纲/符号错误、状态机缺陷、GPU 回读失效 |
| **P2 中等** | 16 | 语义错误、序列化丢数据、性能病理、非确定性来源 |
| **P3 轻微** | 12 | 死代码、文档漂移、API 加固点 |
| **误报澄清** | 6 | 既有文档与子代理误判，**修了会更糟** |

最核心的判断：**PhysicsBlock 的物理积分与约束公式本身基本正确**（详见 §6），问题集中在三类工程性缺陷——

1. **异步碰撞检测的生命周期管理**：投递即返回、跨帧不等待，而所有增删改入口都没有屏障（P0-1～P0-4 是同一根因的四个表现，构成完整的 UAF 链）；
2. **零/退化输入无保护**：`1.0f/0.0f = +inf` 导致「零质量守卫」永不成立（P0-6）、退化几何除零（P0-11）、轴对齐射线 ±inf 与死循环（P0-7/P0-8）——这类 NaN 一旦进入 `speed/angleSpeed` 就**无法自行恢复**；
3. **多线程分片维度选错**：碰撞检测按*索引*分片、冲量求解按*裁决器*分片，而共享刚体会跨裁决器（P0-9），检测阶段还会直接写共享对象状态（P1-1）。

---

## 1. P0 致命问题（优先修复）

### P0-1 异步碰撞检测任务跨帧驻留，而 `RemoveObject`/`AddObject`/`SetMapFormwork` 全都没有等待屏障

- **位置**：`PhysicsBlock/PhysicsWorld.cpp:876-891`（投递，不等待）、`PhysicsBlock/PhysicsWorld.cpp:334-344`（下一帧帧首才 wait）、`PhysicsBlock/PhysicsWorld.cpp:1250-1612`（RemoveObject 系）、`PhysicsBlock/PhysicsWorld.cpp:1179-1225`（SetMapFormwork）
- **代码证据**：
  ```cpp
  // PhysicsWorld.cpp:881-886
  // 判断物体间的碰撞（不影响位置，所以可以不用强制等待完成）
  xTn.push_back(mThreadPool.enqueue(XT_Fun, i, xThreadNum, &mCollideOutputs[i]));
  // 返回后不 wait，直接结束帧
  ```
  ```cpp
  // PhysicsWorld.cpp:1266-1267（RemoveObject 内部）
  PhysicsShapeS[i] = PhysicsShapeS.back();
  PhysicsShapeS.pop_back();   // 随后 :1348 delete shape;
  ```
  已核实：`WaitForCollisionThreads()` 定义在 `PhysicsWorld.cpp:1163`、声明在 `PhysicsWorld.hpp:559`，**在 PhysicsBlock 内部无任何调用点**（`grep RemoveCollisionPair|WaitForCollisionThreads` 仅命中 :240/:285/:1163）。
- **机制与后果**：
  1. `XT_Fun`（`PhysicsWorld.cpp:350-555`）在线程池里按**容器下标区间**遍历 `PhysicsShapeS[SizeD]`，而下标区间在任务开始时就固定；主线程的 `swap-and-pop` 会让工作线程读到越界元素或错误对象，`delete shape` 之后则是**对已释放内存调用虚函数**（`PFGetType`/`PFGetMass`/`PFGetCollisionR`）。
  2. 更隐蔽的一条：工作线程已把新裁决器推入 `mCollideOutputs[k].newGroup`（`PhysicsWorld.cpp:191-195` 所有权转移），而 `RemoveObject` 只清理 `CollideGroupS`/`NewCollideGroup`/`DeleteCollideGroup`，**从不触碰 `mCollideOutputs`**（grep 证实 `mCollideOutputs` 只出现在 :227/:878-886/:1143-1151）。于是下一帧 `ResolveCollideGroup()` 在 `PhysicsWorld.cpp:254` 的 `CollideGroupS.emplace(J->key, ...)` 因为键已被擦除而**成功**，这个指向已 delete 对象的裁决器被推进 `CollideGroupVector` 并长期驻留 → 此后每帧 `PreStep`（:672-683）和 `ApplyImpulse`（:709-723）都在解引用它。
  3. 同窗口内 `AddObject` 的 `mGridSearch.Add`（`GridSearch.hpp:539-553` 写 `mSlots/mNext/mCellHead`）与工作线程的 `mGridSearch.Get`（`PhysicsWorld.cpp:369`）并发 → 数据竞争。
- **触发条件**：任何在「本帧 PhysicsEmulator 之后、下一帧 PhysicsEmulator 之前」调用的增删改。工程内实际存在：`GameMods/FruitNinja/FruitNinja.cpp:256`（切中即 `RemoveObject`→`delete`，且它就在 PhysicsEmulator 之前执行）、`PhysicsBlock/PhysicsLiquid.cpp:151/200`（液体粒子每帧增删）、`GameMods/Character/GamePlayer.cpp:166`、`GameMods/Arms/Arms.cpp:280`、`PhysicsTest/PhysicsTest.cpp:1219-1220`。注意 `FruitNinja.cpp:63/237/471/496` 显式调用了 `WaitForCollisionThreads()`，说明作者知道这个坑，只是没有把屏障放进 API 内部。
- **修复建议**：在 `RemoveObject`/`AddObject`/`SetMapFormwork` 入口统一加屏障（`for (auto &tf : xTn) tf.wait(); xTn.clear();`），或把等待移到 `PhysicsEmulator` 末尾、取消跨帧驻留；`RemoveObject` 还需同时清理 `mCollideOutputs[*].newGroup/deleteGroup` 中涉及该对象的裁决器，并在 `ResolveCollideGroup` 的 emplace 前校验键两端存活。

### P0-2 `RemoveObject` 删除裁决器却不注销 `PhysicsCollision` 的 stay 记录 → 每帧悬垂回调

- **位置**：`PhysicsBlock/PhysicsWorld.cpp:1302-1322`（形状）、`:1386-1406`（粒子）及圆/线的同构分支；消费侧 `PhysicsBlock/PhysicsCollision.cpp:269-290`
- **代码证据**：
  ```cpp
  // PhysicsWorld.cpp:1302-1322（节选）
  CollideGroupS.erase(it);
  DeleteArbiter(arb);          // ← 缺 mCollision.RemoveCollisionPair(arb)
  ```
  ```cpp
  // PhysicsCollision.cpp:275（由 PhysicsEmulator:844 每帧驱动）
  arbiter.A_CollisionBinding.OnStay((PhysicsFormwork*)(arbiter.arbiter->mOriginalObject1), ...);
  ```
  对照：`RemoveCollisionPair` 在 PhysicsWorld.cpp 中**只出现在 :240 与 :285**（即 `ResolveCollideGroup` 的 deleteGroup 路径）。
- **机制与后果**：`AddCollisionPair` 把 `BaseArbiter*` 原样存进 `mCollisionArbiterStayS`/`mMapCollisionArbiterStayS`（`PhysicsCollision.cpp:214-220`、:263-266），只有 `RemoveCollisionPair`（:293-328）会摘除并触发 `OnExit`。`RemoveObject` 绕过它 → stay 里的裸指针在 `newElement` 复用后指向**别的碰撞对**（读到别人的 `mOriginalObject1/2`），`ProcessCollisions` 仍按旧记录回调，并把已 delete 的对象传给 `OnStay`；同时该对**永不触发 `OnExit`**（持续伤害、音效、高亮、门状态永不复位）。
- **触发条件**：任何 `RemoveObject`，只要该对象当时处于碰撞中（几乎必然）。
- **修复建议**：四个分支在 `DeleteArbiter(arb)` 之前补 `mCollision.RemoveCollisionPair(arb);`。

### P0-3 `SetMapFormwork` 换图不清理旧地形的裁决器与碰撞记录 → 幽灵地面 + 双重求解

- **位置**：`PhysicsBlock/PhysicsWorld.cpp:1179-1225`（`GridWindSize`/`SetMapRange`/`mGridCenter` 三行之外无任何清理）；`PhysicsBlock/PhysicsWorld.cpp:23-30`（`Map_Delete` 宏）；`PhysicsBlock/PhysicsCollision.cpp:252-266`
- **代码证据**：
  ```cpp
  // PhysicsWorld.cpp:1208-1210
  GridWindSize = MapFormwork_->FMGetMapSize();
  mGridSearch.SetMapRange(...);
  mGridCenter = mGridSearch.GetGridCenter();
  ```
  ```cpp
  // PhysicsWorld.cpp:23-30（Map_Delete 宏，只删除用"当前" wMapFormwork 构造的键）
  ```
- **机制与后果**：地形裁决器（ArbiterS/P/C/L）的 `object2` 是构造时捕获的 `MapFormwork*`，且**跨帧持久**（A/D 系裁决器解引用前没有空指针检查）。换图后窄相用新指针构造键，而旧键 `(obj, oldMap)` 再无删除路径 → 旧裁决器永久留在 `CollideGroupVector` 里，每帧对**已释放的旧地图**做 `PreStep`/`ApplyImpulse`（幽灵地面），并与新地图的裁决器并存 → 同一物体被两套地形双重求解。
- **触发条件**：运行中换图。工程内多处：`PhysicsTest/PhysicsDemo.cpp` 的 `SetMapFormwork(mMapStatic)/(mMapDynamic)`、`Dungeon.cpp:93/180`、`Labyrinth.cpp:181/257`、`FixedMaze.cpp:87/143`（含 `SetMapFormwork(nullptr)`）。
- **修复建议**：`SetMapFormwork` 内遍历并清空所有地形类裁决器（连同 `mCollision` 的地形绑定），或把 `MapFormwork*` 换成带版本号的句柄。

### P0-4 `GridSearch::SetMapRange` 不重置槽位与对象 `mGridIndex` → `Remove` 静默失败 → 幽灵槽位 → 下次重建对已析构对象虚调用

- **位置**：`PhysicsBlock/GridSearch.hpp:480-482`（重建）、`:566-599`（Remove）、`:786`（重建时对每个非空槽位取 `PFGetCollisionR()`）
- **代码证据**：
  ```cpp
  // GridSearch.hpp:480-482
  std::vector<unsigned int>().swap(mCellHead);
  mCellHead.resize(GridSize, UINT_MAX);
  mDirtyCells.clear();
  // mSlots / mNext / mFreeSlots / mNewIndex / GridExtrovert 以及每个对象的 mGridIndex 全部保持旧值
  ```
  ```cpp
  // GridSearch.hpp:786
  mNewIndex[slot] = atIndex(Formwork->PFGetPos(), Formwork->PFGetCollisionR());  // virtual 调用
  ```
- **机制与后果**：`Remove` 用**旧** `mGridIndex` 去查新格表（:574）必然落空，回退查 `GridExtrovert`（:590-598）也找不到 → 落到 `:599` **静默 return，槽位不回收**。调用方随即 `delete` 对象 → `mSlots[slot]` 成为悬垂指针 → 下一帧重建时 `:786` 对每个非空槽位执行**虚函数调用**（从已释放内存取 vptr）= UB/崩溃；即便侥幸返回，垃圾格号会把悬垂指针挂进 `mCellHead`，之后 `Get` 返回它、`XT_Fun` 再 `i->PFGetType()` = 二次 UAF。
- **触发条件**：`SetMapFormwork(有效地图)`（内部走 `SetMapRange`）之后、下一次网格重建（`PhysicsWorld.cpp:856-866`）之前调用 `RemoveObject`。可达路径：`MazeMods.cpp:397/728` 换图；`Dungeon.cpp:93` 注册地图时已有玩家碰撞体（`GamePlayer.cpp:111`）。
- **修复建议**：`SetMapRange` 内同步重置全部槽位状态（非空槽位 `mGridIndex/mNewIndex = UINT_MAX`、`mNext = UINT_MAX`；清 `GridExtrovert`/`mFreeSlots`）；`Remove` 改为返回 `bool`，找不到时不得静默。

### P0-5 `BaseOutline` 的 JSON 构造函数从不分配 `MaxOutlineCentreMass` → 野指针写 + `delete[]` 野指针

- **位置**：`PhysicsBlock/BaseOutline.hpp:42-52`（构造）、`PhysicsBlock/BaseOutline.hpp:22`（成员无初始化器）、`PhysicsBlock/BaseOutline.cpp:29-34`（析构）；触发链 `PhysicsBlock/PhysicsShape.hpp:35-41` → `PhysicsShape.cpp:184-201`
- **代码证据**：
  ```cpp
  // BaseOutline.hpp:42-52
  BaseOutline(const nlohmann::json& data) : BaseGrid(data)
  {
      int size = width * height * 3;
      if (size < 4) { size = 4; }
      FrictionSet = new FLOAT_[size];
      OutlineSet  = new Vec2_[size];
      JsonContrarySerialization(data);   // ← MaxOutlineCentreMass 从未 new
  }
  ```
- **机制与后果**：`MaxOutlineCentreMass` 是裸指针且无默认初始化器 → indeterminate。`PhysicsShape(const json&)` 在构造体内立即 `UpdateAll()` → `UpdateMinOutline(CentreMass)` → `MaxOutlineCentreMass[OutlineSize] = MaxOutlineCentre(x, y);`（`BaseOutline.cpp:222/230/239/249`）在野指针上写；此后每帧碰撞都读（`PhysicsBaseCollide.cpp:179/211/303`）；析构再 `delete[]` 同一野指针。
- **触发条件**：**任何一次从 JSON 反序列化 PhysicsShape**（存档读取、地图加载），不需要特殊数据即必然发生。触发点：`PhysicsBlock/PhysicsWorld.cpp:1919` `AddObject(new PhysicsShape(data["PhysicsShapeS"][i]));`。
- **修复建议**：补 `MaxOutlineCentreMass = new Vec2_[size];`（建议把三个数组的分配抽成共用 `AllocateOutline(size)`），三个指针成员统一 `= nullptr`，析构判空。

### P0-6 `PhysicsShape::UpdateInfo` 的「零质量守卫」写法错误 → 空形状/零质量产生 NaN 或 +inf 并扩散到整片物体

- **位置**：`PhysicsBlock/PhysicsShape.cpp:93-105`（另 `:150-154` 同为 `MomentInertia == 0` 判据）
- **代码证据**（父代理第一手核实）：
  ```cpp
  CentreShape /= Size;
  invMass = 1.0 / mass;
  if (invMass == 0)      // FLOAT_ = float（BaseStruct.hpp:13）时 1.0f/0.0f = +inf，恒不等于 0
  { mass = FLOAT_MAX; CentreMass = CentreShape; }
  else
  { CentreMass /= mass; } // mass==0 → 0 除 → NaN
  ```
- **机制与后果**：
  - `Size == 0`（无 `Collision` 格的空形状）→ `CentreShape /= 0` 直接 NaN；
  - `mass == 0` → `invMass = +inf`，走进 `else` 分支 `CentreMass /= 0` = NaN，随后 `:107-112` 的 `+{0.5,0.5}`、`pos += vec2angle(CentreMass - UsedCentreMass, angle)`、`OldPos = pos` 全部污染，`:142` 的 `lpos -= CentreMass` 让 `MomentInertia` = NaN → `invMomentInertia` = NaN；
  - NaN 经 `PhysicsBaseArbiter.cpp:85`（`c->massNormal = 1.0/kNormal`）、`:135`（`c->Pn = std::max(NaN, 0)`）、`:147`（`object2->speed += object2->invMass * Pn`）写入**对方**物体 → 表现为「整片物体瞬间消失」。
  - 之所以平时没炸：静态形状用块 `mass = FLOAT_MAX`（`PhysicsDemo.cpp:358/419`）累加成 `+inf`，`1/inf = 0` 恰好命中守卫，掩盖了守卫本身写错。
- **触发条件**：用户可达——`ImGuiPhysics.cpp:242-250` 把 mass 拖成 0 后立即 `Object->UpdateAll()`；或构造出的形状没有 `Collision` 格。
- **修复建议**：判据改为 `if (!(mass > 0.0f))` / `if (!(MomentInertia > 0.0f))`，并在 `Size == 0` 时提前返回。

### P0-7 `BaseGrid::BresenhamDetection(ivec2)` 在 `dx == 0 && dy < 0` 时死循环

- **位置**：`PhysicsBlock/BaseGrid.cpp:53-91`（判定在 :79-89）
- **代码证据**：
  ```cpp
  int err = dx - dy;
  while (true) {
      /* 唯一出口：起点格 Collision == true */
      if (end.x == start.x && end.y == start.y) return {false, {0,0}, FLOAT_{}};
      e2 = err << 1;
      if (e2 > -dy) { err -= dy; start.x += sx; }
      if (e2 < dx)  { err += dx; start.y += sy; }
  }
  ```
- **机制与后果**：取 `start=(0,0)`、`end=(0,-1)`：`dx=0, dy=1, sx=-1, sy=-1, err=-1`；每轮 `e2 = -2`，两个条件（`-2 > -1`、`-2 < 0`）**恒假** → `start`、`err` 永不变化，终止判定也不成立（`start.y` 恒 0，`end.y = -1`）→ **无出口死循环**（已逐轮模拟 6 轮确认）。零长射线同理。因为唯一出口是「命中碰撞格」，只要沿途没有碰撞格就必然挂死。
- **触发条件**：射线整数化后满足 `dx == 0 && dy < 0`（或零长）。整数版经 `PhysicsShape::PsBresenhamDetection(glm::ivec2)`（`PhysicsShape.cpp:242-245`）与公开接口 `RayCollide`/`ApproachDrop`（`PhysicsShape.cpp:211-233`）可被任意角度调用。
- **修复建议**：函数开头分流处理 `start.x == end.x`（沿 y 单步扫描）与 `start.y == end.y`（沿 x），或把判定改成 `e2 >= -dy` / `e2 <= dx`；并加 `dx + dy + 1` 最大步数兜底。

### P0-8 浮点版 Bresenham 对轴对齐射线算出 ±inf，并且 `LineSquareFocus` 的越界裁剪点会放大该错误

- **位置**：`PhysicsBlock/BaseGrid.cpp:122-125`（`Difference`/`invDifference`）、`:156-175`（修正分支对退化线永不成立）；源头 `PhysicsBlock/BaseCalculate.cpp:66-99`、`:102-114`
- **代码证据**：
  ```cpp
  // BaseGrid.cpp:122-125
  FLOAT_ Difference    = (end.x - start.x) / (start.y - end.y);  // 水平射线：x/0 = ±inf
  FLOAT_ invDifference = 1.0 / Difference;
  FLOAT_ valX = start.x - end.x;
  FLOAT_ valY = start.y - end.y;
  ```
  ```cpp
  // BaseCalculate.cpp:66-99（节选）
  if (end.x == start.x) { if ((start.x > 0) && (start.x <= width)) { ... } else data.Focus = false; return data; }
  ```
- **机制与后果**：`Difference` 一旦是 `inf`，`calcFromY` 的 `Difference * (end.y - info.pos.y) + end.x` 产出 `pos.x = ±inf`；该 inf 传出后 `PhysicsBaseCollide.cpp:185-189` 的 `separation = info.pos.y - DropPos.y` = inf，`:191` 取 `-abs` 仍是 inf，接触点 `position` 也是 inf → 冲量爆炸或 `inf - inf` = NaN 污染整个 island 的裁决器状态。触发条件极常见：**自由落体、沿平地纯水平滑动**（两帧位移只有单一分量）时 `start.x == end.x` 或 `start.y == end.y` 成立。
- **附带缺陷（同一族）**：`LineSquareFocus` 把 `x == 0 / y == 0 / x == width / y == height` 四个**合法贴边**位置判为不相交（`>` 而非 `>=`，`BaseCalculate.cpp:68/86`），且一般分支的两个轴裁剪互不穿插（只有 else-if 链、裁完不回头校验另一轴）→ 产生越界裁剪点（例：`start=(5,11)`、`end=(7,10)`、`W=H=10` → `data.start = {25, 0}`），正是 P0-7/P0-8 的燃料。注释掉的正确实现（`BaseCalculate.cpp:119-164`）应恢复（Liang–Barsky / Cohen–Sutherland）。
- **修复建议**：浮点版开头显式分流轴对齐射线（`start.y == end.y` / `start.x == end.x`），只在两者都不等时走 `Difference` 公式，并对 `Difference` 加 `std::isfinite` 断言；`LineSquareFocus` 改闭区间判定 + 统一 t 区间裁剪。

### P0-9 `ApplyImpulse` 按「裁决器下标」分片：同一刚体被多线程同时读改写

- **位置**：`PhysicsBlock/PhysicsWorld.cpp:701-738`（分片 :709，迭代 :725-738）
- **代码证据**：
  ```cpp
  // PhysicsWorld.cpp:701（注释）+ :709
  // 虽然会增加不确定性，但是我暂时不需求确定性。
  ThreadTaskAllot(SizeD, SizeY, CollideGroupVector.size(), T_Num, Tx);
  ...
  CollideGroupVector[SizeD]->ApplyImpulse();
  ```
  ```cpp
  // PhysicsBaseArbiter.cpp:172-178（被并发执行的写点）
  object1->speed -= object1->invMass * Pt;
  object2->speed += object2->invMass * Pt;
  ```
- **机制与后果**：分片维度是**裁决器下标**，而一个刚体同时出现在多个裁决器中（堆叠方块既有「对地面」也有「对邻块」）→ 两个线程对同一 `PhysicsShape` 的 `speed/angleSpeed` 做非原子读改写 → 丢更新（堆叠缓沉、抖动、穿透）+ UB。全库仅 `PhysicsTrigger.hpp:226`、`PhysicsKinematic.hpp:271` 有 `mutable std::mutex`，arbiter/particle 路径**无任何互斥或原子**。对比：`PhysicsSpeed`（:565-630）与 `PhysicsPos`（:769-836）两个 pass 按**物体**分片、集合互不相交，是安全的——只有求解这一步分片选错了维度。另 `:725-738` 每次迭代一次全局屏障，粒度过细。
- **触发条件**：`xThreadNum = min(hardware_concurrency(), 8)` ≥ 2 且存在共享刚体的两个以上裁决器（正常场景必然）。
- **修复建议**：按物体（或连通岛/岛分组）分片，或改走 `:742-756` 的串行分支；至少给 `speed/angleSpeed` 的累加加原子或每线程局部累加后归并。

### P0-10 多条窄相路径写 `contacts[]` 没有 `PhysicsContactMaxSize` 上限 → 越界写裁决器内存

- **位置**：`PhysicsBlock/PhysicsBaseCollide.cpp:298-323`、`:337-364`（Shape-Map 两轮）、`:440-492`（Circle-Map）、`:753-828`（Line-Shape/Line-Map）、`:917-1020`
- **代码证据**：
  ```cpp
  // PhysicsBaseCollide.cpp:319-321
  contacts->w_side = ContactSize; ++contacts; ++ContactSize;   // 循环条件：for (i < Shape->OutlineSize)
  ```
  ```cpp
  // BaseArbiter.hpp:198
  Contact contacts[PhysicsContactMaxSize];   // BaseDefine.h:34 #define PhysicsContactMaxSize 20
  ```
  已核实：全文件只有 `:174` 与 `:206`（Shape-Shape 两轮）带 `ContactSize < PhysicsContactMaxSize` 保护；其余分支的 `ContactSize` 自增点（:321/:363/:459/:490/:596/:618/:765/:800/:824/:957/:992/:1016）循环条件均无上限。
- **机制与后果**：`FMGetMinOutline`（`MapStatic.cpp:199-243`）对每个碰撞格可 push 最多 4 个角点且不去重、无上限；Shape-Map 第一轮仅按 `OutlineSize` 循环即可能写 >20 个（宽 64 的形状 51 个），第二轮遍历 `(2*radius+3)²` 格 × 每格 4 点。越过 `contacts[19]` 后覆盖 `numContacts`、`key`、`mOriginalObject1` 以及相邻堆对象 → 内存破坏 + 后续按垃圾 `numContacts` 遍历。
- **触发条件**：轮廓点数 > 20 的形状接触薄地形，或大圆靠近曲折地形。
- **修复建议**：两处循环都加 `&& ContactSize < PhysicsContactMaxSize`，并给轮廓数量设上限。

### P0-11 退化几何除零产生 NaN，并经热启动永久驻留

- **位置**：`PhysicsBlock/PhysicsBaseCollide.cpp:507-516`（圆-粒子）、`:532-544`（圆-圆）、`:614-615`（圆-形状）、`:852`（线-圆）、`:888`（线-粒子）
- **代码证据**（父代理第一手核实）：
  ```cpp
  // PhysicsBaseCollide.cpp:507-518
  Vec2_ dp = Particle->pos - Circle->pos;
  FLOAT_ L = ModulusLength(dp);
  FLOAT_ R2 = Particle->radius + Circle->radius; R2 *= R2;
  if (R2 > L) { L = SQRT_(L); contacts->separation = L - Circle->radius; contacts->normal = dp / L; ... }
  ```
- **机制与后果**：圆心/粒子重合时 `L == 0` → `0/0 = NaN`；点-线旧位置恰在线段上、线与圆同心时同理。NaN 经 `PreStep`（`PhysicsBaseArbiter.cpp:85` `c->massNormal = 1.0/kNormal`）与 `ApplyImpulse` 写入 `speed/angleSpeed`，而 `Update`（`:18-41`）按 `w_side` **保留旧 `Pn`** → NaN 不会自行恢复（永久污染）。像素坐标常为整数，同心/共线/沿线段滑行极易命中。
- **修复建议**：归一化前判 `if (L < 1e-6f) { 用确定性兜底轴; }` 或直接返回 0 接触；在 `PhysicsParticle/PhysicsAngle` 的积分入口加 NaN 检测与复位。

---

## 2. P1 严重问题

### P1-1 碰撞检测阶段直接写共享对象状态（检测本应是只读查询）

- **位置**：`PhysicsBlock/PhysicsWorld.cpp:399`、`:469`（`((PhysicsParticle*)i)->OldPosUpDataBool = false;`）；`PhysicsBlock/PhysicsBaseCollide.cpp:270`、`:408`、`:776`、`:894`、`:968`
- **证据**：`Particle->OldPos = info.pos - (contacts->normal * FLOAT_(0.1)); Particle->OldPosUpDataBool = false;`，其中 `:270`（Shape-Particle）用 `+`、`:408`（Particle-Map）用 `-`，同一语义符号相反。
- **后果**：碰撞检测在多个 worker 上并发执行，对同一粒子的非原子写 → 撕裂值；且把扫掠原点挪到接触点附近，同帧内该对象参与的其他碰撞对以被篡改的 `OldPos` 为起点 → 真实扫掠路径丢失（漏检）。
- **修复**：推出量移到求解后的位置修正阶段，或写入独立的 `PushOutPos` 字段（检测函数保持只读）。

### P1-2 地图生成回调派发到线程池，与物理线程读同一 `GridBlock` 位域并发

- **位置**：`GameMods/UnlimitednessMapMods/Dungeon.cpp:39-52`（写）、`:76`（投递）、`PhysicsBlock/PhysicsWorld.cpp:886`（读）
- **证据**：`LDungeon->MultithreadingGenerate.push_back(TOOL::mThreadPool->enqueue(&GenerateBlock, mT, x, y, Data));` + `(*mT)->at((int)ix, (int)iy).Collision = CollisionBool;`
- **后果**：`Collision/Entity/Event` 是同一字节内的 1 位位域，写入是读-改-写，而物理窄相在另一 worker 上读同一字节 → 竞争，可能丢更新或读到相邻位 → 单帧幻影碰撞（非崩溃级）。
- **修复**：地形生成/删除回调改在物理帧边界同步执行，或把 `Collision` 拆成独立字节/`std::atomic`。

### P1-3 GPU 回读缓冲没有 `HOST_COHERENT`，也没有任何 `vkInvalidateMappedMemoryRanges`

- **位置**：`PhysicsBlock/PhysicsGPU.cpp:610-617`、`:637-641`（分配）、`:514`（`UnpackBodyBuffer`）、`:1087-1088`（submitSync 后立即回读）；`Vulkan/buffer.cpp`（只 `vkMapMemory`）
- **证据**：
  ```cpp
  // PhysicsGPU.cpp:610-617
  mBodyStaging = new VulKan::Buffer(mDevice, N * BODY_BYTES, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT);   // 缺 HOST_COHERENT
  ```
  全工程无 `vkInvalidateMappedMemoryRanges`/`vkFlushMappedMemoryRanges` 调用。
- **后果**：非一致（non-coherent）内存设备上，`UnpackBodyBuffer` 读到的是**陈旧显存**（speed/angleSpeed/Pn/Pt）→ GPU 分支物理解算结果与 CPU 分支分歧（多数 N 卡驱动侥幸正确，掩盖了问题）。
- **修复**：改用 `HOST_VISIBLE | HOST_COHERENT`，或在 `submitSync` 后对四个 staging 缓冲（body/arbiter/joint/junction）调用 `vkInvalidateMappedMemoryRanges`。
- **附**：`PhysicsGPU.cpp:836-838/882-886` 只把 `mArbiterCount` 加入 hostBarriers，`mJointCount/mJunctionCount` 缺 `HOST_WRITE → SHADER_READ` 屏障（靠 HOST_COHERENT 侥幸）；`:674-690` 删除 VkBuffer/Pipeline/CommandPool 前无 `vkDeviceWaitIdle`。

### P1-4 液体求解全链路无 NaN/Inf 检测与清理，污染不可恢复

- **位置**：`PhysicsBlock/PhysicsLiquid.cpp:1502-1540`（`Update` 仅判 `n == 0 || time <= 0 || mWorld == nullptr`）
- **证据**：唯一限速器 `EnforceCFL` 的 `ClampDisplacement` 用 `if (len2 > maxLen * maxLen) v *= maxLen / sqrt(len2);` —— NaN 时比较恒假 → 钳制不触发；`FindNeighbors` 的 `ToInt(NaN)` → `INT_MIN`，`HashCell` 的 `(unsigned)gx * 92837111u` 回绕；NaN 经 `mPairDir/mPairW/mLambda/mDelta`（`mLambda[i] + mLambda[j]`）传遍邻居，数帧内污染整池，无回滚路径。
- **修复**：`IntegrateFluid`/`SolveFluidJacobi`/`CommitFluid` 入口加 `std::isfinite` 过滤与复位；`ClampDisplacement` 显式判 `!std::isfinite`。

### P1-5 `mBoundaryDisp` 对非近壁粒子不清零 → 陈旧位移跨子步、跨帧驱动漂移

- **位置**：`PhysicsBlock/PhysicsLiquid.cpp:1027-1031`（清零在 `if (!mNearSolid[i]) { continue; }` **之后**）；消费侧 `PhysicsLiquid.cpp:1387-1399`（`EnforceCFL` 对全部 `i` 读 `mBoundaryDisp[i]`）
- **证据**：`const Vec2_ b = mBoundaryDisp[i]; const Vec2_ a = (mPos[i] - b) - mPrevPos[i]; ... mPos[i] = mPrevPos[i] + d + (b - bVel);`
- **后果**：贴壁弹开的粒子在开阔水域仍携带旧 `b`（可达 0.054/子步，帧内累计约 0.32）→ 水面「抽一下」与伪湍流。
- **修复**：把清零提到 `continue` 之前。

### P1-6 `FlushSolidGap` 把整帧间隙位移当速度叠加，且不受 CFL 限速

- **位置**：`PhysicsBlock/PhysicsLiquid.cpp:1330`（`mSolidGapT[bi] += move;`）、`:1363`（`const Vec2_ dv = mSolidGapT[bi] / time;`）、`:1368`（`body->speed += dv;`）；对照 `CommitSolids` 有 `maxSpeed = cfl*h/(time/substeps)` + Clamp（`:1453-1473`）
- **后果**：6 子步 × 0.054 = 0.324，除以 1/60 得约 **19.4 m/s**；demo 参数（spacing 0.5 → gap = 1.0）可达 **27 m/s** → 水面蹦床式周期弹跳。物理上位置修正不应再折算成速度。
- **修复**：速度注入改为受限的松弛项（或只做位置修正），并复用 CFL 钳制。

### P1-7 `invMass == 0`（含 `mass == 0`）的液体粒子被当成质量 1 参与约束求解

- **位置**：`PhysicsBlock/PhysicsLiquid.cpp:890-891`、`:918`、`:943`（`const FLOAT_ invM = (i >= n) ? SampleWeight(i) : FLOAT_(1);`）；`:784`（`mVel[i] += dt * (g + p->invMass * p->force);`）；质量 0 的来源 `PhysicsParticle.cpp:17-24`
- **后果**：不可动粒子仍以权重计入邻居密度 `rho`（`:871`）→ 密度虚高、粒子间持续互推，且 `SampleWeight` 与 `invMass` 语义混用。
- **修复**：求解统一用真实 `invMass`（并处理 0），密度核的固相权重与质量分离。

### P1-8 邻居数上限 `kMaxNeighbors = 64` 截断破坏作用力对称性

- **位置**：`PhysicsBlock/PhysicsLiquid.cpp:604-641`（`if (r2 < h2 && id != i && count < kMaxNeighbors)`）；消费侧 `:856-886`、`:931-940`（`mLambda[i] + mLambda[j]`、`mGrad[i] - mGrad[j]`）
- **后果**：要求互为邻居，而 `i` 见 `j`、`j` 不见 `i` 时压力不守恒成对 → 界面处净动量注入。静止邻居约 11 个/粒子（spacing 0.5、h 0.9375）尚安全；粒子被压到 `r < h/4.5` 时触发。
- **修复**：对称化（收集后取交集/按距离取前 k 并双向登记），或把 `kMaxNeighbors` 提高到 h 内最密排布的理论上界。

### P1-9 压力约束单向且无静水压强梯度 → 托举只靠瞬态过密（「贴底锁死」的真实机制）

- **位置**：`PhysicsBlock/PhysicsLiquid.cpp:894-897`
- **证据**：`FLOAT_ lambda = -c / (sumGrad2 + param.eps); if (c < FLOAT_(0)) { lambda = FLOAT_(0); }`（只压不拉），`c = rho/restDensity - 1` → 静息水体 `λ ≡ 0`，无 `ρgh` 静压项。
- **后果**：`EnforceSolidGap`（gap = `2*max(1.6r, 0.05)`，默认 0.48，`:1260-1342`）只保证几何缝隙，不创造力。既有文档 `PhysicsLiquid贴底锁死分析.md` 的机制判断**属实**（其「方案 1 已落地」也属实，但引入了 P1-6 的能量注入）。
- **修复**：引入静水压/密度偏移项，或对固液界面使用单边约束 + 位置修正（不加速度）。

### P1-10 无固液黏性/阻力：`param.friction` 在液体路径完全未使用

- **位置**：`PhysicsBlock/PhysicsLiquid.cpp:931-943`（压力项 + 表面张力仅 `!isSample && j < n`，即流体↔流体）、`:1408-1450`（`ApplyXSPH` 只遍历 `i < n` 且 `if (mInside[i]) continue;`）
- **后果**：液体不携带刚体、无水中阻力（物体在水中不受减速），与视觉预期和 `param.friction` 的参数语义不符。
- **修复**：压力/黏性核扩展到固相采样点，并按 `friction` 施加切向阻力。

### P1-11 `PhysicsJunction` 两端点重合时 `Normal = dp / 0` → NaN 污染

- **位置**：`PhysicsBlock/PhysicsJunction.cpp:40`（另 `:118`、`:184`、`:241`）；防护 `:74`（`if (k == 0) return;`）拦不住 NaN
- **后果**：NaN 写入 `speed/angleSpeed` 后不可自愈（junction 无累积冲量、无 warm start，见 P2-8）。
- **修复**：`dp` 长度小于阈值时跳过该约束或使用上一次的方向。

### P1-12 `PhysicsJoint` 用 `glm::inverse(K)`，两端不可移动时 K 奇异；softness 恒 0

- **位置**：`PhysicsBlock/PhysicsJoint.cpp:77-80`；`softness` 由 `Set`（`:37`）置 0
- **后果**：`invMass = invMomentInertia = 0` 时 K 全零 → `inverse` 得 inf/NaN；`softness = 0` 意味着正则化永不生效，任何近奇异配置都会放大数值噪声（承重抖动）。
- **修复**：加 `softness` 正则项（如 `K(0,0) += softness`）并对 K 的可逆性做守卫（用伪逆或轴分解）。

### P1-13 Line 路径投影参数把 `len²` 当 `len` 用 → 法线方向随机化

- **位置**：`PhysicsBlock/PhysicsBaseCollide.cpp:744-747`、`:884-888`、`:947-952`
- **证据**：`FLOAT_ len = ModulusLength(AB); FLOAT_ t = dotProduct / len;` —— `ModulusLength` 返回 `x² + y²`（`BaseCalculate.hpp:81-84`），正确应为 `dot(AP,AB)/dot(AB,AB)`。
- **后果**：`|AB| = 10` 时 `t` 放大 10 倍，垂足被推到线段之外，`normal` 变成任意方向（只有 `|AB| ≈ 1` 时才碰巧正确）→ Line-Shape/Line-Particle/Line-Map 三处法线全错。
- **修复**：改用 `FLOAT_ lenSq = Dot(AB, AB); t = dotProduct / lenSq;` 并加 `lenSq ≈ 0` 保护。

### P1-14 `separation` 语义错误（用距离代替穿透深度、缺半径项）

- **位置**：`PhysicsBlock/PhysicsBaseCollide.cpp:761`、`:889`、`:953`、`:447`
- **证据**：`contacts->separation = -Modulus(contacts->position - DropPos);` —— `Modulus` 返回欧氏距离（恒正），取负后与穿透深度无关，也未减半径项；`:447` 的 `-abs(info.pos.y - d.y)` 符号可正可负。
- **后果**：`PreStep`（`PhysicsBaseArbiter.cpp:95` `std::min(0.0f, c->separation + k_allowedPenetration)`）的 bias 直接变成错误的位置修正量（推入或弹出量错误）。
- **修复**：统一为「沿接触法线的重叠量（负值 = 穿透）」，逐路径核对法线与分离量一致性。

### P1-15 `PhysicsShape::UpdateInfo` 写 `CentreMass`/`pos` 的同时，`PhysicsAngle::PhysicsSpeed` 在 `invMass == 0` 时提前返回，吞掉 torque 清零与角度积分

- **位置**：`PhysicsBlock/PhysicsAngle.cpp:132-143`（`if (invMass == 0) return;` 位于 `torque = 0` 之前）、`:152-159`、`:70-73`（`AddTorque` 不判 `invMass` 也不重置 `StaticNum`）
- **后果**：`invMass == 0` 期间 torque 无限累加，`PhysicsKinematic.cpp:159-210` 恢复 `invMass` 后下一帧一次性释放（突然高速自旋）；`invMass == 0` 但 `invI > 0` 的物体 `angle` 永不积分；`PhysicsBaseArbiter.cpp:140-149` 的角冲量也被 `invMass` 门控绑死。
- **修复**：torque 清零与 `StaticNum` 复位移到早退之前；角冲量门控改用 `invMomentInertia`。

### P1-16 静止判定三重不一致 + `OldPos` 跨 2 帧比较

- **位置**：`PhysicsBlock/PhysicsParticle.cpp:118-144`、`PhysicsBlock/PhysicsWorld.cpp:363/377/445/502/546`（硬编码 `StaticNum > 10`）、`PhysicsBlock/BaseDefine.h:38`（`PhysicsSleepThreshold 10` **全仓零引用**）、`PhysicsBlock/PhysicsBaseCollide.cpp:271/409/777/895/969`（置 `OldPosUpDataBool = false`）
- **后果**：文档所述「连续静止 ≥ 200 帧休眠」从不生效、调参无效；接触中的物体旧位置更新被冻结 → 下一帧比较的是**相隔 2 帧**的位置，`X(n-2) == X(n)` 的周期性抖动被判为「没动」→ 涨过 10 后该碰撞对被跳过 → 接触面穿插；`OldPos == pos` 用精确浮点相等，无容差；`invMass == 0` 的物体 `StaticNum` 永不增长（静态几何体每帧全量窄相）。
- **修复**：统一阈值常量（或删除死宏）、比较加 epsilon、接触时的 `OldPos` 冻结改为只影响 CCD 不影响静止判定。

### P1-17 `PhysicsKinematic` 状态机缺陷：裸指针键残留 + 退出时 `mass == 0` 永久冻结 + 无类型保护的向下转型

- **位置**：`PhysicsBlock/PhysicsKinematic.cpp:290-298`（`RemoveKinematicData`，声明于 `PhysicsKinematic.hpp:238`，**全仓零调用**；`PhysicsWorld.cpp:1250` 的 `RemoveObject` 也不调）、`:184-209`（进入无条件 `invMass = 0`，退出用 `if (p->mass > 0) invMass = 1/mass` 守卫）、`:284`（`static_cast<PhysicsAngle*>(object)->angle = ...`）、`:121`（`data.Motion.IsRotating = true;` 无类型限制）
- **证据**：
  ```cpp
  // PhysicsKinematic.cpp:31/34（对任意 PhysicsFormwork 无条件调用）
  data.SavedAngleSpeed = object->PFAngleSpeed();
  object->PFAngleSpeed() = 0.0f;
  ```
  ```cpp
  // PhysicsParticle.hpp:199-203（Release 下 assert 消失，别名 invMass）
  virtual FLOAT_ &PFAngleSpeed() { assert(0 && "[Error]: 粒子不存在角速度!"); return invMass; }
  ```
- **后果**：
  1. `mKinematicData` 以裸指针为键（`GetOrCreateData`，`:306-314`）且永不清理 → 地址复用后新物体继承 `IsKinematic`/`SavedSpeed` → 「幽灵惯性」；
  2. `mass == 0` 的物体退出运动学态后永久冻结（悬停不动）；
  3. 对粒子调用 `RotateTo` 后 `static_cast<PhysicsAngle*>` 越界写堆（`PhysicsAngle` 在 `PhysicsParticle` 之后追加成员）——**当前调用点仅 `GameMods/PhysicsTest/PhysicsDemo.cpp:1445/1468/1469`，两对象都是 `PhysicsShape`（有 angle），故属潜伏缺陷，不是现行崩溃**；
  4. Debug 下「粒子 + kinematic」直接断言中断；Release 下把 `invMass` 当角速度读改写。
- **修复**：`RemoveObject` 内调 `RemoveKinematicData`；退出时无守卫地重算 `invMass`；`RotateTo`/`UpdateKinematicMotion` 加 `PFGetType()` 判定（非 angle 直接拒绝）。

### P1-18 触发器系统：质心单点判定漏事件 + 层级过滤死功能 + 删除不补发 `OnTriggerExit`

- **位置**：`PhysicsBlock/PhysicsTrigger.cpp:176-179`（`config.TriggerBounds.Contains(obj->PFGetPos())`）、`:115/126`（`TriggerLayers`，`ProcessTriggers`（`:152-211`）从不读取）、`:17-27` 与 `:247-252`（`RemoveTrigger`/`Clear` 直接 `erase`，唯一派发路径是 `ProcessTriggers` 的差集 `:199-208`）
- **后果**：单帧位移大于触发区尺寸时必漏 Enter/Stay/Exit；`TriggerLayers` 为死功能；删除触发器/清空时**不补发 `OnTriggerExit`** → 持续伤害、门、计数器永不复位。
- **修复**：用扫掠/包围盒相交判定；读取 `TriggerLayers`；删除前对活跃重叠派发 Exit。

### P1-19 `PhysicsAssembly` 存档往返丢成员 + `Add` 无幂等/判空

- **位置**：`PhysicsBlock/PhysicsAssembly.cpp:101-111`（`data["childCount"] = mChildren.size();` 而 children 数组循环写的是 `mChildDescriptors`，仅 `BuildChildDescriptors` 后非空）、`:30-34`（`Add`）
- **后果**：存档往返后装配体成员丢失（`ResolveChildren`（`PhysicsWorld.cpp:1948-1952`）得空表 → 「零成员幽灵装配体」）；同一子体可属多个装配体（第一个装配体 `mChildren` 仍留指针，`SameAssembly`（`PhysicsWorld.hpp:518`）失效）、`Add(nullptr)` 崩溃。
- **修复**：序列化以 `mChildren` 为唯一数据源（或先 `BuildChildDescriptors`）；`Add` 判空 + 去重 + 校验 `child->assembly == nullptr`。

---

## 3. P2 中等问题

- **P2-1 宽相 AABB 只看当前 `pos`，不覆盖 `OldPos → pos` 扫掠**：`PhysicsBlock/PhysicsBaseCollide.cpp:16-154`（`:23` 只比较两物体当前 `pos`），而窄相明确是扫掠式（`:303`、`:391`、`:442` 从 `OldPos` 发射线）；宽相否决后 `PhysicsWorld.cpp:386-412` 直接 `break` → 高速/小半径物体穿隧漏检（半径 8、单帧位移 20 时可穿过 16 像素厚的墙而完全无接触）。修复：粗筛用 `OldPos ∪ pos` 的包围盒或按位移扩展半径。
- **P2-2 世界坐标当网格坐标用**：`PhysicsBaseCollide.cpp:781`、`:805` 传 `Vec2_` 给只有 `(unsigned int,unsigned int)`/`(glm::uvec2)` 重载的 `BaseGrid::GetCollision`（`BaseGrid.hpp:94/111`）→ 负坐标回绕、小数丢失、无 `centrality` 偏移（对照 `MapStatic.hpp:134` 正确做 `ToInt(start + centrality)`）。修复：改用 `Shape->DropCollision(begin).Collision`（`PhysicsShape.cpp:36-56` 已正确做逆旋转 + 质心偏移 + clamp）。
- **P2-3 Bresenham 起点格命中即判碰撞**：`PhysicsBlock/BaseGrid.cpp:63-71`（先用 `if (block.Collision) return ...` 判起点格），调用方 `PhysicsBlock/PhysicsBaseCollide.cpp:388`、`:442` → 贴地/初始嵌入时每帧无效接触与位置修正（抖动/弹跳）。修复：起点格用包含性判定过滤。
- **P2-4 接触点顺序不稳定 → 热启动错配；`Update` 不拷贝 `friction`**：`PhysicsArbiter.cpp:9-12`（`w_side` 只是数组下标 `contacts[i].w_side = i`，无几何含义）+ `PhysicsBaseArbiter.cpp:30-38`（只拷 `normal/position/separation/w_side/Pn/Pt`，**不拷 `friction`**）→ 两帧间接触点顺序变化即把旧冲量复用到无关接触点（抖动）；摩擦系数每帧被重置为默认 0.2（所有物体同摩擦）。
- **P2-5 每帧跑两遍全量碰撞检测，两份代码已语义分歧**：`PhysicsWorld.cpp:902-1157` 是 `:350-555` 的复制（`:366-367` 有 `!JZ && mass != FLOAT_MAX` 过滤，`:928` 没有）→ 同一场景两条路径产出不同碰撞对；同帧调用 `PhysicsEmulator` + `PhysicsInformationUpdate`（`PhysicsTest.cpp:496`）时检测做两遍并整批 `Update/Delete` 刚建好的裁决器。另 `:1129-1139` 的网格重建块在 `#if ThreadPoolBool` **之外** → 把该宏设为 0 会编译失败。
- **P2-6 `PhysicsCollision.cpp:204` 的 B 方 `OnEnter` 首参传错**：传的是 `Ait->first`（A 对象）而非 `Bit->first`，与 `:197`（A 方）以及 `ProcessCollisions`（`:275-280`）、`RemoveCollisionPair`（`:301-305`）的 `(objA, objB)` 约定不一致 → 回调实现若按首参判断「自己是谁」会拿错对象。
- **P2-7 `PhysicsWorld::GetWorldEnergy` 漏掉 `PhysicsLineS`**：`PhysicsWorld.cpp:1684-1706`（通篇只遍历 Shape/Particle/Circle），而 `PhysicsLine` 继承 `PhysicsAngle`（有 mass/speed/angleSpeed/MomentInertia）且参与重力与求解 → 线条为主场景能量面板漏项；静态体（`mass = FLOAT_MAX`）也未排除。
- **P2-8 `PhysicsJunction` 无累积冲量与钳制**：`PhysicsJunction.hpp:73` 声明 `Vec2_ P` 但从未使用 → 帧内 10 次迭代（`PhysicsWorld.cpp` 求解循环）每次施加近全额冲量 → 过冲嗡振；`spring/rubber` 的 bias 缩放（`PhysicsJunction.hpp:100-102`）与 `ElasticityType 0.005`（`:118-122`）相差 200 倍 → 近刚性且几乎无速度阻尼；`cord` 的 `biasType` 置 0（`:83-88`）在压缩方向仍全额消速（应为单边张力 `P ≥ 0`）。
- **P2-9 line–particle 分支没有 AABB 预筛（缺接口）**：`PhysicsWorld.cpp:519-523` 无 `CollideAABB`，`PhysicsBaseCollide.hpp:16-72` 的七个重载中**没有 `(PhysicsLine*, PhysicsParticle*)`** → 每帧最多 L×P 次池分配 + 距离计算（50 线 × 500 粒子 = 25000 次/帧无效窄相）。
- **P2-10 `GridSearch::Add` 无幂等 + `mGridIndex` 单字段**：`GridSearch.hpp:539-553`（无「已注册」检查）、`PhysicsFormwork.hpp:17`（未注册/网格外共用哨兵 `UINT_MAX`）→ 同一对象多槽位 → `Get` 返回多次 → 同一碰撞对被求解多次（冲量倍增）；`Remove` 只摘一个槽位，其余成幽灵（→ UAF）。修复：注册状态拆出 `mOwner + mSlot`，`Add` 判重。
- **P2-11 风系统：`new[]`/`delete` 混用 + 全项目无消费者**：`PhysicsWorld.cpp:1216-1224`（`new Vec2_[...]`）对 `:126`/`:1218`（`delete GridWind;`）→ UB；且 `GridWind/WindBool/GridWindSize` 除声明、分配、序列化外无任何使用（力的唯一入口 `PhysicsSpeed(time, GravityAcceleration)` 只传重力）→ 死代码却按地图尺寸占内存（256×256 = 512 KB）。
- **P2-12 `PhysicsTrigger` 之外的两处「检测即写状态」与地图越界读**：`MapDynamic.hpp:66-71` 的 `static GridBlock emptyBlock{};` 被所有越界查询共享（一次越界写永久影响所有实例；`FMBresenhamDetection/FMGetCollide` 的越界分支都落到它 → 越界线段被静默判为「无碰撞」）。
- **P2-13 `MapDynamic` 构造期分块扫描边界判定用 `>` 而非 `>=` → 堆越界读**：`MapDynamic.cpp:50-73`（判定在 `:55` `if ((fx > width) || (fy > height))`，而 `DataBool` 只有 `width*height` 个元素）→ `fx == width` / `fy == height` 可通过检查，`:61`/`:73` 索引最大到 `width*height + height`（`MapDynamic(50,30)` → width=3200,height=1920，越界达 1920 字节）。修复：改 `>=`。
- **P2-14 `MovePlate` 接口 UB 与未初始化**：`MovePlate.h:108-116`（`constexpr` 构造函数内 `new char[]`、板块数组不初始化 → `GetPlate`/`ExcursionGetPlate` 在首次 `ALLUpData` 前返回野指针）、`:196-201`（`unsigned int xx = (x / mEdge) - mPosX + mOriginX;` 负 `double` → `unsigned` 是 UB，且与 `ExcursionGetPlate` 的 floor 语义在负坐标差一格）、`:263-274`（`ALLUpData` 对从未生成过的板块也调 `mDeleteCallback`，回调默认 `nullptr` → 空指针调用必崩）。修复：去 `constexpr`、构造值初始化 + 回调判空、负坐标先判。
- **P2-15 `MovePlate::UpData` 两个分支返回值语义不一致**：`MovePlate.h:236-259`（大位移分支走 `ALLUpData` 全图重建，仍返回整图位移 `Info.X/Y`）；调用方 `GameMods/UnlimitednessMapMods/UnlimitednessMapMods.cpp:285-288` 当单帧段位移用 → 传送/重置/镜头跳转后迷雾与缓存巨量错位。修复：`MovePlateInfo` 增加 `bool Rebuild` 或该分支清零 X/Y。
- **P2-16 序列化与 union 叠放不一致 + 反序列化不校验**：`BaseGrid.cpp:186-200` 只存 `Healthpoint/mass`（与 `DistanceField/DirectionField` 共用匿名 union，`BaseStruct.hpp:45-57`）→ 存档往返确定性静默损坏；`:207-218` 中 `if (NewBool)` 为假时 `Grid` 未初始化仍被写；`width/height` 与 `data["Grid"].size()` 未校验。

---

## 4. P3 轻微 / 维护性 / 加固点

- **P3-1 `new[]`/`delete` 不匹配共 4 处**（MSVC 下通常「能跑」，但属 UB，ASan/`_CRTDBG` 会报）：`MapDynamic.cpp:26` ↔ `:118`、`MapDynamic.cpp:27` ↔ `:119`、`MapDynamic.cpp:32` ↔ `:107`、`MovePlate.h:114-115` ↔ `:124-125`。另 `MapDynamic` 析构 `:114-117` 手工调用 `BaseGridBuffer[i].~BaseGrid()`。
- **P3-2 轮廓容量按 3 点/格分配但单格最多写 4 点**：`BaseOutline.cpp:16`（`Width*Height*3`）vs `:121-155`（`OutlineUnit` 四条独立判定）、`:164-212`（`LightweightOutlineUnit`）、`:214-255`（`MinOutlineUnit`）——**已证明写入数恒不超过容量**（`Σ ≤ 2C + W·H ≤ 3·W·H`，推导见 §6 误报澄清 M-3），故不是缺陷；仍建议改 4/格并加写入前硬保护，以免后续改动破坏该不变量。
- **P3-3 `ToInt` 对负整数多减 1**：`BaseCalculate.hpp:21-24`（double）、`:32-35`（float）`return val >= 0 ? static_cast<int>(val) : (static_cast<int>(val) - 1);` —— `static_cast` 已向零截断（对负数即 ceil），再减 1 越过 floor：`ToInt(-2.0) = -3`、`ToInt(-1.0) = -2`（只有负数且带小数部分才正确）；`ToInt(vec2)` 逐分量继承。调用面 `BaseGrid.cpp:106`、`MapDynamic.cpp:167` 等 → 负坐标区整体偏一格。修复：用 `std::floor`。
- **P3-4 `MaxOutlineCentre` 每次调用两次 `new[]` + O(row_count²) 枚举**：`BaseOutline.cpp:264-344`（`:279-280`、`:337-338`），被每个轮廓点调用（调用点 `:127/135/143/151/172/180/188/197/207/222/231/240/250`）→ Θ(周长 × 面积) 级堆操作，每帧发生（`PhysicsShape::UpdateAll`）。
- **P3-5 `GridBlock`/`CollisionInfoD` 未初始化位域**：`BaseStruct.hpp:45-69`（匿名 union 位域）与 `:94-109`；`BaseGrid.cpp:104` 的 `CollisionInfoD Collisioninfo{false};` 只初始化 union 首字节，`:132` 才赋 `Direction`，零长射线时读取 indeterminate 值（`PhysicsBaseCollide.cpp:183/214/348` 按随机值挑轴）。修复：加默认成员初始化器 / 显式构造函数。
- **P3-6 `BaseSerialization` 条件编译改变继承列表与虚函数声明 + 基类析构非虚**：`BaseSerialization.hpp:17/22-32/45-50/61-87`、`:72`（`~BaseSerialization() {};`）→ 宏翻转或跨 TU 不一致即 ODR/ABI 违规；`[推测]` 仓库内未发现经 `BaseSerialization*` 删除的路径。修复：宏只控制实现体，析构改 `virtual`。
- **P3-7 `BaseDefine.h:51` 注释与 `:57` 取值相反（注释已过时）**：本机 `C:\VulkanSDK\1.4.341.1` 存在、根 `CMakeLists.txt:139-140/198/207` 已接入 Vulkan，故 `#define PhysicsGPUBool 1` **本机可编译**；风险是无 SDK 的机器/CI 直接编译失败，而注释不能当守卫。修复：由 CMake 注入 `PhysicsGPUBool=0/1`，头文件用 `#ifndef` 兜底。
- **P3-8 死代码簇**（当前无调用点，启用即触发）：`BaseCalculate.cpp:17-38` `q_sqrt`（0x5f3759df 实为 `1/sqrt`，且 `reinterpret_cast` 严格别名 UB）、`:272-293` `SquareToRadial`（`PYpos /= fabs(cos(angle))` 除零）、`:390-401` `Morton2D`（`uint_fast16_t` 在 MSVC 为 32 位，>0xFFFF 位平面污染）、`:410-414` `DropUptoLineShortes`（返回交点到原点距离而非点线距离）、`:424-436` `DropLineShortesIntersect`（`start == end` 时 `0/0 = NaN`）；`EnergyConservation.hpp:21/85`（头文件内非 inline 非模板 → 多 TU 重复定义，全仓无调用点，故**不存在**「双重施加能量/永动」）。
- **P3-9 `PhysicsLiquid` 的性能与维护点**：`PhysicsLiquid.cpp:576-586`（`mHashFirst` 每帧 `assign(kHashSize, -1)`，37 万项 ≈ 1.48 MB/帧）、`:518-524`（四个工作数组按 `<` 判据只增不减：N = 10128 时 `mNeighbors` 2.6 MB、`mPairDir/mPairW` 约 5.2 MB）、`:1288-1341`（逐刚体就地改位姿 → 同帧结果依赖 `mSolids` 注册顺序）、`:157-183`（`AddGrid` 就地改写**共享** `param`，渲染端 `ColorByDensity` 读同一份；`mass` 只写粒子字段而求解全当 1；第二次 `AddGrid` 沿用旧参数 → 两团水密度失配）、`:30`（`kPi = 3.141592`）、`:832-835`（每子步重算 `SolverConst` 各系数，而 `PhysicsLiquid.hpp:219-232` 的 `SolverConst` 与 `:239-249` 的 `SolidLayout` 在 cpp 中**零引用** → 调参陷阱）。
- **P3-10 `PhysicsAuxiliaryVision` 缺判空 + 调试渲染性能**：`:267-282`（`DrawJoints/DrawJunctions` 直接 `i->body1->pos`）、`:414-445`（`DrawShape/DrawCircle/DrawLine`）均无判空，且 `DrawJoints/DrawJunctions` 在 `if (showAuxiliary)` 之外仍每帧调用（`PhysicsTest.cpp:448-449` vs `:501`）；`:347-350` 的网格遍历对 `MapDynamic(50,50)` 每帧约 3200×3200 ≈ 1.02e7 次虚调用（该处坐标换算本身**正确**）。
- **P3-11 `ImGuiPhysics` 的输入与路径问题**：`:181/191/193`（三个 `DragScalar` 传 `p_min = p_max = NULL` → 质量/转动惯量/碰撞半径可拖到 0 或负，直接触发 P0-6）、`:226-242`（`static glm::ivec2 GridPos{0}` 跨物体共享 + `Object->at(GridPos)`（`BaseGrid.hpp:83` 无检查）→ 64×64 选 (32,32) 后切 8×8 形状索引 288 ≥ 64 = 堆越界读写）、`:34/53/67`（相对路径 `PhysicsBlock.ini`，`CMakeLists.txt:241` 只复制到 target 目录 → 工作目录不同则静默回落默认值并另写新 INI）、`ImGuiPhysics.hpp:22-28`（按 INI 读入的 vector 长度写 4 元素颜色数组 → 分量 > 4 即越界写相邻全局）。
- **P3-12 其他 API/代码卫生**：`PhysicsFormwork.hpp:19-20`（`~PhysicsFormwork(){}` 非虚 + 多继承，经基类指针 delete 属 UB）、`PhysicsShape` 隐式拷贝共享 `BaseOutline` 三个裸指针（双 `delete[]`，`[推测]` 无触发点）、`PhysicsParticle.cpp:96/118` 在定义处标 `inline`（头文件声明为虚函数）、`PhysicsAngle.hpp:155/162` 缺 `override`、`PhysicsKinematic.cpp:316-325`（`GetData` 返回函数内 `static KinematicData` 的可变引用）、`GridSearch.hpp:122/137-140`（裸 `new[]`/`delete[]` 无拷贝控制，被按值拷贝会双删，`[推测]` 当前仅由 `PhysicsWorld.hpp:232` 持有）、`GridSearch.hpp:744-752`（`GetDividedVision` 同时改 `i` 与 `_storey` → 多画一个「根层之上」的假层，仅调试可视化）、`MapStatic.cpp:17`（`SetCentrality` 用 `>` 允许等于 `width/height`）、`PhysicsWorld.cpp:1232-1240`（`UpdateGridPosition` 的阈值基准 `mGridCenter` 被立即改写 → 阈值失真）、`PhysicsWorld.hpp:354-367`（`ApplyImpulseSize` 只在 `AddObject` 时重算，`RemoveObject`（`:1610-1611`）只递减 `ObjectSize` 不重算 → 批量删除后迭代数偏高不振；`ObjectSize/5` 为整数除法）、`PhysicsWorld.cpp:878-879`（`mCollideOutputs.clear(); resize(xThreadNum);` 每帧释放重建 16 个 vector）、`PhysicsWorld.cpp:100-176`（析构只回收 `CollideGroupS` 里的裁决器，`mCollideOutputs[*].newGroup/DeleteCollideGroup` 中未 Resolve 的裁决器从不回收）。

---

## 5. 修复路线图（建议顺序）

| 阶段 | 内容 | 理由 |
|---|---|---|
| **第 1 步（止血，改动小）** | `1.0f/0.0f` 守卫（P0-6）、`BaseOutline` 补分配（P0-5）、`delete` → `delete[]`（P3-1、P2-11）、`contacts` 上限（P0-10）、除零保护（P0-11、P1-11）、Bresenham 出口（P0-7、P0-8） | 每处 1–5 行，消除确定性 UB/NaN/死循环 |
| **第 2 步（并发模型）** | 在 `RemoveObject`/`AddObject`/`SetMapFormwork` 加屏障 + 清理 `mCollideOutputs`；`SetMapFormwork` 清地形裁决器；补 `RemoveCollisionPair`（P0-1～P0-3、P2-6）；`SetMapRange` 重置槽位（P0-4）；求解改按物体分片（P0-9）；检测阶段去写状态（P1-1） | 这是「偶发崩溃/物体消失」的主根因；建议同时把 `ThreadPoolBool` 的语义写进注释 |
| **第 3 步（物理正确性）** | `len²` 量纲（P1-13）、`separation` 语义（P1-14）、扫掠 AABB（P2-1）、静止判定统一（P1-16）、torque 门控（P1-15）、液体 NaN/边界位移/速度注入（P1-4～P1-6） | 影响手感与稳定性的系统性错误 |
| **第 4 步（生命周期与状态机）** | kinematic 数据回收（P1-17）、触发器 Exit 与层级（P1-18）、装配体序列化（P1-19）、序列化 union（P2-16） | 状态残留类缺陷，影响长时运行 |
| **第 5 步（GPU 与性能）** | staging 内存属性/屏障（P1-3）、`MaxOutlineCentre` 缓冲复用（P3-4）、检测重复实现合并（P2-5）、液体数组复用（P3-9） | 性能与 GPU 正确性 |

**回归测试建议**：① 开 ASan/`_CRTDBG` 跑一帧加载存档（应报 P0-5）；② 在 `PhysicsEmulator` 之后立刻 `RemoveObject` 一个正在碰撞的对象，重复千帧（应报 P0-1/P0-2）；③ 运行中换图（应报 P0-3）；④ 用带小数/负坐标与轴对齐自由落体构造射线（应报 P0-7/P0-8）。

---

## 6. 误报澄清（这些**不要**改，改了更糟）

- **M-1 既有 `OPTIMIZATION_ANALYSIS.md` 的「BUG-9：切向冲量符号不一致（`PhysicsBaseArbiter.cpp:442` vs `:98`）」为误报。** 父代理逐行证明：`PreStep` 用 `tangent = Cross(normal, 1.0)`（`:433`）配 `P = Pn*normal - Pt*tangent`（`:442`），`ApplyImpulse` 用 `tangent = Cross(normal, -1.0)`（`:485`）配 `Pt = dPt*tangent`、`-= invMass*Pt`（`:498-500`）；因 `T⁻ = -T⁺`，`ApplyImpulse` 的切向增量 = `-invM*dPt*T⁻` = `+invM*dPt*T⁺`，与 `PreStep` 暖启动的切向分量 `+invM*Pt*T⁺` **同号同向** → 两处自洽。摩擦耗散方向也已验证正确（`vt_new = vt*(1 - invM*massTangent)`）。**该条应从既有文档的 BUG 清单中撤销。**
- **M-2 「Shape/Circle/Line 与地形的接触法线整体反向（物体被吸进地形）」为误报。** 父代理取证：`BaseGrid::BresenhamDetection` 的 `Direction` 枚举（`PhysicsBlockTypes.hpp:146`：0=Right、1=Up、2=Left、3=Down）经 `vec2angle({-1,0}, k*90°)` 映射为 `(-1,0)/(0,-1)/(1,0)/(0,1)`，即**法线恒等于射线行进方向**；而射线起点恒为物体旧位置、指向障碍内部 → 法线就是 A→B，与求解器约定（`PhysicsBaseArbiter.cpp:95/98/102/108`，标准 Box2D 式 `bias` 与 `±invMass*P`）一致。`PhysicsBaseCollide.cpp:318/405/456` 三处同一表达式语义一致，`:360` 的第二轮取负也自洽（该轮射线由形状外侧射向 `Drop`）。**残留的次要问题**：法线取「行进方向」只在正面入射时等于真实面法线，擦地/斜向滑入时会偏（非符号错误，可另立改进项）。
- **M-3 「轮廓数组按 3 点/格分配、单格可写 4 点 → 堆越界」为误报（已给出不超过容量的证明）。** 记 `C` = `Collision` 格数、`E` = 空格数、`F` = 一次调用中写满 4 点的格数，则总写点数 `Σ ≤ 4F + 3(C - F) = 3C + F`。三种 `*Unit` 中「写满 4 点」都要求若干个**特定偏移**的邻居为空：
  - `OutlineUnit`（`BaseOutline.cpp:121-155`）：7 个被检邻居 `(x-1,y)`、`(x,y-1)`、`(x-1,y-1)`、`(x,y+1)`、`(x+1,y-1)`、`(x+1,y)`、`(x+1,y+1)` 全空即四条判定全成立。**注意 `(x-1,y+1)` 不在被检之列**，所以「非孤立格」也可能写满 4 点——这正是该条容易被误判为越界的原因（父代理第一手核对 `BaseOutline.cpp:164-212` 的 `LightweightOutlineUnit` 后确认了同类结构）。
  - `MinOutlineUnit`（`:214-255`）：四角各要求其三邻域全空 ⇒ 8 邻域全空。
  - `LightweightOutlineUnit`（`:164-212`）：`TR` 要求 `(x+1,y)`、`(x,y-1)`、`(x+1,y-1)` 为空，`BR` 要求 `(x+1,y+1)`、`(x+1,y)`、`(x,y+1)` 为空，共 5 个必备空格。
  这些偏移互不相同，故一个空格最多被 5～8 个格当作「必备空格」；对必备数为 `k` 的变体有 `k·F ≤ k·E` ⇒ `F ≤ E`（`MinOutlineUnit` 更强：`F ≤ E/8`）。于是 **`Σ ≤ 3C + F ≤ 3C + E = 2C + W·H ≤ 3·W·H`**，即写入数恒不超过容量（`W·H ≤ 1` 时容量被 `if (size < 4) size = 4;` 兜到 4 ≥ 3）。**仍建议加固**（容量改 4/格 + 每次写入前 `if (OutlineSize >= capacity) return;`），因为这依赖「单格至多 4 点」这一隐含不变量，后续加角点就会破坏它。
- **M-4 「A/D 类 `PreStep` 早退未初始化 `massNormal`/`bias`」为伪阳性。** `PhysicsBaseArbiter.cpp:409-410` 的 `if (object1->invMass == 0) return;` 与 `ApplyImpulse` 的 `:456-457` 是同一守卫；`invMass` 恢复非零后的首帧 `PreStep` 会完整重算 `massNormal/massTangent/bias` 才轮到 `ApplyImpulse`，不存在「垃圾值参与求解」路径（属脆弱设计，可加断言）。
- **M-5 `PhysicsCollision` 的 stay 列表 swap-remove 不会重复插入。** `AddCollisionPair` 仅在 `PhysicsWorld.cpp:254/259`（线程池分支 `:299/304`）的 `CollideGroupS.emplace(...).second == true` 时调用，每个 arbiter 键唯一，故 `PhysicsCollision.cpp:308-309`、`:323-324` 的尾部覆盖 + `pop_back` 是安全的。
- **M-6 已核对为正确的实现（勿重复排查）**：`ArbiterKey` 构造内部已排序（`BaseArbiter.hpp:52-64`），混用 `(line,particle)`/`(particle,line)` 不影响键一致性；`ArbiterKey::PoolID` 不参与比较/哈希但有明确用途（`PhysicsWorld.hpp:101`）；主循环用 `<=` 比较无关指针去重是安全的（键已归一化，插入/删除对称）；`GridSearch::atIndex` 会把对象放进完全包住其 AABB 的格（无「大物体被 `Get` 漏检」）；`PhysicsLine` 的 `I = radius²m/12`、`pos` = 中点、`radius = L/2`、`angle` 指向 `begin` 四者自洽；`PhysicsCircle` 的 `I = 0.5mr²`；`AddForce` 的力臂与 `torque += Arm.x*Force.y - Arm.y*Force.x` 符号正确且力不重复累加；`Cross(ω, r) = (-ω*r.y, ω*r.x)` 正确；`ModulusLength` 返回 `|v|²`（`GetWorldEnergy` 的 `½mv² + ½Iω²` 公式本身正确，只是漏了 `PhysicsLineS`）；`MinSpoilageFactor` 已按秒归一（`0.9994^60 ≈ 0.9646/秒`，`speed` 侧的帧率依赖**已修好**，但 `MovementUtil.hpp:35-36` 仍用裸 `0.9994`/帧，与 `PhysicsSpeed` 不一致）；12 种裁决组合（S/P/C/L 地形 + SS/SP/CS/CC/CP/LC/LS/LP）全部可达无缺失；`PhysicsParticle` 的默认成员初始化器齐全（`PhysicsParticle.hpp:44-99`），故「构造函数未设 `mass/invMass`」不是缺陷；线程池任务数组在每阶段后都 `clear()`（`PhysicsWorld.cpp:343/606/669/737/812/865/913/1119/1138/1170`），无泄漏；`PhysicsGPU` 路径不是死代码（`GameMods/PhysicsTest` 参与构建，根 `CMakeLists.txt:155`，UI 可 `SetUseGPUApplyImpulse(true)`）。

---

## 7. 与既有文档的差异与更正

| 既有文档 | 状态 | 更正 |
|---|---|---|
| `OPTIMIZATION_ANALYSIS.md` §12 BUG-9（切向冲量符号） | **误报** | 见 M-1，应撤销 |
| `OPTIMIZATION_ANALYSIS.md` §4.1 / §12 的行号 | **已漂移** | 「PreStep 多线程被 `& 0` 禁用」现位于 `PhysicsWorld.cpp:637`（文档写 `:548`）；引用前需按符号重定位 |
| `OPTIMIZATION_ANALYSIS.md` 关于 PreStep 并行的定性 | 仍成立 | `#if (Definite != 1) & ThreadPoolBool & 0` 使整段并行块成为死代码（且 `&` 被当作 `&&` 用） |
| `PhysicsBlock.md:242`「StaticNum ≥ 200 视为静止」 | **过期** | 代码里休眠跳对用硬编码 `> 10`，`PhysicsSleepThreshold 10`（`BaseDefine.h:38`）全仓零引用；200 只是 `StaticNum` 的累加上限（`PhysicsParticle.cpp:126`） |
| `PhysicsBlock.md` / `README.md` 的文件索引 | **不全** | 未收录 `PhysicsCollision.hpp/.cpp` |
| `PhysicsLiquid液体模拟冗余分析.md` | 行号整体偏移 | 自称 hpp 335 行 / cpp 1330 行，实际 hpp 345 / cpp 1568（越靠后偏移越大，200–238 行）；其点名的 R5（λ/Δp 两遍重算梯度）、R6（`mPairW` 哨兵双重写）、R8（`ParallelRange` 每次分配 function+future，含 `count < 1024` 串行门）、R9（`FindNeighbors` 两遍重复 floor/hash）**确未做**；R11「`mPrevPos[i] = p->pos` 死存储」**结论过强**（`mPrevPos` 是 `EnforceCFL` 的唯一输入，不能删）；R13（circle 采样空转）`[推测]` 未能仅凭代码判定；R1（`SolidLayout` 半落地）、R14（`MarkInside` 帧内位置陈旧）属实 |
| `PhysicsLiquid贴底锁死分析.md` | 段落定位准确、区间尾部偏大 4–13 行 | 实际：`TerrainSignedDist2` L1148-1176、`TerrainFree` L1186-1200、`FindTerrainEscape` L1209-1239、`EnforceSolidGap` L1260-1342、`FlushSolidGap` L1349-1372、子步调用 L1512-1536、帧末 L1538；「方案 1 已落地」属实但引入 P1-6 的能量注入；§六「只解除几何阻塞、不创造力」与代码一致 |
| 两份液体文档均未覆盖 | **新发现** | 本次 P1-4（NaN 无防护）、P1-5（`mBoundaryDisp` 陈旧）、P1-7（`invMass == 0` 当质量 1）、P1-8（邻居截断） |
| `BaseDefine.h:51-52` 注释「本仓库不含 VulkanSDK / VulkanTool，必须为 0」 | **过时** | `E:\Physics\PixelClean-main\Vulkan`、`VulkanTool` 目录存在，5 个头文件齐全，`VULKAN_SDK=C:\VulkanSDK\1.4.341.1`，根 `CMakeLists.txt:139-140/198/207` 已接入；`PhysicsGPUBool 1` 本机可编译 |

---

## 8. 附录：审计范围与方法

- **基础层**：`BaseDefine.h`、`BaseStruct.hpp`、`BaseCalculate.hpp/.cpp`、`BaseSerialization.hpp`、`BaseGrid.hpp/.cpp`、`BaseOutline.hpp/.cpp`
- **地图与空间索引**：`MapFormwork.hpp/.cpp`、`MapStatic.hpp/.cpp`、`MapDynamic.hpp/.cpp`、`MovePlate.h`、`GridSearch.hpp/.cpp`
- **物理对象**：`PhysicsFormwork.hpp`、`PhysicsBlockTypes.hpp`、`PhysicsParticle.hpp/.cpp`、`PhysicsAngle.hpp/.cpp`、`PhysicsShape.hpp/.cpp`、`PhysicsCircle.hpp`、`PhysicsLine.hpp`
- **碰撞检测**：`PhysicsBaseCollide.hpp/.cpp`、`PhysicsCollision.hpp/.cpp`
- **裁决器与约束**：`BaseArbiter.hpp`、`PhysicsArbiter.hpp/.cpp`、`PhysicsBaseArbiter.hpp/.cpp`、`PhysicsJoint.hpp/.cpp`、`PhysicsJunction.hpp/.cpp`、`EnergyConservation.hpp`
- **主循环**：`PhysicsWorld.hpp/.cpp`
- **液体**：`PhysicsLiquid.hpp/.cpp`（对照两份既有分析文档）
- **GPU/工具层**：`PhysicsGPU.hpp/.cpp`、`ImGuiPhysics.hpp/.cpp`、`PhysicsAuxiliaryVision.hpp/.cpp`、`PhysicsAssembly.hpp/.cpp`、`PhysicsKinematic.hpp/.cpp`、`PhysicsTrigger.hpp/.cpp`
- **方法**：分模块只读审计 + 跨模块交叉验证；所有 P0/P1 条目由父代理逐条回读源码复核；对「法线反向」「切向符号」「轮廓越界」「PreStep 早退」四条既有/子代理结论做了**反向取证并判定为误报**（§6）。带 `[推测]` 的条目为静态推断。
