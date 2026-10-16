# 在 Android 手机上跑通 PixelClean（Vulkan C++ 游戏引擎）编译链路 —— 完整实战记录

> 目标工程：https://github.com/wuxingwushu/PixelClean
> 结论先行：**可以。** 本文记录的是"路线 A"——不装 Android Studio、不用 x86 电脑，**完全在这台 arm64 手机本机命令行里**，从源码一路做到可安装、可启动、能出画面的签名 APK。
> 实测设备：荣耀 PPG-AN00（骁龙 8 Gen2 / Adreno，Android 17 / SDK 37，arm64-v8a，15.6GB RAM，剩 ~190GB 存储）。

---

## 0. 一句话总结

Termux 的 apt 仓库里其实躺着一条**原生的 aarch64 Android 交叉工具链**（clang / cmake / make / ninja / glslangValidator / aapt2 / d8 / apksigner / kotlinc / openjdk）。把它解包到本 app 私有目录、烧好 RUNPATH、修掉两个 Android 特有的坑，就能在手机上交叉编译出 `libpixelclean.so`，再用命令行手工拼 APK。整条链路 232 个 C++ 源文件全部编译通过，最终 APK 安装后**真机渲染出了 Vulkan 游戏界面**。

后续真机复查又揪出并修复了三个手搓打包链路的缺陷——**缺应用图标、全面屏左右黑边（letterbox）、日文名 BGM 无声**（根因分别是 Manifest 裸奔和 aapt2 文件名编码，见第 12、13 节），最终包已做到图标正常、全屏铺满、音乐正常。

---

## 1. 为什么"手机能编译"这件事成立

PixelClean 官方只给了两条路：Windows（MSVC + Vulkan SDK）和 Android（Android Studio + NDK）。两条路都需要一台 x86 开发机。但拆开看，它真正需要的东西是：

| 需求 | 官方方案 | 手机本机替代 |
|---|---|---|
| C++17 编译器 | MSVC / NDK clang | **Termux clang 21**（原生 aarch64，能直接产出 `--target=aarch64-linux-android26`） |
| 构建系统 | CMake + Ninja | **Termux cmake 4.4 + make / ninja** |
| Vulkan 头文件 | Vulkan SDK | **Termux vulkan-headers**（`vulkan/vulkan.h` 1.4.365） |
| GLSL→SPIR-V | `glslangValidator.exe`（compile.bat） | **Termux glslang**（`glslangValidator`，写个 sh 脚本替代 .bat） |
| Android 系统头 | NDK sysroot | **Termux ndk-sysroot + libandroid-stub**（`jni.h`/`android/native_window.h`） |
| Android 存根库 | NDK `platforms/android-26` | **从 NDK zip 里抽出 8 个 stub `.so`**（liblog/libvulkan/libandroid/libOpenSLES…） |
| 第三方库 | 仓库自带源码 | **仓库已 vendored**（imgui/zlib/spdlog/libevent/Soloud/WFC，全部有 CMakeLists） |
| Kotlin 编译 | Gradle/AGP | **Termux kotlinc 2.4 + d8** |
| 打包签名 | AGP | **Termux aapt2 + apksigner + keytool** |

关键点：**NDK 的编译工具链二进制是 x86_64 的，在 ARM 手机上根本跑不起来**（`exec format error`）。所以正确姿势不是"下载 NDK 用它"，而是"用 Termux 的原生 clang，只从 NDK 里借它需要的东西"——即 sysroot 头文件和 stub 库。

---

## 2. 环境搭建（Termux 包解包到私有目录）

本会话运行在 DeepSeek Harness 的沙箱里，没有安装 Termux App，但**可以直接下载 Termux 的 `.deb` 包并解包**——Android 的 linker 能加载这些原生 aarch64 二进制。

### 2.1 下载并解包

Termux 仓库索引：`https://mirrors.tuna.tsinghua.edu.cn/termux/apt/termux-main/dists/stable/main/binary-aarch64/Packages`（实测清华源 ~9MB/s，官方源 ~240KB/s，务必用国内镜像）。

需要的包（依赖闭包共 ~48 个）：

```
clang libllvm lld libcompiler-rt llvm        # 编译器
cmake make ninja                              # 构建
ndk-sysroot libandroid-stub vulkan-headers    # Android 头 + 存根
glslang spirv-tools shaderc glm              # 着色器 + 数学库
aapt2 d8 apksigner openjdk-21 kotlin         # 打包链
zlib libffi libandroid-spawn ncurses ...      # 传递依赖
```

`.deb` 是 `ar` 归档，内含 `data.tar.xz`。用 Python 解到统一前缀 `root/data/data/com.termux/files/usr/`（保持 Termux 布局，很多工具靠相对路径找资源）：

```python
# 关键：解 tar 时要用 filter 放行符号链接，否则 AbsoluteLinkError
t.extractall('root', filter=lambda ti, p: ti)
```

> **踩坑 1**：`curl` 并行下载多个 deb 会被截断（`ContentTooShortError`）。改用 `curl -C -` 断点续传 + 校验 `Size` 字段，或串行下载。

### 2.2 RUNPATH 烧录（最关键的通用坑）

