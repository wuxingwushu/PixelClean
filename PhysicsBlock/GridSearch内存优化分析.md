# PhysicsBlock 物理引擎分析 & GridSearch 内存优化方案

> 分析对象：`PhysicsBlock/`（基于像素网格的 2D 刚体物理引擎）
> 问题焦点：`PhysicsBlock/GridSearch.hpp` 中四叉网格搜索的**内存占用过大**问题
> 文档内容：① 引擎整体架构分析 ② GridSearch 深度剖析（含索引数学与隐藏问题）③ 内存根源定位与量化 ④ 五套修复方案（含优缺点/伪代码）⑤ 方案对比与推荐路线

---

## 一、PhysicsBlock 引擎整体分析

### 1.1 引擎定位与特性

基于像素网格的 2D 刚体物理引擎，支持多种物理对象（网格形状、粒子、圆、线段）、关节约束、绳索连接、静态/动态地图碰撞检测，以及基于四叉树的网格搜索加速。全库以头文件 + 少量 `.cpp` 的形态存在，核心结构如下：

```
BaseSerialization ─┬─ BaseGrid ── BaseOutline
                   │       │
                   │       └─── MapStatic / MapDynamic —— MapFormwork (接口)
                   └──────────────────────────┘
PhysicsFormwork (接口) ── PhysicsParticle ── PhysicsAngle ──┬─ PhysicsCircle
                                                           ├─ PhysicsLine
                                                           └─ PhysicsShape (多继承 BaseOutline)

BaseArbiter ── PhysicsBaseArbiterAA/AD/A/D ──> 12 种具体 Arbiter (SS/SP/S/P/C/CS/CP/CC/LC/LS/LP/L)
BaseJunction ── PhysicsJunctionSS/S/P/PP  (绳/弹簧/橡皮筋/杠杆)
PhysicsWorld ── 持有 地图指针 + 5 种对象容器 + GridSearch + 碰撞对哈希 + 12 个 MemoryPool + 线程池
```

### 1.2 仿真主流程（`PhysicsWorld::PhysicsEmulator`）

```
1. 累积外力（重力 + 风/网格风）
2. PhysicsSpeed()   —— v += t·(g + F/m)
3. PhysicsPos()     —— p += t·v
4. GridSearch::UpData()        —— 更新四叉网格（对象移动后归位）
5. Broad Phase（AABB）         —— 基于 GridSearch::Get 粗筛候选对
6. Narrow Phase（Collide）     —— 精确碰撞检测（12 种组合）
7. Arbiter 创建/更新/删除      —— 无锁 per-thread 输出缓冲 + 碰撞对哈希表
8. PreStep / ApplyImpulse ×N   —— 冲量迭代求解（线程池 + 逐线程 MemoryPool）
9. Joint / Junction 求解
```

### 1.3 引擎已具备的优化手段（作为背景）

| 优化点 | 位置 | 说明 |
|---|---|---|
| 莫顿码（Z-order） | `BaseCalculate.hpp` / `GridSearch.hpp` | 二维 → 一维位交错，提高缓存局部性 |
| 四叉网格分层 | `GridSearch.hpp` | 按碰撞半径将对象放入不同粗细层 |
| swap-and-pop 删除 | `GridSearch`、`CollideGroupVector` | 移除元素 O(1)，避免 erase 搬移 |
| 静止对象跳过 | `PhysicsWorld.cpp` | `StaticNum > 10` 时跳过碰撞遍历 |
| 无锁 MemoryPool | `PhysicsWorld.hpp` | 每线程独占 Arbiter 池，避免帧内堆分配 |
| 线程池 | `ThreadPool.h` | 碰撞检测/网格更新并行化 |
| 内联函数 / 头文件实现 | 全库 | 减少调用开销，便于 LTO |

**引擎整体内存管理是清醒的**（Arbiter 池、无锁缓冲、swap-pop），唯独 `GridSearch` 的网格容器是"稠密预分配 vector 套 vector"，与引擎其他部分的严谨程度形成反差 —— 这正是本次问题所在。

---

## 二、GridSearch 深度剖析

### 2.1 数据结构

```cpp
std::vector<std::vector<PhysicsFormwork*>> Grid;   // 主网格：一维展开的四叉树，每格一个 vector
std::vector<PhysicsFormwork*> GridExtrovert;       // 网格外对象（超大对象）
std::vector<PhysicsFormwork*> mAllObjects;         // 所有对象扁平列表（UpData 遍历用）
UpDataVector* UpDataPtr;                           // 多线程 UpData 的每线程临时缓冲
unsigned int* ExcursionVector;                     // 预计算每层偏移（CurerExcursionVector=1）
int mStorey; unsigned int mGridDim; Vec2_ mExcursion; unsigned int mThreadCount;
```

