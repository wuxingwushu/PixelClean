file(GLOB_RECURSE Character ./  *.cpp)

add_library(CharacterLib  ${Character})

# 链接 GOBT 框架（header-only INTERFACE 库），继承其 include 目录
target_link_libraries(CharacterLib PUBLIC gobot)