Termux 二进制依赖它自己 `lib/` 目录下的 `.so`（libc++、libLLVM、libclang-cpp…）。在真 Termux 里靠环境变量 `LD_LIBRARY_PATH` 指路，但**本沙箱里不能全局导出 `LD_LIBRARY_PATH`**——一旦导出，会污染 toybox 系统命令（`ls`/`cat`/`tail` 全部报 `cannot locate symbol "__android_log_bswrite"`，因为 Termux 的同名库劫持了系统库）。

**根治办法：用 patchelf 把绝对 lib 路径烧进每个 ELF 的 RUNPATH**，之后不需要任何环境变量：

```bash
R=.../root/data/data/com.termux/files/usr
# 所有 .so
for p in $R/lib/*.so*; do patchelf --set-rpath $R/lib $p; done
# 所有 bin
for f in $R/bin/*; do [ ELF ] && patchelf --set-rpath $R/lib $f; done
```

> **踩坑 2**：JVM（openjdk）的库互相引用，RUNPATH 要写全 `$R/lib:$J/lib:$J/lib/server`，且 `java`/`keytool`/`d8`/`apksigner` 启动器都要单独 patch。
> **踩坑 3**：用 Python 二进制替换字符串给 make/cmake 打补丁时，写回会丢可执行位，记得 `chmod +x`；替换长度必须严格相等（用 `\x00` 填充，不能用 `/` 填充——路径末尾带斜杠会导致 exec 失败）。

### 2.3 包装脚本的 shebang

`d8`/`apksigner`/`kotlinc` 是 shell 脚本，shebang 写死 `/data/data/com.termux/files/usr/bin/sh`（真 Termux 路径，本沙箱不存在）。而且 **Android 禁止直接 `execve` 位于 `/data` 分区的 shell 脚本**（只有 ELF 能跑）。解决：把脚本 shebang 改指向解包出来的 `root/.../bin/bash`（需 `pkg install bash`——payload 自带的 "bash" 其实是 toybox sh，不支持 `=~` 等 bash 语法）。

`kotlinc` 的官方脚本还依赖 `KOTLIN_HOME` 推导，直接写个极简 wrapper 绕过：

```bash
#!/.../bin/bash
exec $JVM/bin/java -cp "$KH/lib/kotlin-compiler.jar:$KH/lib/kotlin-stdlib.jar:..." \
     org.jetbrains.kotlin.cli.jvm.K2JVMCompiler "$@"
```

---

## 3. 源码补丁：让 Windows-only 工程能在大小写敏感文件系统上编译

这是**任何 Linux/CI 构建都会遇到、只是被 Windows 掩盖了**的问题，与手机无关但必须先修。对全部 114 个引擎 `.cpp` 做 Android 目标语法扫描，从"14 通过 / 100 失败"修到"全部通过"：

### 3.1 大小写不匹配（50+ 文件）

Windows 文件系统大小写不敏感，工程里混用了错误大小写的 `#include`。Linux 直接 `file not found`。用符号链接解决（比改源码省事）：

```bash
ln -s Window.h Vulkan/window.h          # include "window.h"
ln -s VulkanTool VulKanTool             # include "../VulKanTool/texture.h"
ln -s Vulkan VulKan                     # include "../VulKan/image.h"
ln -s Environment/imgui ImGui           # include "../ImGui/imgui.h"
```

### 3.2 真实代码 bug：`MemoryPool.h`（MSVC 容忍，clang 报错）

`Tool/MemoryPool.h:138,161` 写的是 `memoryPool.freeSlots`，但成员名是 `freeSlots_`。MSVC 的 permissive 模式不报错，clang 直接拒绝：

```cpp
- freeSlots_ = memoryPool.freeSlots;
+ freeSlots_ = memoryPool.freeSlots_;
```

### 3.3 缺失的 vendored 头文件

`.gitignore` 里 `Environment/*` 只白名单了几个目录，导致 **`Environment/vma/vk_mem_alloc.h` 没进仓库**（但 `Vulkan/device.h` 要 `#include <vma/vk_mem_alloc.h>`）。`glm` 同理（56 处引用）。解决：

- `vk_mem_alloc.h`：从 `https://chromium.googlesource.com/external/github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/+/refs/heads/main/include/vk_mem_alloc.h?format=TEXT` 下载后 base64 解码（GitHub raw 直连被 TLS 干扰，jsDelivr 也 404，googlesource 可用）。
- `glm` / `shaderc`：Termux apt 直接有包。

### 3.4 libevent 的 `event-config.h`

这是 CMake configure 阶段生成的（`configure_file`），手工语法扫描时看不到，真实 CMake 构建里自动就有——**不是问题**。

---

## 4. CMake 交叉编译：踩平 Android 沙箱的两个致命坑

### 4.1 toolchain 文件

```cmake
set(CMAKE_SYSTEM_NAME Android)
set(CMAKE_SYSTEM_VERSION 26)
set(TCR /path/to/root/data/data/com.termux/files/usr)
set(CMAKE_C_COMPILER   ${TCR}/bin/clang)
set(CMAKE_CXX_COMPILER ${TCR}/bin/clang++)
set(CMAKE_AR ${TCR}/bin/llvm-ar)
set(CMAKE_RANLIB ${TCR}/bin/llvm-ranlib)
set(CMAKE_MAKE_PROGRAM ${TCR}/bin/make CACHE FILEPATH "")
set(TRIPLE aarch64-linux-android26)
set(CMAKE_C_FLAGS_INIT   "--target=${TRIPLE} -fPIC")
set(CMAKE_CXX_FLAGS_INIT "--target=${TRIPLE} -fPIC")
set(CMAKE_FIND_ROOT_PATH ${TCR})
set(CMAKE_SYSTEM_INCLUDE_PATH ${TCR}/include)
set(CMAKE_SYSTEM_LIBRARY_PATH ${TCR}/lib)
```

