# =============================================================================
# PhysicsBlock3D —— 以 PhysicsBlock（2D 像素网格刚体物理）为蓝本逐类改写的
#                   三维物理引擎（体素网格 + 四元数 + 3×3 惯量张量）
#
# 与 2D 版的关系：
#   · 完全独立的目录与库目标（PhysicsBlock3DLib），不依赖 PhysicsBlockLib；
#   · 全部符号放在 namespace PhysicsBlock3D，全部宏加 3D 后缀，可与 2D 版共存于
#     同一个可执行文件而不冲突；
#   · 仅复用 Tool/ 下的三个基础设施头文件（MemoryPool / ThreadPool / json），
#     其中 MemoryPool 用的是本目录修正过的 MemoryPool3D.h。
#
# 本库只做物理，不含任何画面渲染（与 2D 版一致）。
# =============================================================================

file(GLOB_RECURSE PhysicsBlock3D ./ *.cpp)

add_library(PhysicsBlock3DLib ${PhysicsBlock3D})

# C++17（与主工程一致）：四元数、std::optional 等需要
target_compile_features(PhysicsBlock3DLib PRIVATE cxx_std_17)

# 头文件目录（供外部 include "PhysicsWorld3D.hpp" 等）
target_include_directories(PhysicsBlock3DLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# 关闭 Vulkan/GPU 后端：3D 版明确不含 PhysicsGPU（2D 的三个着色器全部硬编码
# vec2/cross2D，3D 需整体重写）。若将来要接入，定义为 1 并补齐 shader。
# target_compile_definitions(PhysicsBlock3DLib PUBLIC PhysicsBlock3DUseGPU=1)
