file(GLOB_RECURSE VULKAN ./  *.cpp)

add_library(vulkanLib  ${VULKAN})

# Vulkan SDK configuration
if(DEFINED ENV{VULKAN_SDK})
    target_include_directories(vulkanLib PRIVATE $ENV{VULKAN_SDK}/Include)
    target_link_directories(vulkanLib PRIVATE $ENV{VULKAN_SDK}/Lib)
endif()

target_link_libraries(vulkanLib PRIVATE vulkan-1.lib)