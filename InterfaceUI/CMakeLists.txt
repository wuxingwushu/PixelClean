file(GLOB_RECURSE Arms ./  *.cpp)

add_library(InterfaceUI ${Arms})

target_include_directories(InterfaceUI PUBLIC
    ${CMAKE_SOURCE_DIR}/Environment
)

target_link_libraries(InterfaceUI PUBLIC imgui)