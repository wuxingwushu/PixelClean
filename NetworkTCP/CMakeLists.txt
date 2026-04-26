file(GLOB_RECURSE NetworkTCP ./  *.cpp)

add_library(NetworkTCPLib  ${NetworkTCP})

target_link_libraries(NetworkTCPLib PUBLIC zlib)