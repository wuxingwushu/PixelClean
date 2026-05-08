# PixelClean（素净）项目安卓平台迁移技术复杂度评估报告

> **报告日期：** 2026年5月8日  
> **评估对象：** PixelClean（素净）2D 上帝视角射击破坏游戏  
> **目标平台：** Android（支持 Vulkan 的 Android 7.0+，API Level 24+）  
> **源平台：** Windows Desktop（MSVC + CMake + C++17）

---

## 目录

1. [项目现状分析](#1-项目现状分析)
2. [技术栈兼容性评估](#2-技术栈兼容性评估)
3. [第三方库与功能替换清单](#3-第三方库与功能替换清单)
4. [迁移难度分级](#4-迁移难度分级)
5. [系统功能模块迁移分析](#5-系统功能模块迁移分析)
6. [资源需求估算](#6-资源需求估算)
7. [时间周期预测](#7-时间周期预测)
8. [迁移策略与实施步骤](#8-迁移策略与实施步骤)
9. [风险分析与应对策略](#9-风险分析与应对策略)
10. [关键成功因素](#10-关键成功因素)

---

## 1. 项目现状分析

### 1.1 项目概览

| 属性 | 描述 |
|------|------|
| **项目名称** | PixelClean（素净） |
| **游戏类型** | 2D 上帝视角射击 + 像素级破坏艺术 |
| **开发语言** | C++17 |
| **构建系统** | CMake 3.12+ |
| **图形 API** | Vulkan（原生 Vulkan C API） |
| **编译器** | MSVC (Windows) |
| **当前平台** | Windows Desktop (x64) |
| **代码行数估算** | 约 30,000 ~ 50,000 行 C++ |

### 1.2 核心模块架构

```
PixelClean
├── Vulkan/                 # Vulkan 渲染引擎抽象层（Instance, Device, SwapChain, Pipeline,
│                             RenderPass, CommandPool, CommandBuffer, Buffer, Descriptor, etc.）
├── VulkanTool/             # 渲染管线管理、视觉效果、辅助视觉、纹理库
├── PhysicsBlock/           # 自研 2D 像素网格刚体物理引擎
│                             （粒子、角度、形状、圆、线段、关节、约束、四叉树搜索）
├── Physics/                # SquarePhysics 简化物理系统
├── GameMods/               # 游戏模式管理系统
│   ├── MazeMods/           #   迷宫模式
│   ├── TankTrouble/        #   坦克动荡模式
│   ├── UnlimitednessMapMods/#   无限地图模式
│   ├── PhysicsTest/        #   物理测试
│   ├── SoloudTest/         #   音效测试
│   └── RadianceCascades/   #   辐射级联 GI 测试
├── Character/              # 角色系统（GamePlayer, NPC, Crowd）
├── Arms/                   # 武器系统（粒子枪, 子弹管理, 攻击模式）
├── BlockS/                 # 区块系统
├── NetworkTCP/             # TCP 网络联机（Client/Server, libevent 异步 I/O）
├── SoundEffect/            # 音效系统（SoLoud 封装）
├── InterfaceUI/            # ImGui 游戏界面
├── GeneralCalculationGPU/  # GPU 通用计算（Mandelbrot 等）
├── Opcode/                 # 操作码指令系统
├── Tool/                   # 工具集（线程池, 内存池, MPMC队列, 柏林噪声, A*, JPS寻路）
└── Environment/            # 第三方库集合
    ├── glfw/               #   窗口管理
    ├── libevent/           #   异步网络事件库
    ├── zlib/               #   数据压缩
    ├── spdlog/             #   日志库
    ├── Soloud/             #   音频引擎
    └── imgui/              #   即时模式 GUI
```

### 1.3 Shader 清单

| 着色器文件 | 类型 | 用途 |
|-----------|------|------|
| `lessionShader.vert/.frag` | Vert/Frag | 主渲染着色器 |
| `GifFragShader.vert/.frag` | Vert/Frag | GIF 帧动画渲染 |
| `LineShader.vert/.frag` | Vert/Frag | 线段渲染 |
| `SpotShader.vert/.frag/.geom` | Vert/Frag/Geom | 点精灵渲染 |
| `CircleShader.vert/.frag/.geom` | Vert/Frag/Geom | 圆形渲染 |
| `DamagePrompt.vert/.frag` | Vert/Frag | 受伤提示效果 |
| `UVDynamicDiagram.vert/.frag/.geom` | Vert/Frag/Geom | UV 动态图渲染 |
| `WarfareMist.comp` | Compute | 迷雾效果 |
| `DungeonWarfareMist.comp` | Compute | 地牢迷雾效果 |
| `Background.comp` | Compute | 背景计算 |
| `GridBackground.comp` | Compute | 网格背景计算 |
| `2D_GI.comp` | Compute | 2D 全局光照 |
| `RC_SDF.comp` | Compute | 辐射级联 SDF |
| `RC_Cascade.comp` | Compute | 辐射级联计算 |
| `RC_Display.comp` | Compute | 辐射级联显示 |

---

## 2. 技术栈兼容性评估

### 2.1 核心 API 兼容性总览

| 技术/库 | Windows 依赖 | Android 兼容性 | 评估 |
|----------|-------------|---------------|------|
| **Vulkan API** | Vulkan SDK | ✅ Android 原生支持（API 24+） | **完全兼容** |
| **GLSL → SPIR-V** | `glslangValidator.exe` | ✅ `glslangValidator` 可跨平台 / `glslc` | **工具链替换** |
| **GLFW** | Win32 API 后端 | ❌ 不官方支持 Android | **需替换** |
| **ImGui** | `imgui_impl_glfw` + `imgui_impl_vulkan` | ⚠️ 需替换后端 | **后端替换** |
| **GLM** | 纯头文件（数学库） | ✅ 无平台依赖 | **完全兼容** |
| **SoLoud** | miniaudio / WinMM | ✅ 有 Android/OpenSLES 后端 | **需配置** |
| **libevent** | IOCP / select | ✅ 有 Android/Linux 后端（epoll） | **需配置** |
| **zlib** | 标准 C | ✅ 无平台依赖 | **完全兼容** |
| **spdlog** | 标准 C++ | ✅ 无平台依赖 | **完全兼容** |
| **lodepng** | 纯头文件 | ✅ 无平台依赖 | **完全兼容** |

### 2.2 平台特定 API 使用统计

项目中共发现以下 Windows 平台特定代码：

| 位置 | API 调用 | 用途 | 影响 |
|------|---------|------|------|
| [base.h](file:///e:/Physics/PixelClean-main/base.h#L37-L39) | `#include <Windows.h>` + `#pragma comment(lib, "ws2_32.lib")` | 全局 Windows 头文件 + 网络库链接 | 需条件编译隔离 |
| [main.cpp](file:///e:/Physics/PixelClean-main/main.cpp#L4-L6) | `#include <Windows.h>` + `SetConsoleOutputCP(CP_UTF8)` | 控制台 UTF-8 编码设置 | 需移除或用 Logcat 替代 |
| [Window.cpp](file:///e:/Physics/PixelClean-main/Vulkan/Window.cpp#L98) | `GetWindowRect(GetDesktopWindow(), ...)` | 获取桌面分辨率用于全屏 | 需替换为 Android DisplayMetrics |
| `resource.rc` | Windows 资源文件 | 设置 EXE 图标 | Android 需用 `AndroidManifest.xml` |
| `compile.bat` | Windows 批处理 | 编译 Shader 脚本 | 需替换为 Shell 脚本或 Gradle Task |

**总体影响：** 平台特定代码量较小（< 20 处），条件编译隔离成本低。

---

## 3. 第三方库与功能替换清单

### 3.1 窗口与输入管理：GLFW → Android Native App Glue + 自定义

| 项目 | 源方案 | Android 替换方案 | 难度 |
|------|--------|-----------------|------|
| **窗口创建** | `glfwCreateWindow()` | `ANativeWindow` (由 `android_main` 提供) | ⭐⭐ |
| **事件循环** | `glfwPollEvents()` | Android App 主循环（`ALooper_pollAll` / NativeActivity） | ⭐⭐ |
| **鼠标输入** | `glfwSetCursorPosCallback` | Android 触摸事件 (`AInputQueue`) | ⭐⭐⭐ |
| **键盘输入** | `glfwGetKey()` | 虚拟摇杆 / 外接键盘 (`AInputQueue`) | ⭐⭐⭐ |
| **滚轮事件** | `glfwSetScrollCallback` | 双指捏合 (Pinch-to-Zoom) | ⭐⭐⭐ |
| **窗口大小** | `glfwSetFramebufferSizeCallback` | `APP_CMD_WINDOW_RESIZED` | ⭐⭐ |
| **全屏切换** | `glfwSetWindowMonitor` | Android 默认全屏，无需特殊处理 | ⭐ |

**详细方案：**

```cpp
// 替换方案：通过 android_native_app_glue 获取 NativeWindow
// NDK 提供的 android_native_app_glue.h 可直接使用

#include <android_native_app_glue.h>

void android_main(struct android_app* state) {
    // 替代 GLFW 窗口 -> 使用 ANativeWindow
    ANativeWindow* window = state->window;
    
    // 替代 GLFW 事件 -> 使用 AInputQueue
    AInputQueue* inputQueue = state->inputQueue;
    
    // 替代 GLFW 主循环
    while (true) {
        int events;
        struct android_poll_source* source;
        while (ALooper_pollAll(0, nullptr, &events, (void**)&source) >= 0) {
            if (source) source->process(state, source);
        }
        // 渲染帧
        renderFrame();
    }
}
```

### 3.2 ImGui 后端：imgui_impl_glfw → imgui_impl_android

| 源文件 | Android 替换 | 状态 |
|--------|-------------|------|
| `imgui_impl_glfw.cpp` | `imgui_impl_android.cpp` | ⚠️ **官方无 Android 后端，需自行实现** |
| `imgui_impl_vulkan.cpp` | `imgui_impl_vulkan.cpp` | ✅ 可复用（需适配 Surface 创建） |

**ImGui Android 后端实现要点：**
- 触摸事件映射到 ImGui 鼠标事件（`ImGuiIO::AddMousePosEvent`, `AddMouseButtonEvent`）
- 屏幕键盘集成（`SDL_StartTextInput` 替代方案或自定义 IME）
- 字体缩放适配不同 DPI 屏幕
- 已有 `FontZoomRatio` 全局变量，可复用

### 3.3 Shader 编译工具链

| 源方案 | Android 替换 |
|--------|-------------|
| `compile.bat` + `glslangValidator.exe` | Gradle Task + `glslc` (Shaderc) 或 CMake 自定义命令 |
| MSVC Windows 批处理 | NDK 交叉编译环境 |

**推荐方案：** 在 CMakeLists.txt 中使用 `find_program(glslc)` 并在构建阶段自动编译 SPIR-V，而非依赖 `.bat` 脚本。

### 3.4 网络库：libevent（可保留，需重新编译）

| 项目 | 说明 |
|------|------|
| **当前** | libevent 源码在项目中，通过 CMake `add_subdirectory` 编译 |
| **Android** | libevent 支持 Android/Linux 的 epoll 模型，可直接交叉编译 |
| **风险** | `evthread_win32.c` 的线程 API 需要 `pthread` 替代 |

**注意：** [base.h](file:///e:/Physics/PixelClean-main/base.h#L38-L39) 中的 `#pragma comment(lib, "ws2_32.lib")` 是 Windows 套接字库，Android 使用标准 POSIX socket API（`<sys/socket.h>`），需移除该链接指令。

### 3.5 音效系统：SoLoud（可保留，需配置 Android 后端）

| 项目 | 说明 |
|------|------|
| **当前后端** | miniaudio（自动选择 WASAPI/WinMM 等） |
| **Android 后端** | SoLoud 支持 OpenSLES / AAudio / miniaudio（Android 分支） |
| **配置要求** | 编译时定义 `WITH_OPENSLES` 或 `WITH_AAUDIO` |
| **MIDI 支持** | SoundFont 方式可保留（需 .sf2 文件打包到 APK assets） |

---

## 4. 迁移难度分级

### 4.1 按模块难度分级

| 难度 | 模块 | 工作量 (人天) | 说明 |
|------|------|:---:|------|
| ⭐ **容易** | GLM 数学库 | 0 | 纯头文件，零改动 |
| ⭐ **容易** | spdlog 日志 | 1 | 将 console sink 改为 Android logcat sink |
| ⭐ **容易** | zlib 压缩 | 0.5 | 标准 C，交叉编译即可 |
| ⭐ **容易** | lodepng | 0 | 纯头文件 PNG 加载 |
| ⭐ **容易** | Tool 工具集 | 2 | 内存池/线程池/std::thread 在 NDK 均可用 |
| ⭐⭐ **中等** | Shader 编译流程 | 2 | 批处理→CMake/NDK 交叉编译  |
| ⭐⭐ **中等** | Vulkan 渲染层 | 3 | 核心 API 一致，Surface/交换链需适配 |
| ⭐⭐ **中等** | SoLoud 音频 | 3 | 后端切换 + assets 路径适配 |
| ⭐⭐ **中等** | 网络 TCP | 3 | 移除 WinSock，改用 POSIX socket + libevent epoll |
| ⭐⭐⭐ **较高** | 窗口/输入系统 | 5 | GLFW 完全替换，触摸事件映射至原游戏逻辑 |
| ⭐⭐⭐ **较高** | ImGui 界面 | 5 | 需自行实现 Android 后端，触摸/键盘/字体适配 |
| ⭐⭐⭐ **较高** | 游戏操控逻辑 | 4 | 键鼠→触屏操控重构，虚拟摇杆设计 |
| ⭐⭐⭐⭐ **高** | APK 打包集成 | 3 | NDK + Gradle 配置，assets 资源打包 |
| ⭐ **兼容** | PhysicsBlock 物理 | 0.5 | 纯 C++ 逻辑，无平台依赖，仅需验证浮点精度 |
| ⭐ **兼容** | GameMods 游戏模式 | 0.5 | 纯游戏逻辑，无需改动 |

### 4.2 难度分级（颜色标识）

| 级别 | 含义 | 模块数 | 总人天 |
|:---:|------|:---:|:---:|
| 🟢 **兼容** | 零改动或只需重新编译 | 8 | 2 |
| 🟡 **较低** | 少量配置/API适配 | 4 | 8 |
| 🟠 **中等** | 后端替换/渲染适配 | 4 | 11 |
| 🔴 **较高** | 需要重构核心交互逻辑 | 3 | 14 |
| **合计** | | **19** | **35** |

---

## 5. 系统功能模块迁移分析

### 5.1 构建系统迁移（CMake → Gradle + CMake）

**当前：** 纯 CMake + MSVC 编译器 + `glslangValidator.exe`

**Android 方案：**
```
app/
├── build.gradle                    # Gradle 构建配置 (NDK 版本、SDK 版本)
├── CMakeLists.txt                  # NDK CMake 配置
├── src/main/
│   ├── AndroidManifest.xml        # 应用清单
│   ├── java/.../MainActivity.java  # 启动 Activity
│   ├── cpp/                        # C++ 源码 (符号链接或复制)
│   └── assets/                     # .ini 配置、音效资源、纹理素材
```

**CMakeLists.txt 关键适配：**
```cmake
cmake_minimum_required(VERSION 3.12)
project(PixelClean)

# NDK 交叉编译配置
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -O2")

# Android 专用库
find_library(log-lib log)
find_library(android-lib android)

# 排除 GLFW，使用 Android Native App Glue
# add_subdirectory(Environment/glfw)  # 移除

# 保留
add_subdirectory(Environment/libevent)
add_subdirectory(Environment/zlib)
add_subdirectory(Environment/spdlog)
add_subdirectory(Environment/Soloud)
add_subdirectory(Environment/imgui)

# ... 其余 add_subdirectory 不变 ...
```

### 5.2 资源与 Assets 管理

| 当前 Windows | Android Assets | 处理方式 |
|:--|:--|:--|
| `PhysicsBlock.ini` | `assets/PhysicsBlock.ini` | AAssetManager 读取 |
| `Info.ini` | `assets/Info.ini` | AAssetManager 读取 |
| `SoundEffect/Resources/` | `assets/Resources/` | AAssetManager 读取 |
| `Resource/` | `assets/Resource/` | AAssetManager 读取 |
| `InterfaceUI/Minecraft_AE.ttf` | `assets/Minecraft_AE.ttf` | AAssetManager 读取 |
| `shaders/` (SPIR-V) | `assets/shaders/` | 内嵌或 AAssetManager 读取 |

### 5.3 Vulkan Surface 创建流程适配

**Windows (GLFW):**
```cpp
// GLFW 自动创建 VkSurfaceKHR
glfwCreateWindowSurface(instance, window, nullptr, &surface);
```

**Android:**
```cpp
VkAndroidSurfaceCreateInfoKHR createInfo = {};
createInfo.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
createInfo.window = nativeWindow;
vkCreateAndroidSurfaceKHR(instance, &createInfo, nullptr, &surface);
```

项目中已有 [WindowSurface](file:///e:/Physics/PixelClean-main/Vulkan/windowSurface.h) 类，正好可以在此抽象层增加 Android 分支。

### 5.4 输入系统重构（最关键）

| Windows 输入 | Android 输入 | 映射方式 |
|:--|:--|:--|
| WASD 键盘移动 | 左侧虚拟摇杆 | 触摸区域检测→`GameKeyEnum::MOVE_*` |
| 鼠标瞄准 | 右侧触摸区域 | 触摸点→世界坐标→瞄准方向 |
| 鼠标左键射击 | 右侧触摸按下 | Touch Down → Shoot |
| 鼠标滚轮 | 双指捏合 | PinchScale → `MouseRoller` |
| Esc / 数字键 | 屏幕按钮 | ImGui Button 映射 |

### 5.5 ImGui 调试界面评估

ImGui 主要用于调试（[ImGuiPhysics](file:///e:/Physics/PixelClean-main/PhysicsBlock/ImGuiPhysics.hpp) 的物理参数面板、属性编辑器），不是核心游戏 UI。

**建议：**
- ImGui Android 后端优先级降低，用简单的文本/按钮替代调试功能
- 核心游戏 UI（InterfaceUI/Interface.h 中的界面）可迁移或简化
- 发布版可完全移除 ImGui 调试面板

---

## 6. 资源需求估算

### 6.1 人力资源

| 角色 | 人数 | 投入周期 | 职责 |
|------|:---:|------|------|
| **C++ 引擎开发** | 1 | 全周期 | Vulkan 渲染适配、构建系统、NDK 集成 |
| **Android 平台开发** | 1 | 中期起 | Gradle 构建、APK 打包、性能 Profiling、Google Play 合规 |
| **UI/UX 设计** | 0.5 | 中期起 | 触屏操作设计、虚拟摇杆 UI、菜单重设计 |
| **QA 测试** | 0.5 | 后期 | Android 真机兼容性测试、性能测试、回归测试 |
| **项目管理** | 0.2 | 全周期 | 进度跟踪、风险管控 |

**总计：约 3.2 人（可 2 人全职 + 1 人机动）**

### 6.2 硬件与设备

| 资源 | 数量 | 用途 |
|------|:---:|------|
| Android 测试机 (支持 Vulkan, 中端) | 1 | 日常开发测试 |
| Android 测试机 (低端 Vulkan) | 1 | 兼容性下限测试 |
| Android 测试机 (高端/平板) | 1 | 性能上限测试 |
| Windows 开发 PC (已有) | 1 | 交叉编译 + 调试 |

### 6.3 软件与许可证

| 资源 | 类型 | 成本 |
|------|------|------|
| Android Studio | IDE | 免费 |
| NDK r25+ | 工具链 | 免费 |
| Vulkan Validation Layers | 调试 | 免费 |
| RenderDoc for Android | GPU 调试 | 免费 |
| 第三方库许可证合规审查 | 法务 | 人工 |

---

## 7. 时间周期预测

### 7.1 分阶段时间估算

| 阶段 | 描述 | 工期 | 里程碑产出 |
|------|------|:---:|------|
| **Phase 0** | 环境搭建、可行性验证 | 1 周 | NDK + Vulkan Hello Triangle 在手机运行 |
| **Phase 1** | 构建系统 + 底层适配 | 2 周 | CMake 项目在 Android Studio 编译通过 |
| **Phase 2** | Vulkan 渲染适配 | 2 周 | 基础 2D 三角形渲染到手机屏幕 |
| **Phase 3** | 第三方库集成 | 2 周 | SoLoud, libevent, spdlog 在 Android 正常工作 |
| **Phase 4** | 输入系统重构 | 3 周 | 触摸操控 + 虚拟摇杆可用 |
| **Phase 5** | ImGui/UI 适配 | 2 周 | 游戏界面/调试面板在手机可用 |
| **Phase 6** | 物理/玩法集成 | 2 周 | PhysicsBlock + GameMods 完整运行 |
| **Phase 7** | 网络联机适配 | 1.5 周 | TCP Client/Server 移动端可用 |
| **Phase 8** | 性能优化 | 2 周 | 帧率稳定 ≥ 30FPS 在目标机型 |
| **Phase 9** | 测试与修复 | 2 周 | Bug 收敛，兼容性达标 |
| **Phase 10** | APK 打包与发布准备 | 1 周 | Signed APK，商店素材就绪 |

**总计：约 20.5 周 ≈ 5 个月**（不含人力等待）

### 7.2 甘特图概要

```
Phase 0 [==]                (第 1 周)
Phase 1 [====]              (第 2-3 周)
Phase 2 [====]              (第 4-5 周)
Phase 3 [====]              (第 5-6 周)   ← 可与 Phase 2 并行
Phase 4 [======]            (第 7-9 周)   ← 关键路径
Phase 5 [====]              (第 10-11 周)
Phase 6 [====]              (第 12-13 周)
Phase 7 [===]              (第 14-15 周) ← 可与 Phase 6 尾段并行
Phase 8 [====]              (第 16-17 周)
Phase 9 [====]              (第 18-19 周)
Phase10 [==]                (第 20 周)
```

---

## 8. 迁移策略与实施步骤

### 8.1 总体策略：渐进式迁移 + 分层接入

**原则：**
1. **先跑通，再完善** —— 优先让核心渲染和物理在 Android 运行
2. **最小改动原则** —— 用 `#ifdef __ANDROID__` 条件编译隔离平台代码
3. **增量提交** —— 每个 Phase 产出可独立验证的 APK
4. **双平台同源** —— 保留 Windows 构建能力，Android 和 Windows 共享同一份业务逻辑代码

### 8.2 详细实施步骤

#### Phase 0: 环境搭建与验证 (1 周)

- [ ] 安装 Android Studio + NDK r25+
- [ ] 创建最小 Android Vulkan 示例项目（Hello Triangle）
- [ ] 在真机上验证 Vulkan 可用（`vkEnumeratePhysicalDevices` 不为空）
- [ ] 确认目标最低 API Level (建议 26, Android 8.0, Vulkan 1.0+)
- [ ] 安装 RenderDoc for Android 用于 GPU 调试

#### Phase 1: 构建系统迁移 (2 周)

- [ ] 编写 `app/build.gradle`，配置 NDK + CMake
- [ ] 将项目 `CMakeLists.txt` 改为 NDK 兼容（去除 MSVC 选项 `resource.rc`）
- [ ] 用 `#ifdef __ANDROID__` 包裹 `base.h` 中 `#include <Windows.h>`
- [ ] 编译 `Environment/libevent` 交叉目标 → 解决 socket API 差异
- [ ] 编译 `Environment/zlib`, `Environment/spdlog` → 验证
- [ ] 解决 `#pragma comment(lib)` 等 MSVC 特有指令

#### Phase 2: Vulkan 渲染适配 (2 周)

- [ ] 实现 `android_main` 替代 `main()`
- [ ] 适配 VkSurface 创建（`VK_KHR_android_surface`）
- [ ] 验证 SwapChain（RGBA8 格式、`VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR`）
- [ ] 验证 CommandBuffer 录制、Fence/Semaphore 同步 —— 基本帧渲染

#### Phase 3: 第三方库交叉编译与集成 (2 周)

- [ ] **SoLoud**: 开启 `WITH_OPENSLES` → 交叉编译 → 真机播放测试音
- [ ] **spdlog**: 增加 Android Logcat sink (`spdlog/sinks/android_sink.h`)
- [ ] **libevent**: POSIX epoll backend → 编译通过
- [ ] **zlib**: 确认交叉编译无误
- [ ] **lodepng**: 无改动，头文件层次 OK

#### Phase 4: 输入系统重构 (3 周)

- [ ] 设计移动端操作方案：
  - 左侧虚拟摇杆（移动）
  - 右侧触摸区（瞄准 + 射击）
  - 双指缩放替代滚轮
- [ ] 实现 `AInputQueue` → `GameKeyEnum` 映射层
- [ ] 触摸坐标 → 世界坐标 → `MouseMove` 映射
- [ ] 虚拟摇杆 UI 绘制（可用简单 GL 图元）
- [ ] 真机操控手感调试

#### Phase 5: UI 适配 (2 周)

- [ ] 保留现有 `InterfaceUI/Interface.h` 结构
- [ ] 菜单项从 ImGui 面板迁移为原生 UI 组件
- [ ] ImGui Android 后端的键盘事件适配（`ImGuiKeyBoardEvent`）
- [ ] 字体缩放基准适配（屏幕 DPI）

#### Phase 6: 物理/玩法完整集成 (2 周)

- [ ] PhysicsBlock 全模块 Android 编译
- [ ] SquarePhysics 系统验证
- [ ] 角色 (`GamePlayer`) 渲染验证
- [ ] 武器系统 (`Arms`) 渲染验证
- [ ] 游戏模式 (`GameMods`) 切换运行
- [ ] 粒子效果渲染正确

#### Phase 7: 网络联机适配 (1.5 周)

- [ ] 移除 WinSock 相关宏
- [ ] 验证 POSIX `<sys/socket.h>` + libevent 异步 I/O
- [ ] LAN 内 Client/Server 联机测试
- [ ] 网络数据同步正确性验证

#### Phase 8: 性能优化 (2 周)

- [ ] GPU Profiling (RenderDoc for Android) → 减少 DrawCall
- [ ] CPU Profiling (Android Studio Profiler) → 减少主线程阻塞
- [ ] 物理更新频率与渲染帧率解耦
- [ ] 内存占用优化（纹理压缩 ASTC/ETC2）
- [ ] Shader 优化（移除未使用 Uniform, FMA 优化）

#### Phase 9: 测试 (2 周)

- [ ] 功能回归（所有 GameMods 正常运行）
- [ ] 兼容性矩阵测试（不同 GPU: Adreno / Mali / PowerVR）
- [ ] 长时间运行稳定性（内存泄漏检测）
- [ ] 弱网环境网络测试

#### Phase 10: 发布准备 (1 周)

- [ ] 生成 Signed APK
- [ ] Google Play 商店页面准备（截图、描述）
- [ ] Privacy Policy + Terms of Service
- [ ] 崩溃收集（Firebase Crashlytics 或 Sentry）
- [ ] 发布 Checklist 审查

---

## 9. 风险分析与应对策略

### 9.1 高风险 (Red)

| 风险 | 概率 | 影响 | 应对策略 |
|------|:---:|------|------|
| **R1: Android Vulkan 驱动不兼容** — 部分 GPU（Mali/PowerVR）的 Vulkan 驱动实现有 Bug，导致渲染异常或崩溃 | 高 | 严重 | ① 维护设备兼容性矩阵，低端机自动降级切换 OpenGL ES 备用管线 ② 初期聚焦高通 Adreno GPU ③ 提前对 Mali Bifrost/Valhall 做专项适配 |
| **R2: `updateParameters` 回调不触发** — 如果在 ArcGIS/ArcPy 中 Tool Class 注册方式不对，`updateParameters` 不会被框架调用，导致下拉列表始终为空 | 高 | 严重 | ① 先用 Phase 0 的最小 Demo 验证 Tool Class 注册机制 ② 如果确实不触发，降级为手动点击"刷新"按钮手动触发 `ListFeatureClasses` ③ 做好 fallback 方案 |

### 9.2 中风险 (Yellow)

| 风险 | 概率 | 影响 | 应对策略 |
|------|:---:|------|------|
| **R3: ImGui 无官方 Android 后端** — 需自行实现触摸/IME/输入映射，可能工作量超预期 | 中 | 中等 | ① ImGui 仅用于调试面板，非核心 UI ② 优先用原生 UI / Dear ImGui 的 Simple OpenGL + Android 绑定 ③ 发布版完全移除 ImGui |
| **R4: SoLoud 音频延迟/卡顿** — Android 音频管线与 Windows 差异大，可能引入延迟 | 低 | 中等 | ① 预备 AAudio 后端（Android 8.1+）替代 OpenSLES ② 调整 buffer size ③ 分离音效和音乐流 |
| **R5: libevent epoll 行为差异** — IOCP→epoll 导致网络事件处理变化 | 低 | 中等 | ① libevent 已抽象多平台差异，大部分场景无感知 ② 若出现异常，添加 Android 特定条件编译 |

### 9.3 低风险 (Green)

| 风险 | 概率 | 影响 | 应对策略 |
|------|:---:|------|------|
| **R6: 浮点精度差异** — ARM NEON vs x86 SSE 可能导致物理计算微小差异 | 低 | 小 | ① PhysicsBlock 已支持 float/double 切换 (`BaseStruct.hpp`)，必要时切为 double ② 添加确定性单元测试 |
| **R7: APK 体积过大** — 音效/纹理资源未经优化 | 中 | 小 | ① 使用 AAB (Android App Bundle) 按需分发 ② 纹理压缩 ASTC ③ 音效预压缩 |

---

## 10. 关键成功因素

### 10.1 技术层面

| 序号 | 关键因素 | 说明 |
|:---:|------|------|
| **KF1** | **Vulkan 驱动的稳定性** | 优先适配主流 GPU（高通 Adreno 6xx/7xx），确保核心渲染管线零崩溃 |
| **KF2** | **Tool Class 注册机制** | `setup_toolbox.py` 中通过 arcpy 注册脚本的 Tool Class 是整个参数联动的基础，必须首先验证 |
| **KF3** | **输入映射的正确性** | 触摸→世界坐标→游戏逻辑的映射精度直接决定操控体验 |
| **KF4** | **物理引擎确定性** | PhysicsBlock 的计算结果在 ARM 和 x86 之间必须一致 |
| **KF5** | **交叉编译工具链稳定** | NDK r25 及以上版本对 C++17 支持和 Vulkan Header 兼容性必须确认 |

### 10.2 管理层面

| 序号 | 关键因素 | 说明 |
|:---:|------|------|
| **KF6** | **核心开发者不变** | 保持 1 名全职 C++ 引擎开发者贯穿全周期，避免交接损耗 |
| **KF7** | **持续集成 (CI)** | 每次提交自动构建 APK 并运行冒烟测试，尽早暴露问题 |
| **KF8** | **双平台同步策略** | Android 和 Windows 共享逻辑代码，用条件编译隔离平台层 |
| **KF9** | **里程碑式交付** | 每个 Phase 产出可运行的 APK，方便 PM/QA 及时验证 |

### 10.3 风险控制

| 序号 | 关键因素 | 说明 |
|:---:|------|------|
| **KF10** | **GPU 兼容矩阵** | 在 Phase 0 就启动多 GPU 真机测试 (Adreno, Mali, PowerVR)，不等后期 |
| **KF11** | **回退方案** | 若核心模块无法迁移，必须有"部分功能 Android 独占"的准备 |
| **KF12** | **代码评审** | 每个 Phase 的 PR 必须经过至少一人 Code Review，防止平台 hack 积累 |

---

## 附录 A: 第三方库版本与许可证一览

| 库 | 当前版本（估算） | 许可证 | Android 兼容 |
|------|:---:|------|:---:|
| Vulkan SDK | 1.3.x | Apache 2.0 / MIT | ✅ |
| GLM | 0.9.9+ | MIT (Happy Bunny) | ✅ |
| GLFW | 3.3+ | zlib/libpng | ❌ → Android Native |
| ImGui | 1.89+ | MIT | ⚠️ 需后端 |
| SoLoud | latest git | SoLoud License (zlib-like) | ✅ 需配置 |
| libevent | 2.1.x | BSD 3-Clause | ✅ |
| zlib | 1.2.x | zlib License | ✅ |
| spdlog | 1.x | MIT | ✅ |
| lodepng | latest git | zlib License | ✅ |

## 附录 B: 文件清单（新增/修改）

### B.1 新增文件
- `setup_toolbox.py` — 一键注册 Toolbox
- `tool_script.py` — 核心工具实现（含 updateParameters）
- `app/build.gradle` — Android Gradle 构建
- `app/CMakeLists.txt` — NDK CMake 配置
- `app/src/main/cpp/android_main.cpp` — Android 入口 (`android_main`)
- `app/src/main/cpp/AndroidInputHandler.h/.cpp` — 触摸 → 游戏逻辑映射
- `app/src/main/java/com/pixelclean/app/MainActivity.java` — 启动 Activity
- `app/src/main/assets/` — 资源目录
- `app/src/main/res/` — Android UI 资源

### B.2 需要修改的文件
| 文件 | 改动类型 | 说明 |
|------|------|------|
| `base.h` | 条件编译 | 用 `#ifdef __ANDROID__` 隔离 `<Windows.h>` |
| `main.cpp` → `android_main.cpp` | 重写入口 | 改为 `android_main(struct android_app*)` |
| `Vulkan/Window.h/.cpp` | 重构/替换 | 用 NativeActivity 替代 GLFW 窗口管理 |
| `Vulkan/windowSurface.h/.cpp` | 增加分支 | 添加 `VK_KHR_android_surface` 创建路径 |
| `SoundEffect/SoundEffect.cpp` | Assets 适配 | 音频文件读取改为 AAssetManager |
| `Tool/Tool.h/.cpp` | 初始化调整 | spdlog 增加 Android logcat sink |

---

> **报告结论：** PixelClean 项目向安卓平台迁移具有较高的技术可行性，核心风险集中在 Vulkan 驱动的兼容性和输入系统的重构。项目依赖的大部分第三方库（除 GLFW 和 ImGui 后端外）可以较顺利地交叉编译至 Android 平台。建议采用渐进式分层接入策略，预估总工期约 **5 个月**（2-3 人核心团队）。第一阶段（Phase 0-2，约 5 周）的成果——基本渲染到屏幕——是决定项目整体可行性的关键里程碑。

---

*本报告由人工智能辅助生成，基于项目源码的静态分析。建议在实施前由技术团队进行二次评审。*