每个对象通过 `PhysicsFormwork::mGridIndex`（无符号 int，`UINT_MAX` 表示在网格外）记住自己所在格的索引。

### 2.2 索引数学（必须精确理解）

`SetMapRange(Size)`：`Size *= 4` → `mGridDim = 4·Size`，`mExcursion = Size/2`（世界坐标平移，使 `[-2·Size, 2·Size]` 映射到 `[0, 4·Size]`）；同时 `mStorey = log2(4·Size)`；格总数：

```
GridSize = (4^(mStorey+1) - 1) / 3
         ≈ 4/3 · (4·Size)² = 5.33 · Size²
```

- **层偏移**：`Excursion(storey) = (4^(mStorey-storey+1) - 1) / 3`
- **层内索引**：莫顿模式 `at(x,y,off,s) = off + Morton2D(x,y)`；线性模式 `= off + x·2^(mStorey+1-s) + y`（当前 `Morton_define=1`，线性路径不参与运算）
- **层分级**：`Storey(R)` 用 `_BitScanReverse` 取 `ceil(2R)+1` 的最高位 +1 —— 对象越大层号越大（层越粗）；层 `s` 的格子边长 = `2^s` 世界单位，每维格子数 = `2^(mStorey-s)`
- **越界处理**：`atIndex` 先 clamp 到 `[0, mGridDim-1]`，再判断对象是否跨格子边界，跨则 `++_storey` 升到上一层；仍不合法（索引 ≥ Grid.size()）则入 `GridExtrovert`

### 2.3 数据流

| 方法 | 行为 | 调用时机 |
|---|---|---|
| `Add` | 算格 → push_back → 记 `mGridIndex` | 对象创建 |
| `Remove` | mAllObjects 线性扫描 swap-pop；格内 & Extrovert 扫描 swap-pop | 对象销毁 |
| `UpData` | 遍历 `mAllObjects`，逐对象重算格号，跨格则搬移 | 每帧 1 次（主线程） |
| `UpDaraWorkeTask + End` | 并行算新格号入 per-thread 缓冲 → 主线程统一搬移 | 每帧（线程池） |
| `Get(pos,R,Out)` / `Get(S,E,Out)` | 把范围转成网格坐标，从最底层向上一层遍历，`insert` 合并到 Out | **每帧对每个对象调用（碰撞粗筛，worker 线程并发）** |
| `GetDividedVision` | 纯几何计算（返回视觉矩形），不访问 Grid | 调试 UI |
| `MoveGridTo / UpdateGridOffset` | 更新偏移；前者触发整树 UpData | 世界平移 |

**关键约束**：`Get` 会在多个 worker 线程**并发只读**调用，因此任何改造都必须保证"更新后、查询前"的同步语义（现状：UpData/End 在 main 完成，之后 workers 才开跑，天然满足）。

### 2.4 隐藏问题（分析中发现的附带缺陷）

1. **`atIndex` 越界读（内存安全 bug）**：当对象大到 `_storey > mStorey`（例如 `Size=256` 网格中 `R ≥ 512` 的对象，或大对象贴边触发的 `++_storey` 升级），`ExcursionVector[_storey]` 会**读出数组末尾之外的内存**，随后 `index < Grid.size()` 检查的是垃圾值 —— 可能把对象插进错误格（数据破坏），或恰好越界才进 Extrovert。应 clamp 到 `mStorey` 并直接判为网格外对象。
2. **`Excursion` 偏移公式导致 75% 结构性浪费**：层 `s` 的偏移把"自己这一层"也计入了前缀和（应为"所有更细层 + 根"之和），因此每层区域之间留下巨大空洞。以 `Size=256` 为例，`GridSize=1,398,101`，但任何合法查询只能寻址到约 `(4^10-1)/3 + 1 = 349,526` 个格子 —— **75% 的格子从结构上不可达**。修正公式可使同等寻址能力下数组长度缩到 1/4，或把最细层（边长 1）真正用起来。
3. **`SetMapRange` 不释放旧内存**：`Grid.clear(); Grid.resize(GridSize)` —— `clear()` 不清 capacity，`resize` 会复用旧容量；**换成小地图时内存不释放**。需要 `std::vector<...>().swap(Grid)` 或 `shrink_to_fit`。
4. **`Remove` 的 O(n) 扫描**：mAllObjects 线性查找（销毁对象多时成本明显），且与 `UpDaraWorkeTask` 的 per-thread `FormworkIndex` 无 `reserve`，每帧 realloc 抖动。
5. **`Morton2D` 用 `uint_fast16_t`**：坐标截断到 16 位，`mGridDim ≥ 2^16`（`Size ≥ 16384`）时索引冲突；工程上通常不会触发，但缺少断言保护。
6. `Get` 中 `P = Grid.size(); P >>= 2;` 逐层当偏移用，依赖 `GridSize = 4·Excursion(i)+1` 的整除巧合，逻辑脆弱且难维护（与 `ExcursionVector` 两份实现易漂移）。

