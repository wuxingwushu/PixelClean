# WFC 核心源代码文件
set(WFC_SOURCES
    src/wfc.cpp
    src/wave.cpp
    src/propagator.cpp
)

# 创建静态库
add_library(WaveFunctionCollapse STATIC ${WFC_SOURCES})

# 设置包含目录
target_include_directories(WaveFunctionCollapse PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# 根据构建类型设置输出名称
set_target_properties(WaveFunctionCollapse PROPERTIES OUTPUT_NAME "WaveFunctionCollapse")