### 4.2 坑 A：Termux cmake/make 硬编码了 `/data/data/com.termux/files/usr/bin/sh`

生成的 Makefile 里 `SHELL = /data/data/com.termux/files/usr/bin/sh`，本沙箱该路径不存在 → 每个编译步骤 `Error 127`。这个字符串**烧在 cmake 和 make 二进制里**，`CMAKE_MAKE_PROGRAM_SHELL` 变量在 cmake 4.4 上不生效。

**解决**：二进制里等长替换成 `/system/bin/sh`（见 2.2 踩坑 3 的长度/权限注意事项）。

### 4.3 坑 B：`CMAKE_SYSTEM_NAME Android` 触发读 NDK 专有路径

Termux 版 cmake 的 `CMakeDetermineSystem.cmake` 在 Android 分支会读 `$ENV{PREFIX}/include/android/api-level.h` 推导 API level。`PREFIX` 没设时变成 `/include/android/api-level.h` → 失败。

**解决**：configure 时 `export PREFIX=$TCR`（指向含 `include/android/api-level.h` 的 Termux 前缀）。

### 4.4 坑 C：ninja 在本沙箱里 spawn 子进程一律 127

ninja 用 `posix_spawn` 调度子任务，在这个 SELinux `untrusted_app` 上下文里被拦。而 **make 用 fork+execve 是通的**。所以最终用 `-G "Unix Makefiles"`，放弃 ninja。

### 4.5 完整 configure 命令

```bash
R=.../root/data/data/com.termux/files/usr
env -i PATH=$R/bin:/system/bin PREFIX=$R \
  $R/bin/cmake -G "Unix Makefiles" \
  -B build_android -S MyApplication/app/src/main/cpp \
  -DCMAKE_TOOLCHAIN_FILE=.../android-toolchain.cmake \
  -DCMAKE_BUILD_TYPE=Release
```

工程自带的 `MyApplication/app/src/main/cpp/CMakeLists.txt` 已经把 Android 分支写得很完整（`find_library(vulkan)`、imgui android backend、Soloud OpenSLES backend、`-Wl,-z,max-page-size=16384` 16KB 页对齐等），无需改动。

### 4.6 编译

```bash
env -i PATH=$R/bin:/system/bin $R/bin/make -j8
```

结果：**232 个 `.o` 全部产出，6 个第三方静态库（imgui/zlib/spdlog/libevent/soloud/wfc）+ `libpixelclean.so`（5.9MB）链接成功**，仅 15 条无害 warning。

验证产物：

```
Machine: AArch64, Type: DYN (Shared object)
DT_NEEDED: liblog libandroid libvulkan libOpenSLES libc libc++_shared libdl libm
导出 17 个 Java_com_pixelclean_MainActivity_native* JNI 符号
```

> 注意：`libc++_shared.so` 需要随 APK 一起打包（工程用了 c++_static，但 imgui 等静态库链到 shared runtime；从 Termux libc++ 包取现成的）。

---

## 5. 着色器编译（替代 compile.bat）

`compile.bat` 是 Windows 批处理，列出 36 条 `glslangValidator -V xxx.glsl -o yyy.spv`。移植成 POSIX sh（`shaders/compile_shaders.sh`），逐条翻译即可。手机上实测 **36/36 全部编译成功**，`spirv-val` 校验通过。

```bash
sh shaders/compile_shaders.sh <out_dir> $R/bin/glslangValidator
```

---

## 6. 命令行手工打 APK（绕开 Gradle/AGP）

AGP 的 native 步骤会去找 NDK 自带的 x86_64 工具（跑不了），所以整个 APK 用命令行手搓：

### 6.1 布局

```
apkwork/
  AndroidManifest.xml          # package=com.pixelclean, 用系统 Theme.Material.NoActionBar（避开 Material AAR 依赖）
                               # ⚠ 首版裸 Manifest 引发 letterbox/无图标，必须补 uses-sdk/versionCode，见第 12 节
  res/                          # 启动图标：首版是 Python 造的纯色占位图（坑 #16），应直接用工程 res/mipmap-* 真图标
  assets/                       # Info.ini, PhysicsBlock.ini, Resource/, shaders/*.spv, *.ttf, BlockS/
  lib/arm64-v8a/               # libpixelclean.so + libc++_shared.so
  classes/                      # kotlinc 产物
```

### 6.2 步骤

```bash
# 1) Kotlin → class（MainActivity.kt 需要 BuildConfig，手工补一个 .kt stub）
kotlinc2 -cp android.jar -jvm-target 17 BuildConfig.kt MainActivity.kt -d apkwork/classes

# 2) class + kotlin-stdlib → dex
d8 --release --min-api 26 --lib android.jar \
   --classpath kotlin-stdlib.jar --output dexout  *.class kotlin-stdlib.jar

# 3) 资源编译 + 链接（-A 带 assets）
aapt2 compile --dir apkwork/res -o compiled/res.zip
aapt2 link -o base.apk -I android.jar --manifest AndroidManifest.xml compiled/res.zip -A apkwork/assets

# 4) 用 Python zipfile 把 classes.dex 和 lib/*.so 追加进 base.apk（沙箱无 zip 命令）

# 4.5) ⚠ 修复 aapt2 对非 ASCII assets 文件名的 cp437 转写（否则日文/中文资源静默失效，见 13.3）

# 5) 签名（keytool 造 keystore，apksigner 签；apksigner 的 PATH 必须含 $JVM/bin——它内部裸调 java，见坑 #17）
keytool -genkeypair -keystore debug.keystore -storepass android -alias androiddebugkey \
        -keyalg RSA -keysize 2048 -dname "CN=PixelClean" -validity 10000
apksigner sign --ks debug.keystore --ks-pass pass:android --key-pass pass:android \
        --out PixelClean-signed.apk PixelClean.apk
apksigner verify --verbose PixelClean-signed.apk   # v2/v3 true（v1 仅 minSdk<24 才需要）
```