---

## 三、内存问题根源分析（核心）

### 3.1 内存模型

当前每个格子是：

```
std::vector<PhysicsFormwork*>
├─ 固定开销：24 B（x64：begin/end/cap 三个指针；x86 为 12 B）
└─ 一旦被写入：独立堆块 = malloc头(~16 B，对齐粒度) + 容量×8 B
```

### 3.2 量化（x64，空载 = 仅 Grid 容器本身）

| `SetMapRange(Size)` | mGridDim=4·Size | mStorey | GridSize=格数 | vector 开销 (×24B) | 实际可用格(25%) |
|---|---|---|---|---|---|
| 64 | 256 | 8 | 87,381 | **2.1 MB** | 21,846 |
| 128 | 512 | 9 | 349,525 | **8.4 MB** | 87,382 |
| 256 | 1024 | 10 | 1,398,101 | **33.6 MB** | 349,526 |
| 512 | 2048 | 11 | 5,592,405 | **134 MB** | 1,398,102 |
| **1024** | 4096 | 12 | **22,369,621** | **537 MB** | 5,592,406 |
| 2048 | 8192 | 13 | 89,478,485 | **2.1 GB** | 22,369,622 |

而 `PhysicsWorld::SetMapFormwork` 里：

```cpp
GridWindSize = MapFormwork_->FMGetMapSize();                    // 地图尺寸（格子）
mGridSearch.SetMapRange(std::max(GridWindSize.x, GridWindSize.y)); // 直接 = 地图尺寸！
```

**地图多大，网格就有多大，且内存是地图尺寸的平方关系**：1024 的地图 → 537 MB 空载内存，这还不含任何对象。

### 3.3 问题成因排序

1. **根治项：稠密预分配 × 每格 24 B 常数** —— `O(4^mStorey · 24B)`，是内存的主体，且与对象数量无关（对象可能只有几百个）。
2. **结构性浪费：偏移公式错位** —— 75% 的格子永远不可达（见 2.4-2）。
3. **每格独立堆分配** —— 每个被占用格一个 malloc 块（16 B 头 + 容量 8·k），对象移动后旧格容量永久保留 → **高水位内存永不回落 + 堆碎片化**；`UpData` 每帧在格子间搬移还会反复触发分配/释放。
4. **重建成本** —— `SetMapRange` / `MoveGridTo` 触发 22M 个 vector 的构造/析构，峰值瞬时内存与耗时都高。
5. **次要**：`UpDaraWorkeTask` 无 reserve 的每帧 realloc；`Remove` O(n) 扫描。

> 结论：正确的修复方向是 **"把内存从 ∝ 格子数 改为 ∝ 对象数/占用格数"**，或至少在稠密前提下把每格常数从 24 B 压到 4~8 B 并消灭逐格堆分配。

---

## 四、修复方案

### 方案 A：最小改动止血（限制层数 + 释放策略 + 修复越界）

**做法**
- A1：`SetMapRange` 中给 `mStorey` 设上限（如 `min(mStorey, 8)` 或按对象数量自适应），最细层变粗（格边长 2^k），`GridSize` 直接除以 4^k。`Size=1024` 限 8 层 → 87,381 格 → **2.1 MB**（-99.6%）。
- A2：`Grid.clear()` 改为 `std::vector<...>().swap(Grid)`（或 `resize` 前 `shrink_to_fit`），修掉换小地图不释放内存的问题。
- A3：修复 `atIndex` 越界读：`_storey > mStorey` 时 clamp 到 `mStorey` 并走 Extrovert。
- A4：`UpDaraWorkeTask` 对 `FormworkIndex` `reserve(BlockSize)`。

**优点**
- 改动 10 行以内，零 API 变化，行为语义不变（只是每格候选更多）。
- 立竿见影：内存降到可接受水平，风险极低，可当天上线。

**缺点**
- 治标不治本：没有消灭 24 B/格常数与"每格一个堆块"；未来尺寸需求再涨依旧爆。
- 粗格子 → `Get` 返回候选对象增多（Broad Phase 过滤更多假阳性），对象密度高时碰撞检测耗时上升。
- 仍保留 22M 格量级的构造/析构成本与 75% 结构性浪费。

**适用**：作为**立即止血 + 应急版本**，配合方案 C 分阶段推进。

---

### 方案 B：稀疏哈希表（`unordered_map` / 开放寻址，容量 ∝ 占用格）

