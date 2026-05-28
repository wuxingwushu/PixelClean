file(GLOB_RECURSE AudioSources ./ *.cpp)

add_library(AudioEngineLib ${AudioSources})

target_link_libraries(AudioEngineLib soloud)

target_include_directories(AudioEngineLib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../Environment/Soloud/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../
)