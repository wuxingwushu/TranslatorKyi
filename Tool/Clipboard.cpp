#include "Clipboard.h"
#include "Charset.h"
#include <windows.h>
#include <cstdio>
#include <cstring>

namespace TOOL {

	void CtrlAndC() {
		keybd_event(17, 0, 0, 0);//按下 ctrl
		keybd_event(67, 0, 0, 0);//按下 c
		keybd_event(17, 0, KEYEVENTF_KEYUP, 0);//松开 ctrl 
		keybd_event(67, 0, KEYEVENTF_KEYUP, 0);//松开 c
	}

	void CtrlAndV() {
		keybd_event(17, 0, 0, 0);//按下 ctrl
		keybd_event(86, 0, 0, 0);//按下 v
		keybd_event(17, 0, KEYEVENTF_KEYUP, 0);//松开 ctrl
		keybd_event(86, 0, KEYEVENTF_KEYUP, 0);//松开 v
	}

	std::string ClipboardTochar() {
		int ClipboardBoll = 5;
		while (ClipboardBoll > 0) {
			ClipboardBoll--;
			//打开失败时剪贴板并没有被打开，再 CloseClipboard 是错的操作（旧代码就是这么写的）
			if (!OpenClipboard(NULL))//打开剪贴板
			{
				printf("打开剪贴板失败\n");
				continue;
			}

			//优先取 Unicode 版：浏览器、VS Code、Office 这些现代程序常常只放 CF_UNICODETEXT，
			//旧代码只读 CF_TEXT，遇到这类来源要么拿不到内容、要么只拿到半截。
			//本函数的约定不变：返回的仍然是 ANSI(GBK) 字节，调用方照旧用 UnicodeToUtf8 转成 UTF-8。
			if (IsClipboardFormatAvailable(CF_UNICODETEXT)) {
				HANDLE hUnicode = GetClipboardData(CF_UNICODETEXT);
				const wchar_t* pWide = (hUnicode != NULL) ? (const wchar_t*)GlobalLock(hUnicode) : NULL;
				if (pWide != NULL) {
					std::string CharS = TOOL::ws2s(std::wstring(pWide));
					GlobalUnlock(hUnicode);
					CloseClipboard();//关闭剪贴板
					return CharS;
				}
			}

			HGLOBAL hmem = GetClipboardData(CF_TEXT);//获取剪切板内容块
			if (hmem == NULL)    // 对剪切板分配内存
			{
				printf("获取剪切板内容块错误!!!\n");
				CloseClipboard();//关闭剪贴板
				continue;
			}

			const char* pText = (const char*)GlobalLock(hmem);//获取内容块的地址
			if (pText == NULL)//锁定失败：必须关闭剪贴板，否则一直占着不放
			{
				printf("锁定剪贴板内存失败!!!\n");
				CloseClipboard();//关闭剪贴板
				continue;
			}

			std::string CharS = pText;
			GlobalUnlock(hmem);//解除内存锁定
			CloseClipboard();//关闭剪贴板
			return CharS;
		}
		return std::string();//5 次都没成功也要有返回值（旧代码直接从函数末尾掉出去，是未定义行为）
	}

	void CopyToClipboard(std::string str) {
		int ClipboardBoll = 5;
		while (ClipboardBoll > 0) {
			ClipboardBoll--;
			if (!OpenClipboard(NULL))//打开剪贴板
			{
				puts("打开剪贴板失败\n");
				continue;//没打开就不能去关，旧代码这里会 CloseClipboard 一个没开的剪贴板
			}

			if (!EmptyClipboard())       // 清空剪切板，写入之前，必须先清空剪切板
			{
				puts("清空剪切板失败\n");
				CloseClipboard();
				continue;
			}

			//同时放一份 Unicode 版：现代程序（浏览器、VS Code、Office）粘贴时优先读
			//CF_UNICODETEXT，只放 CF_TEXT 的话粘出来就可能变成“鎺㈡祴”这种乱码。
			const std::wstring Wide = TOOL::s2ws(str);
			if (!Wide.empty())
			{
				const size_t WideBytes = (Wide.size() + 1) * sizeof(wchar_t);
				HGLOBAL hWide = GlobalAlloc(GMEM_MOVEABLE, WideBytes);
				wchar_t* lpWide = (hWide != NULL) ? (wchar_t*)GlobalLock(hWide) : NULL;
				if (lpWide != NULL)
				{
					memcpy_s(lpWide, WideBytes, Wide.c_str(), WideBytes);
					GlobalUnlock(hWide);                   // 解除内存锁定
					if (SetClipboardData(CF_UNICODETEXT, hWide) == NULL)
					{
						GlobalFree(hWide);                 //系统没接管就得自己释放，否则泄漏
					}
				}
				else if (hWide != NULL)
				{
					GlobalFree(hWide);
				}
			}

			HGLOBAL hMemory;
			if ((hMemory = GlobalAlloc(GMEM_MOVEABLE, strlen(str.c_str()) + 1)) == NULL)    // 对剪切板分配内存
			{
				puts("内存赋值错误!!!\n");
				CloseClipboard();
				continue;
			}

			LPTSTR lpMemory;
			if ((lpMemory = (LPTSTR)GlobalLock(hMemory)) == NULL)             // 将内存区域锁定
			{
				puts("锁定内存错误!!!\n");
				GlobalFree(hMemory);               //锁不上也要把内存还回去，旧代码这里直接泄漏
				CloseClipboard();
				continue;
			}

			memcpy_s(lpMemory, strlen(str.c_str()) + 1, str.c_str(), strlen(str.c_str()) + 1);   // 将数据复制进入内存区域

			GlobalUnlock(hMemory);                   // 解除内存锁定

			if (SetClipboardData(CF_TEXT, hMemory) == NULL)
			{
				puts("设置剪切板数据失败!!!\n");
				GlobalFree(hMemory);
				CloseClipboard();
				continue;
			}

			CloseClipboard();//关闭剪贴板
			return;
		}
	}
}