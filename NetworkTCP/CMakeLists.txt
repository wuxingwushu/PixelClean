# NetworkTCP 模块
# 基于 libevent + zlib 的网络复制系统，用于多人联机游戏的数据同步
# 依赖：zlib（数据压缩）

# 收集当前目录及子目录下所有 .cpp 文件
file(GLOB_RECURSE NetworkTCP ./  *.cpp)

# 创建静态库 NetworkTCPLib
add_library(NetworkTCPLib  ${NetworkTCP})

# 链接 zlib 库（用于网络数据的压缩/解压）
target_link_libraries(NetworkTCPLib PUBLIC zlib)