> **踩坑**：`apksigner` 的 `keytool` 子进程也需要 JVM 库 RUNPATH 烧录到位（见 2.2 踩坑 2）。

---

## 7. 安装与真机验证

### 7.1 安装路径的 SELinux 坑

- `pm install` 的 system_server **读不了 `/storage/emulated/0`（fuse 上下文）**，也读不了 app 私有目录。
- 正确做法：把 APK 放到 **`/data/local/tmp`**（shell 域可读）。本沙箱 app uid 写不进 `/data/local/tmp`，所以先放 `/sdcard`，再让 Shizuku 的 shell（uid 2000）`cp` 到 `/data/local/tmp` 后 `pm install`。

```bash
# shell 通道
cp /storage/emulated/0/PixelClean.apk /data/local/tmp/ && chmod 644 /data/local/tmp/PixelClean.apk
pm install -r -t /data/local/tmp/PixelClean.apk    # → Success
```

### 7.2 启动与出画面

```bash
am start -W -n com.pixelclean/.MainActivity
# Status: ok, LaunchState: COLD, TotalTime: ~110ms
```

logcat 显示渲染线程持续出帧：

```
D vulkan: [PresentOneSwapchain] ... m_renderTid is 12644   # 每 ~8ms 一次
D com.pixelclean: addLayerName, tid:12644
```

**截图确认：屏幕渲染出了完整的 Vulkan 游戏画面——彩色像素方块背景 + ImGui「游戏设置」面板（字体大小/音乐音量/服务器端口/Vulkan 验证层/渲染设备等控件，中文正常显示）。** 进程 `pidof com.pixelclean` 稳定存活。

> 退出记录核对：应用最终由用户手动关闭，`dumpsys` 的 ApplicationExitInfo 显示 `reason=1 (EXIT_SELF), status=0`，无 Fatal signal、无 tombstone——即全程无崩溃，冷启动到出画面到交互均正常。

至此：源码 → 交叉编译 `.so` → 着色器 SPIR-V → 手工打包签名 APK → 安装 → 真机启动出画面，**全链路闭环**。

---

## 8. 完整坑清单速查（下次照抄即可）

| # | 坑 | 症状 | 解法 |
|---|---|---|---|
| 1 | NDK 工具是 x86_64 | `exec format error` | 用 Termux 原生 clang，只借 NDK 的 sysroot 头 + stub 库 |
| 2 | 不能全局导出 LD_LIBRARY_PATH | toybox 命令 `__android_log_bswrite` 报错 | patchelf 烧 RUNPATH 进每个 ELF |
| 3 | cmake/make 硬编码 Termux sh 路径 | make `Error 127` | 二进制里等长替换为 `/system/bin/sh`（`\x00` 填充） |
| 4 | ninja posix_spawn 被 SELinux 拦 | 子命令一律 127 | 改用 `-G "Unix Makefiles"`（fork+execve 可通） |
| 5 | CMAKE_SYSTEM_NAME=Android 读 NDK 路径 | cmake configure 失败 | `export PREFIX=$TCR` |
| 6 | payload "bash" 是 toybox sh | kotlinc `=~` 语法错误 | `pkg install bash`，脚本 shebang 指过去 |
| 7 | Android 禁 execve /data 上的脚本 | wrapper `No such file` | shebang 用绝对路径 + 解包的真 bash |
| 8 | 大小写 include（Windows 掩盖） | `window.h/VulKan/ImGui not found` | 符号链接 |
| 9 | MemoryPool `freeSlots` | clang `no member` | 改成 `freeSlots_` |
| 10 | vma/glm 被 .gitignore 吃掉 | `vk_mem_alloc.h not found` | googlesource 下载 + Termux glm |
| 11 | deb 并行下载截断 | `ContentTooShortError` | `curl -C -` 续传 + Size 校验 |
| 12 | pm 读不了 fuse/私有目录 | `Unable to open file` | APK 放 `/data/local/tmp` 再装 |
| 13 | 二进制补丁丢执行位 | `Permission denied` | 写完 `chmod +x` |
| 14 | 手机包比电脑包小近一半 | 疑似编译缺料 | 实为打包链路差异：电脑 `noCompress(ttf)` +13MB、双 ABI +2.5MB、AndroidX/dex +3.8MB、库资源 +1.3MB，引擎 `.so` 两边等价。详见第 11 节逐条目对账 |
| 15 | 手搓 Manifest 缺 `<uses-sdk>` | targetSdk=0 → 系统按 legacy 16:9 letterbox，全面屏左右黑边，任务 `nonResizable` | 补 `minSdkVersion=26 targetSdkVersion=36` + `resizeableActivity=true`（见 12.1） |
| 16 | 打包用占位纯色图标 | 桌面显示系统默认图标 | 从工程 `res/mipmap-*` 拷真图标重新 aapt2 compile/link；顺手补 `versionCode`/`roundIcon` |
| 17 | apksigner 裸调 `java` | `java: inaccessible or not found` | wrapper 里 `exec java -jar …`，PATH 必须含 `$JVM/bin`（不是只设 JAVA_HOME） |
| 18 | aapt2 按 cp437 转写非 ASCII assets 文件名 | 中文/日文资源解压后变乱码名，引擎按真名查找失败：MIDI 无声、`Pixel图片/` 贴图缺失，且无任何显式报错 | link 后签名前用 Python 把条目名 `encode('cp437').decode('utf-8')` 重写（见 13.3） |
| 19 | MIDI 依赖 SoundFont 先加载成功 | `IsMidiFontLoaded()==false` 时整个 `.mid` 枚举分支被跳过，BGM 静默消失 | 确认 `TimGM6mb.sf2` 路径存在且 load 返回 0；logcat 搜 `failed to load SoundFont` |

