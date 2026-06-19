# 粒子系统重构 - 方案 D（Component-Based Pool）详细执行计划

> 本文档基于 [ParticleSystem_Redesign.md](file:///c:/Github/PixelClean/ParticleSystem_Redesign.md) 中的方案 D，结合对现有代码库的完整研究，给出可落地的分阶段执行步骤。
>
> **目标架构**：将单一 `ParticleSystem` 拆分为 `ParticleWorld`（facade）+ `ParticlePool` / `ParticleEmitter` / `ParticleUpdater` / `ParticleRenderer` 四个独立组件，采用 SSBO + Instanced Rendering，支持 CPU/GPU 更新可切换，并保持对外接口兼容。

---

## 一、现状与约束（执行前必读）

### 1.1 现有架构关键事实

| 事实 | 位置 | 对方案 D 的影响 |
|------|------|----------------|
| `ParticleSystem` 持有 `PileUp<Particle>* mParticle` 粒子池 | [ParticleSystem.h](file:///c:/Github/PixelClean/Arms/ParticleSystem.h) | 需替换为 `ParticlePool` |
| `Arms`（子弹系统）与 `ParticlesSpecialEffect` **共享同一个 `mParticle` 池** | [Arms.cpp](file:///c:/Github/PixelClean/Arms/Arms.cpp) `ShootBullets`/`DeleteBullet` | 迁移必须同时覆盖两者，否则池语义不一致 |
| `Buffer` 仅有 Vertex/Index/Uniform/Stage 四种工厂方法，**无 SSBO** | [buffer.cpp](file:///c:/Github/PixelClean/Vulkan/buffer.cpp) | 必须新增 `createStorageBuffer` |
| `MainPipeline` 的 DescriptorSetLayout 仅 3 个 binding（VP UBO / Object UBO / Sampler），**无 SSBO、无 push constant** | [CreatePipeline.cpp](file:///c:/Github/PixelClean/VulkanTool/CreatePipeline.cpp) | 需新增粒子专用 Pipeline + Layout |
| 粒子复用 `lessionShader.vert/frag`，**不支持 `gl_InstanceIndex`、无 SSBO** | [lessionShader.vert](file:///c:/Github/PixelClean/shaders/lessionShader.vert) | 需新增 `particle.vert/frag` |
| `SpecialEffectsEvent` 中 `DeleteSpecialEffects(i)` 后 `i` 未回退，swap-and-pop 跳过元素 | [ParticlesSpecialEffect.cpp](file:///c:/Github/PixelClean/Arms/ParticlesSpecialEffect.cpp) L62-79 | 重构时顺带修复 |
| `PileUp::~PileUp()` / `ContinuousData::~ContinuousData()` 误用 `delete`（应 `delete[]`） | [PileUp.h](file:///c:/Github/PixelClean/Tool/PileUp.h) L21 / [ContinuousData.h](file:///c:/Github/PixelClean/Tool/ContinuousData.h) L23 | 新组件不复用，可绕过 |
| `PixelTexture::ModifyImage` 每次 `submitSync`，粒子生成瓶颈 | [PixelTexture.cpp](file:///c:/Github/PixelClean/VulkanTool/PixelTexture.cpp) L84-87 | 方案 D 用 SSBO 传颜色，彻底移除 `PixelTexture` |
| `application.cpp` 第 797 行 ImGui 显示 `mParticleSystem->mParticle->GetNumber()` | [application.cpp](file:///c:/Github/PixelClean/application.cpp) | facade 需提供等价查询接口 |
| GameMod（TankTrouble/MazeMods/UnlimitednessMapMods）使用粒子系统 | `c:\Github\PixelClean\GameMods\*` | 需逐一检查调用点 |

### 1.2 必须保持兼容的对外接口

以下接口被外部调用，方案 D 的 `ParticleWorld` facade 必须保持签名不变：

```cpp
// ParticleSystem 对外（application.cpp 调用）
void RecordingCommandBuffer(RenderPass*, SwapChain*, Pipeline*);
void GetCommandBuffer(std::vector<VkCommandBuffer>*, unsigned int F);
void resetThreadCommandPools();
void ThreadUpdateCommandBuffer();

// ParticlesSpecialEffect 对外（Arms 调用）
void GenerateSpecialEffects(double x, double y, unsigned char* colour, double angle, double speed);
void DeleteSpecialEffects(unsigned int index);
void SpecialEffectsEvent(unsigned int Fndex, double time);
```

### 1.3 方案 D 的设计取舍

方案 D 在原设计文档中被标记为"过度设计"。本计划采取**渐进式组件化**策略：
- **必做**：拆分 Pool/Emitter/Updater/Renderer 四组件 + SSBO + Instanced（获得方案 A 的全部性能收益）
- **可选**：GPU Updater（Compute Shader）作为 `ParticleUpdater` 的一个实现，**首期不实现**，仅保留接口
- **不做**：不引入 ECS、不引入脚本化配置、不引入多 World 实例（当前只有一个 World）

这样既获得组件化的扩展性，又避免过度设计。

---

## 二、目标架构

### 2.1 组件关系图

```
┌─────────────────────────────────────────────────────────────────┐
│                       ParticleWorld (facade)                     │
│  - 持有 Pool / Emitter / Updater / Renderer 的所有权             │
│  - 对外暴露与旧 ParticleSystem 兼容的接口                        │
│  - 协调组件间数据流                                              │
└───┬───────────┬───────────────┬───────────────┬───────────────┬─┘
    │           │               │               │               │
    ▼           ▼               ▼               ▼               ▼
┌────────┐ ┌──────────┐  ┌─────────────┐ ┌─────────────┐ ┌──────────┐
│Particle│ │Particle  │  │Particle     │ │Particle     │ │Particle  │
│Pool    │ │Emitter   │  │Updater(CPU) │ │Renderer     │ │Data(SOAP)│
│        │ │          │  │ /Updater(GPU│ │              │ │          │
│索引池  │ │发射规则  │  │  可选)      │ │Instanced Draw│ │CPU镜像   │
└────────┘ └──────────┘  └─────────────┘ └─────────────┘ └──────────┘
    │           │               │               │               │
    └───────────┴───────────────┴───────────────┴───────────────┘
                              共享
                                 │
                                 ▼
                    ┌─────────────────────────┐
                    │   GPU SSBO (Instance)   │
                    │  [pos,color,scale,angle]│
                    │  持久映射，每帧 memcpy   │
                    └─────────────────────────┘
                                 │
                                 ▼
                    ┌─────────────────────────┐
                    │  1 个 DescriptorSet     │
                    │  binding0: VP UBO       │
                    │  binding1: Instance SSBO│
                    └─────────────────────────┘
                                 │
                                 ▼
                    ┌─────────────────────────┐
                    │  1 次 vkCmdDrawIndexed  │
                    │  (6, activeCount)       │
                    └─────────────────────────┘
```

### 2.2 数据流

```
[Emitter] --allocate(index)--> [Pool]  (返回空闲索引)
[Emitter] --write CPU data--> [ParticleDataCPU[index]]
[Updater] --每帧读取/修改--> [ParticleDataCPU[index]]  (位置/缩放/生命周期)
[Updater] --死亡时--> [Pool.free(index)] + [Renderer: 标记实例无效]
[Renderer] --每帧--> 把 ParticleDataCPU 拷贝到 SSBO 映射内存
[Renderer] --录制--> 1 次 bind + 1 次 draw(instanceCount=activeCount)
```

### 2.3 文件落点

所有新组件放在 `c:\Github\PixelClean\Arms\Particle\` 子目录下，由 `Arms/CMakeLists.txt` 的 `GLOB_RECURSE` 自动收录，**无需修改构建系统**。

```
c:\Github\PixelClean\Arms\
├── ParticleSystem.h              ← 保留为兼容外壳（内部委托给 ParticleWorld）
├── ParticleSystem.cpp            ← 保留为兼容外壳
├── ParticlesSpecialEffect.h/.cpp ← 保留为兼容外壳（委托给 Emitter+Updater）
├── Arms.h/.cpp                   ← 迁移调用点
└── Particle\                     ← 新增目录
    ├── ParticleCommon.h          ← 公共结构体（ParticleInstanceData 等）
    ├── ParticlePool.h/.cpp       ← 索引池
    ├── ParticleEmitter.h/.cpp    ← 发射器
    ├── ParticleUpdater.h/.cpp    ← CPU 更新器
    ├── ParticleUpdaterGPU.h/.cpp ← GPU 更新器（占位，首期空实现）
    ├── ParticleRenderer.h/.cpp   ← 实例化渲染器
    └── ParticleWorld.h/.cpp      ← facade
```

Shader 新增：
```
c:\Github\PixelClean\shaders\
├── particle.vert                 ← 新增
├── particle.frag                 ← 新增
└── compile.bat                   ← 追加编译行
c:\Github\PixelClean\FilePath.h   ← 追加 spv 路径宏
```

---

## 三、核心数据结构设计

### 3.1 GPU 实例数据（SSBO 元素）

```cpp
// c:\Github\PixelClean\Arms\Particle\ParticleCommon.h
#pragma once
#include <glm/glm.hpp>

namespace GAME {

// SSBO 中单个粒子的数据，std140 对齐
// 总大小: 64 (mat4) + 16 (vec4) = 80 字节，满足 16 字节对齐
struct ParticleInstanceData {
    glm::mat4 modelMatrix;   // 64 字节
    glm::vec4 color;         // 16 字节 (RGBA)
};

// CPU 侧粒子属性（用于 Updater 计算）
struct ParticleCPUData {
    float x{0.0f};
    float y{0.0f};
    float speed{0.0f};
    float angle{0.0f};       // 运动方向（弧度）
    float zoom{1.0f};        // 缩放
    float zoomDecay{0.3f};   // 缩放衰减系数（原硬编码 0.3f）
    glm::vec4 color{1.0f};   // RGBA
    bool  alive{false};      // 是否活跃
};

// 粒子最大数量上限（SSBO 大小据此分配）
constexpr uint32_t MAX_PARTICLES = 8192;

} // namespace GAME
```

### 3.2 ParticlePool（索引池）

```cpp
// c:\Github\PixelClean\Arms\Particle\ParticlePool.h
#pragma once
#include <vector>
#include <cstdint>

namespace GAME {

// 线程安全的索引池（LIFO 栈）
// 替代旧的 PileUp<Particle>，但只存索引，不存指针，无浅拷贝风险
class ParticlePool {
public:
    explicit ParticlePool(uint32_t capacity);

    // 分配一个索引，返回 -1 表示池空
    int32_t allocate();

    // 归还索引
    void free(uint32_t index);

    uint32_t capacity() const { return mCapacity; }
    uint32_t freeCount() const { return static_cast<uint32_t>(mFreeList.size()); }
    uint32_t activeCount() const { return mCapacity - freeCount(); }

private:
    uint32_t              mCapacity;
    std::vector<uint32_t> mFreeList;   // 空闲索引栈
};

} // namespace GAME
```

**设计要点**：
- 只存索引（`uint32_t`），彻底消除 `Particle` 含 raw pointer 的浅拷贝问题
- LIFO 栈式分配，缓存友好
- 首期不加锁（粒子发射/回收均在主线程）；若后续 Updater 迁移到 GPU 再考虑

### 3.3 ParticleEmitter（发射器）

```cpp
// c:\Github\PixelClean\Arms\Particle\ParticleEmitter.h
#pragma once
#include "ParticleCommon.h"
#include "ParticlePool.h"
#include <vector>

namespace GAME {

// 负责粒子发射规则。当前只实现"单点发射"（兼容旧 GenerateSpecialEffects）
// 后续可派生出 ExplosionEmitter / TrailEmitter / EnvironmentEmitter
class ParticleEmitter {
public:
    ParticleEmitter(ParticlePool& pool, std::vector<ParticleCPUData>& cpuData);

    // 兼容旧 GenerateSpecialEffects 的接口
    // colour 指向 4 字节 RGBA
    // 返回分配到的索引，-1 表示池满
    int32_t emit(float x, float y, const unsigned char colour[4],
                 float angle, float speed);

private:
    ParticlePool&                  mPool;
    std::vector<ParticleCPUData>&  mCPUData;
};

} // namespace GAME
```

### 3.4 ParticleUpdater（更新器）

```cpp
// c:\Github\PixelClean\Arms\Particle\ParticleUpdater.h
#pragma once
#include "ParticleCommon.h"
#include "ParticlePool.h"
#include <vector>
#include <functional>

namespace GAME {

// CPU 更新器：复刻旧 SpecialEffectsEvent 的运动学
class ParticleUpdater {
public:
    using DeathCallback = std::function<void(uint32_t index)>;

    ParticleUpdater(ParticlePool& pool, std::vector<ParticleCPUData>& cpuData);

    // 每帧调用
    // time: 帧时间（秒）
    // onDeath: 粒子死亡时回调（Emitter/Renderer 用它清理）
    void update(double time, DeathCallback onDeath);

private:
    ParticlePool&                  mPool;
    std::vector<ParticleCPUData>&  mCPUData;
};

} // namespace GAME
```

**修复旧 bug**：`update` 内部遍历时若触发死亡，采用"交换到末尾再删除"或"反向遍历"策略，避免旧代码 `i` 未回退导致的跳过问题（详见 [ParticlesSpecialEffect.cpp](file:///c:/Github/PixelClean/Arms/ParticlesSpecialEffect.cpp) L71-73）。

### 3.5 ParticleRenderer（渲染器）

```cpp
// c:\Github\PixelClean\Arms\Particle\ParticleRenderer.h
#pragma once
#include "ParticleCommon.h"
#include "ParticlePool.h"
#include <vulkan/vulkan.h>
#include <vector>

namespace VulKan { class Buffer; class DescriptorSet; class Pipeline; class Device; class CommandPool; }

namespace GAME {

class ParticleRenderer {
public:
    ParticleRenderer(VulKan::Device* device,
                     VulKan::CommandPool* commandPool,
                     uint32_t frameCount,
                     uint32_t maxParticles);
    ~ParticleRenderer();

    // 每帧录制前：把 CPU 数据写入 SSBO
    void uploadInstanceData(const std::vector<ParticleCPUData>& cpuData,
                            uint32_t activeCount,
                            uint32_t frameIndex);

    // 录制绘制命令（1 次 bind + 1 次 draw）
    void recordDraw(VkCommandBuffer cmd,
                    VkPipelineLayout layout,
                    VkPipeline pipeline,
                    VkDescriptorSet descriptorSet,
                    const std::vector<VkBuffer>& vertexBuffers,
                    VkBuffer indexBuffer,
                    uint32_t indexCount,
                    uint32_t instanceCount);

    // 资源访问（供 ParticleWorld 组装 DescriptorSet）
    VulKan::Buffer* getInstanceBuffer(uint32_t frameIndex) const { return mInstanceBuffers[frameIndex]; }
    uint32_t        getMaxParticles() const { return mMaxParticles; }

private:
    VulKan::Device*       mDevice;
    uint32_t              mFrameCount;
    uint32_t              mMaxParticles;
    // 每帧一个 SSBO（双/多缓冲，避免帧内写冲突）
    std::vector<VulKan::Buffer*> mInstanceBuffers;
    void*                 mMappedPtr[3] = {nullptr}; // 持久映射指针（按帧）
};

} // namespace GAME
```

### 3.6 ParticleWorld（facade）

```cpp
// c:\Github\PixelClean\Arms\Particle\ParticleWorld.h
#pragma once
#include "ParticleCommon.h"
#include "ParticlePool.h"
#include "ParticleEmitter.h"
#include "ParticleUpdater.h"
#include "ParticleRenderer.h"
#include <vector>
#include <memory>

namespace VulKan { class Device; class CommandPool; class DescriptorSet; class Pipeline; class RenderPass; class SwapChain; class Buffer; class Sampler; }

namespace GAME {

class ParticleWorld {
public:
    ParticleWorld(VulKan::Device* device,
                  VulKan::CommandPool* commandPool,
                  uint32_t frameCount,
                  uint32_t maxParticles = MAX_PARTICLES);
    ~ParticleWorld();

    // ===== 对外兼容接口（与旧 ParticleSystem 对齐）=====
    // 供 ParticlesSpecialEffect 兼容层调用
    int32_t emit(float x, float y, const unsigned char colour[4],
                 float angle, float speed);
    void    kill(uint32_t index);
    void    update(double time);                    // 对应 SpecialEffectsEvent
    uint32_t activeCount() const { return mPool.activeCount(); }
    uint32_t freeCount() const { return mPool.freeCount(); }   // 兼容旧 mParticle->GetNumber()

    // ===== 渲染接口（对应旧 ParticleSystem）=====
    void recordCommandBuffer(VulKan::RenderPass* rp, VulKan::SwapChain* sc, VulKan::Pipeline* pipe);
    void getCommandBuffers(std::vector<VkCommandBuffer>* out, uint32_t frameIndex);
    void resetCommandPools();

private:
    VulKan::Device*       mDevice;
    uint32_t              mFrameCount;

    ParticlePool          mPool;
    std::vector<ParticleCPUData> mCPUData;   // 固定大小 = maxParticles
    std::unique_ptr<ParticleEmitter>  mEmitter;
    std::unique_ptr<ParticleUpdater>  mUpdater;
    std::unique_ptr<ParticleRenderer> mRenderer;

    // 渲染资源（VP UBO 由外部传入，DescriptorSet/Pipeline 由 World 持有或外部传入）
    // 具体所有权在 Phase 4 确定
};

} // namespace GAME
```

---

## 四、Vulkan 底层扩展

### 4.1 新增 `Buffer::createStorageBuffer`

在 [buffer.h](file:///c:/Github/PixelClean/Vulkan/buffer.h) / [buffer.cpp](file:///c:/Github/PixelClean/Vulkan/buffer.cpp) 新增：

```cpp
// buffer.h 新增声明
static Buffer* createStorageBuffer(
    Device* device,
    VkDeviceSize size,
    void* pData = nullptr,
    bool persistentMapping = false);
```

```cpp
// buffer.cpp 新增实现
Buffer* Buffer::createStorageBuffer(Device* device, VkDeviceSize size,
                                    void* pData, bool persistentMapping) {
    Buffer* buf = new Buffer(device);
    buf->mBufferSize = size;
    buf->mUsage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
                | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    buf->mMemoryProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                           | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    // 复用现有 VMA 创建逻辑（参考 createUniformBuffer）
    buf->createBuffer(pData);
    if (persistentMapping) buf->map();
    return buf;
}
```

**验证**：创建后用 `vkGetBufferMemoryRequirements` 检查 usage 标志正确；映射写入 4 字节测试数据，GPU 读取无校验错误。

### 4.2 新增粒子专用 DescriptorSetLayout

在 [CreatePipeline.cpp](file:///c:/Github/PixelClean/VulkanTool/CreatePipeline.cpp) 新增 `ParticlePipeline` 函数，Layout 配置：

| Binding | 类型 | Stage | 说明 |
|---------|------|-------|------|
| 0 | UBO (VPMatrices) | VERTEX | 共享 VP 矩阵 |
| 1 | SSBO (ParticleInstanceData[]) | VERTEX | 所有粒子实例数据 |

**移除** binding 1 的 ObjectUniform UBO 和 binding 2 的 Sampler（颜色改走 SSBO 的 `vec4 color`）。

### 4.3 Pipeline 实例化配置

`VkPipelineVertexInputState` 配置：
- binding 0: stride=12 (vec3 position), `VK_VERTEX_INPUT_RATE_VERTEX`
- binding 1: stride=8 (vec2 UV), `VK_VERTEX_INPUT_RATE_VERTEX`
- **不使用 instance binding**（实例数据从 SSBO 读，靠 `gl_InstanceIndex` 索引）

`VkPipelineInputAssemblyState`: `VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST`（不变）

混合状态、深度测试、背面剔除保持与 `MainPipeline` 一致。

---

## 五、Shader 设计

### 5.1 particle.vert

```glsl
#version 450

layout(location = 0) in vec3  inPosition;
layout(location = 2) in vec2  inUV;

layout(location = 0) out vec2  fragUV;
layout(location = 1) out vec4  fragColor;

layout(binding = 0) uniform VPMatrices {
    mat4 mViewMatrix;
    mat4 mProjectionMatrix;
} vpUBO;

struct ParticleInstanceData {
    mat4 modelMatrix;
    vec4 color;
};

layout(std140, binding = 1) readonly buffer InstanceBuffer {
    ParticleInstanceData instances[];
};

void main() {
    ParticleInstanceData inst = instances[gl_InstanceIndex];
    gl_Position = vpUBO.mProjectionMatrix * vpUBO.mViewMatrix
                * inst.modelMatrix * vec4(inPosition, 1.0);
    fragUV    = inUV;
    fragColor = inst.color;
}
```

### 5.2 particle.frag

```glsl
#version 450

layout(location = 0) in  vec2 fragUV;
layout(location = 1) in  vec4 fragColor;
layout(location = 0) out vec4 outColor;

void main() {
    // 旧 shader 用 1×1 纹理采样得到纯色，现在直接用顶点传入颜色
    // UV 在 [0,1] 四边形内，无需纹理
    outColor = fragColor;
}
```

### 5.3 Shader 注册

1. 在 [compile.bat](file:///c:/Github/PixelClean/shaders/compile.bat) 追加：
   ```bat
   %GLSLANG% -V particle.vert -o %OUTPUT_DIR%\particle_vert.spv
   %GLSLANG% -V particle.frag -o %OUTPUT_DIR%\particle_frag.spv
   ```
2. 在 [FilePath.h](file:///c:/Github/PixelClean/FilePath.h) 追加路径宏：
   ```cpp
   #define PARTICLE_VERT_SHADER_PATH SHADER_DIR "particle_vert.spv"
   #define PARTICLE_FRAG_SHADER_PATH SHADER_DIR "particle_frag.spv"
   ```

---

## 六、分阶段执行计划

### Phase 0：准备与基线（无功能变更）

**目标**：建立可回滚的基线，记录重构前性能数据。

**步骤**：
1. 用 git 创建重构分支 `refactor/particle-component`。
2. 在 `application.cpp` 的 ImGui 面板增加：DrawCall 计数、帧时间、活跃粒子数（已有 `mParticle->GetNumber()`，补充活跃数）。
3. 跑一次基准场景（如 TankTrouble 大量爆炸），记录：帧时间、DrawCall 数、内存占用。
4. 截图保存为基线，用于 Phase 7 对比。

**验证**：基线场景可稳定运行 60s 无崩溃。

**产出**：基线性能数据（写入 commit message 或临时笔记，不入库）。

---

### Phase 1：Vulkan 底层扩展

**目标**：提供 SSBO 能力，不触碰业务代码。

**步骤**：
1. 在 [buffer.h](file:///c:/Github/PixelClean/Vulkan/buffer.h) / [buffer.cpp](file:///c:/Github/PixelClean/Vulkan/buffer.cpp) 新增 `createStorageBuffer`（见 4.1）。
2. 在 [description.h](file:///c:/Github/PixelClean/Vulkan/description.h) 确认 `UniformParameter` 已支持 `VK_DESCRIPTOR_TYPE_STORAGE_BUFFER`（研究报告显示 DescriptorSet 已走 `pBufferInfo` 分支，应已支持，需验证）。
3. 写一个最小测试：在 `application.cpp` 临时创建一个 SSBO，写入 16 字节，用 compute shader 或临时 pipeline 读取并验证（可选，若信任实现可跳过）。
4. 编译通过，原有场景行为不变。

**验证**：
- `Buffer::createStorageBuffer` 创建成功，`vkGetBufferMemoryRequirements` 返回的 usage 含 `STORAGE_BUFFER_BIT`。
- 持久映射指针非空，写入后 GPU 可读。
- 全量回归：原有粒子场景行为与 Phase 0 完全一致。

**回滚**：仅新增方法，未修改旧路径，可直接 revert 单个 commit。

---

### Phase 2：Shader 与 Pipeline

**目标**：新增粒子专用 shader 和 pipeline，但不接入业务。

**步骤**：
1. 新增 [shaders/particle.vert](file:///c:/Github/PixelClean/shaders/particle.vert) / [shaders/particle.frag](file:///c:/Github/PixelClean/shaders/particle.frag)（见第五章）。
2. 修改 [shaders/compile.bat](file:///c:/Github/PixelClean/shaders/compile.bat) 追加编译行。
3. 修改 [FilePath.h](file:///c:/Github/PixelClean/FilePath.h) 追加 spv 路径宏。
4. 在 [PipelineS.h](file:///c:/Github/PixelClean/VulkanTool/PipelineS.h) 的 `PipelineMods` 枚举新增 `ParticleMods`。
5. 在 [CreatePipeline.cpp](file:///c:/Github/PixelClean/VulkanTool/CreatePipeline.cpp) 新增 `ParticlePipeline` 函数：
   - DescriptorSetLayout: binding 0 = VP UBO, binding 1 = Instance SSBO
   - 顶点输入：复用 MainPipeline 的 position/UV binding
   - 其余状态与 MainPipeline 一致
6. 在 [PipelineS.cpp](file:///c:/Github/PixelClean/VulkanTool/PipelineS.cpp) 的 switch 中增加 `ParticleMods` 分支调用 `ParticlePipeline`。
7. 编译 shader，确认 spv 生成。

**验证**：
- `compile.bat` 执行无报错，spv 文件生成。
- `ParticlePipeline` 创建的 `VkPipeline` 句柄非空。
- 全量回归：原有场景行为不变（新 pipeline 未被使用）。

**回滚**：revert Phase 2 的所有 commit。

---

### Phase 3：核心组件实现

**目标**：实现 Pool/Emitter/Updater/Renderer 四个组件，独立可测，不接入业务。

**步骤**：
1. 创建 `c:\Github\PixelClean\Arms\Particle\` 目录。
2. 实现 [ParticleCommon.h](file:///c:/Github/PixelClean/Arms/Particle/ParticleCommon.h)（结构体定义，见 3.1）。
3. 实现 [ParticlePool.h/.cpp](file:///c:/Github/PixelClean/Arms/Particle/ParticlePool.h)（见 3.2）：
   - 构造时 `mFreeList = [0, 1, ..., capacity-1]`
   - `allocate()` pop_back，空则返回 -1
   - `free(index)` push_back
4. 实现 [ParticleEmitter.h/.cpp](file:///c:/Github/PixelClean/Arms/Particle/ParticleEmitter.h)（见 3.3）：
   - `emit()` 调用 `mPool.allocate()`，写入 `mCPUData[index]`
   - 颜色从 `unsigned char[4]` 转 `glm::vec4`（除以 255.0f）
5. 实现 [ParticleUpdater.h/.cpp](file:///c:/Github/PixelClean/Arms/Particle/ParticleUpdater.h)（见 3.4）：
   - 复刻旧运动学：`x += speed*time*cos(angle)`, `y += speed*time*sin(angle)`, `zoom -= time*zoomDecay`
   - **修复迭代 bug**：死亡时用 swap-and-pop，但循环用 `while(i < n)` 且死亡时不 `++i`（因为末尾元素交换到了 i 位置，需重新检查）
   - 死亡回调通知外部
6. 实现 [ParticleRenderer.h/.cpp](file:///c:/Github/PixelClean/Arms/Particle/ParticleRenderer.h)（见 3.5）：
   - 构造时为每帧创建一个 SSBO（`createStorageBuffer`，持久映射）
   - `uploadInstanceData()`：遍历活跃粒子，把 `ParticleCPUData` 转成 `ParticleInstanceData`（构建 modelMatrix = translate * rotate * scale），memcpy 到映射内存
   - `recordDraw()`：bindPipeline + bindVertexBuffers + bindIndexBuffer + bindDescriptorSets + `vkCmdDrawIndexed(6, instanceCount, 0, 0, 0)`
7. 实现 [ParticleUpdaterGPU.h/.cpp](file:///c:/Github/PixelClean/Arms/Particle/ParticleUpdaterGPU.h)（占位）：
   - 空实现，仅声明接口，注释说明"Phase 8 实现"
8. 编译通过。

**验证**（单元级，可临时在 application.cpp 加测试代码后移除）：
- `ParticlePool`：allocate 满 8192 次后返回 -1；free 后可重新 allocate。
- `ParticleEmitter`：emit 后 `mCPUData[index]` 字段正确，`pool.activeCount()` +1。
- `ParticleUpdater`：构造一个 zoom=0.1 的粒子，`update(1.0)` 后该粒子死亡，`pool.activeCount()` -1，回调被调用。
- `ParticleRenderer`：SSBO 映射指针非空，`uploadInstanceData` 后读取映射内存内容正确。

**回滚**：删除 `Arms/Particle/` 目录。

---

### Phase 4：ParticleWorld facade 与 DescriptorSet 组装

**目标**：用 facade 组合四组件，创建粒子专用 DescriptorSet，但不替换旧 ParticleSystem。

**步骤**：
1. 实现 [ParticleWorld.h/.cpp](file:///c:/Github/PixelClean/Arms/Particle/ParticleWorld.h)（见 3.6）：
   - 构造时：创建 Pool（capacity=maxParticles）、CPUData（size=maxParticles）、Emitter、Updater、Renderer
   - `emit()` 委托 Emitter
   - `update(time)` 委托 Updater，死亡回调里标记实例无效（zoom=-1 或 alive=false）
   - `recordCommandBuffer()` / `getCommandBuffers()` / `resetCommandPools()` 委托 Renderer
2. 在 ParticleWorld 中创建 DescriptorSet：
   - 1 个 DescriptorPool（size=1）
   - 1 个 DescriptorSet，binding 0 写 VP UBO（外部传入的 `VPMatrices` buffer），binding 1 写 Renderer 的 Instance SSBO
   - 每帧更新 binding 0 的 VP UBO（若 VP 变化）
3. ParticleWorld 持有共享四边形顶点/UV/Index Buffer（可从旧 ParticleSystem 复用，或新建）。
4. 在 `application.cpp` 临时创建一个 `ParticleWorld`，手动 emit 几个粒子，调用 update + recordCommandBuffer，**与旧系统并行渲染**（用不同的 pipeline），目视验证新粒子可见且运动正确。
5. 验证无误后，移除临时测试代码。

**验证**：
- 新粒子在屏幕上正确显示为彩色四边形。
- 粒子沿角度方向运动，缩放逐渐减小至消失。
- DrawCall 计数：N 个新粒子只增加 1 个 DrawCall。
- 旧粒子系统不受影响。

**回滚**：删除 ParticleWorld 相关代码，revert application.cpp 临时改动。

---

### Phase 5：使用方迁移 - ParticlesSpecialEffect

**目标**：将 `ParticlesSpecialEffect` 改为 `ParticleWorld` 的兼容外壳。

**步骤**：
1. 修改 [ParticlesSpecialEffect.h](file:///c:/Github/PixelClean/Arms/ParticlesSpecialEffect.h)：
   - `SpecialEffects` 结构体移除 `Pixel` 和 `Buffer` 指针，改为存 `uint32_t particleIndex`（ParticleWorld 中的索引）
   - 成员 `ParticleSystem* mParticleSystem` 改为 `ParticleWorld* mWorld`
   - 保持 `GenerateSpecialEffects` / `DeleteSpecialEffects` / `SpecialEffectsEvent` 签名不变
2. 修改 [ParticlesSpecialEffect.cpp](file:///c:/Github/PixelClean/Arms/ParticlesSpecialEffect.cpp)：
   - `GenerateSpecialEffects`：调用 `mWorld->emit(x, y, colour, angle, speed)`，把返回的索引存入 `SpecialEffects.particleIndex`
   - `DeleteSpecialEffects`：调用 `mWorld->kill(index)`
   - `SpecialEffectsEvent`：调用 `mWorld->update(time)`（注意：旧接口有 `Fndex` 参数，新 World 内部按 frameIndex 处理，需把 Fndex 透传或由 World 自管理）
3. **处理 `Fndex` 语义**：旧 `SpecialEffectsEvent(Fndex, time)` 的 Fndex 是当前帧索引，用于更新对应帧的 UBO。新架构下 Renderer 每帧 upload 时已知 frameIndex，故 `ParticleWorld::update(time)` 不需要 Fndex；但为兼容，`ParticlesSpecialEffect::SpecialEffectsEvent(Fndex, time)` 内部忽略 Fndex，直接调 `mWorld->update(time)`。需在注释中说明。
4. `ContinuousData<SpecialEffects>` 仍保留（存活跃粒子的索引和元数据），但 `SpecialEffects` 只剩 `x/y/speed/angle/zoom/particleIndex`，无指针。
5. 编译通过。

**验证**：
- 原有粒子特效场景（爆炸、受击）行为与 Phase 0 一致。
- DrawCall 数量从 N 降到 1（可通过 ImGui 或 RenderDoc 确认）。
- 无内存泄漏（粒子反复生成销毁 1000 次，活跃数稳定）。

**回滚**：revert ParticlesSpecialEffect 的修改，恢复旧实现。

---

### Phase 6：使用方迁移 - Arms（子弹系统）

**目标**：将 `Arms` 的子弹渲染也迁移到 ParticleWorld。

**步骤**：
1. 研究 [Arms.cpp](file:///c:/Github/PixelClean/Arms/Arms.cpp) 的 `ShootBullets` / `DeleteBullet` / `BulletsEvent`：
   - `ShootBullets`：从 `mParticleSystem->mParticle->pop()` 借粒子，写颜色 + ModelMatrix
   - `BulletsEvent`：每帧根据物理粒子位置更新 ModelMatrix
   - `DeleteBullet`：归还粒子，触发爆炸特效
2. **关键决策**：子弹与特效是否共用同一个 ParticleWorld？
   - **方案 6A（推荐）**：共用一个 World，子弹和特效都从同一个 Pool 分配索引。优点：一次 DrawCall 画全部。缺点：子弹和特效的更新逻辑不同（子弹跟随物理粒子，特效自由运动），需在 Updater 中区分。
   - **方案 6B**：子弹用独立 World。优点：更新逻辑隔离。缺点：2 次 DrawCall。
   - **本计划采用 6A**，在 `ParticleCPUData` 中增加 `enum class Type { Bullet, Effect }` 字段，Updater 根据 Type 走不同分支。
3. 修改 `ParticleCPUData` 增加 `Type type` 字段（更新 3.1 的结构体）。
4. 修改 `ParticleUpdater::update`：
   - `Type::Bullet`：从外部物理粒子数据同步位置/角度（需要 Arms 提供一个回调或数据指针）
   - `Type::Effect`：原有自由运动学
5. 修改 [Arms.h](file:///c:/Github/PixelClean/Arms/Arms.h) / [Arms.cpp](file:///c:/Github/PixelClean/Arms/Arms.cpp)：
   - `mParticleSystem` 指针改为 `ParticleWorld*`
   - `ShootBullets`：调用 `mWorld->emit(...)` 并标记 `Type::Bullet`，存索引到 `ContinuousMap`
   - `BulletsEvent`：遍历活跃子弹，把物理粒子的位置/角度写入 `mCPUData[index]`（通过 World 暴露的 `getCPUData()` 或 `setBulletTransform(index, x, y, angle)`）
   - `DeleteBullet`：调用 `mWorld->kill(index)` + 触发爆炸特效（`mParticlesSpecialEffect->GenerateSpecialEffects`）
6. 编译通过。

**验证**：
- 子弹正常发射、飞行、命中、爆炸。
- 子弹 + 爆炸特效合计 1 次 DrawCall。
- 无粒子泄漏（长时间射击后活跃数回落）。

**回滚**：revert Arms 修改。若 6A 复杂度过高，降级到 6B（子弹独立 World）。

---

### Phase 7：旧代码清理与 application.cpp 接入

**目标**：移除旧 ParticleSystem 实现，application.cpp 完全切换到 ParticleWorld。

**步骤**：
1. 修改 [application.cpp](file:///c:/Github/PixelClean/application.cpp)：
   - 第 255-271 行：把 `ParticleSystem` 的创建改为 `ParticleWorld` 的创建
   - 第 622-623 行：窗口重建时调用 `mParticleWorld->resetCommandPools()` + `recordCommandBuffer()`
   - 第 797 行：ImGui 显示 `mParticleWorld->freeCount()`（剩余可分配数）和 `activeCount()`
   - 第 975-978 行：析构 `mParticleWorld`
2. 把 [ParticleSystem.h](file:///c:/Github/PixelClean/Arms/ParticleSystem.h) / [ParticleSystem.cpp](file:///c:/Github/PixelClean/Arms/ParticleSystem.cpp) 改为兼容外壳：
   - `ParticleSystem` 内部持有一个 `ParticleWorld*`，所有方法委托
   - 保留 `mParticle` 指针的兼容访问（`GetNumber()` 委托到 `mWorld->freeCount()`），供未迁移的 GameMod 临时使用
3. 检查 GameMod（TankTrouble/MazeMods/UnlimitednessMapMods）的粒子调用点：
   - 若直接访问 `mParticleSystem->mParticle`，改为通过 `ParticleSystem` 兼容外壳
   - 若调用 `GenerateSpecialEffects` 等高层接口，无需改动
4. 移除旧代码：
   - 删除 `ParticleSystem` 中的 `PileUp<Particle>* mParticle`、`mDescriptorSet**`、`PixelTextureS`、`mUniformParams`、`mThreadCommandPoolS`、`mThreadCommandBufferS` 等旧成员
   - 删除 `ThreadUpdateCommandBuffer` / `ThreadCommandBufferToUpdate` 的旧实现
   - 删除 `Particle` 结构体（或保留空壳）
5. 编译通过。

**验证**：
- 全量回归：所有 GameMod 场景行为与 Phase 0 一致或更优。
- DrawCall 数量：粒子 + 子弹合计 1 次。
- 内存占用：DescriptorSet 从 N 个降到 1 个，PixelTexture 从 N 个降到 0 个。
- 无崩溃、无内存泄漏（跑 10 分钟压力测试）。

**回滚**：此阶段改动大，建议拆成多个小 commit，逐个 revert。

---

### Phase 8：测试与验证

**目标**：系统性验证正确性与性能。

**步骤**：
1. **功能测试矩阵**：

| 场景 | 粒子数 | 预期 | 验证方法 |
|------|--------|------|----------|
| 空场景 | 0 | 无 DrawCall，无崩溃 | RenderDoc 确认 |
| 单粒子 | 1 | 1 次 DrawCall，instanceCount=1 | RenderDoc |
| 少量 | 10 | 1 次 DrawCall，instanceCount=10 | RenderDoc |
| 中量 | 100 | 1 次 DrawCall，instanceCount=100 | RenderDoc |
| 大量 | 1000 | 1 次 DrawCall，instanceCount=1000 | RenderDoc |
| 满载 | 8192 | 1 次 DrawCall，instanceCount=8192 | RenderDoc |
| 溢出 | 8193 | 第 8193 个 emit 返回 -1，不崩溃 | 日志确认 |
| 反复生灭 | 0↔1000 循环 | 活跃数稳定，无泄漏 | 1000 次循环后活跃数归 0 |
| 子弹+特效 | 混合 | 1 次 DrawCall | RenderDoc |

2. **性能对比**（与 Phase 0 基线）：

| 指标 | Phase 0 基线 | Phase 8 目标 | 测量方法 |
|------|-------------|-------------|----------|
| DrawCall（1000 粒子） | 1000 | 1 | ImGui 计数 |
| BindDescriptorSet | 1000 | 1 | RenderDoc |
| 帧时间（1000 粒子） | 基线值 | 下降 50%+ | ImGui 帧时间 |
| 内存（DescriptorSet 数） | 1000 | 1 | VMA 统计 |
| 内存（PixelTexture 数） | 1000 | 0 | VMA 统计 |

3. **兼容性测试**：
   - Windows 10/11 + NVIDIA/AMD/Intel 核显各一台
   - Android（若支持）：`__ANDROID__` 路径下 Renderer 录制逻辑需确认（旧代码 Android 走同步路径，新架构天然单线程录制，更简单）

4. **回归测试**：所有 GameMod 逐一跑通。

**验证**：全部测试项通过，性能达标。

---

### Phase 9（可选演进）：GPU Updater

**目标**：把 Updater 迁移到 Compute Shader，支持 10 万+ 粒子。

**首期不实现**，仅保留 `ParticleUpdaterGPU` 占位。当 Phase 8 后确有性能瓶颈时再启动，步骤：
1. 新增 `particle.comp`，实现位置/缩放/生命周期更新 + dead list（atomic counter）。
2. SSBO 改为双缓冲（Compute 写 / Draw 读）。
3. `vkCmdDrawIndexedIndirect`，instanceCount 由 Compute 写入 indirect buffer。
4. CPU 只负责发射（写入 ring buffer）。

---

## 七、兼容性策略

### 7.1 接口兼容层

`ParticleSystem` 和 `ParticlesSpecialEffect` 保留为外壳，内部委托 `ParticleWorld`：

```cpp
// ParticleSystem.h（重构后）
class ParticleSystem {
public:
    ParticleSystem(VulKan::Device* device, int Number);
    // ... 旧方法签名不变，全部委托 mWorld
    uint32_t GetFreeCount() const { return mWorld->freeCount(); } // 兼容旧 mParticle->GetNumber()
private:
    std::unique_ptr<ParticleWorld> mWorld;
};
```

### 7.2 渐进迁移

- Phase 5/6 完成后，旧 `ParticleSystem` 可立即删除内部实现，仅保留外壳
- GameMod 若有直接访问 `mParticle` 的代码，通过外壳的 `GetFreeCount()` 等方法兼容
- 若 GameMod 用了 `PileUp<Particle>` 的其他方法（如 `pop()`/`add()`），需逐一适配（研究报告显示主要调用点是 `Arms`，已在本计划覆盖）

### 7.3 双跑验证

Phase 4 可让新旧系统并行渲染（不同 pipeline），目视对比，确认新系统正确后再切换。

---

## 八、风险评估与回滚

### 8.1 风险矩阵

| 风险 | 概率 | 影响 | 缓解措施 |
|------|------|------|----------|
| SSBO 在某些移动 GPU 上有大小限制 | 低 | 中 | MAX_PARTICLES=8192，单粒子 80 字节，总 640KB，远低于限制 |
| `gl_InstanceIndex` 在旧 Vulkan 上不支持 | 极低 | 高 | Vulkan 1.0+ 均支持，项目已用 Vulkan，无风险 |
| Arms 子弹与特效共用 Pool 导致语义混乱 | 中 | 中 | Phase 6 用 Type 字段区分；若复杂度过高，降级到方案 6B（独立 World） |
| GameMod 有未发现的粒子调用点 | 中 | 低 | Phase 7 用兼容外壳兜底，外壳保留 `mParticle` 兼容访问 |
| 旧 `Fndex` 语义迁移错误导致帧错位 | 中 | 高 | Phase 5 重点验证；Renderer 每帧 upload 时用当前 frameIndex，与旧逻辑等价 |
| `ParticleCPUData` 与 SSBO 数据不同步 | 中 | 中 | Renderer upload 时以 `activeCount` 为准，死亡粒子不 upload |
| Android 路径回归 | 低 | 高 | Phase 8 在 Android 设备实测 |

### 8.2 回滚策略

- 每个 Phase 独立 commit，可单独 revert
- Phase 1-4 不影响旧系统，随时可回滚
- Phase 5-6 是关键切换点，建议在分支上完成并充分测试后再合并
- Phase 7 删除旧代码前，确保 Phase 5/6 已稳定运行 24h

---

## 九、验收标准

### 9.1 功能验收
- [ ] 所有 GameMod 场景行为与重构前一致
- [ ] 粒子生成/销毁正确，无泄漏（1000 次循环后活跃数归 0）
- [ ] 粒子溢出（超过 MAX_PARTICLES）不崩溃
- [ ] 子弹与特效混合渲染正确

### 9.2 性能验收
- [ ] 1000 粒子场景 DrawCall = 1
- [ ] 1000 粒子场景 BindDescriptorSet = 1
- [ ] 1000 粒子场景帧时间较基线下降 ≥ 50%
- [ ] DescriptorSet 数量 = 1
- [ ] PixelTexture 数量 = 0

### 9.3 代码质量验收
- [ ] 无 raw pointer 浅拷贝（`Particle` 结构体已移除或不含指针）
- [ ] 无 `delete` 误用（新组件用 `std::vector` / `unique_ptr`）
- [ ] `SpecialEffectsEvent` 迭代 bug 已修复
- [ ] `GetCommandBuffer` 用 `activeCount` 而非 `freeCount` 判断
- [ ] 新增代码有注释说明设计意图

### 9.4 兼容性验收
- [ ] `ParticleSystem` 对外接口签名不变
- [ ] `ParticlesSpecialEffect` 对外接口签名不变
- [ ] `application.cpp` 调用点仅替换对象类型，逻辑不变
- [ ] Windows + Android 均通过

---

## 十、逻辑完整性自检

> 以下是对方案 D 执行计划本身的逻辑检查，确认无遗漏、无矛盾。

### 10.1 数据流闭环检查

| 数据 | 产生 | 消费 | 闭环？ |
|------|------|------|--------|
| 粒子索引 | Pool.allocate() | Pool.free()（Updater 死亡回调） | ✅ 闭环 |
| CPU 粒子属性 | Emitter.emit() 写入 | Updater.update() 读写 / Renderer.upload 读取 | ✅ 闭环 |
| GPU 实例数据 | Renderer.upload 写入 SSBO | Shader 读取 | ✅ 闭环 |
| VP 矩阵 | 外部 Camera 提供 | DescriptorSet binding 0 | ✅ 闭环 |
| 死亡通知 | Updater 回调 | Emitter/World 清理 | ✅ 闭环 |

### 10.2 资源生命周期检查

| 资源 | 创建 | 销毁 | 泄漏风险 |
|------|------|------|----------|
| ParticlePool | ParticleWorld 构造 | ParticleWorld 析构 | 无（栈成员） |
| CPUData vector | ParticleWorld 构造 | ParticleWorld 析构 | 无（栈成员） |
| SSBO（每帧） | Renderer 构造 | Renderer 析构 | 需确认 Renderer 析构释放 |
| DescriptorSet | ParticleWorld 构造 | ParticleWorld 析构 | 需确认释放 |
| Pipeline | 外部 CreatePipeline | 外部管理 | 无（World 不持有所有权） |
| 顶点/UV/Index Buffer | World 持有或复用 | World 析构 | 需明确所有权 |

**结论**：Renderer 析构必须释放 SSBO 和映射指针；World 析构必须释放 DescriptorSet。已在 3.5/3.6 接口中体现。

### 10.3 线程安全检查

- 旧架构：Windows 多线程录制二级 CB，Android 单线程
- 新架构：Renderer 录制是单线程（1 次 bind + 1 次 draw），无需多线程
- Updater 在主线程跑，Pool 无锁
- **结论**：新架构线程模型更简单，无并发风险

### 10.4 接口兼容性检查

| 旧接口 | 新实现 | 兼容？ |
|--------|--------|--------|
| `ParticleSystem::RecordingCommandBuffer` | `ParticleWorld::recordCommandBuffer` 委托 | ✅ |
| `ParticleSystem::GetCommandBuffer` | `ParticleWorld::getCommandBuffers` 委托 | ✅ |
| `ParticleSystem::resetThreadCommandPools` | `ParticleWorld::resetCommandPools` 委托 | ✅ |
| `ParticleSystem::ThreadUpdateCommandBuffer` | `ParticleWorld` 内部自动录制（每帧） | ✅（语义微调：旧是显式调用，新可在 getCommandBuffers 时懒录制） |
| `mParticleSystem->mParticle->GetNumber()` | `ParticleSystem::GetFreeCount()` 委托 | ✅ |
| `ParticlesSpecialEffect::GenerateSpecialEffects` | 委托 `World::emit` | ✅ |
| `ParticlesSpecialEffect::DeleteSpecialEffects` | 委托 `World::kill` | ✅ |
| `ParticlesSpecialEffect::SpecialEffectsEvent(Fndex, time)` | 委托 `World::update(time)`，忽略 Fndex | ✅（Fndex 语义已内化） |

### 10.5 与原设计文档方案 D 的对照

| 原文档方案 D 要点 | 本计划覆盖 |
|------------------|-----------|
| ParticleWorld facade | ✅ 第三章 3.6 |
| Emitter 组件 | ✅ 3.3 |
| ParticlePool (SSBO) | ✅ 3.2（Pool 管索引，SSBO 由 Renderer 管） |
| Updater (CPU or GPU) | ✅ 3.4 CPU + 3.4 GPU 占位 |
| Renderer (Instanced Draw) | ✅ 3.5 |
| 可切换 CPU/GPU 更新 | ✅ 接口预留，Phase 9 实现 GPU |
| 组件化、可组合 | ✅ 四组件独立，World 组合 |

### 10.6 遗漏项排查

| 检查项 | 状态 |
|--------|------|
| 是否考虑了 Arms 共享池 | ✅ Phase 6 |
| 是否考虑了 GameMod 调用点 | ✅ Phase 7 步骤 3 |
| 是否考虑了 Android 路径 | ✅ Phase 8 步骤 3 |
| 是否考虑了窗口重建 | ✅ Phase 7 步骤 2（application.cpp L622-623） |
| 是否考虑了 ImGui 显示 | ✅ Phase 7 步骤 2（application.cpp L797） |
| 是否考虑了 Shader 编译注册 | ✅ Phase 2 |
| 是否考虑了 CMake | ✅ 文件落点用 GLOB_RECURSE，无需改 CMake |
| 是否考虑了内存对齐 | ✅ ParticleInstanceData 80 字节，std140 对齐 |
| 是否考虑了帧多缓冲 | ✅ Renderer 每帧一个 SSBO |
| 是否考虑了死亡粒子不渲染 | ✅ Renderer upload 时以 activeCount 为准 |

### 10.7 潜在矛盾排查

1. **矛盾**：3.6 中 `ParticleWorld::update(time)` 无 Fndex 参数，但 5.5 中 `SpecialEffectsEvent(Fndex, time)` 需要透传。
   **解决**：World 内部维护 `mCurrentFrameIndex`，在 `getCommandBuffers(frameIndex)` 时设置；`update` 不需要 Fndex。已在 7.1 说明。

2. **矛盾**：Phase 6 方案 6A 让子弹和特效共用 Pool，但子弹的更新依赖外部物理粒子数据，与 Updater 的自主更新冲突。
   **解决**：`ParticleCPUData` 增加 `Type` 字段，Updater 对 `Type::Bullet` 调用外部回调同步位置。已在 Phase 6 步骤 2-4 说明。

3. **矛盾**：3.5 中 Renderer 持有每帧 SSBO，但 4.1 的 `createStorageBuffer` 持久映射是可选参数。
   **解决**：Renderer 构造时 `persistentMapping=true`，映射指针存入 `mMappedPtr[frame]`。已在 3.5 成员变量体现。

**结论**：方案 D 执行计划逻辑完整，数据流闭环，资源生命周期明确，接口兼容，无遗漏关键场景，无未解决矛盾。

---

## 十一、执行检查清单（按 Phase 顺序）

- [ ] Phase 0：基线性能数据已记录
- [ ] Phase 1：`createStorageBuffer` 已实现并验证
- [ ] Phase 2：`particle.vert/frag` 已编译，`ParticlePipeline` 已创建
- [ ] Phase 3：Pool/Emitter/Updater/Renderer 四组件已实现并单元测试通过
- [ ] Phase 4：ParticleWorld facade 已组装，并行渲染验证通过
- [ ] Phase 5：ParticlesSpecialEffect 已迁移，特效场景回归通过
- [ ] Phase 6：Arms 子弹系统已迁移，子弹+特效混合渲染通过
- [ ] Phase 7：旧代码已清理，application.cpp 已切换，全量回归通过
- [ ] Phase 8：功能/性能/兼容性测试全部通过
- [ ] 第九章验收标准全部满足

---

**文档版本**：v1.0
**基于代码库状态**：截至 2026-06-18
**适用对象**：PixelClean 粒子系统重构执行者
