cmake_minimum_required(VERSION 3.12)
project(TranslatorKyi)

set(CMAKE_CXX_STANDARD 17)

if(CMAKE_BUILD_TYPE AND (CMAKE_BUILD_TYPE STREQUAL "Debug"))
	message("Debug模式")
else()
	message("Release模式")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /O3")
endif()

aux_source_directory(. DIRSRCS)



include_directories(
	SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}./Environment/GLFW/include
	SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}./Environment/VulKan/Include
	SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}./Environment/Visual_Leak_Detector/Include
	SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}./Environment/tesseract-x64-windows/include
	SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}./Environment/curl-7.86.0/include
	SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}./Environment/jsoncpp/include
	SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}./Environment/OpenSSL-Win64/include
	SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}./Environment/spdlog/include
)

if(CMAKE_BUILD_TYPE AND (CMAKE_BUILD_TYPE STREQUAL "Debug"))
	link_directories(
		SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}./Environment/GLFW/lib
		SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}./Environment/VulKan/Lib
		SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}./Environment/Visual_Leak_Detector/lib/Win64
		SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}./Environment/OpenSSL-Win64/lib
		SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}./Environment/jsoncpp/lib/Debug
		SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}./Environment/tesseract-x64-windows/debug/lib
		SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}./Environment/spdlog/lib/Debug
	)
else()
	link_directories(
		SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}./Environment/GLFW/lib
		SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}./Environment/VulKan/Lib
		SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}./Environment/Visual_Leak_Detector/lib/Win64
		SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}./Environment/OpenSSL-Win64/lib
		SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}./Environment/jsoncpp/lib/Release
		SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}./Environment/tesseract-x64-windows/lib
		SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}./Environment/spdlog/lib/Release
	)
endif()



execute_process(COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/launch.vs.json ${PROJECT_SOURCE_DIR}/.vs)# 设置的DLL文件路径
execute_process(COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/Data.ini ${CMAKE_CURRENT_BINARY_DIR})
execute_process( COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/TTF ${CMAKE_CURRENT_BINARY_DIR}/TTF)
execute_process( COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/Opcode ${CMAKE_CURRENT_BINARY_DIR}/Opcode)
execute_process( COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/TessData ${CMAKE_CURRENT_BINARY_DIR}/TessData)# 把 tesseract 的模型拷进去
execute_process(COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/product.ico ${CMAKE_CURRENT_BINARY_DIR})

if(CMAKE_BUILD_TYPE AND (CMAKE_BUILD_TYPE STREQUAL "Debug"))
	execute_process( COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/Environment/tesseract-x64-windows/debug/bin ${CMAKE_CURRENT_BINARY_DIR}) # 拷贝文件夹
else()
	execute_process( COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/Environment/tesseract-x64-windows/bin ${CMAKE_CURRENT_BINARY_DIR}) # 拷贝文件夹
endif()

add_subdirectory(Vulkan)
add_subdirectory(texture)
add_subdirectory(ImGui)
add_subdirectory(Tool)
add_subdirectory(Function)

add_executable(TranslatorKyi  ${DIRSRCS}
	resource.rc#设置exe程序图标
)


# 设置MT/MTd
#set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
#set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")

# set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")#关闭控制台





if(CMAKE_BUILD_TYPE AND (CMAKE_BUILD_TYPE STREQUAL "Debug"))
	target_link_libraries(
		TranslatorKyi 
		vulkanLib 
		textureLib 
		imguiLib 
		ToolLib 
		FunctionLib 
	
		vulkan-1.lib
		glfw3.lib
		jsoncpp_static.lib
		libcurl-d.lib
		libssl_static.lib
		libcrypto.lib
		spdlogd.lib
		#vld.lib

		leptonica-1.83.1d.lib
		tesseract53d.lib
	)
else()
	set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")#关闭控制台

	target_link_libraries(
		TranslatorKyi 
		vulkanLib
		textureLib
		imguiLib 
		ToolLib 
		FunctionLib 
	
		vulkan-1.lib 
		glfw3.lib
		jsoncpp_static.lib 
		libcurl.lib 
		libssl_static.lib 
		libcrypto.lib
		spdlog.lib

		leptonica-1.83.1.lib
		tesseract53.lib
	)
endif()
	