**验证方法速查**：
- letterbox/图标问题 → `dumpsys package <pkg> | grep versionCode`（出现 `versionCode=0 minSdk=1 targetSdk=0` 即 Manifest 裸奔）；
- 图标像素内容 → 纯 Python `zlib` 解码 PNG IHDR/IDAT 检查（沙箱无 PIL）；
- 非 ASCII 资源名 → `zipfile` 逐条目打印 `repr(filename)` 与磁盘真名字节比对；
- 音频是否在播 → `dumpsys media.audio_flinger | grep <pkg>` 看有无 active Track。

---

## 9. 资源与耗时

- 磁盘：工具链前缀 ~1.2GB，NDK zip 下载 669MB（只解 stub 用），APK 产物 21.8MB。
- 编译：232 个 C++ 文件 + 6 静态库 + 1 shared，`make -j8` 约 **3–4 分钟**（8 核）。
- 着色器：36 个 < 5 秒。
- 打包：Kotlin 编译 ~1 分钟，其余秒级。
- 网络：全程国内镜像（清华 Termux 源 + 腾讯云 Gradle + dl.google.com）实测 9–20 MB/s。

## 10. 可复现性说明

本文所有步骤均在荣耀 PPG-AN00（Android 17 / arm64-v8a）的 DeepSeek Harness 沙箱内**真实执行并验证**，非纸面推演。产物：

- `PC/build_android/libpixelclean.so`（5.9MB，AArch64，17 个 JNI 导出）
- `apkwork/PixelClean-signed.apk`（21.9MB，v2/v3 签名，已安装运行；已含第 12/13 节修复：真图标 + uses-sdk + 非 ASCII 文件名回转）
- `probe/android_shaders/*.spv`（36 个 SPIR-V）
- `shaders/compile_shaders.sh`（compile.bat 的 POSIX 移植）

换一台 arm64 + Android 8+ + 支持 Vulkan 的手机，按第 2→8 节顺序即可复刻；打包环节务必把第 12、13 节的三处修正（Manifest 补全、真图标、cp437 名字回转）直接纳入首版，不要等真机复查再补。

---

## 11. 手机 APK（21.8MB） vs 电脑 APK（41.1MB）体积差异对账

手机手搓链路产出的 `apkwork/PixelClean-signed.apk` 是 21.8MB，电脑端 Android Studio/Gradle 产出的参考包 `PixelClean安卓安装包参考原文件.apk` 是 41.1MB。用 Python `zipfile` 逐条目对比后确认：**这 19.3MB 的差值没有一字节是"手机编译质量差"造成的**——核心引擎 `libpixelclean.so` 两边产物等价（未压缩 6.20 vs 6.21MB，压缩后 2.17 vs 2.33MB）。差异全部来自**打包链路配置和依赖选择**。

### 11.1 逐项对账（压缩后体积）

| 差异项 | 手机版 | 电脑版 | Δ | 根因 |
|---|---|---|---|---|
| `assets/Resource/Minecraft_AE.ttf` | 3.09MB **DEFLATE** | 16.16MB **STORED** | **+13.1MB** | 电脑端 `build.gradle.kts` 的 `aaptOptions.noCompress("ttf", ...)` 把 16MB 字体原样塞包；手机手搓打包脚本压缩了它。**最大单项** |
| `lib/x86_64/libpixelclean.so` | 无 | 2.47MB | +2.5MB | 电脑 `abiFilters += listOf("arm64-v8a","x86_64")` 编了两个 ABI；手机 arm 真机只带 arm64-v8a |
| `classes.dex` | 0.68MB | 3.13MB | +2.5MB | 电脑 `dependencies` 带 `androidx.core-ktx` + `material`（传递拉进 appcompat/recyclerview/constraintlayout…）+ Kotlin stdlib，且 debug 构建 `isMinifyEnabled=false`；手机版手写 Manifest 用系统 `Theme.Material.NoActionBar` 绕开 AAR，kotlinc+d8 只编自己的类 |
| `resources.arsc` + `res/*` | ≈0（10 个 mipmap） | 0.97 + 0.37MB（819 个文件） | +1.3MB | 同上：AndroidX/Material 库自带资源、主题，经 AAPT2 混淆短名后的 xml/png |
| `kotlin/*`, `DebugProbesKt.bin`, `assets/dexopt/baseline.prof` | 无 | ~0.02MB | 微量 | AGP 注入的 Kotlin 元数据 + baseline profile，手搓链路没有 |
| `lib/arm64-v8a/libc++_shared.so` | 0.47MB | 无 | −0.5MB（手机独有） | 工程用了 c++_static，但 Termux 侧 imgui 等静态库链到 shared runtime（见 4.6 注）；电脑 NDK `c++_static` 静态链入故无此文件 |

