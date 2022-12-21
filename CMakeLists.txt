cmake_minimum_required(VERSION 3.12)
project(Translate-miku)

set(CMAKE_CXX_STANDARD 17)



aux_source_directory(. DIRSRCS)

include_directories(
SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}./Environment/x64-windows/include
SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}./Environment/DXSDK/include
SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}./Environment/Visual_Leak_Detector/include
SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}./Environment/jsoncpp/include
SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}./Environment/OpenSSL-Win64/include
)

link_directories(
SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}./Environment/x64-windows/lib
SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}./Environment/DXSDK/lib/x64
SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}./Environment/Visual_Leak_Detector/lib/Win64
SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}./Environment/jsoncpp/lib/Release
SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}./Environment/OpenSSL-Win64/lib
)

# execute_process(COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/DLL/bz2.dll ${CMAKE_CURRENT_BINARY_DIR}) # 拷贝单个文件
execute_process( COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/DLL ${CMAKE_CURRENT_BINARY_DIR}) # 拷贝文件夹
execute_process(COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/launch.vs.json ${PROJECT_SOURCE_DIR}/.vs)# 设置的DLL文件路径
execute_process(COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/product.ico ${CMAKE_CURRENT_BINARY_DIR})
execute_process(COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/Data.ini ${CMAKE_CURRENT_BINARY_DIR})
execute_process( COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/tessdata ${CMAKE_CURRENT_BINARY_DIR}/tessdata)# 把 tesseract 的模型拷进去



add_executable(Translate-miku  ${DIRSRCS}
	resource.rc#设置exe程序图标
)

add_subdirectory(Function) # 对子文件夹进行CMake
add_subdirectory(imgui)


#Set_Target_Properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS_RELEASE "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")#关闭控制台

target_link_libraries(Translate-miku FunctionLib imguiLib tesseract52.lib leptonica-1.82.0.lib d3dx11.lib vld.lib jsoncpp.lib libcurl.lib libssl.lib libcrypto.lib)