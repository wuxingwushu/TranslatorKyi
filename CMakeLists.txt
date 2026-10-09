# ==============================================================================
#  TranslatorKyi —— 参照 PixelClean 的 CMake 风格
#  所有第三方库都由 Environment/ 目录下的「源码」通过 add_subdirectory() 直接引入，
#  不再链接预先编译好的 .lib。
# ==============================================================================
cmake_minimum_required(VERSION 3.20)

# curl(需要 C)、leptonica(需要 C)、glfw(需要 C)
project(TranslatorKyi C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 单配置生成器（Ninja / Makefile）没给 CMAKE_BUILD_TYPE 时默认 Release
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# ------------------------------------------------------------------ 编译选项
# 注意：这里**故意不定义 WIN32_LEAN_AND_MEAN**（PixelClean 里有，但本工程代码依赖
#       windows.h 顺带引入的 shellapi.h —— Interface.cpp 的 ShellExecute、Window.cpp 的
#       NOTIFYICONDATA/Shell_NotifyIcon 都来自它）。
if(MSVC)
	add_compile_options(/wd4201 /utf-8 /FS /MP)
	add_compile_definitions(NOMINMAX)
endif()

set(TK_ENV "${CMAKE_CURRENT_SOURCE_DIR}/Environment")

# 所有第三方库统一按静态库编译（工程最终产物是单个 exe）
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

# ------------------------------------------------------------------ Vulkan SDK
if(NOT DEFINED ENV{VULKAN_SDK} AND WIN32)
	file(GLOB _tk_vk_dirs "C:/VulkanSDK/*")
	if(_tk_vk_dirs)
		list(SORT _tk_vk_dirs COMPARE NATURAL ORDER DESCENDING)
		list(GET _tk_vk_dirs 0 _tk_vk_root)
		set(ENV{VULKAN_SDK} "${_tk_vk_root}")
	endif()
endif()

if(DEFINED ENV{VULKAN_SDK})
	message(STATUS "Vulkan SDK: $ENV{VULKAN_SDK}")
	set(TK_VULKAN_INC "$ENV{VULKAN_SDK}/Include")
	if(CMAKE_SIZEOF_VOID_P EQUAL 8)
		set(TK_VULKAN_LIB "$ENV{VULKAN_SDK}/Lib")
	else()
		set(TK_VULKAN_LIB "$ENV{VULKAN_SDK}/Lib32")
	endif()
	if(NOT EXISTS "${TK_VULKAN_LIB}/vulkan-1.lib")
		message(FATAL_ERROR "在 ${TK_VULKAN_LIB} 中找不到 vulkan-1.lib，请检查 Vulkan SDK 安装。")
	endif()
	include_directories(SYSTEM ${TK_VULKAN_INC})
	link_directories(SYSTEM ${TK_VULKAN_LIB})
	# 供子目录 Vulkan/CMakeLists.txt 使用（Lib=x64 / Lib32=x86）
	set(VULKAN_LIB_DIR "${TK_VULKAN_LIB}")
else()
	message(FATAL_ERROR "未找到 Vulkan SDK，请安装 Vulkan SDK（默认路径 C:/VulkanSDK/<版本>）。")
endif()

# ==============================================================================
#  第三方库头文件路径
#  注意：这些 include_directories() 必须写在下面 add_subdirectory() 之前，
#        否则本工程自己的 target 拿不到这些路径。
# ==============================================================================
include_directories(
	SYSTEM ${TK_ENV}/glfw/include
	SYSTEM ${TK_ENV}/imgui
	SYSTEM ${TK_ENV}/imgui/backends
	SYSTEM ${TK_ENV}/spdlog/include
	SYSTEM ${TK_ENV}/jsoncpp-master/include/json          # 工程里用的是 <json.h>
	SYSTEM ${TK_ENV}/angelscript_2.38.0/sdk/angelscript/include
	# glm 不再随仓库携带：直接用 Vulkan SDK 自带的 glm（${TK_VULKAN_INC} 已在上面 include_directories，
	# 编译时 <glm/glm.hpp> 实际一直解析到 SDK 那份 —— 见 CL.read.1.tlog 记录）
	SYSTEM ${TK_ENV}/curl-master/include                  # <curl/curl.h>
	SYSTEM ${TK_ENV}/tesseract-main/include               # <tesseract/baseapi.h>
	SYSTEM ${CMAKE_BINARY_DIR}/Environment/tesseract/include   # tesseract 生成的 version.h
	SYSTEM ${TK_ENV}/llama.cpp-master/include                 # <llama.h>
	SYSTEM ${TK_ENV}/llama.cpp-master/ggml/include            # <ggml.h> / <ggml-backend.h>
)

# ==============================================================================
#  glfw / imgui / spdlog（Environment 里已裁剪好，直接引用）
# ==============================================================================
add_subdirectory(${TK_ENV}/glfw   ${CMAKE_BINARY_DIR}/Environment/glfw)
add_subdirectory(${TK_ENV}/imgui  ${CMAKE_BINARY_DIR}/Environment/imgui)
add_subdirectory(${TK_ENV}/spdlog ${CMAKE_BINARY_DIR}/Environment/spdlog)

# ==============================================================================
#  jsoncpp  （目标名：jsoncpp_static）
# ==============================================================================
set(JSONCPP_WITH_TESTS              OFF CACHE BOOL "" FORCE)
set(JSONCPP_WITH_POST_BUILD_UNITTEST OFF CACHE BOOL "" FORCE)
set(JSONCPP_WITH_EXAMPLE            OFF CACHE BOOL "" FORCE)
set(JSONCPP_WITH_CMAKE_PACKAGE      OFF CACHE BOOL "" FORCE)
set(JSONCPP_WITH_PKGCONFIG_SUPPORT  OFF CACHE BOOL "" FORCE)
set(JSONCPP_WITH_INSTALL            OFF CACHE BOOL "" FORCE)
set(BUILD_STATIC_LIBS               ON  CACHE BOOL "" FORCE)
add_subdirectory(${TK_ENV}/jsoncpp-master ${CMAKE_BINARY_DIR}/Environment/jsoncpp)

# ==============================================================================
#  AngelScript  （目标名：angelscript）
#  ⚠ 它的 add_subdirectory 被**故意**放在本文件后面 llama.cpp 段之后，顺序不能调换！
#    原因：AngelScript 在 MSVC 下会 enable_language(ASM_MASM)，并把
#          source/as_callfunc_x64_msvc_asm.asm 作为汇编源文件加进 angelscript 库；
#          而 llama.cpp 的 ggml 会 project(... ASM) 重新启用通用 ASM 语言，把
#          .asm 的扩展名归属从 ASM_MASM 手里抢走。若 AngelScript 先注册、ggml 后抢，
#          regenerate 出来的 angelscript.vcxproj 会把它写成 <None Include="...asm">
#          （即不参与编译、不生成 .obj），链接时便报：
#            LNK2019: 无法解析的外部符号 CallX64 / GetReturnedFloat / GetReturnedDouble
#            LNK1120: 3 个无法解析的外部命令
#          先让 ggml 抢完，AngelScript 再注册 ASM_MASM，生成的才是 <MASM Include="...">
#          （实测四种顺序：仅 AS→MASM ✓ / AS在前+llama在后→None ✗ /
#            llama在前+AS在后→MASM ✓ / 事后补 enable_language 无效 ✗）
# ==============================================================================

# ==============================================================================
#  curl  （目标名：libcurl / CURL::libcurl）
#  SSL 后端使用 Windows 自带的 Schannel —— 这样完全不需要 OpenSSL 就能用 https。
# ==============================================================================
set(BUILD_CURL_EXE            OFF CACHE BOOL "" FORCE)
set(BUILD_STATIC_LIBS         ON  CACHE BOOL "" FORCE)
set(BUILD_STATIC_CURL         OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING             OFF CACHE BOOL "" FORCE)
set(BUILD_EXAMPLES            OFF CACHE BOOL "" FORCE)
set(CURL_DISABLE_INSTALL      ON  CACHE BOOL "" FORCE)
set(CURL_ENABLE_EXPORT_TARGET OFF CACHE BOOL "" FORCE)
set(CURL_USE_SCHANNEL         ON  CACHE BOOL "" FORCE)
set(CURL_WINDOWS_SSPI         ON  CACHE BOOL "" FORCE)
set(CURL_ZLIB                 OFF CACHE BOOL "" FORCE)
set(CURL_BROTLI               OFF CACHE BOOL "" FORCE)
set(CURL_ZSTD                 OFF CACHE BOOL "" FORCE)
set(CURL_USE_LIBPSL           OFF CACHE BOOL "" FORCE)
set(CURL_USE_LIBSSH2          OFF CACHE BOOL "" FORCE)
set(CURL_USE_LIBSSH           OFF CACHE BOOL "" FORCE)
set(USE_LIBIDN2               OFF CACHE BOOL "" FORCE)
set(USE_WIN32_IDN             OFF CACHE BOOL "" FORCE)
set(USE_NGHTTP2               OFF CACHE BOOL "" FORCE)
set(USE_NGTCP2                OFF CACHE BOOL "" FORCE)
set(USE_QUICHE                OFF CACHE BOOL "" FORCE)
set(CURL_DISABLE_LDAP         ON  CACHE BOOL "" FORCE)
set(CURL_DISABLE_LDAPS        ON  CACHE BOOL "" FORCE)
set(BUILD_LIBCURL_DOCS        OFF CACHE BOOL "" FORCE)
set(BUILD_MISC_DOCS           OFF CACHE BOOL "" FORCE)
set(ENABLE_CURL_MANUAL        OFF CACHE BOOL "" FORCE)
set(CURL_WERROR               OFF CACHE BOOL "" FORCE)
set(PICKY_COMPILER            OFF CACHE BOOL "" FORCE)

# 静态链接 libcurl 时必须定义这个宏：否则 curl/curl.h 会把所有 API 声明成
# __declspec(dllimport)（变成 __imp_curl_xxx），链接静态库时就找不到符号。
# （工程自己的 .cpp 只 include <curl/curl.h>，不会自动拿到 libcurl 目标上的这个宏）
add_compile_definitions(CURL_STATICLIB)

add_subdirectory(${TK_ENV}/curl-master ${CMAKE_BINARY_DIR}/Environment/curl)

# ==============================================================================
#  leptonica + tesseract（OCR）
#
#  tesseract 的 CMake 用 find_package(Leptonica CONFIG) 找 leptonica，
#  而源码树里没有 LeptonicaConfig.cmake，所以这里：
#    1) add_subdirectory 编译 leptonica；
#    2) 把 leptonica/src/*.h 复制成 <shim>/leptonica/*.h
#       （工程代码写的是 #include <leptonica/allheaders.h>，是安装版布局）；
#    3) 生成一个 LeptonicaConfig.cmake 指向上面的 shim。
#  另外 tesseract 只需要 pixCreate/pixClipRectangle 这类内存操作，
#  不需要 png/jpeg/tiff 解码，所以 TIFF/ARCHIVE/CURL 全部关掉。
# ==============================================================================
set(TK_LEPTONICA_DIR  "${TK_ENV}/leptonica-master")
set(TK_TESSERACT_DIR  "${TK_ENV}/tesseract-main")

if(EXISTS "${TK_LEPTONICA_DIR}/CMakeLists.txt")
	# --- 1) 编译 leptonica ---
	# OCR 只需要内存 Pix（pixCreate/pixClipRectangle 等），不涉及 png/jpeg/tiff 解码
	set(BUILD_PROG    OFF CACHE BOOL "" FORCE)
	set(STRICT_CONF   OFF CACHE BOOL "" FORCE)
	set(ENABLE_ZLIB   OFF CACHE BOOL "" FORCE)
	set(ENABLE_PNG    OFF CACHE BOOL "" FORCE)
	set(ENABLE_GIF    OFF CACHE BOOL "" FORCE)
	set(ENABLE_JPEG   OFF CACHE BOOL "" FORCE)
	set(ENABLE_TIFF   OFF CACHE BOOL "" FORCE)
	set(ENABLE_WEBP   OFF CACHE BOOL "" FORCE)
	set(ENABLE_OPENJPEG OFF CACHE BOOL "" FORCE)
	add_subdirectory(${TK_LEPTONICA_DIR} ${CMAKE_BINARY_DIR}/Environment/leptonica)

	# 兼容不同版本的 target 名
	foreach(_tk_lept_cand leptonica lept Leptonica)
		if(TARGET ${_tk_lept_cand})
			set(TK_LEPTONICA_TARGET ${_tk_lept_cand})
			break()
		endif()
	endforeach()
	if(NOT TK_LEPTONICA_TARGET)
		message(FATAL_ERROR "leptonica 已编译，但没有找到 leptonica 库 target。")
	endif()

	# --- 2) 头文件垫片：<shim>/leptonica/*.h ---
	# 工程代码写的是 #include <leptonica/allheaders.h>（安装版布局），
	# 而源码树里 .h 都在 src/ 且 config_auto.h / endianness.h 是构建时生成的。
	set(TK_LEPTONICA_SHIM "${CMAKE_BINARY_DIR}/Environment/leptonica_include")
	file(GLOB _tk_lept_headers
	     "${TK_LEPTONICA_DIR}/src/*.h"
	     "${CMAKE_BINARY_DIR}/Environment/leptonica/src/config_auto.h"
	     "${CMAKE_BINARY_DIR}/Environment/leptonica/src/endianness.h")
	foreach(_tk_h IN LISTS _tk_lept_headers)
		get_filename_component(_tk_h_name "${_tk_h}" NAME)
		configure_file("${_tk_h}" "${TK_LEPTONICA_SHIM}/leptonica/${_tk_h_name}" COPYONLY)
	endforeach()
	# 两种写法都要能解析：工程里写的是 <leptonica/allheaders.h>（安装版布局），
	# 而 tesseract 自己的源码里写的是 #include "allheaders.h"（不带目录前缀）。
	include_directories(SYSTEM ${TK_LEPTONICA_SHIM} ${TK_LEPTONICA_SHIM}/leptonica)

	# --- 3) 生成 LeptonicaConfig.cmake 给 tesseract 用 ---
	set(_tk_lept_ver "1.88.0")
	file(READ "${TK_LEPTONICA_DIR}/CMakeLists.txt" _tk_lept_txt)
	string(REGEX MATCH "\n[ \t]*VERSION[ \t]+([0-9]+\\.[0-9]+\\.[0-9]+)" _tk_lept_ver_line "${_tk_lept_txt}")
	if(CMAKE_MATCH_1)
		set(_tk_lept_ver "${CMAKE_MATCH_1}")
	endif()

	set(TK_LEPTONICA_CMAKE_DIR "${CMAKE_BINARY_DIR}/Environment/leptonica_cmake")
	file(WRITE "${TK_LEPTONICA_CMAKE_DIR}/LeptonicaConfig.cmake"
"set(Leptonica_VERSION \"${_tk_lept_ver}\")
set(Leptonica_INCLUDE_DIRS \"${TK_LEPTONICA_SHIM};${TK_LEPTONICA_SHIM}/leptonica\")
set(Leptonica_LIBRARIES \"${TK_LEPTONICA_TARGET}\")
set(Leptonica_FOUND TRUE)
")
	file(WRITE "${TK_LEPTONICA_CMAKE_DIR}/LeptonicaConfigVersion.cmake"
"set(PACKAGE_VERSION \"${_tk_lept_ver}\")
if(PACKAGE_FIND_VERSION VERSION_GREATER PACKAGE_VERSION)
  set(PACKAGE_VERSION_COMPATIBLE FALSE)
else()
  set(PACKAGE_VERSION_COMPATIBLE TRUE)
  if(PACKAGE_FIND_VERSION VERSION_EQUAL PACKAGE_VERSION)
    set(PACKAGE_VERSION_EXACT TRUE)
  endif()
endif()
")
	set(Leptonica_DIR "${TK_LEPTONICA_CMAKE_DIR}" CACHE PATH "leptonica config (generated)" FORCE)

	# --- 4) 编译 tesseract（目标名：libtesseract）---
	set(OPENMP_BUILD                OFF CACHE BOOL "" FORCE)
	set(BUILD_TRAINING_TOOLS        OFF CACHE BOOL "" FORCE)   # 训练工具需要 ICU，关掉
	set(BUILD_TESTS                 OFF CACHE BOOL "" FORCE)
	set(DISABLE_TIFF                ON  CACHE BOOL "" FORCE)
	set(DISABLE_ARCHIVE             ON  CACHE BOOL "" FORCE)
	set(DISABLE_CURL                ON  CACHE BOOL "" FORCE)
	set(INSTALL_CONFIGS             OFF CACHE BOOL "" FORCE)
	set(ENABLE_UNITY_BUILD          OFF CACHE BOOL "" FORCE)
	set(ENABLE_PRECOMPILED_HEADERS  OFF CACHE BOOL "" FORCE)
	set(ENABLE_CCACHE               OFF CACHE BOOL "" FORCE)
	set(ENABLE_NINJA_POOL           OFF CACHE BOOL "" FORCE)
	set(GRAPHICS_DISABLED           ON  CACHE BOOL "" FORCE)
	add_subdirectory(${TK_TESSERACT_DIR} ${CMAKE_BINARY_DIR}/Environment/tesseract)

	# DebugPixa 的构造函数会调 bmfCreate() 读一份**内置的 TIFF 字体**
	# （Environment/tesseract-main/src/ccstruct/debugpixa.h:18）。本次 leptonica 是源码直引，
	# 而 Environment 里没有 TIFF/PNG/JPEG 的库源码，所以 leptonica 编出来不含任何图像编解码器
	# （生成的 config_auto.h 里 HAVE_LIBTIFF 等全是 #undef）：bmfCreate 必然失败，每次 OCR
	# 初始化都会往 stderr 刷 4 行 "Error in pixReadMemTiff: function not present"。
	# DebugPixa 只服务于 tesseract 的调试图像转储（默认全程关闭），而 tesseract 源码里本来就留了
	# TESSERACT_DISABLE_DEBUG_FONTS 这个开关（debugpixa.h:15）：打开后 fonts_ = nullptr，
	# 根本不调 bmfCreate，也就不再需要 leptonica 的 TIFF 解码能力。
	# 注意：它只改构造函数体、不改类布局，所以对 libtesseract 之外的代码没有 ODR 影响。
	if(TARGET libtesseract)
		target_compile_definitions(libtesseract PRIVATE TESSERACT_DISABLE_DEBUG_FONTS)
	endif()

	set(TK_OCR_LIBS libtesseract)
else()
	set(TK_OCR_LIBS "")
	message(WARNING
		"未找到 leptonica 源码目录：${TK_LEPTONICA_DIR}\n"
		"  OCR 功能需要它，请下载 leptonica 源码（>= 1.80，推荐 1.85.0）解压到该目录，\n"
		"  下载地址：https://github.com/DanBloomberg/leptonica/releases\n"
		"  下载完成后重新运行 CMake 即可自动接入。")
endif()

# ==============================================================================
#  llama.cpp（本地 LLM 推理；目标名：llama，并由它 PUBLIC 依赖 ggml / ggml-base / ggml-cpu）
#
#  用 add_subdirectory 引入时 llama.cpp 自己会发现 CMAKE_SOURCE_DIR != 自己的目录，
#  于是 LLAMA_STANDALONE=OFF：tools / examples / server / tests / common **默认全部关闭**，
#  只编译 llama 本体 + ggml 的 CPU 后端，全程离线，不会下载任何东西
#  （ggml 唯一的联网分支是 GGML_OPENMP_FETCH，默认 OFF，这里再显式关一次）。
#  注意：common 库才会拉 cpp-httplib（以及它背后的 OpenSSL），本项目不需要，保持 OFF。
# ==============================================================================
set(LLAMA_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_TOOLS    OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_SERVER   OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_COMMON   OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_APP      OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_MTMD     OFF CACHE BOOL "" FORCE)
set(LLAMA_OPENSSL        OFF CACHE BOOL "" FORCE)   # 本工程不用 OpenSSL
set(LLAMA_CURL           OFF CACHE BOOL "" FORCE)
set(LLAMA_ALL_WARNINGS   OFF CACHE BOOL "" FORCE)   # 不要 /W4 警告刷屏
set(LLAMA_FATAL_WARNINGS OFF CACHE BOOL "" FORCE)

# 只用 CPU 后端：显式关掉所有 GPU / 加速器后端，避免误用机器上已装的 Vulkan 之类工具链
set(GGML_CUDA             OFF CACHE BOOL "" FORCE)
set(GGML_VULKAN           OFF CACHE BOOL "" FORCE)
set(GGML_SYCL             OFF CACHE BOOL "" FORCE)
set(GGML_HIP              OFF CACHE BOOL "" FORCE)
set(GGML_OPENCL           OFF CACHE BOOL "" FORCE)
set(GGML_METAL            OFF CACHE BOOL "" FORCE)
set(GGML_RPC              OFF CACHE BOOL "" FORCE)
set(GGML_WEBGPU           OFF CACHE BOOL "" FORCE)
set(GGML_ZENDNN           OFF CACHE BOOL "" FORCE)
set(GGML_BLAS             OFF CACHE BOOL "" FORCE)
set(GGML_BACKEND_DL       OFF CACHE BOOL "" FORCE)
set(GGML_CPU_ALL_VARIANTS OFF CACHE BOOL "" FORCE)  # 要多套 CPU 变体（分发用）才开，会大幅加长编译
set(GGML_OPENMP_FETCH     OFF CACHE BOOL "" FORCE)  # ON 会去 GitHub 下 LLVM OpenMP，还需要 7-Zip

if(EXISTS "${TK_ENV}/llama.cpp-master/CMakeLists.txt")
	add_subdirectory(${TK_ENV}/llama.cpp-master ${CMAKE_BINARY_DIR}/Environment/llama.cpp)
	set(TK_LLAMA_LIBS llama)
else()
	set(TK_LLAMA_LIBS "")
	message(WARNING
		"未找到 llama.cpp 源码目录：${TK_ENV}/llama.cpp-master\n"
		"  需要本地 LLM 推理时请下载 llama.cpp 源码解压到该目录，\n"
		"  下载地址：https://github.com/ggml-org/llama.cpp/archive/refs/heads/master.zip\n"
		"  下载完成后重新运行 CMake 即可自动接入。")
endif()

# ==============================================================================
#  AngelScript  （目标名：angelscript）
#  工程 AngelScript/ 目录里已有 scriptbuilder/scriptstdstring 的本地副本，
#  因此不需要再编译 add_on 里的同名源码。
#  ⚠ 必须放在上面 llama.cpp 段**之后**（理由见本文件前面的说明注释）：先让 ggml 的
#     project(... ASM) 抢走 .asm 扩展名，再由本库的 enable_language(ASM_MASM) 重新注册，
#     否则 as_callfunc_x64_msvc_asm.asm 会退化成 <None>，链接期报 LNK2019 (CallX64 …)。
# ==============================================================================
set(AS_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
add_subdirectory(${TK_ENV}/angelscript_2.38.0/sdk/angelscript/projects/cmake
                 ${CMAKE_BINARY_DIR}/Environment/angelscript)

# ==============================================================================
#  加密 / 编码：已不再依赖 OpenSSL
#  工程里只用到 MD5 / base64 / AES-128-ECB（Function/Translate.cpp），
#  现在改为调用 Windows 自带的 CNG(BCrypt) 与 Crypt32，因此不需要任何第三方库，
#  只需链接系统库 bcrypt.lib（BCrypt*）与 crypt32.lib（CryptBinaryToStringA 等）。
# ==============================================================================
set(TK_CRYPTO_LIBS bcrypt.lib crypt32.lib)

# ==============================================================================
#  资源文件（保持原有行为）
# ==============================================================================
if(EXISTS ${PROJECT_SOURCE_DIR}/launch.vs.json)
	execute_process(COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/launch.vs.json ${PROJECT_SOURCE_DIR}/.vs)
endif()
execute_process(COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/Data.ini ${CMAKE_CURRENT_BINARY_DIR})
execute_process(COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/TTF      ${CMAKE_CURRENT_BINARY_DIR}/TTF)
execute_process(COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/Modes      ${CMAKE_CURRENT_BINARY_DIR}/Modes)
execute_process(COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/Opcode   ${CMAKE_CURRENT_BINARY_DIR}/Opcode)
execute_process(COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/TessData ${CMAKE_CURRENT_BINARY_DIR}/TessData)  # tesseract 语言模型
execute_process(COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/product.ico ${CMAKE_CURRENT_BINARY_DIR})
execute_process(COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/Language ${CMAKE_CURRENT_BINARY_DIR}/Language)

# ==============================================================================
#  本工程自己的子模块
# ==============================================================================
add_subdirectory(Vulkan)
add_subdirectory(texture)
add_subdirectory(ImGui)
add_subdirectory(Tool)
add_subdirectory(Function)
add_subdirectory(AngelScript)

aux_source_directory(. DIRSRCS)

add_executable(TranslatorKyi ${DIRSRCS}
	resource.rc   # exe 图标
)

# ==============================================================================
#  链接：第三方库现在全部是源码目标，直接写 target 名即可
# ==============================================================================
target_link_libraries(TranslatorKyi
	# 本工程子模块
	vulkanLib
	textureLib
	imguiLib
	ToolLib
	FunctionLib
	AngelScriptLib

	# Vulkan（SDK 提供的 lib，非源码）
	vulkan-1.lib

	# Environment 源码库
	glfw
	imgui
	spdlog
	jsoncpp_static
	libcurl
	angelscript

	# OCR（源码）
	${TK_OCR_LIBS}

	# 本地 LLM 推理（llama.cpp 源码：llama -> ggml / ggml-base / ggml-cpu）
	${TK_LLAMA_LIBS}

	# Windows 系统库（MD5 / base64 / AES 的 CNG 实现）
	${TK_CRYPTO_LIBS}
)

# ==============================================================================
#  按配置设置链接参数：Debug 保留控制台，Release 系列不弹控制台窗口
#  （LINK_FLAGS 里不要塞带空格的生成器表达式，MSBuild 会把整串当成文件名）
# ==============================================================================
set_target_properties(TranslatorKyi PROPERTIES
	LINK_FLAGS_DEBUG          ""
	LINK_FLAGS_RELEASE        "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup"
	LINK_FLAGS_RELWITHDEBINFO "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup"
	LINK_FLAGS_MINSIZEREL     "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup"
)
