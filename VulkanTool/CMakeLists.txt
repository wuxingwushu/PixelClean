file(GLOB_RECURSE VULKANTOOL ./  *.cpp)

add_library(vulkanToolLib  ${VULKANTOOL})

# Vulkan SDK configuration
if(DEFINED ENV{VULKAN_SDK})
    target_include_directories(vulkanToolLib PRIVATE $ENV{VULKAN_SDK}/Include)
    target_link_directories(vulkanToolLib PRIVATE $ENV{VULKAN_SDK}/Lib)
endif()

target_link_libraries(vulkanToolLib PRIVATE vulkan-1.lib)
