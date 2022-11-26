cmake_minimum_required(VERSION 3.12)
project(Translate-miku)

set(CMAKE_CXX_STANDARD 17)

aux_source_directory(. DIRSRCS)

include_directories(
SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}./Environment/tesseract_x64-windows/include
SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}./Environment/leptonica_x64-windows/include
)

link_directories(
SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}./Environment/tesseract_x64-windows/lib
SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}./Environment/leptonica_x64-windows/lib
)

# execute_process(COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/DLL/bz2.dll ${CMAKE_CURRENT_BINARY_DIR}) # 拷贝单个文件
# execute_process( COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/DLL ${CMAKE_CURRENT_BINARY_DIR}) # 拷贝文件夹
execute_process(COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/launch.vs.json ${PROJECT_SOURCE_DIR}/.vs)

add_executable(Translate-miku  ${DIRSRCS})

add_subdirectory(Function) # 对子文件夹进行CMake


set(CMAKE_PREFIX_PATH ./Environment/msvc2019_64)#设置环境路径
#开启QT6
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 COMPONENTS Core Gui Widgets 3DCore REQUIRED)

# Qt6::Core Qt6::Gui Qt6::Widgets

target_link_libraries(Translate-miku FunctionLib tesseract52.lib leptonica-1.82.0.lib Qt6::Core Qt6::Gui Qt6::Widgets Qt6::3DCore)