# 翻译姬(TranslatorKyi)
> 主要功能：

+ 选择翻译
+ 替换翻译
+ 截图翻译
+ 自定义脚本
+ 坚果云WebDav

> 引用的技术

+ 图形API：[VulKan](https://vulkan.lunarg.com/)
+ 窗口构建：[GLFW](https://www.glfw.org/)
+ 图片识别：[Tesseract-OCR](https://tesseract-ocr.github.io/tessdoc/Home.html)
+ UI界面：[ImGui](https://github.com/ocornut/imgui)
+ 网络请求：[Curl](https://github.com/curl/curl)
+ 解析数据：[JsonCpp](https://github.com/open-source-parsers/jsoncpp)
+ 数据加密：[OpenSSL](https://github.com/openssl/openssl)
+ 日志：[SpdLog](https://github.com/gabime/spdlog)
+ 脚本：[AngelScript](https://www.angelcode.com/angelscript/)

> 文件结构

```
Translate-miku
    └ Environment ┐环境
    │             DX11SDK
    │             jsoncpp
    │             OpenSSL-Win64
    │             VulKan
    │             tesseract
    │             spdlog
    │             GLFW
    │             AngelScript
    └ TessData ┐模型
    │          ...
    └ AngelScript ┐脚本
    │             ...
    └ TTF ┐字体
    │     ...
    └ imgui ┐UI
    │       ...
    └ Function ┐功能
    │          ...
    └ Tool ┐工具
    │      ...
    └ Vulkan ┐图形API
    │        ...
    └ ...
```

> 环境构建：

> 我的使用的Environment文件下载： [百度网盘](https://pan.baidu.com/s/1jQSt2NMCDpvJEjL2n34P7Q?pwd=7cl0)提取码：7cl0

+ Tesseract-OCR
	> [vcpkg](https://github.com/microsoft/vcpkg)
	> [tesseract-ocr](https://github.com/tesseract-ocr/tesseract/tree/5.2.0)
	> 编译生成动态库 api 命令：
	> ```
	> 在你下载的tesseract源码打开CMD输入指令："你的路径\vcpkg.exe install tesseract:x64-windows-static"
	> ```