file(GLOB_RECURSE SoundEffect ./  *.cpp)

add_library(SoundEffectLib  ${SoundEffect})

# 链接 Soloud 源码库
target_link_libraries(SoundEffectLib soloud)

# 设置 include 路径
target_include_directories(SoundEffectLib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../Environment/Soloud/include
)