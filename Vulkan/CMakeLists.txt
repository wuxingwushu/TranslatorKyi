file(GLOB_RECURSE VULKAN ./  *.cpp)

add_library(vulkanLib  ${VULKAN})

# Vulkan SDK configuration
if(DEFINED ENV{VULKAN_SDK})
    target_include_directories(vulkanLib PRIVATE $ENV{VULKAN_SDK}/Include)
    # 使用根 CMakeLists.txt 按目标架构选择的库目录（Lib=x64 / Lib32=x86）
    if(NOT DEFINED VULKAN_LIB_DIR)
        set(VULKAN_LIB_DIR "$ENV{VULKAN_SDK}/Lib")
    endif()
    target_link_directories(vulkanLib PRIVATE ${VULKAN_LIB_DIR})
endif()

target_link_libraries(vulkanLib PRIVATE vulkan-1.lib)