两边**共同**的大头完全一致：`mp3` 5.57MB、`sf2` 5.97MB、`png` ~3.7–3.9MB 均 STORED（电脑版 3.89 含 app icon，手机版 3.73 为纯 assets），arm64 `.so` 也基本相同。说明资源拷贝和 C++ 编译产物都是等价正确的。

### 11.2 结论

手机链路本质产出了一个「极简 / release-like」包：**单 ABI、零 AndroidX、资源全压缩**；电脑链路是「带完整 AndroidX+Material 依赖、双 ABI、debug 未 minify、noCompress 名单原样存字体」的标准 Gradle 包。

### 11.3 若想让电脑包瘦下来（按收益排序）

1. **把 `ttf` 移出 `aaptOptions.noCompress` 名单** → 立省 ~13MB，收益最大。前提：运行时无需 `AssetFileDescriptor` 直接 mmap 字体偏移（ImGui FreeType 通常从流读取，压缩不影响）。
2. 只发真机 ABI：`abiFilters += "arm64-v8a"` → 省 2.5MB。
3. release 变体开 `isMinifyEnabled=true` + R8 + resource shrinking → dex 和 res 再压一大截。
4. `useLegacyPackaging=false`（配 `extractNativeLibs=false`）→ 额外省安装后磁盘占用。

反向操作（让手机包对齐电脑行为）：在手搓脚本里把 `Minecraft_AE.ttf` 改成 STORED——但那是往大了改，通常没必要。

### 11.4 复现对比的命令（沙箱内直接可用，无需 aapt2）

```python
import zipfile, collections
def scan(p):
    z=zipfile.ZipFile(p); c=collections.Counter()
    for i in z.infolist():
        ext=i.filename.rsplit('.',1)[-1].lower() if '.' in i.filename else '-'
        c[(ext,'STORED' if i.compress_type==0 else 'DEFLATE')]+=i.compress_size
    return c
a=scan('apkwork/PixelClean-signed.apk'); b=scan('PixelClean安卓安装包参考原文件.apk')
# 按 (扩展名, 压缩方式) 分桶，即可一眼定位 ttf 的 STORED/DEFLATE 差异、多出的 x86_64 .so、dex 膨胀
```

> **方法论要点**：APK 就是一个 zip，体积差异先用 `zipfile` 逐条目 diff（区分 `file_size` 原始 / `compress_size` 入包 / `compress_type` 0=STORED 8=DEFLATE），再按 `lib|assets|dex|arsc|res` 分类汇总。压缩方式不同（STORED vs DEFLATE）造成的差异，往往比代码本身大得多——本项目最大单项就是那个 13MB 的 `ttf`。

---

## 12. 真机复查发现的两个打包缺陷：缺图标 + 左右黑边（已修复验证）

安装首版手搓 APK 后真机复查发现：**① 桌面没有应用图标（系统默认机器人）；② 游戏画面不铺满屏幕，左右各 ~356px 黑边**（2800×1264 屏上画面只有 2223 宽，16:9 letterbox）。两个问题同根同源：**手搓 Manifest 写得太裸**。

### 12.1 根因诊断

| 现象 | 根因 | 证据 |
|---|---|---|
| 无应用图标 | 打包时用了临时 Python 生成的**纯色占位小图**（413 字节、整幅 #C83C28 单色），而工程 `MyApplication/app/src/main/res/mipmap-*/` 里其实躺着 19KB 的真图标 | 逐像素解码 `apkwork/res/mipmap-xxxhdpi/ic_launcher.png`：192×192 全部 36864 个像素同色 |
| 左右黑边 | Manifest 缺 `<uses-sdk>` → 系统按 **targetSdk=0** 处理，触发 legacy 兼容策略：任务被标记 `nonResizable`，在 19.5:9 屏上强制按 16:9 letterbox 显示 | 修复前 `dumpsys package`：`versionCode=0 minSdk=1 targetSdk=0`；`dumpsys activity`：`Task{... nonResizable}`；`mBounds=Rect(356,0 - 2579,1264)`（宽 2223 = 1264×16/9） |
| （同根因） | `versionCode` 缺失（=0）、缺 `android:roundIcon` 也一并补上 | 同上 |

> **教训**：手搓链路绕开 AGP 时，AGP 从 `defaultConfig` 自动注入到 Manifest 的字段（`versionCode`/`minSdk`/`targetSdk`）必须手写补全——缺一个 `targetSdkVersion` 就足以让系统在全面屏上给你的应用套 16:9 黑边。Kotlin/引擎代码里正确的刘海屏处理（`LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES`、`setDecorFitsSystemWindows(false)`）救不了这个——letterbox 发生在窗口层，SurfaceView 拿到的 `ANativeWindow_getWidth()` 已经是被裁过的 2223。

### 12.2 修复步骤（全部命令行可复现）

