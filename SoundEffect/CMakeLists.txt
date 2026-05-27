file(GLOB_RECURSE SoundEffect ./  *.cpp)

add_library(SoundEffectLib  ${SoundEffect})

target_link_libraries(SoundEffectLib soloud AudioEngineLib)

target_include_directories(SoundEffectLib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../Environment/Soloud/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../
)