# 翻译姬(Translate-miku)
> 主要功能：

+ 选择翻译
+ 截图翻译

> 引用的技术

+ [Tesseract-OCR](https://tesseract-ocr.github.io/tessdoc/Home.html)
+ [QT](https://www.qt.io/zh-cn/)
+ 爬虫

> 环境构建：

+ QT6安装:

	> [QT6](https://download.qt.io/official_releases/online_installers/)
	> [Toos](https://download.qt.io/archive/vsaddin/)

+ Tesseract-OCR
	> [vcpkg](https://github.com/microsoft/vcpkg)
	> [tesseract-ocr](https://github.com/tesseract-ocr/tesseract/tree/5.2.0)
	> 编译生成动态库 api 命令：
	> ```
	> vcpkg install tesseract:x64-windows
	> ```