**做法**
- 把 `Grid` 换成 `std::unordered_map<uint32_t, CellContent>`（每层一个，key = 层内莫顿码；或全局 key = `(层号 << 32) | 莫顿码`），只有被对象占据的格子才建立条目。
- 空格查询返回静态空容器（或使用 `CellRef` 视图对象，内部空条目返回空）。
- 无 `CurerExcursionVector` 时偏移可直接运行时算；有 map 后 `ExcursionVector` 只保留层首偏移。
- 自定义哈希：莫顿码本身分布良好，建议再加 splitmix64 混淆，防止恶意/退化分布造成长链。

**优点**
- **内存 ∝ 占用格数（≤ 对象数）**，与地图尺寸完全解耦：1000 对象的地图，无论多大，网格内存都在几十 KB 级。
- 对外接口可保持完全不变（`at()` 需要适配返回值）。
- 逐格 vector 概念保留，`Get` 收集逻辑基本照搬。

**缺点**
- 每帧 `UpData`（对象移动）→ map 的 erase/insert 反复执行，触发**每帧哈希分配**，与引擎"帧内零分配"的 MemoryPool 哲学冲突；对象大面积移动时是性能退化点。
- 缓存局部性差：哈希探测 + 指针追逐，`Get` 大量随机访存，宽场景下粗筛反而变慢。
- `unordered_map` 单条目开销 32~64 B + 内容 vector 24 B + 堆块头，**每占用格约 100~150 B**，是密集网格单格成本的数百倍（但只在占用格出现）。
- 迭代顺序不稳定，调试与可复现性变差。

**适用**：超大尺寸（Size ≥ 4096）+ 低对象密度场景；或作为方案 C 的"溢出模式"。

---

### 方案 C：稠密索引数组 + 链表（推荐主方案）

**核心思路**：每格不再挂一个 `vector`，而是只存 4 字节链头；对象用"格内链表"串起来，遍历时顺链收集。**每格内存 24 B → 4 B，且整个索引区一整块连续分配（一次 malloc），消灭逐格堆分配。**

数据结构（推荐 C-2 槽位版，不改任何外部头文件）：

```cpp
std::vector<uint32_t> mCellHead;   // 4 B/格：格内链表头（槽位下标），UINT_MAX 表示空
std::vector<PhysicsFormwork*> mSlots;      // 8 B/对象：对象槽位容器（稳定槽位，永不 swap）
std::vector<uint32_t> mNext;       // 4 B/对象：槽位 i 在格内链表中的下一个槽位
std::vector<uint32_t> mFreeSlots;  // 空闲槽位复用栈（Remove 时回收）
std::vector<uint32_t> mDirtyCells; // 上帧被写过的格（配合"只清脏格"）
```

- `Add`：取空槽（或追加）→ `mNext[slot]=mCellHead[idx]; mCellHead[idx]=slot`（头插 O(1)）。
- `Remove`：按 `mGridIndex` 定位格，沿链找到该对象 O(k)（k=格内对象数，通常个位数）删除；槽位入 `mFreeSlots`。
- `UpData`（改为**整树重建**，每帧一次）：
  ```cpp
  // 1. 只清空上帧用过的格（脏格列表），成本 ∝ 对象数而非格数
  for (auto c : mDirtyCells) mCellHead[c] = UINT_MAX;
  mDirtyCells.clear();
  // 2. 主循环：对所有活跃对象重算格号 → 头插重建
  //    可并行：UpDaraWorkeTask 各线程只算"对象→格号"写 per-thread 数组，
  //            主线程 UpDaraWorkeTaskEnd 统一重建链表（与现流程结构一致）
  for (auto id : mActiveIds) {
      uint32_t idx = atIndex(...);
      if (idx < mCellHead.size()) {
          mNext[id] = mCellHead[idx]; mCellHead[idx] = id;
          if (mCellHeadWasEmpty) mDirtyCells.push_back(idx);  // 记录首次写入
      } else { GridExtrovert 列表维护; }
  }
  ```
- `Get(S,E,Out)`：层循环不变；逐格改为 `for (u32 id = head; id != UINT_MAX; id = mNext[id]) Out.push_back(mSlots[id]);`。Extrovert 语义保留。
- 层偏移同时改为紧凑公式（顺带消除 75% 结构性浪费，见 2.4-2），`GridSize` 只保留实际可达层。

**内存核算（x64）**：

| Size | 4B/cell 头数组 | + 对象(槽8B+next4B)×10k | 对比现状 |
|---|---|---|---|
| 256 | 5.6 MB | 0.12 MB | 33.6 MB → **5.7 MB**（-83%） |
| 1024 | 89.5 MB | 0.12 MB | 537 MB → **89.6 MB**（-83%） |
| 2048 | 358 MB | 0.12 MB | 2.1 GB → **358 MB**（-83%） |

