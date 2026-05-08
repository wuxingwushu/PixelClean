file(GLOB_RECURSE RadianceCascades ./  *.cpp)

add_library(RadianceCascadesLib  ${RadianceCascades})

target_link_libraries(RadianceCascadesLib PUBLIC imgui)
