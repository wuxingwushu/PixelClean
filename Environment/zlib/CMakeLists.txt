cmake_minimum_required(VERSION 3.12)
project(zlib)

# zlib 源代码文件
set(ZLIB_SOURCES
    adler32.c
    compress.c
    crc32.c
    deflate.c
    gzclose.c
    gzlib.c
    gzread.c
    gzwrite.c
    infback.c
    inffast.c
    inflate.c
    inftrees.c
    trees.c
    uncompr.c
    zutil.c
)

# 创建静态库
add_library(zlib STATIC ${ZLIB_SOURCES})

# 设置包含目录（父目录，以便使用 #include "zlib/zlib.h"）
target_include_directories(zlib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)

# 编译定义
target_compile_definitions(zlib PRIVATE -D_CRT_SECURE_NO_DEPRECATE)

# 根据构建类型设置输出名称
if(CMAKE_BUILD_TYPE AND (CMAKE_BUILD_TYPE STREQUAL "Debug"))
    set_target_properties(zlib PROPERTIES OUTPUT_NAME "zlibd")
else()
    set_target_properties(zlib PROPERTIES OUTPUT_NAME "zlib")
endif()
