# 翻译姬(TranslatorKyi)
> 主要功能：

+ 选择翻译
+ 截图翻译

> 引用的技术

+ 图形API：[VulKan](https://vulkan.lunarg.com/)
+ 窗口构建：[GLFW](https://www.glfw.org/)
+ 图片识别：[Tesseract-OCR](https://tesseract-ocr.github.io/tessdoc/Home.html)
+ UI界面：[ImGui](https://github.com/ocornut/imgui)
+ 网络请求：[Curl](https://github.com/curl/curl)
+ 解析数据：[JsonCpp](https://github.com/open-source-parsers/jsoncpp)
+ 数据加密：[OpenSSL](https://github.com/openssl/openssl)
+ 日志：[SpdLog](https://github.com/gabime/spdlog)

> 文件结构

```
Translate-miku
    └ Environment ┐
    │             DX11SDK
    │             jsoncpp
    │             OpenSSL-Win64
    │             VulKan
    │             tesseract
    │             spdlog
    │             GLFW
    └ TessData ┐
    │          ...
    └ TTF ┐
    │     ...
    └ imgui ┐
    │       ...
    └ Function ┐
    │          ...
    └ Tool ┐
    │      ...
    └ Vulkan ┐
    │        ...
    └ ...
```

> 环境构建：

> 我的使用的Environment文件下载： [百度网盘](https://pan.baidu.com/s/12bCmMmZBNSkOoNVyDw3j6g?pwd=sglz)

+ Tesseract-OCR
	> [vcpkg](https://github.com/microsoft/vcpkg)
	> [tesseract-ocr](https://github.com/tesseract-ocr/tesseract/tree/5.2.0)
	> 编译生成动态库 api 命令：
	> ```
	> 在你下载的tesseract源码打开CMD输入指令："你的路径\vcpkg.exe install tesseract:x64-windows-static"
	> ```