若采用紧凑偏移 + 只保留 25% 可达容量（或把原始 4× 富余全部拿去增加最细层分辨率），上表数字再除 4：1024 → **22.7 MB**。

**优点**
- 内存 6× 下降（24B→4B/格）且**单块连续分配、零逐格 malloc**；`mCellHead` 可用 `new uint32_t[]` / `std::vector` 一分配到底。
- **稳态每帧零分配**：重建只写 4 B 数组段落，没有 vector 增长/释放；脏格列表保证清空成本 ∝ 对象数。
- 缓存友好：`mCellHead` 与 `mSlots`/`mNext` 均为连续数组；链表遍历深度 1 层指针追逐，远优于 unordered_map 的随机访存。
- 天然适配并行：现有多线程 UpData 流程（算索引并行 + 应用串行）可原样保留。
- 可扩展：未来 Size 再大，把 `mCellHead` 换成分层稀疏哈希（= 方案 C+B 混合），遍历逻辑不变，仅 `head` 获取方式变化。
- 公共 API（SetMapRange/Add/Remove/Get×2/GetDividedVision/GetGridExtrovert/UpData/UpDaraWorkeTask(End)/MoveGridTo/UpdateGridOffset/SetThreadCount/GetGridCenter）**全部保持不变**；`at()` 仅在类内部使用，可自由改造。

**缺点**
- 需要重写 GridSearch 内部（约 300~400 行改动），实现量中等。
- 引入"槽位"概念，`Remove` 后需正确维护自由槽位栈；不设 `prev` 指针时 Remove 为 O(k) 单格扫描（k 通常个位数，可接受；必要时加 `mPrev` 双向链实现 O(1)）。
- 链表的随机插入顺序使"同一格内对象"在 mSlots 中顺序与访问顺序不再相关（对物理无影响，但调试输出顺序会变）。
- 若不做紧凑偏移修正，仍保留 25% 结构性浪费。

**适用**：**推荐作为正式方案**，一次改造彻底解决内存问题。

---

### 方案 D：扁平化池（SoA：每层一个 flat 数组 + 格 → (offset, count) 切片）

**核心思路**：每个非空格子在 pooled 对象数组中分配一段连续切片（`uint32 off, uint32 cnt`，8 B/格）；`UpData` 重建 = 统计计数 → 前缀和 → 填充；`Get` 遍历切片 = **纯顺序内存访问（缓存最优）**。

```
CellOff[cell] 8B  |  CellCnt[cell] 8B  |  Pool[off..off+cnt)  → 对象指针连续
```

**优点**
- 查询遍历是**连续内存**，是五种方案里缓存表现最好的（优于 C 的链指针）。
- 无逐格堆分配、无侵入式字段、一次大分配、天然支持并行重建（per-thread 计数再归并）。
- `UpData` 语义天然成为"每帧重建"，代码逻辑比 C 更简单直接。

**缺点**
- 稠密格表 8 B/格（C 是 4 B/格），大尺寸时仍是常数级负担；彻底解决需再做稀疏化（矛盾点）。
- 两遍重建（count → fill），对象多时拷贝更多指针；且重建期间不能穿插 Add/Remove（需要推迟对象变更）。
- `Get` 期间不能有并发写（与现状一致，但方案 C 的"局部链表写"更弹性）。

**适用**：需要极致查询性能、且能接受"每帧整表重建"的场景；通常与 C 的稀疏化结合使用（C 若需要"连续切片遍历"可直接换成 D 的池）。

---

### 方案 E：组合方案（推荐落地路线）

| 阶段 | 内容 | 收益 | 风险 |
|---|---|---|---|
| **Phase 1（立即）** | 方案 A1（mStorey 上限）+ A2（swap 释放）+ A3（越界修复）+ A4（reserve） | 内存 -90%+，修内存安全 bug | 极低（纯局部修正） |
| **Phase 2（根治）** | 方案 C：4 B/格头数组 + 槽位链表 + 脏格清零 + 紧凑偏移 | 内存 -83%~95%（叠加 Phase1 后 1024 地图约 20~25 MB），稳态零分配 | 中（内部重写，API 不变，需要回归测试） |
| **Phase 3（可选）** | C 的 `mCellHead` 在超大 Size（≥4096）时切换为分层稀疏哈希（C+B 混合）；或升级为 D 的 flat 池 | 内存彻底 ∝ 对象数；查询性能更优 | 中高（需要基准对比保持性能不回退） |