```bash
T=<TEST 根>   # 工作区
# 1) 换真图标（5 个密度）
SRC=PixelClean/MyApplication/app/src/main/res
for d in mdpi hdpi xhdpi xxhdpi xxxhdpi; do
  cp $SRC/mipmap-$d/ic_launcher.png $SRC/mipmap-$d/ic_launcher_round.png $T/apkwork/res/mipmap-$d/
done

# 2) Manifest 补齐：根节点 android:versionCode/versionName；
#    <uses-sdk android:minSdkVersion="26" android:targetSdkVersion="36"/>
#    application 加 android:roundIcon、android:resizeableActivity="true"（activity 同样加）
```

然后按第 6 节原链路重跑（注意 `apksigner` 需要把 `$JVM/bin` 加进 PATH，脚本里裸调 `java`）：

```bash
aapt2 compile --dir apkwork/res -o rebuild/res.zip
aapt2 link -o rebuild/base.apk -I android-sdk/android-36/android.jar \
       --manifest apkwork/AndroidManifest.xml rebuild/res.zip -A apkwork/assets
# Python zipfile 追加 classes.dex + lib/arm64-v8a/*.so（同 6.2 步骤 4）
env -i PATH=$JVM/bin:$R/bin:/system/bin HOME=$T apksigner sign \
    --ks debug.keystore --ks-pass pass:android --key-pass pass:android \
    --out rebuild/PixelClean-signed.apk rebuild/PixelClean.apk
```

### 12.3 真机验证结果

- `dumpsys package com.pixelclean` → `versionCode=1 minSdk=26 targetSdk=36`，`nonResizable` 标记消失；
- `aapt2 dump badging` → 5 个密度 `application-icon-*` 全部注册；
- 桌面图标显示为 PixelClean 真实图标；
- 游戏截图：**2800×1264 完整铺满，左右黑边完全消失**（旧图 356px 黑边 → 新图 0）。

产物：`apkwork/PixelClean-signed.apk`（21.9MB，v2/v3 签名，已安装）。

---

## 13. 背景音乐（夜に駆ける.mid）没有声音：aapt2 中文文件名双重编码坑

### 13.1 症状与排查链路

游戏菜单 BGM（`Resource/Music/夜に駆ける.mid`，引擎经 SoLoud + TimGM6mb.sf2 软合成播放）完全无声，但 MP3 音效路径正常。排查：

1. 文件确实在 APK 里，大小 57786 字节无误——但条目名是 **`σñ£πü½Θºåπüæπéï.mid`**（乱码）；
2. 引擎加载链：`AudioEngine::Initialize()` 先 load `TimGM6mb_sf2`（宏展开 = `"./Resource/Music/TimGM6mb.sf2"`，**相对路径**）；`SpatialAudio::LoadAllResources()` 用 `TOOL::FilePath("./Resource/Music", …, "mid", …)` **枚举目录**收集 `.mid` 文件名再逐个 `LoadMidi(名, 路径, 音色库)`；`PlaySimple(u8"夜に駆ける", MIDI, …)` 按**名字**查 bank。任何一环文件名对不上，MIDI 就静默缺席（MP3 是纯 ASCII 名，不受影响）。

### 13.2 根因：aapt2 按 cp437 解读非 ASCII 文件名

`aapt2 link -A apkwork/assets` 打包 assets 时，把磁盘上的 UTF-8 文件名原始字节**按 cp437 码表转写**进 zip 条目名，并（错误地）置 UTF-8 标志位。效果：

- 磁盘真名：`夜に駆ける.mid` = UTF-8 字节 `E5 A4 9C E3 81 AB E9 A7 86 E3 81 91 E3 82 8B .mid`
- APK 内条目：这些字节被当作 cp437 字符再编码成 UTF-8 → `σñ£πü½Θºåπüæπéï.mid`

Kotlin `extractAssets()` 解压后磁盘上就是乱码名；引擎枚举出的 `SoundMidi[0]` 是乱码串，与代码里写死的 `u8"夜に駆ける"` 查不到 → `PlaySimple asset not found` → 无声。电脑 AGP 链路（`PixelClean安卓安装包参考原文件.apk`）条目名字节是**正确的原始 UTF-8**（且未置 UTF-8 标志，Android 按本地编码读恰好正确），所以电脑包有声。

受影响的不止这一个文件：本次重建包共 **42 个条目**中招（`夜に駆ける.mid` + 整个 `assets/BlockS/Pixel图片/` 41 个 png——这些方块贴图在迷宫模式同样加载不到）。

### 13.3 修复：打包后用 Python 重写条目名

在 `aapt2 link` 之后、签名之前，加一步名字修复（zip 条目名 `encode('cp437')` 即可还原 aapt2 读入的原始 UTF-8 字节，再按 UTF-8 解码写回）：

```python
import zipfile
zin=zipfile.ZipFile('PixelClean.apk'); zout=zipfile.ZipFile('PixelClean-namefix.apk','w')
for item in zin.infolist():
    name=item.filename
    if any(ord(c)>127 for c in name):
        try: name=name.encode('cp437').decode('utf-8')   # 还原真名
        except Exception: pass
    zi=zipfile.ZipInfo(name, date_time=item.date_time)
    zi.compress_type=item.compress_type; zi.external_attr=item.external_attr
    zout.writestr(zi, zin.read(item.filename))
zout.close()
# 然后再 apksigner sign
```

Python `zipfile` 写非 ASCII 名会自动置 UTF-8 标志位，Android `AssetManager`/解压均按 UTF-8 处理，正确。

