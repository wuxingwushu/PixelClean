file(GLOB_RECURSE WFCTest ./  *.cpp)

add_library(WFCTestLib  ${WFCTest})

target_link_libraries(WFCTestLib WaveFunctionCollapse)