**配套必做**（穿插在任意阶段）：
- 修正 `Excursion` 偏移公式为紧凑前缀和（`off(i) = 1 + Σ_{j>i} 4^(mStorey-j)`），消除 75% 结构性浪费；若要保持现在的最细层粒度（格边长 2），可以直接把 `GridSize` 缩至原来的 1/4。
- `GridExtrovert` 与 `mAllObjects` 保持现状即可（内存 ∝ 对象数，无问题）。

---

## 五、方案对比总表

| 维度 | 方案A 止血 | 方案B 稀疏哈希 | 方案C 链表索引 ⭐ | 方案D flat池 | 方案E 组合 |
|---|---|---|---|---|---|
| 内存（1024 地图，1 万对象） | ~2~33 MB | ~1~2 MB | **~23~90 MB** | ~90~180 MB | **~20~25 MB** |
| 内存是否 ∝ 格子数 | 是（但缩小） | **否（∝占用格）** | 是（4B/格，可再稀疏化） | 是（8B/格） | 否 |
| 每帧分配 | 有（搬移时） | **有（哈希 erase/insert）** | **无（稳态零分配）** | 无（重建大块） | 无 |
| Get 缓存局部性 | 差（逐格 vector 跳访） | 差（哈希随机访存） | 中（连续数组+1 层链指针） | **优（连续切片）** | 优 |
| 多线程 UpData 兼容 | 保持 | 需改造 | 保持（算并行/应用串行） | 重建可并行 | 保持 |
| 实现量 | ~10 行 | ~200 行 | ~300~400 行 | ~300 行 | ~400 行 |
| 公共 API 变化 | 无 | `at()` 需适配 | **无** | 无 | 无 |
| 是否侵入其他文件 | 否 | 否 | 否（槽位版） | 否 | 否 |
| 附带修复越界 bug | ✅ | ✅ 需处理 | ✅ 需处理 | ✅ 需处理 | ✅ |
| 风险 | 极低 | 中（性能退化风险） | 中（内部重写，需回归） | 中高 | 中 |
| 推荐度 | ★★★★（应急） | ★★（特定场景） | **★★★★★（正式）** | ★★★（极致性能） | ★★★★★（最终形态） |

**综合推荐**：以 **方案 C 为正式主线**（内存下降 83%+、稳态零分配、API 不变、并发模型保留），分两阶段落地：先用方案 A 的修正立即止血并修复 `atIndex` 越界读（Phase 1），再实施方案 C 并同步修正偏移公式（Phase 2）；若未来需要支撑 Size ≥ 4096 的超大世界，再叠加方案 B 的稀疏化（Phase 3）。

---

## 六、验收建议

1. **内存基准**：记录 `SetMapRange` 后与 10k 对象模拟 1000 帧后的峰值内存（可用 `GetProcessMemoryInfo` / VS 诊断工具 / 火焰图）。改造后 1024 地图应从 ~540 MB 降到 ≤ 90 MB（Phase1 后即可验证 ~90% 降幅）。
2. **运行基准**：对比每帧 `CollisionDetectionTimeMS` 与候选对数量，确认不因格子变粗/链表遍历带来明显回退（允许 ±10%）。
3. **正确性回归**：`Get` 结果集合与改造前完全一致（可用排序后比对）；重跑现有场景（碰撞、触发器、可视化 GetDividedVision）。
4. **越界用例**：构造 `R ≥ mGridDim/2` 的大对象，验证其正确进入 `GridExtrovert` 且无越界读（可用 MSVC `/RTC` + ASan 验证）。
5. **并发验证**：多线程模式下确认 `Get` 与 `UpData` 的先后顺序约束未被破坏（现流程：UpData/End 完成后 workers 才开始查询）。

---

## 七、方案 C 实施状态（已落地 ✅）

### 7.1 改动文件

- **`PhysicsBlock/GridSearch.hpp`**：全面重写网格容器部分（方案 C 核心）
- 未改动任何其他文件（`PhysicsWorld` / `PhysicsTrigger` / `PhysicsAuxiliaryVision` 调用面零改动）

### 7.2 实施内容