### 13.4 验证

- 重装后（`extractAssets` 以 `lastUpdateTime` 为目录戳，重装必然重新解压），`dumpsys media.audio_flinger` 出现 `Track… (active) uid:<com.pixelclean> actual_seconds:16+`，44100Hz 立体声持续输出——BGM 恢复；
- logcat 无 `Failed to load MIDI / SoundFont` 报错；
- 最终包 `apkwork/PixelClean-signed.apk`（21.9MB）。

> **通用教训**：手搓链路凡是让 aapt2/zip 工具处理**非 ASCII 文件名**（中文/日文资源是国产游戏引擎常态），打包完必须逐条目核对名字字节：`zipfile` 读出来名字肉眼乱码 = 中招。修复放在 link 后、签名前，一次 `cp437→utf-8` 回转即可。

---

## 14. 一键部署脚本 setup-pixelclean-env.sh（把第 2 节固化成可执行）

第 2 节的环境搭建过程已固化为 **`setup-pixelclean-env.sh`**（本仓库根目录），全新手机一条命令从零部署整套工具链。已在荣耀 PPG-AN00 上端到端验证：92 个 deb 解包 → RUNPATH 烧录 823 个 ELF → 补丁/wrapper → android.jar → 冒烟测试（clang 交叉链接 / kotlinc 编译运行 / aapt2 / cmake / d8 / apksigner / glslang 全部正常）。

### 14.1 用法

```bash
sh setup-pixelclean-env.sh                  # 部署到脚本所在目录（noexec 时自动回退，见 14.2）
sh setup-pixelclean-env.sh --prefix <dir>   # 指定部署目录（必须可执行 ELF）
sh setup-pixelclean-env.sh --only-lite      # 只装打包链（aapt2/d8/apksigner/kotlin/jdk），跳过 clang 全家桶
sh setup-pixelclean-env.sh --check          # 只校验已部署环境

# 部署完成后激活：
. <部署目录>/env.sh
# 生成物：<部署目录>/android-toolchain.cmake（第 4 节 toolchain 的路径已自动填好）
```

目录布局与本文一致：`probe/root/data/data/com.termux/files/usr/`（工具链前缀）、`android-sdk/android-36/android.jar`、`env.sh`、`android-toolchain.cmake`、`setup.log`。

### 14.2 脚本化过程中新踩的 5 个坑（手工搭建时不存在或方式不同）

| # | 坑 | 症状 | 解法（脚本已内置） |
|---|---|---|---|
| 20 | deb 文件名含 `:`（epoch 版本号，如 `openssl_1:3.6.5_aarch64.deb`） | fuse 共享存储上 `Operation not permitted`，DLFAIL 反复重试 | 落盘名 sanitize（`:`→`_`）；另加 `probe/installed.txt` 断点标记，重跑秒续 |
| 21 | `/storage/emulated/0` 是 **noexec** | 工具全部解好但 `can't execute: Permission denied`（rc=126） | 启动时 exec 探针自检，失败自动回退 `/data/user/0/<宿主app>/files/<目录名>-env`（ext4） |
| 22 | 沙箱预置 `LD_LIBRARY_PATH=payload/runtime/lib`（旧 libc++） | aapt2/cmake 报 `cannot locate symbol "_ZNSt6__ndk113__hash_memoryEPKvm"`——正是坑 #2 的镜像面：不是 Termux 库污染系统，而是**宿主库污染 Termux** | 生成的 `env.sh` 里 `unset LD_LIBRARY_PATH`（RUNPATH 已烧录，根本不需要它） |
| 23 | patchelf 自举死锁 | patchelf 自己依赖 `$TCR/lib/libc++.so`，而烧 RUNPATH 正是它的活 | 仅阶段 2 给 patchelf 子进程临时 `LD_LIBRARY_PATH=$TCR/lib`（Python `env=` 传参，不导出给 shell） |
| 24 | JVM `java.io.tmpdir` 烧死为 Termux `/usr/tmp`（不存在） | kotlinc2 报 `jansi-…so.lck (No such file or directory)` | wrapper 加 `-Djava.io.tmpdir=$TCR/tmp` + `mkdir -p`（jansi 警告不影响编译，但消除噪音） |

### 14.3 探针实现细节（坑 #21 的可移植写法）

toybox 按 `argv[0]` 的 basename 分发命令，直接复制改名执行会因命令名未知而 rc≠0，与 noexec 无法区分。正确做法：复制 toybox 为 `true` 再执行——

```sh
cp /system/bin/toybox "$DIR/.exeprobe/true" && chmod 700 "$DIR/.exeprobe/true"
"$DIR/.exeprobe/true"   # rc=0 → 可执行；rc=126 (Permission denied) → noexec
```

### 14.4 与手工搭建的差异说明

- 脚本把 stub 库来源从"解 NDK zip"（669MB）改为"复制 `/system/lib64/*.so`"（几 MB）：链接期只需符号表，真机系统库天然与设备一致，还省掉 NDK 下载；`libandroid/libOpenSLES/libmediandk/libbinder_ndk` 由 `libandroid-stub` deb 直接提供；
- `PREFIX=$TCR`（坑 #5）由 toolchain 文件生成时一并规避；
- shebang 重写（坑 #7）自动扫描 `$TCR/bin` 全部脚本，指向解包的真 bash（坑 #6）。
