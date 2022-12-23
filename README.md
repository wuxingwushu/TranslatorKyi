# 翻译姬(Translate-miku)
> 主要功能：

+ 选择翻译
+ 截图翻译

> 引用的技术

+ 图片识别：[Tesseract-OCR](https://tesseract-ocr.github.io/tessdoc/Home.html)
+ UI界面：[ImGui](https://github.com/ocornut/imgui)
+ 网络请求：[Curl](https://github.com/curl/curl)
+ 解析数据：[JsonCpp](https://github.com/open-source-parsers/jsoncpp)
+ 数据加密：[OpenSSL](https://github.com/openssl/openssl)
+ 内存泄露检测：[Visual](https://github.com/KindDragon/vld)

> 文件结构

```
Translate-miku
    └ Environment ┐
    │             DX11SDK
    │             jsoncpp
    │             OpenSSL-Win64
    │             Visual_Leak_Detector
    │             tesseract-5.2.0-x64-windows
    │             DLL
    │             DLLDebug
    └ tessdata ┐
    │          ...
    └ imgui ┐
    │       ...
    └ Function ┐
    │          ...
    └   ...
```

> 环境构建：

+ Tesseract-OCR
	> [vcpkg](https://github.com/microsoft/vcpkg)
	> [tesseract-ocr](https://github.com/tesseract-ocr/tesseract/tree/5.2.0)
	> 编译生成动态库 api 命令：
	> ```
	> vcpkg install tesseract:x64-windows
	> ```