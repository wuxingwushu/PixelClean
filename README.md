# PixelClean

基于 Vulkan 的跨平台（Windows / Android）2D/3D 游戏引擎，使用 C++17 编写。项目集成了自研渲染层、物理引擎、音频引擎、网络多人同步、过程化生成等子系统，并通过可插拔的 GameMod 架构承载多种游戏玩法。

---

## 目录

- [项目概述](#项目概述)
- [核心功能说明](#核心功能说明)
- [项目结构](#项目结构)
- [环境要求](#环境要求)
- [安装与配置步骤](#安装与配置步骤)
- [使用指南](#使用指南)
- [配置文件说明](#配置文件说明)
- [常见问题解答](#常见问题解答)
- [维护说明](#维护说明)
- [许可证与第三方库](#许可证与第三方库)

---

## 项目概述

PixelClean 是一个以学习与实践现代图形/游戏引擎技术为目标的工程项目。引擎核心围绕 Vulkan API 构建，覆盖了从窗口管理、设备初始化、交换链、渲染管线、着色器编译到帧同步的完整渲染流程，并在其之上构建了多套游戏玩法演示。

引擎采用模块化设计，各子系统（渲染、物理、音频、网络、UI、相机、角色、武器、方块世界等）以独立目录 + CMake 静态库的形式组织，最终链接为单一可执行文件（Windows）或共享库（Android）。

**支持平台**：
- **Windows**：主开发平台，使用 MSVC + Ninja 构建，以窗口子系统运行。
- **Android**：通过 `MyApplication/` 下的 Gradle 工程与 JNI 桥接层（`native-lib.cpp`）接入，minSdk 26，支持 `arm64-v8a` 与 `x86_64` ABI。

---

## 核心功能说明

### 1. Vulkan 渲染层

- 位于 [Vulkan/](file:///c:/Github/PixelClean/Vulkan) 与 [VulkanTool/](file:///c:/Github/PixelClean/VulkanTool)，对 Vulkan API 进行面向对象封装。
- 涵盖 Instance、Device、SwapChain、RenderPass、Pipeline、CommandBuffer、Buffer、Image、Sampler、Semaphore、Fence 等全部核心对象。
- [VulkanTool/](file:///c:/Github/PixelClean/VulkanTool) 提供管线创建、纹理库、视觉效果、辅助视觉（调试可视化）等高层工具。
- GLSL 着色器位于 [shaders/](file:///c:/Github/PixelClean/shaders)，由 `shaders/compile.bat` 调用 `glslangValidator` 编译为 SPIR-V，CMake 配置阶段自动执行。

### 2. 游戏模式系统（GameMods）

所有玩法继承自 [GameMods](file:///c:/Github/PixelClean/GameMods/GameMods.h) 抽象基类，由 [Application](file:///c:/Github/PixelClean/application.h) 统一调度。内置 9 种模式：

| 模式枚举 | 名称 | 说明 | 多人联机 |
|----------|------|------|----------|
| `Maze_` | 迷宫 | 迷宫探索，集成物理、武器、寻路与网络复制 | 支持 |
| `TankTrouble_` | 坦克动荡 | 固定迷宫内的坦克对战，多种子弹类型 | 不支持 |
| `PhysicsTest_` | 物理测试 | 自研物理引擎功能演示（含 GPU 计算） | 不支持 |
| `SoloudTest_` | SoLoud 音效 | 环境音效演示：遮挡、混响、回声、低通、空间声像 | 不支持 |
| `RadianceCascades_` | 辐射级联 GI | 2D 全局光照算法实现（Radiance Cascades） | 不支持 |
| `FruitNinja_` | 水果忍者 | 像素水果切割玩法 | 不支持 |
| `WFCTest_` | WFC 测试 | 基于 WaveFunctionCollapse 的城市地图生成 | 不支持 |
| `BlockWorld_` | Minecraft 区块世界 | 体素区块地形生成与渲染 | 不支持 |
| `Infinite_` | 无限地图 | 无限滚动地图、动态地形、战争迷雾、AI 寻路 | 不支持 |

### 3. 物理引擎（PhysicsBlock）

- 位于 [PhysicsBlock/](file:///c:/Github/PixelClean/PhysicsBlock)，自研 2D 物理系统。
- 支持刚体、粒子、关节、连接、组装体、触发器、运动学物体。
- 网格化碰撞检测（`BaseGrid`）、碰撞仲裁器（`PhysicsArbiter`）、能量守恒、四叉树/网格搜索加速。
- 提供 GPU 计算着色器加速（`physics_arbiter.comp`、`physics_joint.comp`、`physics_junction.comp`）。
- [PhysicsBlock.ini](file:///c:/Github/PixelClean/PhysicsBlock.ini) 控制质心、碰撞点、法向量、速度、角度等调试可视化开关。

### 4. 音频引擎（Audio）

- 位于 [Audio/](file:///c:/Github/PixelClean/Audio)，基于 [SoLoud](file:///c:/Github/PixelClean/Environment/Soloud) 构建。
- 全局单例 [AudioEngine](file:///c:/Github/PixelClean/Audio/AudioEngine.h)，管理资源库、混音总线、声部、空间音频、房间声学、RTPC 控制器。
- 支持 MIDI 播放（通过 SoundFont `.sf2` 音色库，如 `TimGM6mb.sf2`）。
- 提供 6 条混音总线、室内混响（FreeverbFilter）、走廊回声（EchoFilter）、遮挡低通滤波等效果。

### 5. 网络多人系统（NetworkTCP）

- 位于 [NetworkTCP/](file:///c:/Github/PixelClean/NetworkTCP)，基于 [libevent](file:///c:/Github/PixelClean/Environment/libevent) 实现 TCP 通信。
- [NetworkLayer](file:///c:/Github/PixelClean/NetworkTCP/NetworkLayer.h) 单例封装服务端监听、客户端连接、zlib 压缩过滤、数据包收发。
- [ReplicationManager](file:///c:/Github/PixelClean/NetworkTCP/ReplicationManager.h) 提供状态复制（`ReplicableObject`、`ReplicableComponent`、`ReplicatedProperty`）与事件分发机制。
- 数据包通过 `DataHeader.Key` 路由：`Key=0` 状态复制，`Key=1` 事件分发。

### 6. 相机系统（Camera）

- 位于 [Camera/](file:///c:/Github/PixelClean/Camera)，采用策略模式。
- 投影策略：`PerspectiveProjection`（透视）、`OrthographicProjection`（正交）。
- 控制策略：`FPSControl3D`（第一人称 3D）、`FixedControl`（固定）、`FollowControl2D`（2D 跟随）、`FreeControl2D`（2D 自由）。
- 支持运行时热切换策略。

### 7. 角色与武器系统

- [Character/](file:///c:/Github/PixelClean/Character)：`GamePlayer`（玩家）、`NPC`、`Crowd`（群体）、`DamagePrompt`（伤害提示）、`MovementComponent`（移动组件）、`UVDynamicDiagram`（UV 动态图）。
- [Arms/](file:///c:/Github/PixelClean/Arms)：武器系统，支持手枪、霰弹、机枪、火箭等多种子弹类型，含反弹与爆炸。集成粒子系统（`ParticleWorld`、`ParticleEmitter`、`ParticlePool`、`ParticleRenderer`、`ParticleUpdater`），使用 SSBO + Instanced Rendering 渲染。

### 8. 方块系统（BlockS）

- 位于 [BlockS/](file:///c:/Github/PixelClean/BlockS)，提供 Minecraft 风格的方块与纹理。
- [BlockS/Pixel图片/](file:///c:/Github/PixelClean/BlockS/Pixel图片) 包含安山岩、基岩、砖块、圆石、混凝土、泥土、花岗岩、草、沙砾、下界岩、黑曜石、木板、石头等像素纹理。
- 附带 `PixelTool.py` / `PixelTool.exe` 纹理处理工具。

### 9. 用户界面（InterfaceUI）

- 位于 [InterfaceUI/](file:///c:/Github/PixelClean/InterfaceUI)，基于 [Dear ImGui](file:///c:/Github/PixelClean/Environment/imgui) 构建。
- 提供主界面、副界面、多人界面、设置界面四种切换状态。
- 内置控制台、聊天框、FPS/计时显示、ImGui 纹理封装。

### 10. 工具库（Tool）

- 位于 [Tool/](file:///c:/Github/PixelClean/Tool)，提供通用基础设施：
  - 寻路：`AStar.h`、`JPS.h`（Jump Point Search）、`GridNavigation.h`、`GenerateMaze.h`
  - 噪声：`PerlinNoise.h`、`FastNoiseLite.h`
  - 并发：`ThreadPool.h`、`MPMCQueue.h`、`Queue.h`、`MemoryPool.h`
  - 数据结构：`Quadtree.h`、`trie.h`、`cuckoohash_map.hpp`、`ContinuousData.h`、`ContinuousMap.h`
  - 第三方：`json.hpp`（nlohmann/json）、`sml.hpp`（boost.sml 状态机）
  - 日志：`log.h`（基于 spdlog 封装）、计时器 `Timer.h`

### 11. Android 移动端支持

- [MyApplication/](file:///c:/Github/PixelClean/MyApplication) 为 Android Studio 工程，Gradle Kotlin DSL 配置。
- [native-lib.cpp](file:///c:/Github/PixelClean/MyApplication/app/src/main/cpp/native-lib.cpp) 为 JNI 桥接层，负责生命周期管理（`initBeforeSurface` → `initAfterSurface` → `frameStep`）、输入事件传递（WASD 摇杆、虚拟按键）、渲染循环调度。
- 横屏锁定，要求设备支持 Vulkan（`android.hardware.vulkan.level`）。
- 构建时自动调用 `shaders/compile.bat` 编译着色器，并将 `Resource/`、`Info.ini`、`PhysicsBlock.ini`、字体、方块纹理打包至 assets。

---

## 项目结构

```
PixelClean/
├── main.cpp                  # 程序入口（Windows: main / Android: pixelclean_main）
├── application.h/.cpp        # Application 类：引擎总调度
├── GlobalVariable.h/.cpp     # 全局配置变量与 INI 读写
├── GlobalStructural.h        # 全局结构体定义
├── FilePath.h                # 资源/着色器路径宏
├── DebugLog.h                # 日志宏（Windows printf / Android logcat）
├── Info.ini                  # 主配置文件
├── PhysicsBlock.ini          # 物理调试可视化配置
├── CMakeLists.txt            # Windows 主构建脚本
├── CMakeSettings.json        # VS CMake 配置（x64-Debug / x64-Release）
├── resource.rc               # Windows EXE 图标资源
├── Hammer.ico                # 应用图标
│
├── Vulkan/                   # Vulkan API 封装层
├── VulkanTool/               # Vulkan 高层工具（管线、纹理、特效）
├── Camera/                   # 相机系统（策略模式）
├── PhysicsBlock/             # 自研 2D 物理引擎
├── Audio/                    # 音频引擎（基于 SoLoud）
├── NetworkTCP/               # TCP 网络与状态复制（基于 libevent）
├── Character/                # 角色、NPC、群体、伤害提示
├── Arms/                     # 武器与粒子系统
├── BlockS/                   # 方块与像素纹理
├── InterfaceUI/              # ImGui 用户界面
├── Opcode/                   # 操作码系统
├── Tool/                     # 通用工具库（寻路、噪声、线程池等）
├── GameMods/                 # 游戏模式集合
│   ├── Configuration.h       # 引擎配置基类（持有所有子系统指针）
│   ├── GameMods.h            # 游戏模式抽象基类
│   ├── MazeMods/             # 迷宫模式
│   ├── TankTrouble/          # 坦克动荡
│   ├── PhysicsTest/          # 物理测试
│   ├── SoloudTest/           # 音效演示
│   ├── RadianceCascades/     # 辐射级联 GI
│   ├── FruitNinja/           # 水果忍者
│   ├── WFCTest/              # 波函数坍缩地图生成
│   ├── BlockWorld/           # 体素区块世界
│   └── UnlimitednessMapMods/ # 无限地图
├── shaders/                  # GLSL 着色器源码 + compile.bat
├── Resource/                 # 运行时资源（音乐、音效、字体、图片）
├── Environment/              # 第三方依赖源码
│   ├── glfw/                 # 窗口与输入
│   ├── libevent/             # 网络事件循环
│   ├── zlib/                 # 数据压缩
│   ├── spdlog/               # 日志
│   ├── Soloud/               # 音频
│   ├── imgui/                # 即时模式 UI
│   └── WaveFunctionCollapse/ # 过程化生成
└── MyApplication/            # Android Studio 工程
    └── app/src/main/cpp/native-lib.cpp  # JNI 桥接层
```

---

## 环境要求

### Windows 开发环境

- **操作系统**：Windows 10/11（x64）
- **编译器**：MSVC（Visual Studio 2019/2022，需支持 C++17）
- **构建工具**：CMake ≥ 3.12、Ninja
- **Vulkan SDK**：必需。用于头文件、`vulkan-1.lib` 链接库及 `glslangValidator` 着色器编译器。CMake 会自动搜索 `C:/VulkanSDK/*` 下的最新版本，也可通过 `VULKAN_SDK` 环境变量指定。
- **IDE**（推荐）：Visual Studio（含 C++ 桌面开发与 CMake 工具组件）或 VS Code + CMake Tools 插件

### Android 开发环境

- **操作系统**：Windows 10/11（用于着色器编译脚本 `compile.bat`）
- **Android Studio**：最新稳定版
- **NDK**：26.1.10909125（见 [build.gradle.kts](file:///c:/Github/PixelClean/MyApplication/app/build.gradle.kts)）
- **CMake**：3.22.1（Android 工程内置）
- **JDK**：17
- **minSdk**：26（Android 8.0）/ **targetSdk**：36
- **ABI**：`arm64-v8a`、`x86_64`
- **Vulkan SDK**：用于编译 GLSL 着色器为 SPIR-V（与 Windows 共用 `shaders/compile.bat`）

---

## 安装与配置步骤

### 一、Windows 平台构建

#### 1. 安装 Vulkan SDK

前往 [LunarG 官网](https://vulkan.lunarg.com/) 下载并安装 Vulkan SDK，默认安装至 `C:/VulkanSDK/<版本号>`。安装后确认 `glslangValidator.exe` 存在于 `Bin/` 目录下。

#### 2. 克隆仓库

```bash
git clone <仓库地址> PixelClean
cd PixelClean
```

#### 3. 配置与生成

**方式 A：Visual Studio（推荐）**

直接用 Visual Studio 打开项目根目录，VS 会自动识别 [CMakeSettings.json](file:///c:/Github/PixelClean/CMakeSettings.json) 并提供 `x64-Debug` / `x64-Release` 两个配置。选择配置后即可生成与调试。

**方式 B：命令行**

```powershell
# 在项目根目录执行
cmake -B out\build\x64-Release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build out\build\x64-Release
```

#### 4. 运行

构建产物位于 `out/build/<配置名>/` 目录下。CMake 会自动将以下文件拷贝至构建目录：
- `Info.ini`、`PhysicsBlock.ini`（配置文件）
- `Resource/`（音乐、字体、图片等资源目录）
- `shaders/`（编译后的 `.spv` 着色器）

直接运行 `PixelClean.exe` 即可。注意：Release 模式下使用 `/SUBSYSTEM:WINDOWS` 关闭控制台窗口；Debug 模式保留控制台用于查看日志输出。

### 二、Android 平台构建

#### 1. 安装环境

安装 Android Studio，通过 SDK Manager 安装 NDK `26.1.10909125` 与 CMake `3.22.1`。同时确保 Windows 端已安装 Vulkan SDK（用于着色器编译）。

#### 2. 打开工程

用 Android Studio 打开 [MyApplication/](file:///c:/Github/PixelClean/MyApplication) 目录，Gradle 会自动同步。

#### 3. 资源与着色器准备

构建前 Gradle 会自动执行以下任务（见 [build.gradle.kts](file:///c:/Github/PixelClean/MyApplication/app/build.gradle.kts)）：
- `compileShaders`：调用 `shaders/compile.bat` 编译 GLSL → SPIR-V
- `prepareAssets`：将 `Info.ini`、`PhysicsBlock.ini`、`Resource/`、字体、方块纹理拷贝至 `src/main/assets/`
- `copyShaders`：将 `.spv` 拷贝至 `src/main/assets/shaders/`

#### 4. 编译与运行

连接 Android 设备（需 Android 8.0+ 且支持 Vulkan），点击 Run 即可。支持 `arm64-v8a` 真机与 `x86_64` 模拟器。

---

## 使用指南

### 启动与主界面

启动程序后进入 ImGui 主界面，可在主界面选择游戏模式、进入设置界面调整参数，或进入多人界面配置联机。

### 通用操作

| 操作 | 按键 | 说明 |
|------|------|------|
| 移动 | `W` `A` `S` `D` | 前后左右移动（按键可在 `Info.ini` 配置） |
| 上升/下降 | `Space` / `Shift` | 3D 模式下垂直移动 |
| 退出/返回 | `ESC` | 返回上级菜单或退出当前模式 |
| 控制台 | `/` | 切换调试控制台 |
| 鼠标移动 | — | 视角控制（FPS 模式下） |
| 鼠标左键 | — | 攻击/交互 |
| 鼠标滚轮 | — | 缩放（2D 模式） |
| 切换鼠标模式 | `~` | 指针锁定/释放 |
| 数字键 1/2 | `1` `2` | 各模式自定义功能（如切换武器、重新生成地图） |

### 各游戏模式说明

- **迷宫**：WASD 探索迷宫，左键射击，支持多人联机对战。
- **坦克动荡**：在固定迷宫中驾驶坦克对战，子弹可反弹。
- **物理测试**：演示自研物理引擎的刚体、碰撞、GPU 计算等功能。
- **SoLoud 音效**：在建筑群中移动，体验遮挡、混响、回声等环境音效。
- **辐射级联 GI**：2D 全局光照演示，可用鼠标绘制光源。
- **水果忍者**：鼠标划动切割下落的水果。
- **WFC 测试**：按 `2` 重新生成波函数坍缩城市地图，按 `1` 切换周期性输出。
- **Minecraft 区块世界**：体素地形漫游，5×5×5 区块网格。
- **无限地图**：无限滚动地图，支持像素破坏、战争迷雾、AI 寻路。

### 多人联机

1. 在主界面进入"多人界面"。
2. 一方作为服务端启动（监听端口，默认 `25565`）。
3. 其他方作为客户端，输入服务端 IP 与端口连接。
4. 目前仅 **迷宫** 模式支持多人联机（见 [GameMods.h](file:///c:/Github/PixelClean/GameMods/GameMods.h) 中 `GameModsSupportsMultiplayer`）。

---

## 配置文件说明

### Info.ini

主配置文件，位于项目根目录，运行时由 [GlobalVariable.cpp](file:///c:/Github/PixelClean/GlobalVariable.cpp) 的 `Global::Read()` / `Global::Storage()` 读写。

```ini
[Window]
Height=1080          # 窗口高度
Width=1920           # 窗口宽度

[ServerTCP]
Port=25565           # 服务端监听端口

[ClientTCP]
IP=127.0.0.1         # 客户端连接的服务端 IP
Port=25565           # 客户端连接的服务端端口

[Set]
VulKanValidationLayer = 1   # Vulkan 验证层开关（Debug 建议 1，Release 建议 0）
Monitor = 1                 # 显示器选择
MonitorCompatibleMode = 0   # 显示器兼容模式
FullScreen = 0              # 全屏开关
MusicVolume = 2.0           # 音乐音量
SoundEffectsVolume = 0.2    # 音效音量
FontZoomRatio = 3.0         # 字体缩放比

[Key]
KeyW = W             # 前进键
KeyS = S             # 后退键
KeyA = A             # 左移键
KeyD = D             # 右移键
```

### PhysicsBlock.ini

物理调试可视化配置，控制各类物理辅助视图的显隐与颜色（RGBA 0~1 浮点）。

```ini
[Auxiliary]
ForceBool=1                    # 力矢量
SpeedBool=1                    # 速度矢量
PosBool=1                      # 位置标记
AngleBool=1                    # 角度标记
CollisionDropBool=1            # 碰撞点
SeparateNormalVectorBool=1     # 分离法向量
GridDividedBool=1              # 网格分割
CollisionDropToCenterOfGravityBool=1  # 碰撞点到质心连线
# 以下默认关闭：
CentreMassBool=0               # 质心
CentreShapeBool=0              # 质心形状
OldPosBool=0                   # 旧位置
OutlineBool=0                  # 轮廓
MaxOutlineCentreMassBool=0     # 最大轮廓质心
```

---

## 常见问题解答

### Q1：构建时报错 "glslangValidator not found"

**原因**：CMake 未找到 Vulkan SDK 或其中的着色器编译器。

**解决**：
1. 确认已安装 Vulkan SDK，且 `C:/VulkanSDK/<版本>/Bin/glslangValidator.exe` 存在。
2. 或手动设置环境变量 `VULKAN_SDK` 指向 SDK 安装目录。

### Q2：运行时崩溃，提示找不到 `.spv` 文件或资源

**原因**：着色器未编译，或资源未拷贝至运行目录。

**解决**：
- Windows：确认 CMake 配置阶段 `compile_shaders` 目标执行成功，且构建目录下存在 `shaders/*.spv`、`Resource/`、`Info.ini`、`PhysicsBlock.ini`。
- Android：确认 Gradle 的 `compileShaders`、`prepareAssets`、`copyShaders` 任务执行成功，检查 `src/main/assets/` 内容。

### Q3：Vulkan 验证层报错

**原因**：Debug 模式下 `VulKanValidationLayer = 1` 会启用验证层，输出详细错误。

**解决**：根据验证层日志修复代码；若仅为性能测试，可在 `Info.ini` 中设为 `0` 关闭。

### Q4：Android 端启动黑屏或闪退

**原因**：设备不支持 Vulkan，或 ABI 不匹配。

**解决**：
1. 确认设备 Android 版本 ≥ 8.0 且支持 Vulkan（`android.hardware.vulkan.level`）。
2. 真机使用 `arm64-v8a`，模拟器使用 `x86_64`。
3. 查看 logcat（标签 `PixelClean`）定位崩溃信息，可将 [DebugLog.h](file:///c:/Github/PixelClean/DebugLog.h) 中 `PIXEL_ENABLE_LOG` 设为 `1` 开启日志。

### Q5：多人联机无法连接

**原因**：端口被占用、防火墙拦截或 IP/端口配置错误。

**解决**：
1. 检查 `Info.ini` 中 `[ServerTCP].Port` 与 `[ClientTCP].IP/Port` 配置。
2. 确认服务端端口未被占用，防火墙放行。
3. 客户端与服务端需运行相同的游戏模式（仅迷宫支持联机）。

### Q6：中文显示乱码

**原因**：源文件编码或字体问题。

**解决**：项目使用 UTF-8 编码（MSVC 已加 `/utf-8` 选项），界面字体为 `Resource/Minecraft_AE.ttf`，确保该字体文件存在且支持中文。

### Q7：如何切换 Debug/Release 模式？

- **Visual Studio**：在配置下拉菜单选择 `x64-Debug` 或 `x64-Release`。
- **命令行**：`-DCMAKE_BUILD_TYPE=Debug` 或 `-DCMAKE_BUILD_TYPE=Release`。
- Release 模式启用 `/O2` 优化并关闭控制台窗口；Debug 模式保留控制台输出。

---

## 维护说明

### 构建系统

- Windows 主构建脚本：[CMakeLists.txt](file:///c:/Github/PixelClean/CMakeLists.txt)，采用 `aux_source_directory` 收集根目录与 Camera 目录源码，各子模块通过 `add_subdirectory` 加入并以静态库形式链接。
- Android 构建：[MyApplication/app/src/main/cpp/CMakeLists.txt](file:///c:/Github/PixelClean/MyApplication/app/src/main/cpp/CMakeLists.txt)，使用 `file(GLOB_RECURSE)` 递归收集各模块源码，编译为共享库 `libpixelclean.so`。
- 着色器编译：[shaders/compile.bat](file:///c:/Github/PixelClean/shaders/compile.bat)，逐个调用 `glslangValidator -V` 编译 `.vert/.frag/.comp/.geom` 为 `.spv`。新增着色器后需在此脚本中追加编译命令。

### 新增游戏模式

1. 在 [GameMods/](file:///c:/Github/PixelClean/GameMods) 下新建子目录，实现继承自 `GameMods` 的类。
2. 在 [GameMods.h](file:///c:/Github/PixelClean/GameMods.h) 的 `GameModsEnum`、`GameModsEnumName`、`GetGameModeStableId`、`GameModsSupportsMultiplayer` 中注册新模式。
3. 在 [application.h](file:///c:/Github/PixelClean/application.h) 中 include 新模式头文件。
4. 在 [CMakeLists.txt](file:///c:/Github/PixelClean/CMakeLists.txt) 中 `add_subdirectory` 并在 `target_link_libraries` 中链接。
5. Android 端在 [native CMakeLists.txt](file:///c:/Github/PixelClean/MyApplication/app/src/main/cpp/CMakeLists.txt) 中追加 `file(GLOB_RECURSE)` 与链接。

### 日志系统

- [DebugLog.h](file:///c:/Github/PixelClean/DebugLog.h) 提供 `LOGV/LOGD/LOGI/LOGW/LOGE/LOGF` 宏，通过 `PIXEL_ENABLE_LOG` 开关控制。
- Windows 下输出至 `printf`/`stderr`；Android 下输出至 logcat（标签 `PixelClean`）。
- 高级日志使用 [Tool/log.h](file:///c:/Github/PixelClean/Tool/log.h)（基于 spdlog 封装）。

### 资源管理

- 运行时资源位于 [Resource/](file:///c:/Github/PixelClean/Resource)：音乐（`Music/`，含 `.mp3`、`.mid`、`.sf2`）、材质图片（`Material/`）、ImGui 图片（`ImGuiImage/`）、GIF 纹理（`GifTexture/`）、字体（`Minecraft_AE.ttf`）。
- 资源路径宏定义于 [FilePath.h](file:///c:/Github/PixelClean/FilePath.h)，新增资源后在此添加路径宏。
- 方块纹理位于 [BlockS/Pixel图片/](file:///c:/Github/PixelClean/BlockS/Pixel图片)。

### 第三方依赖

所有第三方库源码内置于 [Environment/](file:///c:/Github/PixelClean/Environment) 目录，随项目一同编译，无需额外安装：

| 依赖 | 目录 | 用途 |
|------|------|------|
| GLFW | `Environment/glfw/` | 窗口创建与输入处理（Windows） |
| libevent | `Environment/libevent/` | TCP 网络事件循环 |
| zlib | `Environment/zlib/` | 数据压缩（网络传输） |
| spdlog | `Environment/spdlog/` | 日志库 |
| SoLoud | `Environment/Soloud/` | 音频引擎 |
| Dear ImGui | `Environment/imgui/` | 即时模式 UI |
| WaveFunctionCollapse | `Environment/WaveFunctionCollapse/` | 过程化地图生成 |

### 编码规范

- C++17 标准，MSVC 启用 `/utf-8` 保证源文件与执行字符集均为 UTF-8。
- 启用 `/wd4201` 允许匿名结构体/联合体（Microsoft 扩展）。
- 定义 `WIN32_LEAN_AND_MEAN` 与 `NOMINMAX` 避免 Windows 头文件污染。
- 跨平台代码使用 `#if defined(_WIN32)` / `#if defined(__ANDROID__)` 宏区分。

### 性能注意事项

- Release 模式启用 `/O2`（MSVC）或 `-O3`（GCC/Clang）优化。
- 物理引擎支持多线程（`ThreadPool`）与内存池（`MemoryPool`），通过 `MemoryPoolBool` 与 `ThreadPoolBool` 宏开关。
- 粒子系统使用 SSBO + Instanced Rendering，减少绘制调用。
- 部分 GameMod 存在已知性能瓶颈（如 [UnlimitednessMapMods](file:///c:/Github/PixelClean/GameMods/UnlimitednessMapMods/TECHNICAL_ANALYSIS.md) 中的全量更新问题），维护时请参考对应技术分析文档。

---

## 许可证与第三方库

本项目主体代码的许可证由项目所有者决定。各第三方依赖保留其原始许可证：

- **GLFW**：zlib/libpng License
- **libevent**：BSD 3-Clause License
- **zlib**：zlib License
- **spdlog**：MIT License
- **SoLoud**：zlib License（见 [Audio/LICENSE](file:///c:/Github/PixelClean/Audio/LICENSE)）
- **Dear ImGui**：MIT License（见 [Environment/imgui/LICENSE.txt](file:///c:/Github/PixelClean/Environment/imgui/LICENSE.txt)）
- **WaveFunctionCollapse**：MIT License

Vulkan SDK 由 LunarG 以独立许可协议提供，详见 [LunarG 官网](https://vulkan.lunarg.com/)。
