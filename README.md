# 素净 (PixelClean)

> 上帝视角，突突突，破坏艺术！
> 
> 一款基于 **Vulkan** 的 2D 像素风物理沙盒游戏引擎，支持像素级破坏、自研物理引擎、多人联机、多种游戏模式，并同时面向 Windows 和 Android 平台。

---

## 目录

- [项目概述](#项目概述)
- [功能特性](#功能特性)
- [技术栈](#技术栈)
- [项目结构](#项目结构)
- [核心模块说明](#核心模块说明)
  - [Vulkan 渲染层](#vulkan-渲染层)
  - [渲染工具层](#vulkan-工具层)
  - [PhysicsBlock 物理引擎](#physicsblock-物理引擎)
  - [武器系统 (Arms)](#武器系统-arms)
  - [角色系统 (Character)](#角色系统-character)
  - [游戏模式 (GameMods)](#游戏模式-gamemods)
  - [网络通信 (NetworkTCP)](#网络通信-networktcp)
  - [音效系统 (SoundEffect)](#音效系统-soundeffect)
  - [UI 界面 (InterfaceUI)](#ui-界面-interfaceui)
  - [GPU 通用计算 (GeneralCalculationGPU)](#gpu-通用计算-generalcalculationgpu)
  - [操作码系统 (Opcode)](#操作码系统-opcode)
  - [工具库 (Tool)](#工具库-tool)
- [着色器概览](#着色器概览)
- [构建与运行](#构建与运行)
  - [环境要求](#环境要求)
  - [Windows 构建](#windows-构建)
  - [Android 构建](#android-构建)
- [配置说明](#配置说明)
- [游戏模式详解](#游戏模式详解)
- [操作说明](#操作说明)
- [注意事项](#注意事项)

---

## 项目概述

**素净 (PixelClean)** 是一个从底层自研的 2D 游戏引擎项目。它围绕 **像素级破坏** 核心理念，实现了完整的游戏循环：玩家操控一个 16×16 像素的角色，使用多种武器破坏环境与敌人，所有交互均由自研物理引擎驱动，并通过 Vulkan 渲染管线呈现。

项目采用 **C++17** 编写，使用 **CMake** 构建系统，深度集成了 Vulkan、GLFW、ImGui、GLM、VMA 等工业级库，并自研了一套完整的 2D 刚体物理引擎（PhysicsBlock），涵盖碰撞检测、约束求解、多线程并行计算等高级特性。

---

## 功能特性

- **Vulkan 渲染管线** — 封装了从 Instance、Device、SwapChain、RenderPass、Pipeline 到 CommandBuffer 的完整 Vulkan 对象，支持多重采样抗锯齿和深度测试
- **自研 2D 物理引擎（PhysicsBlock）** — 支持矩形/圆形/粒子/线段/关节/组装体等多种物理形状，基于冲量迭代的碰撞求解，网格空间搜索，线程池并行
- **像素级破坏系统** — 每个 16×16 的游戏对象其纹理支持逐像素 Alpha 破坏，受击后像素消失，要害区域（中心 4×4）被击中将判定死亡
- **多种武器模式** — 手枪（单点）、霰弹（扇形散射）、机枪（连发），通过数字键 1/2 切换
- **粒子系统** — 基于 Vulkan 多线程 CommandBuffer 录制的高效粒子渲染，支持对象池复用
- **GPU 通用计算** — 战争迷雾（Compute Shader）、曼德勃罗集合渲染（Mandelbrot）、辐射级联全局光照（Radiance Cascades）
- **多种游戏模式** — 迷宫探索、坦克动荡、无限地牢、物理编辑器、SoLoud 音效演示、辐射级联 GI 演示
- **TCP 多人联机** — 基于 libevent 异步事件驱动，支持服务器/客户端模式，角色状态与像素破坏数据实时同步
- **3D 音效系统** — 基于 SoLoud，支持 MP3 播放、MIDI 播放（含 SoundFont 音色库）、空间音效（距离衰减、立体声声像、室内混响、墙壁遮挡滤波）
- **ImGui 集成界面** — 主菜单、游戏设置、监视器面板、性能分析、控制台命令输入
- **丰富的工具库** — A* / JPS 寻路、四叉树空间分割、柏林噪声、连续板块系统（无限地图）、内存池、线程池、无锁队列、计时器等
- **跨平台** — Windows 完整支持，Android 通过条件编译适配（含 JNI 和 Gradle 构建）

---

## 技术栈

| 类别 | 技术 | 用途 |
|------|------|------|
| 图形 API | [Vulkan 1.3](https://vulkan.lunarg.com/) | 底层渲染与 GPU 计算 |
| 窗口 | [GLFW](https://www.glfw.org/) | 跨平台窗口管理与输入 |
| UI | [Dear ImGui](https://github.com/ocornut/imgui) | 即时模式 GUI |
| 数学 | [GLM](https://github.com/g-truc/glm) | 矩阵/向量运算 |
| 内存分配 | [VMA](https://gpuopen.com/vulkan-memory-allocator/) | Vulkan 显存管理 |
| 日志 | [SpdLog](https://github.com/gabime/spdlog) | 结构化日志输出 |
| 网络 | [libevent](https://github.com/libevent/libevent) | 异步 TCP 事件驱动 |
| 压缩 | [zlib](http://www.zlib.net/) | 网络数据压缩 |
| 音频 | [SoLoud](https://solhsa.com/soloud/) | 音频引擎 |
| JSON | [nlohmann/json](https://github.com/nlohmann/json) | 配置/数据序列化 |
| 噪声 | PerlinNoise / FastNoiseLite | 程序化生成 |
| 图片 | [stb_image](https://github.com/nothings/stb) / lodepng | 纹理加载 / PNG 输出 |
| 构建 | CMake 3.12+ | 跨平台构建系统 |
| 编译 | MSVC / Clang | C++17 编译器 |

---

## 项目结构

```
PixelClean/
├── Application.*              # 应用入口、主循环、渲染同步
├── Camera.*                   # 2D/3D 摄像机（正交投影）
├── GlobalStructural.h         # 全局数据结构定义
├── GlobalVariable.*           # 全局变量与 INI 配置读写
├── FilePath.h                 # 资源路径宏定义
├── base.h                     # 基础头文件（平台头文件、GLM、GLFW/Vulkan 包含）
├── CMakeLists.txt             # 顶层构建脚本
├── Info.ini                   # 运行时配置文件
├── PhysicsBlock.ini           # 物理调试可视化配置文件
│
├── Vulkan/                    # Vulkan 底层封装（面向对象包装）
│   ├── Instance.*             # VkInstance 创建与验证层
│   ├── Device.*               # 物理/逻辑设备选择、VMA 初始化
│   ├── Window.*               # 跨平台窗口（GLFW / ANativeWindow）
│   ├── WindowSurface.*        # 平台 Surface 创建
│   ├── SwapChain.*            # 交换链、帧缓冲、深度/多重采样图像
│   ├── RenderPass.*           # 渲染通道与子通道管理
│   ├── Pipeline.*             # 图形管线（着色器、顶点输入、混合等）
│   ├── CommandPool.*          # 指令池
│   ├── CommandBuffer.*        # 指令缓冲录制与提交
│   ├── Buffer.*               # VMA 加速的 GPU 缓冲区
│   ├── Image.*                # VMA 加速的 GPU 图像
│   ├── DescriptorSet*.*       # 描述符集布局、池与集合
│   ├── Shader.*               # 着色器模块加载
│   ├── Sampler.*              # 纹理采样器
│   ├── Fence.*                # GPU 同步栅栏
│   └── Semaphore.*            # GPU 同步信号量
│
├── VulkanTool/                # 渲染高级工具
│   ├── PipelineS.*            # 管线集合管理器（7 种管线）
│   ├── CreatePipeline.*       # 各管线工厂函数
│   ├── Texture.*              # 图片纹理（文件/内存加载）
│   ├── PixelTexture.*         # 像素纹理（支持 CPU 端读写与 GPU 上传）
│   ├── TextureLibrary.*       # 纹理资源库（批量加载）
│   ├── VisualEffect.*         # 通用渲染对象（顶点/UV/索引/贴图 + 描述符 + 指令缓冲）
│   ├── AuxiliaryVision.*      # 调试辅助视觉（画线/点/圆，支持静态/动态/一次性）
│   ├── ShaderTexture.*        # 渲染到纹理（离屏渲染）
│   └── Calculate.*            # Compute Shader 管线封装
│
├── PhysicsBlock/              # ★ 自研 2D 物理引擎
│   ├── BaseStruct.hpp         # 基础类型定义（float/double 切换、网格块、碰撞信息）
│   ├── BaseDefine.h           # 全局宏（像素块边长、迭代次数、休眠阈值）
│   ├── BaseCalculate.hpp      # 数学工具（快速平方根、力分解、线段遮罩）
│   ├── BaseGrid.*             # 网格系统（像素块管理、AABB 碰撞检测）
│   ├── BaseOutline.*          # 轮廓提取算法
│   ├── PhysicsFormwork.hpp    # 物理对象基类（虚接口：位置/速度/质量/转动惯量）
│   ├── PhysicsWorld.hpp       # 物理世界（重力/风力、网格搜索、碰撞对管理、多线程并行）
│   ├── PhysicsShape.*         # 矩形刚体（含像素碰撞体积）
│   ├── PhysicsCircle.*        # 圆形刚体
│   ├── PhysicsParticle.*      # 粒子（无体积点）
│   ├── PhysicsLine.*          # 线段（无质量约束）
│   ├── PhysicsJoint.*         # 关节约束（距离/旋转）
│   ├── PhysicsJunction.*      # 绳索连接
│   ├── PhysicsAssembly.*      # 物理组装体（多刚体组合）
│   ├── PhysicsArbiter.*       # 碰撞解析器（不同形状间的冲量计算）
│   ├── PhysicsKinematic.*     # 运动学物体管理
│   ├── PhysicsTrigger.*       # 触发器系统（Enter/Stay/Exit 回调）
│   ├── PhysicsCollision.*     # 碰撞回调管理（层级掩码）
│   ├── PhysicsAngle.*         # 角度约束
│   ├── GridSearch.hpp         # 网格空间搜索加速
│   ├── EnergyConservation.hpp # 能量守恒监控
│   ├── MapStatic.*            # 静态地图（不可移动网格）
│   ├── MapDynamic.*           # 动态地图（可破坏网格）
│   ├── MapFormwork.hpp        # 地图基类
│   ├── ImGuiPhysics.*         # 物理调试 ImGui 面板
│   ├── MovePlate.h            # 可移动板块管理器
│   ├── BaseSerialization.hpp  # JSON 序列化支持
│   └── PhysicsBlockTypes.hpp  # 触发器/碰撞事件类型定义（LayerMask、Bounds、回调签名）
│
├── Arms/                      # 武器系统
│   ├── Arms.*                 # 武器管理（射击/子弹管理/武器切换）
│   ├── AttackMode.*           # 攻击模式（手枪/霰弹/机枪）
│   ├── ParticleSystem.*       # 粒子/子弹渲染系统（对象池 + 多线程 CommandBuffer 录制）
│   ├── ParticlesSpecialEffect.* # 粒子特效（爆炸碎片等）
│   └── GIF.*                  # GIF 动态纹理支持
│
├── Character/                 # 角色系统
│   ├── GamePlayer.*           # 玩家（16×16 像素碰撞体、像素破坏、要害检测）
│   ├── Crowd.*                # 人群管理（玩家/NPC 集合、超时淘汰）
│   ├── NPC.*                  # NPC 角色
│   ├── DamagePrompt.*         # 受伤方向提示
│   └── UVDynamicDiagram.*     # UV 动画碰撞体（8 帧循环动画）
│
├── GameMods/                  # 游戏模式
│   ├── GameMods.h             # 游戏模式基类虚接口
│   ├── Configuration.h        # 游戏配置结构体（持有渲染/物理/AI 所有组件）
│   ├── MazeMods/              # 迷宫模式（随机生成 + 探索）
│   ├── TankTrouble/           # 坦克动荡（固定迷宫对战）
│   ├── UnlimitednessMapMods/  # 无限地牢探索
│   ├── PhysicsTest/           # 物理编辑器（创建/编辑物理体、网格编辑模式）
│   ├── SoloudTest/            # 3D 音效演示（12 种音频技术）
│   └── RadianceCascades/      # 辐射级联全局光照演示（GPU Compute）
│
├── NetworkTCP/                # TCP 网络通信
│   ├── Server.*               # 服务器（libevent 监听/读写/事件）
│   ├── Client.*               # 客户端（连接/收发）
│   ├── ServerSynchronizeEvents.* # 服务器端同步逻辑
│   ├── ClientSynchronizeEvents.* # 客户端同步逻辑
│   ├── Struct.h               # 同步数据结构定义
│   └── StructTCP.h            # TCP 数据包头/压缩/角色同步结构体
│
├── SoundEffect/               # 音效系统
│   ├── SoundEffect.*          # SoLoud 封装（MP3/MIDI 播放、资源加载）
│   ├── soloud_midi.*          # MIDI 播放器（含 SoundFont）
│   ├── tsf.h / tml.h          # TinySoundFont / TinyMidiLoader
│   └── Resources/             # 音频资源（mp3 / mid 文件）
│
├── InterfaceUI/               # ImGui 界面
│   ├── Interface.*            # 界面系统（主菜单/设置/控制台/聊天/FPS 面板）
│   ├── GUI.h                  # ImGui 工具头文件
│   ├── ImGuiTexture.*         # ImGui 与 Vulkan 纹理桥接
│   ├── Font.h                 # 字体加载
│   └── Minecraft_AE.ttf       # Minecraft 风格字体
│
├── GeneralCalculationGPU/     # GPU 通用计算
│   ├── GPU.*                  # Vulkan Compute Pipeline 封装
│   └── lodepng.*              # PNG 编码库
│
├── Opcode/                    # 操作码（控制台命令系统）
│   ├── Opcode.*               # 操作码解析执行
│   └── OpcodeFunction.*       # 命令函数映射表（Trie 自动补全）
│
├── BlockS/                    # 方块系统（像素块纹理管理）
│   ├── Block.* / BlockS.*     # 方块数据结构
│   ├── PixelTextureS.*        # 像素纹理合集
│   ├── PixelS.h               # 像素定义
│   ├── PixelTool.py / .exe    # 像素图片预处理工具
│   └── Pixel图片/             # 方块纹理资源（Minecraft 风格）
│
├── Tool/                      # ★ 工具库（自研基础设施）
│   ├── Tool.*                 # 库入口 + FPS/计时/字符串/日志
│   ├── AStar.h                # A* 网格寻路
│   ├── JPS.h                  # 跳点搜索寻路
│   ├── Quadtree.h             # 四叉树空间分割
│   ├── ContinuousMap.h        # 连续内存哈希映射
│   ├── ContinuousData.h       # 连续动态数组
│   ├── ContinuousPlate.h      # 连续板块系统（无限地图）
│   ├── MovePlate.h            # 可移动板块（2D/3D）
│   ├── BlockData.h            # 区块化数据管理
│   ├── PileUp.h               # LIFO 栈容器
│   ├── Queue.h                # FIFO 环形队列
│   ├── MemoryPool.h           # 定长内存池
│   ├── ThreadPool.h           # 通用线程池
│   ├── MPMCQueue.h            # 多生产者多消费者无锁队列
│   ├── GenerateMaze.h         # 递归回溯迷宫生成
│   ├── PerlinNoise.*          # 柏林噪声
│   ├── FastNoiseLite.h        # 快速噪声生成器
│   ├── View.h                 # 视锥计算
│   ├── GridNavigation.h       # 网格距离场导航
│   ├── MinArea.h              # 最小面积覆盖
│   ├── Trie.h                 # 前缀树（命令补全）
│   ├── LimitUse.h             # 引用计数限制器
│   ├── Timer.h                # 高精度性能计时器
│   ├── Iterator.h             # 迭代器模板
│   ├── json.hpp               # nlohmann JSON 库
│   ├── sml.hpp                # Boost.SML 状态机库
│   └── cuckoohash_map.hpp     # 布谷鸟哈希表
│
├── shaders/                   # GLSL 着色器源码
│   ├── compile.bat            # 着色器编译脚本（glsl → SPIR-V）
│   ├── lessionShader.*        # 基础顶点/片段着色器
│   ├── GifFragShader.*        # GIF 帧动画着色器
│   ├── WarfareMist.comp       # 战争迷雾（Compute）
│   ├── DungeonWarfareMist.comp# 地牢迷雾（Compute）
│   ├── LineShader.*           # 线段着色器
│   ├── SpotShader.*           # 点着色器（含几何着色器）
│   ├── CircleShader.*         # 圆着色器（含几何着色器）
│   ├── DamagePrompt.*         # 受伤提示着色器
│   ├── UVDynamicDiagram.*     # UV 动画着色器（含几何着色器）
│   ├── Background.comp        # 背景 Compute
│   ├── GridBackground.comp    # 网格背景 Compute
│   ├── 2D_GI.comp             # 2D 全局光照 Compute
│   ├── RC_SDF.comp            # 辐射级联 SDF Compute
│   ├── RC_Cascade.comp        # 辐射级联追踪 Compute
│   └── RC_Display.comp        # 辐射级联合成 Compute
│
├── MyApplication/             # Android 端（Gradle + Kotlin/JNI）
│   ├── app/src/main/
│   │   ├── cpp/               # JNI 原生代码
│   │   │   ├── native-lib.cpp # JNI 入口
│   │   │   ├── VulkanApp.*    # Android Vulkan 适配
│   │   │   └── Platform/Android/ # Android 平台实现 + GLFW 桩
│   │   └── java/com/pixelclean/
│   │       └── MainActivity.kt # Android 主 Activity
│   ├── build.gradle.kts       # Gradle 构建脚本
│   └── gradle/                # Gradle Wrapper
│
└── Environment/               # 第三方库源码
    ├── glfw/                  # GLFW 窗口库
    ├── libevent/              # 异步网络事件库
    ├── zlib/                  # 数据压缩库
    ├── spdlog/                # 日志库
    ├── Soloud/                # 音频引擎
    └── imgui/                 # 即时模式 GUI
```

---

## 核心模块说明

### Vulkan 渲染层

位于 `Vulkan/` 目录，将 Vulkan C API 封装为面向对象的 C++ 类，隐藏了大量底层细节：

- **实例与设备** — `Instance` 支持验证层检测与调试信使；`Device` 通过评分机制自动选取最优 GPU，并集成 VMA 显存管理器
- **交换链** — `SwapChain` 管理多重缓冲、多采样抗锯齿、深度缓冲，自动选择最佳表面格式与呈现模式
- **渲染通道** — `RenderPass` 支持自定义 SubPass 与依赖关系，内置 final + multiSample + depth 三附件体系
- **管线** — `Pipeline` 封装从着色器模块到管线布局的完整构建流程
- **指令系统** — `CommandPool` / `CommandBuffer` 支持主/次指令缓冲、一级/二级录制模式
- **同步原语** — `Fence` / `Semaphore` 封装 GPU 同步

### Vulkan 工具层

位于 `VulkanTool/` 目录，提供更高层次的渲染抽象：

- **PipelineS** — 管线集合管理器，统一管理 7 种渲染管线（主渲染/画线/画点/画圆/GIF/受伤提示/UV 动画），支持全局重建
- **PixelTexture** — 支持 CPU 端直接写入和 GPU 延迟上传的像素纹理，是像素破坏系统的核心
- **AuxiliaryVision** — 强大的调试可视化工具，支持动态线段（通过指针跟随物体）、静态线段/点/圆、一次性绘制，3 种 API 全覆盖
- **TextureLibrary** — 批量纹理加载器，支持 GIF 序列帧和普通素材的目录级加载

### PhysicsBlock 物理引擎

位于 `PhysicsBlock/` 目录，这是一个完全自研的 **2D 刚体物理引擎**，具备以下特性：

- **多种物理形状**：矩形刚体 (PhysicsShape)、圆形 (PhysicsCircle)、粒子点 (PhysicsParticle)、线段 (PhysicsLine)、关节 (PhysicsJoint)、绳索 (BaseJunction)、组装体 (PhysicsAssembly)
- **碰撞检测**：基于网格空间搜索 (GridSearch) 的宽相位 + 精确碰撞检测，支持 12 种碰撞对组合的内联优化解析器
- **冲量迭代求解**：多轮迭代冲量求解，迭代次数根据物体数量自适应调整（`ApplyImpulseAdd`）
- **多线程并行**：基于线程池 (ThreadPool) 的碰撞对并行处理，带互斥锁保护
- **内存池优化**：通过宏生成 (AuxiliaryArbiter) 为每种碰撞对分配独立内存池，减少动态分配
- **碰撞回调系统**：Enter/Stay/Exit 三级回调，支持 32 位层级掩码 (LayerMask)
- **触发器系统**：AABB 触发区域，Enter/Stay/Exit 回调
- **运动学物体**：不受物理驱动但可推动其他物体的刚体
- **静态/动态地图**：基于网格的地图系统，支持可破坏的动态格子（含血量/质量/距离场/方向场）
- **能量守恒监控**：全局能量追踪，支持能量损失系数配置
- **睡眠机制**：静止物体自动休眠（StaticNum 阈值）
- **序列化**：基于 nlohmann/json 的物理世界序列化/反序列化
- **可视化调试**：ImGui 面板 + AuxiliaryVision 力/速度/分离向量/质心绘制

### 武器系统 (Arms)

- **3 种攻击模式**：手枪（单点精确射击）、霰弹（扇形散射 5 发）、机枪（高射速连发）
- **射击流程**：创建子弹物理碰撞体 (PixelCollision) → 从粒子池获取渲染模型 → 设置速度/角度/摩擦 → 上传矩阵到 GPU → 添加到物理世界
- **子弹管理**：每帧同步渲染位置与物理位置，命中后触发粒子特效并回收资源
- **特效系统**：ParticlesSpecialEffect 管理爆炸碎片粒子效果

### 角色系统 (Character)

- **GamePlayer** — 16×16 像素的玩家角色，物理碰撞体驱动移动，支持像素级破坏：
  - 像素破坏：直接操作 GPU 贴图的 Alpha 通道，逐像素消除
  - 要害检测：中心 4×4 区域被击中即判定战斗死亡
  - 受击闪烁状态 (StrikeState) 传入着色器
- **Crowd** — 基于 socket 键值的玩家/NPC 集合管理，支持超时断线自动淘汰
- **DamagePrompt** — 环形缓冲的受伤方向指示器，按角度显示受击来源
- **UVDynamicDiagram** — 可播放 8 帧 3×3 UV 动画的物理碰撞体，同时管理动画帧与物理碰撞网格同步

### 游戏模式 (GameMods)

所有游戏模式继承自 `GameMods` 虚基类，通过 `Configuration` 持有所有核心组件指针：

| 模式 | 枚举值 | 描述 |
|------|--------|------|
| **迷宫模式** | `Maze_` | 随机迷宫生成 + 玩家探索，含 UV 动画图元 |
| **坦克动荡** | `TankTrouble_` | 固定迷宫中的坦克对战 |
| **无限地牢** | `PhysicsTest_` | 无限扩展的地牢探索，基于连续板块系统 |
| **物理测试** | `Infinite_` | 完整物理编辑器：创建/编辑物理体、网格编辑模式、画笔/橡皮擦 |
| **音效测试** | `SoloudTest_` | 3D 空间音效演示：室内/室外检测、墙壁遮挡、混响、回声、低通滤波等 12 项音频技术 |
| **辐射级联 GI** | `RadianceCascades_` | 基于 GPU Compute Shader 的 2D 实时全局光照演示：SDF→5 层级联→色调映射 |

### 网络通信 (NetworkTCP)

- **传输协议**：基于 libevent 异步事件驱动的自定义 TCP 协议，带 zlib 数据压缩
- **数据结构**：`DataHeader` (标签+大小) → `SynchronizeData` → `RoleSynchronizationData` (位置/角度/子弹/像素事件/聊天/破碎)
- **Server** — 单例模式，监听连接 → bufferevent 异步读写 → 同步广播 → 超时检测
- **Client** — 单例模式，连接服务器 → 收发同步数据 → 反序列化更新本地状态
- **同步事件**：ServerSynchronizeEvents / ClientSynchronizeEvents 实现双向数据同步

### 音效系统 (SoundEffect)

- **SoLoud 引擎封装** — 单例模式，自动加载 `./Resources/` 下所有 `.mp3` 和 `.mid` 文件
- **MIDI 支持** — 基于 SoundFont 音色库合成（TinySoundFont + TinyMidiLoader）
- **播放控制** — Play/Pause/Stop/SetVolume，支持循环、音量、声道控制
- **自动回收** — 每帧检测无效 handle 并自动移除
- **原始计划** — 代码中保留了低通/高通/带通/回声/混响等滤波器使用注释

### UI 界面 (InterfaceUI)

- **主菜单**：左右切换游戏模式 → 开始游戏 / 多人游戏 / 游戏设置 / 退出
- **设置界面**：字体缩放比、音量（音乐/音效）、网络（端口/IP）、Vulkan 验证层、监视器、全屏、键位自定义
- **监视器面板**：相机位置、鼠标角度、攻击模式、剩余粒子数、FPS 趋势图、帧耗时明细表+堆图
- **控制台**：`/` 前缀命令输入，支持 Opcode 自动补全 (Trie 树) 和注释提示
- **聊天系统**：底部消息滚动显示，10 秒自动消失，多人模式下网络分发
- **深度集成**：深绿色风格主题 + Minecraft_AE 像素字体
- **缩放适配**：基于 1920×1080 基准分辨率自适应缩放

### GPU 通用计算 (GeneralCalculationGPU)

- **Compute Pipeline 封装** — 描述符集 → 着色器 → 管线布局 → 计算管线 → Dispatch → 回读
- **Mandelbrot 演示** — 存储缓冲计算曼德勃罗集合 → 回读到 CPU → lodepng 编码输出 PNG
- **战争迷雾** — WarfareMist.comp / DungeonWarfareMist.comp 基于玩家位置的 2D 迷雾效果
- **背景渲染** — Background.comp / GridBackground.comp 程序化网格背景

### 操作码系统 (Opcode)

- **命令映射**：Trie 树实现操作码名自动补全 + `map<string, function>` 命令分发
- **支持的命令**：`AddNPC` / `AddNPCPos` / `AddNPCS` / `KillAllNPC` / `ReplaceMap` / `SetMistSwitch` / `SetPipelineLinesMode` / `ReplaceArms`
- **控制台集成**：控制台中输入 `/命令` 即可执行，空格分隔参数，自动匹配最近命令

### 工具库 (Tool)

位于 `Tool/` 目录，是项目自研的 C++ 游戏开发基础库，全部为 header-only 设计，按需通过宏开关启用：

| 类别 | 组件 | 功能 |
|------|------|------|
| **寻路** | AStar.h, JPS.h | 8 方向网格寻路（A* 经典 / JPS 跳点搜索优化） |
| **空间结构** | Quadtree.h, MinArea.h | 区域查询、相交检测、最小面积覆盖 |
| **容器** | ContinuousMap, ContinuousData, BlockData, PileUp, Queue, Trie | 连续内存映射表、动态数组、区块数据、栈、环形队列、前缀树 |
| **内存** | MemoryPool.h, LimitUse.h | 定长块内存池、引用计数限制 |
| **并发** | ThreadPool.h, MPMCQueue.h | 通用线程池、无锁多生产者多消费者队列 |
| **板块系统** | ContinuousPlate.h, MovePlate.h | 无限地图板块管理（动态生成/回收）、2D/3D 可移动板块 |
| **噪声** | PerlinNoise, FastNoiseLite | 柏林噪声、快速多类型噪声生成 |
| **迷宫** | GenerateMaze.h | 递归回溯迷宫生成器 |
| **导航** | GridNavigation.h, View.h | SDF 风格网格距离场、视锥计算 |
| **调试** | Timer.h, log.h, Tool.h | 嵌套性能计时、分级日志、FPS 统计 |
| **第三方** | json.hpp, sml.hpp, cuckoohash_map.hpp | JSON 序列化、状态机、并发哈希表 |

---

## 着色器概览

| 着色器文件 | 类型 | 功能 |
|-----------|------|------|
| `lessionShader.*` | Vert/Frag | 基础 2D 精灵渲染（顶点色 + 纹理采样） |
| `GifFragShader.*` | Vert/Frag | GIF 帧动画渲染（UV 偏移控制帧切换） |
| `LineShader.*` | Vert/Frag | 线段渲染 |
| `SpotShader.*` | Vert/Geom/Frag | 点精灵渲染（几何着色器将点展开为四边形） |
| `CircleShader.*` | Vert/Geom/Frag | 圆心精灵渲染（几何着色器将圆心展开为圆环顶点） |
| `DamagePrompt.*` | Vert/Frag | 受伤闪红渐消效果 |
| `UVDynamicDiagram.*` | Vert/Geom/Frag | 像素 UV 动画 + 动画帧 uniform（几何着色器做实例化展开） |
| `Background.comp` | Compute | 全屏背景纹理生成 |
| `GridBackground.comp` | Compute | 网格背景生成 |
| `WarfareMist.comp` | Compute | 战争迷雾（基于玩家位置的视野计算） |
| `DungeonWarfareMist.comp` | Compute | 地牢迷雾（结合迷宫数据的视野计算） |
| `2D_GI.comp` | Compute | 2D 全局光照 |
| `RC_SDF.comp` | Compute | 辐射级联：鼠标笔画 → 2D 符号距离场 |
| `RC_Cascade.comp` | Compute | 辐射级联：5 层级联光线追踪（乒乓缓冲） |
| `RC_Display.comp` | Compute | 辐射级联：色调映射合成输出 |

所有着色器通过 `shaders/compile.bat` 使用 `glslangValidator` 编译为 SPIR-V 字节码。

---

## 构建与运行

### 环境要求

- **操作系统**：Windows 10/11（主要开发平台），Android 8.0+
- **Vulkan SDK**：1.3+（推荐安装到默认路径 `C:/VulkanSDK/`）
- **CMake**：3.12+
- **编译器**：Visual Studio 2022 (MSVC) 或 Clang
- **构建工具**：Ninja（推荐）或 MSBuild
- **Android（可选）**：Android Studio + NDK + Gradle

### Windows 构建

```bash
# 1. 克隆项目
git clone <repository-url>
cd PixelClean

# 2. 配置 CMake（推荐使用 Ninja 构建器）
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug

# 3. 编译
cmake --build build

# 4. 运行（二进制输出到 build/ 目录）
./build/PixelClean.exe
```

**Release 模式构建**（关闭控制台窗口，优化编译）：
```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

> **注意**：CMake 配置阶段会自动：
> - 检测并选择最新的 Vulkan SDK
> - 将 `Info.ini` 和 `PhysicsBlock.ini` 拷贝到输出目录
> - 将 `SoundEffect/Resources/` 音频文件拷贝到输出目录
> - 将 `Minecraft_AE.ttf` 字体文件拷贝到输出目录
> - 调用 `glslangValidator` 将全部 GLSL 着色器编译为 SPIR-V

### Android 构建

Android 项目位于 `MyApplication/` 目录，使用 **Gradle** 构建系统：

```bash
cd MyApplication

# Windows 环境
./gradlew.bat assembleDebug

# Linux/Mac 环境
./gradlew assembleDebug
```

然后通过 Android Studio 打开 `MyApplication/` 目录运行或调试。

> Android 端使用生命周期分步初始化模式：`initBeforeSurface()` → `initAfterSurface(ANativeWindow*)` → 循环 `frameStep()`。

---

### PhysicsBlock.ini（物理调试可视化配置）

控制物理引擎调试绘制中各种辅助元素的颜色和开关（质心、碰撞点、力向量、速度向量、分离法向量、轮廓、网格线等）。

---

## 游戏模式详解

### 迷宫模式 (Maze)

- 使用 `GenerateMaze` 递归回溯算法生成随机迷宫
- 玩家在其中移动探索，可进行战斗

### 坦克动荡 (TankTrouble)

- 固定布局的战斗迷宫
- 适合对战场景

### 无限地牢 (UnlimitednessMapMods)

- 基于 `ContinuousPlate` / `MovePlate` 板块系统实现的无限扩展地牢
- 使用程序化生成（PerlinNoise）动态创建地牢房间
- 战争迷雾效果（DungeonWarfareMist Compute Shader）

### 物理编辑器 (PhysicsTest)

- 界面中右键创建物理对象（矩形/圆/点/线段）
- 网格编辑模式：逐格编辑物理形状，画笔/橡皮擦/质量设置
- 所有物理对象实时参与物理模拟
- 摄像机可平移/缩放

### 音效演示 (SoloudTest)

- 64×40 格子的侧视图室内场景（A 楼 + B 楼 + 庭院）
- 7 个环境声源（风声、音乐、水滴、嗡嗡声、机器、雨声、电音）
- 实时空间音效：室内/室外检测、墙壁遮挡模拟、混响、回声、低通滤波、距离衰减、立体声声像
- 程序化脚步声（SFXR）
- 碰撞冲击声
- 三条音频总线（环境/音乐/音效）独立控制

### 辐射级联 GI (RadianceCascades)

- 基于 Shadertoy "Radiance Cascades" 算法的 2D 实时全局光照
- 鼠标绘制 → SDF 化 → 5 层级联双向光线追踪 → 色调映射合成
- 每层 16 方向 × 4ⁿ 采样
- 全部在 GPU Compute Shader 上完成
- 运行时参数可调：画笔半径、光滑半径、色调映射指数、光线步进最大步数

---

## 操作说明

| 按键 | 功能 |
|------|------|
| `W/A/S/D` | 移动角色（可在设置中自定义） |
| 鼠标左键 | 开火（当前武器模式） |
| 鼠标右键 | 寻路（A* 路径规划） |
| 鼠标滚轮 | 缩放摄像机 |
| `1` | 切换武器（上一个攻击模式） |
| `2` | 切换武器（下一个攻击模式） |
| `Space` | 特殊动作 |
| `ESC` | 暂停/打开游戏内菜单 |
| `` ` `` / `~` | 切换鼠标捕获模式 |
| `/` | 打开控制台命令输入 |
| `C`（RadianceCascades） | 清空画板（辐射级联模式） |

---