| 项目 | 实现 |
|---|---|
| 每格容器 | `vector<vector<PhysicsFormwork*>>`（24B/格 + 逐格堆块）→ `vector<unsigned int> mCellHead`（4B/格，整块连续） |
| 对象存储 | 新增槽位数组 `mSlots` + 并行 `mNext`/`mNewIndex`（8B+4B+4B/对象），删除 `mAllObjects` |
| 格内组织 | 单向链表：`mCellHead[cell]` → 槽位 → `mNext` 串接；Remove 摘链 O(k)，Add 头插 O(1) |
| 槽位复用 | `mFreeSlots` 栈：Remove 回收、Add 复用，删除搬移 → 下标稳定 |
| UpData | 改为整树重建：只清"脏格"（上帧写入格，`mDirtyCells`），零分配 O(n) |
| 多线程路径 | `UpDaraWorkeTask/End` 签名与流程不变：并行算 `mNewIndex[slot]`（写不相交区间）→ 主线程 `ApplyRebuild()` |
| 层偏移 | 修正为紧凑公式 `off(i) = (4^(mStorey-i)-1)/3`，配合 **mGridDim 取整为 2 的幂**（见 7.7），索引区仅为旧布局的 1/4（对 2 的幂尺寸）或不超过旧布局（任意尺寸） |
| 越界修复 | `atIndex` 对 `_storey > mStorey` 直接返回无效索引（入 Extrovert），修复旧版 `ExcursionVector[_storey]` 越界读；`GetDividedVision` 同步 clamp |
| 配套修复 | `SetMapRange` 用 swap 真正释放旧索引表（换小地图释放内存）；`Get` 移除脆弱的 `P>>=2` 技巧，改用预计算偏移 |

### 7.3 内存收益（与实施前对比）

| SetMapRange(Size) | 实施前（vector 嵌套，空载） | 实施后（4B/格头数组 + 紧凑偏移） | 降幅 |
|---|---|---|---|
| 256 | 33.6 MB | **2.2 MB** (559,241×4B) | **-93%** |
| 1024 | 537 MB | **21.4 MB** (5,592,405×4B) | **-96%** |
| 2048 | 2.1 GB | **85.5 MB** (22,369,621×4B) | **-96%** |

另加：每帧稳态零堆分配（旧实现对象搬移时逐格 realloc + 旧格容量永不回落）、逐格 malloc 头全部消除、索引区单块连续分配。

### 7.4 验证结果

- **编译**：`PhysicsBlockLib` **Debug 与 Release** 双配置构建通过（0 error；所有警告均为仓库既有警告，无新增）
- **回归测试**：`build/GridTest/test_grid.cpp`（独立可执行，覆盖 Add/Get/UpData 单线程与多线程/Remove/超大对象 Extrovert/槽位复用 100 次循环无泄漏/无重复/SetMapRange 重建/GetDividedVision）
  - 结果：**全部通过**；紧凑偏移手算参考值与实现输出完全一致
  - 场景覆盖：**SetMapRange(3) / (10) / (64)** —— 含非 2 的幂尺寸（7.7 修复的越界场景）
- **API 兼容**：全库 grep 确认 `at()`（vector引用版）无任何外部调用，移除安全；`SetThreadCount/Add/Remove/Get×2/GetDividedVision/GetGridExtrovert/UpData/UpDaraWorkeTask(End)/MoveGridTo/UpdateGridOffset/SetMapRange/GetGridCenter` 签名全部保留

### 7.5 行为差异说明（有意为之/需知悉）

1. **`mGridIndex` 编号方案改变**：紧凑偏移后格子绝对编号与旧版不同，但语义（"对象所在格索引，UINT_MAX=网格外"）保持一致；只在日志/序列化中可观察。
2. **`Get` 结果顺序不定**：链表头插顺序 ≠ 插入顺序（旧实现为 vector 追加顺序）；碰撞检测只做集合使用，无影响。
3. **`SetMapRange` 后旧对象**：旧实现对象会从网格中"失联"直到重新 Add；新实现槽位保留，下一次 `UpData` 自动重新归位（行为改进）。
4. **`at()` 三个重载已移除**（返回 vector 引用，无法映射链表）：仅内部使用，外部代码未依赖（grep 验证）；若有第三方调用需改为遍历 `Get` 或直接使用公开查询接口。

### 7.6 后续可选项（未实施，按需）

- 超大尺寸（Size ≥ 4096）时把 `mCellHead` 切换为分层稀疏哈希（方案 B+C 混合）——`LinkCell/Get` 的"取链头"逻辑已与容器解耦，替换成本低
- 每格对象数量大的场景可升级为方案 D（flat 池切片）获得更优遍历缓存
- `Remove` 加 4B/对象的 `mPrev` 可把摘链降为 O(1)（当前 O(k)，k 为格内对象数）

### 7.7 修复记录：非 2 的幂 Size 数组越界（重要！）

**现象**（真实调试暴露）：Debug 下 `GridSearch::Get` 在 `mCellHead[cellIdx]` 处触发越界断言。
调试器现场：`SetMapRange(10)` → `mGridSize=341, mStorey=5, mGridDim=40`；查询 `iE=(12,16)` 在层 1 的坐标 `y=16` 需要 5 位，而层 1 每维只有 `2^(5-1)=16` 格，莫顿码 `off(85)+592=677 > 341` → 越界。

