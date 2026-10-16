# PhysicsTest3D —— 3D 物理沙盒（GameMods 游戏模式）

以 `GameMods/PhysicsTest/`（2D 物理测试模式）为蓝本的 **3D 版**，挂在刚完成的
`PixelClean/PhysicsBlock3D/` 三维物理引擎上。进入方式：主界面模式翻页到
**「3D物理测试」**（稳定 id = 8，位于「无限」之后）。

```
GameMods/PhysicsTest3D/
├── PhysicsTest3D.h / .cpp        外壳：GameMods 子类（输入/相机/UI/渲染/物理步进）
├── PhysicsDemo3D.h / .cpp        24 个演示场景（与 2D 版 Demo 一一对应）
├── PhysicsLogBuffer3D.h / .cpp   ImGui 日志缓冲
└── CMakeLists.txt                PhysicsTest3DLib（GameModsLib 的递归 glob 同样会编它）
```

## 与 2D 版 `PhysicsTest` 的差异

| 方面 | 2D | 3D |
|---|---|---|
| 相机 | 固定 z 俯视，`setCameraPos({x,y,50})` | **自由飞行**（yaw/pitch + WASD/QE/SPACE 移动，照 `GameMods/BlockWorld` 的做法） |
| 鼠标拾取 | 射线打回 z=0 平面 | **真三维拾取**：`screenToWorld` 得方向 → 沿射线步进 → `world->Get(point)` 命中物体 / `FMGetCollide` 命中地图体素 → 回落 y=0 面 |
| 网格编辑 | 二维 `map<pair<int,int>>` | 选中 `PhysicsShape3D` 后按 **Z 层切片**编辑体素（X/Y 画笔），改动即时 `UpdateAll()` |
| GPU 后端 | `PhysicsGPU`（Vulkan 冲量迭代） | **移除**（3D 引擎不含 PhysicsGPU）；用世界自带的五段 CPU 耗时访问器出指标 |
| 键位 | DELETE / R / SHIFT | `GameKeyEnum` 没有这些值：**MOVE_UP=删除选中、MOVE_DOWN=下降、SPACE=上升、Key_1/2=切换场景** |
| 液体 | `PhysicsLiquid` | `PhysicsLiquid3D`（PBF 3D，demo 23：池水 + 水团砸落 + 浮船/沉铁） |

## 24 个场景（`PhysicsDemo3D.h` 的 `DemoFunS3D[]`）

0 外骨骼（kinematic 躯干 + 4 肢 + 4 关节）· 1 几何重心（偏心/高重心/均匀三块对拍）·
2 方块堆叠 · 3 **真三维 5 层金字塔** · 4 粒子束砸方块阵 · 5 关节相连双块 ·
6 斜拉 10 节铁链 + 末端重物 · 7 杠杆（lever 钉支点，两端悬殊重量）· 8 橡皮筋/弹簧双球对 ·
9 长绳拖过台阶棱 · 10 四种半径球（前旋滚动）· 11 **4×4×4=128 球压力测试** ·
12 绳牛顿摆（端点拉 29°）· 13 关节牛顿摆（kinematic 横梁 + 刚性锚点）·
14 阶梯缓坡三种摩擦 · 15 200 高速粒子逼网格重建 · 16 六根不同厚度胶囊 ·
17 粒子绳链 + 装饰杆对比 · 18 组装体/塔/关节/粒子大杂烩 · 19 碰撞回调（Enter/Exit + 地图版）·
20 运动学平移平台 + slerp 转盘 · 21 触发器 + **层级过滤**（球进 gated 区、方块不进）·
22 动态地图（16³ 板块 × 6³，值噪声丘陵 + 天降物雨）· 23 PBF 液体（浮船沉铁）。

## 世界坐标约定（写新场景必读）

- `BuildBoxMap3D(size, h)` 内部 `SetCentrality(size/2)` ⇒ **世界坐标以地图中心为原点**，
  地面顶面在 `y = -size/2 + 1`；场景物体摆放心口里用带负号的 y。
- `MapDynamic3D` 的 `centrality` 恒 0 ⇒ 世界坐标 = 体素坐标（demo 22 地表在 y≈48）。
- `at(x,y,z)` 的内存布局 `(x*H+y)*D+z` 只是排布，**坐标轴语义不变：竖直永远是 y**。
- Junction/Joint 的**静止长度在构造瞬间由两端当时距离定死**：要"拉长的绳"必须先
  在松弛位建约束、再把物体挪开（demo 8/12/13 就是这么写的）。
- `PhysicsWorld3D` 析构**自动删除地图与液体**：场景里 `new` 的物体一律不用手动 delete。

## 验证（本机 NDK clang 交叉编译，逐 TU 全绿 + 无头实跑）

- `PhysicsTest3D.cpp` / `PhysicsDemo3D.cpp` / `PhysicsLogBuffer3D.cpp` / `application.cpp` /
  `InterfaceUI/Interface.cpp` 全部编译 **0 错误 0 警告**；2D 的 `PhysicsTest.cpp`、
  `BlockWorld.cpp` 回归通过（共享头改动未破坏既有模式）。
- **无头运行时冒烟**：把 24 个 demo + `Camera/` + 3D 引擎链成可执行文件在手机上跑，
  每个场景步进 60 帧 —— `=== ALL DEMOS PASSED === 失败项: 0`
  （每场景：世界/地图非空、物体数>0、无 NaN/Inf、无速度爆炸；
  demo19 的碰撞 Enter、demo21 的触发 Enter/Exit 与层级过滤、demo23 的 327 物体液体均实测触发）。
- 测试源码：`/data/user/0/com.deepseek.harness/files/pc3dc/t3ddemo.cpp`
  （链接脚本 `linkdemo.sh`，游戏层编译脚本 `buildgame.sh`）。

## 未验证项（如实声明）

- **Kotlin 侧**（`MainActivity.kt` 加了 `PhysicsTest3D=8` 的 `GameModeId` 与 ModeConfig）
  只做了源码级修改，本环境无 kotlinc/Gradle，未编译验证；即便不重编 APK，未识别的
  模式 id 也会落到默认无控件配置，不会崩溃。
- 完整 APK 链接（全引擎 .so）在本机不可行（`make/cmake` 环境已被清理），验证级别为
  逐翻译单元编译 + 无头实跑，与 PhysicsBlock3D 引擎本体的验收口径一致。
- 移动端触摸与 3D 外壳交互（单指拖拽=转视角还是拾取）未经真机手感调校。
