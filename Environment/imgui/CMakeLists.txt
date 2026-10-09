project(imgui CXX)

set(IMGUI_CORE_SOURCES
    imgui.cpp
    imgui_draw.cpp
    imgui_widgets.cpp
    imgui_tables.cpp
    imgui_demo.cpp
)

set(IMGUI_CORE_HEADERS
    imgui.h
    imgui_internal.h
    imconfig.h
    imstb_rectpack.h
    imstb_textedit.h
    imstb_truetype.h
)

if(ANDROID)
    list(APPEND IMGUI_CORE_SOURCES
        backends/imgui_impl_android.cpp
        backends/imgui_impl_vulkan.cpp
    )
    list(APPEND IMGUI_CORE_HEADERS
        backends/imgui_impl_android.h
        backends/imgui_impl_vulkan.h
    )
elseif(WIN32)
    list(APPEND IMGUI_CORE_SOURCES
        backends/imgui_impl_glfw.cpp
        backends/imgui_impl_vulkan.cpp
    )
    list(APPEND IMGUI_CORE_HEADERS
        backends/imgui_impl_glfw.h
        backends/imgui_impl_vulkan.h
    )
elseif(APPLE)
    list(APPEND IMGUI_CORE_SOURCES
        backends/imgui_impl_glfw.cpp
        backends/imgui_impl_vulkan.cpp
    )
    list(APPEND IMGUI_CORE_HEADERS
        backends/imgui_impl_glfw.h
        backends/imgui_impl_vulkan.h
    )
else()
    list(APPEND IMGUI_CORE_SOURCES
        backends/imgui_impl_glfw.cpp
        backends/imgui_impl_vulkan.cpp
    )
    list(APPEND IMGUI_CORE_HEADERS
        backends/imgui_impl_glfw.h
        backends/imgui_impl_vulkan.h
    )
endif()

add_library(imgui STATIC ${IMGUI_CORE_SOURCES} ${IMGUI_CORE_HEADERS})

target_include_directories(imgui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/backends
)
