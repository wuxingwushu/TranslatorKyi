file(GLOB_RECURSE imgui ./  *.cpp)

add_library(imguiLib  ${imgui} "GUI.h")