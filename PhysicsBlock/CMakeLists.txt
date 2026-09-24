file(GLOB_RECURSE PhysicsBlock ./  *.cpp)

add_library(PhysicsBlockLib  ${PhysicsBlock})

# 本库只做物理，不含任何画面渲染（渲染已整合到 samples/PhysicsRender.{h,cpp}），
# 因此这里不需要链接 glad / OpenGL。