**根因**：`mGridDim = 4·Size` 可为任意值，而 `mStorey = floor(log2(4·Size))`；当 `4·Size` 不是 2 的幂时（如 40），`mGridDim ≠ 2^mStorey`，层内坐标可超出 `2^(mStorey-storey)`，**任何按"每层每维 = 2^(mStorey-storey)"设计的紧凑映射（莫顿码/偏移）都会越界**。
旧实现不崩的原因是旧偏移公式每层预留了 4 倍容量（层坐标多 1 bit）——它正是为任意 `mGridDim ≤ 2^(mStorey+1)-1` 兜底设计的；但旧实现也只是"勉强容错"（越界层区域会与其他层/空洞串用索引，语义错乱），并非真正正确。

**修复**（`SetMapRange`）：
1. `mGridDim` **向上取整到 2 的幂**（`gridDim = next_pow2(4·Size)`），`mExcursion = {gridDim/2, gridDim/2}` —— 世界覆盖 `[-gridDim/2, +gridDim/2)` ⊇ 原范围 `[-2·Size, 2·Size]`，空间语义不缩水，只增不减；
2. 紧凑偏移公式与莫顿编码在 `mGridDim = 2^mStorey` 前提下对**任意 Size** 严格成立；
3. `Get` 中 `cellIdx ≥ mGridSize` 增加一行防御性 `continue`（莫顿码截断/异常输入兜底，几乎零开销）。

**验证**：回归测试新增 `SetMapRange(3/10/64)` 三种尺寸（10 即用户崩溃场景），全部通过；Debug/Release 双配置编译零错误。
注意：此修复后 `mGridDim` 对非 2 的幂 Size 会略大于 `4·Size`（如 Size=10 → 64），对 2 的幂尺寸完全不变。

---

## 附录：方案 C 关键伪代码（槽位链表版）

```cpp
// 成员
std::vector<uint32_t>           mCellHead;    // 每格链头（槽位），UINT_MAX=空
std::vector<PhysicsFormwork*>   mSlots;       // 槽位 → 对象（永不 swap，靠自由栈复用）
std::vector<uint32_t>           mNext;        // 槽位 → 同格下一槽位
std::vector<uint32_t>           mFree;        // 空闲槽位栈
std::vector<uint32_t>           mActive;      // 活跃槽位（重建遍历用）
std::vector<uint32_t>           mDirty;       // 上帧非空格（下次重建只清这些）

uint32_t NewSlot(PhysicsFormwork* o) {
    uint32_t id;
    if (mFree.empty()) { id = (uint32_t)mSlots.size(); mSlots.push_back(o); mNext.push_back(UINT_MAX); }
    else               { id = mFree.back();     mFree.pop_back();    mSlots[id] = o;   mNext[id] = UINT_MAX; }
    mActive.push_back(id);
    return id;
}
void Remove(PhysicsFormwork* o) {
    uint32_t id = o->mGridIndex;                 // 槽位版本：mGridIndex 存槽位号（UINT_MAX=外置）
    if (id != UINT_MAX && id < mSlots.size() && mSlots[id] == o) {
        // 沿链找前驱，从 mCellHead[cell] 摘下（O(k)，k 为格内对象数）
        ...
        mSlots[id] = nullptr; mFree.push_back(id);
        mActive 视实现（可保留而懒清理，或后台压缩）
    } else { /* 从 GridExtrovert 移除 */ }
}
void UpData() {   // 整树重建，O(对象数) + O(脏格数)
    for (uint32_t c : mDirty) mCellHead[c] = UINT_MAX;
    mDirty.clear();
    for (uint32_t id : mActive) {
        auto* o = mSlots[id]; if (!o) continue;
        uint32_t idx = atIndex(o->PFGetPos(), o->PFGetCollisionR());
        if (idx < mCellHead.size()) {
            bool wasEmpty = (mCellHead[idx] == UINT_MAX);
            mNext[id] = mCellHead[idx]; mCellHead[idx] = id;
            if (wasEmpty) mDirty.push_back(idx);
            o->mGridIndex = id;                  // 槽位号
        } else { /* 加入 GridExtrovert，mGridIndex = UINT_MAX */ }
    }
}
void Get(Vec2_ S, Vec2_ E, std::vector<PhysicsFormwork*>& Out) {
    // 层循环与现在一致；逐格：
    for (uint32_t id = mCellHead[globalIdx]; id != UINT_MAX; id = mNext[id])
        Out.push_back(mSlots[id]);
    // Extrovert 逻辑保留
}
```

> 注：槽位版本无需修改 `PhysicsFormwork`；若希望更少指针追逐，可改为侵入式（在 `PhysicsFormwork` 中加 `PhysicsFormwork* GridNext`），实现更简单但需改一个头文件。
