file(GLOB_RECURSE PhysicsTest3D ./  *.cpp)

add_library(PhysicsTest3DLib  ${PhysicsTest3D})

target_link_libraries(PhysicsTest3DLib PUBLIC imgui PhysicsBlock3DLib)
