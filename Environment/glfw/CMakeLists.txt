cmake_minimum_required(VERSION 3.4...3.28)

project(glfw C)

find_package(Threads REQUIRED)

# Establish target libraries and include directories
if (APPLE)

    list(APPEND glfw_LIBRARIES
        "-framework Cocoa"
        "-framework IOKit"
        "-framework CoreFoundation"
        "-framework CoreVideo")

elseif (UNIX)
    find_library(RT_LIBRARY rt)
    if (RT_LIBRARY)
        list(APPEND glfw_LIBRARIES "${RT_LIBRARY}")
    endif()

    find_library(MATH_LIBRARY m)
    if (MATH_LIBRARY)
        list(APPEND glfw_LIBRARIES "${MATH_LIBRARY}")
    endif()

    if (CMAKE_DL_LIBS)
        list(APPEND glfw_LIBRARIES "${CMAKE_DL_LIBS}")
    endif()

    find_package(X11 REQUIRED)

    # Set up library and include paths
    list(APPEND glfw_INCLUDE_DIRS "${X11_X11_INCLUDE_PATH}")
    list(APPEND glfw_LIBRARIES "${X11_X11_LIB}" "${CMAKE_THREAD_LIBS_INIT}")

    # Check for XRandR (modern resolution switching and gamma control)
    if (NOT X11_Xrandr_FOUND)
        message(FATAL_ERROR "The RandR headers were not found")
    endif()

    # Check for Xinerama (legacy multi-monitor support)
    if (NOT X11_Xinerama_FOUND)
        message(FATAL_ERROR "The Xinerama headers were not found")
    endif()

    # Check for Xkb (X keyboard extension)
    if (NOT X11_Xkb_FOUND)
        message(FATAL_ERROR "The X keyboard extension headers were not found")
    endif()

    # Check for Xcursor (cursor creation from RGBA images)
    if (NOT X11_Xcursor_FOUND)
        message(FATAL_ERROR "The Xcursor headers were not found")
    endif()

    # Check for XInput (modern HID input)
    if (NOT X11_Xi_FOUND)
        message(FATAL_ERROR "The XInput headers were not found")
    endif()

    # Check for X Shape (custom window input shape)
    if (NOT X11_Xshape_FOUND)
        message(FATAL_ERROR "The X Shape headers were not found")
    endif()

    list(APPEND glfw_INCLUDE_DIRS "${X11_Xrandr_INCLUDE_PATH}"
                                  "${X11_Xinerama_INCLUDE_PATH}"
                                  "${X11_Xkb_INCLUDE_PATH}"
                                  "${X11_Xcursor_INCLUDE_PATH}"
                                  "${X11_Xi_INCLUDE_PATH}"
                                  "${X11_Xshape_INCLUDE_PATH}")
endif()

set(common_HEADERS src/internal.h src/mappings.h src/platform.h
                   src/null_platform.h src/null_joystick.h
                   include/GLFW/glfw3.h
                   include/GLFW/glfw3native.h)
set(common_SOURCES src/context.c src/init.c src/input.c src/monitor.c
                   src/platform.c src/vulkan.c src/window.c
                   src/null_init.c src/null_monitor.c src/null_window.c
                   src/null_joystick.c)

if (APPLE)
    set(glfw_HEADERS ${common_HEADERS}
                     src/cocoa_platform.h src/cocoa_joystick.h
                     src/cocoa_time.h src/posix_thread.h)
    set(glfw_SOURCES ${common_SOURCES}
                     src/cocoa_init.m src/cocoa_joystick.m
                     src/cocoa_monitor.m src/cocoa_window.m
                     src/cocoa_time.c src/nsgl_context.m
                     src/egl_context.c src/osmesa_context.c
                     src/posix_thread.c src/posix_module.c)
    set_source_files_properties(${glfw_SOURCES} PROPERTIES LANGUAGE C)
elseif (WIN32)
    set(glfw_HEADERS ${common_HEADERS}
                     src/win32_platform.h src/win32_joystick.h
                     src/win32_time.h src/win32_thread.h)
    set(glfw_SOURCES ${common_SOURCES}
                     src/win32_init.c src/win32_joystick.c
                     src/win32_monitor.c src/win32_time.c
                     src/win32_thread.c src/win32_module.c
                     src/win32_window.c
                     src/wgl_context.c src/egl_context.c src/osmesa_context.c)
elseif (UNIX)
    set(glfw_HEADERS ${common_HEADERS}
                     src/x11_platform.h src/xkb_unicode.h
                     src/posix_time.h src/posix_thread.h)
    set(glfw_SOURCES ${common_SOURCES}
                     src/x11_init.c src/x11_monitor.c src/x11_window.c
                     src/xkb_unicode.c src/posix_time.c src/posix_thread.c
                     src/posix_module.c src/glx_context.c
                     src/egl_context.c src/osmesa_context.c)
    list(APPEND glfw_HEADERS src/linux_joystick.h src/posix_poll.h)
    list(APPEND glfw_SOURCES src/linux_joystick.c src/posix_poll.c)
endif()

add_library(glfw ${glfw_SOURCES} ${glfw_HEADERS})
target_include_directories(glfw PUBLIC include)
target_include_directories(glfw PRIVATE src ${glfw_INCLUDE_DIRS})
target_link_libraries(glfw PRIVATE Threads::Threads ${glfw_LIBRARIES})

if (WIN32)
    target_compile_definitions(glfw PRIVATE UNICODE _UNICODE)
endif()

if (MSVC OR CMAKE_C_SIMULATE_ID STREQUAL "MSVC")
    target_compile_definitions(glfw PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_definitions(glfw PRIVATE _DEFAULT_SOURCE)
endif()
