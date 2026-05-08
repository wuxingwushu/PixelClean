file(GLOB_RECURSE PhysicsTest ./  *.cpp)

add_library(PhysicsTestLib  ${PhysicsTest})

target_link_libraries(PhysicsTestLib PUBLIC imgui)