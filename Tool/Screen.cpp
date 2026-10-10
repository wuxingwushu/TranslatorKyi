#include "Screen.h"
#include <windows.h>
#include <new>//std::nothrow：截图的缓冲区分配失败时返回 nullptr，而不是抛异常
#include <cstring>
#include "../Variable.h"//截图尺寸写回 windows_Width/ScreenShot_Width 等

namespace TOOL {

	char* screen(char* buf) {
		//缓冲区改由本函数自己持有。原因：截图尺寸会随分辨率/显示器/DPI 变化，而调用方
		//（application.h 的 buffer，初值 nullptr）原来只在第一次分配，之后再没人重分配也没人释放，
		//分辨率一变，后续 OCR/纹理上传就全按新尺寸去读一块旧尺寸的缓冲区，越界读堆。
		//形参 buf 保留只是为了不改调用点，Tool.h 里已注明返回值不要 delete[]。
		(void)buf;
		static char* Buffer = nullptr;//本函数持有的截图缓冲区
		static size_t BufferBytes = 0;//它当前有多大

		HWND window = GetDesktopWindow();
		HDC _dc = GetWindowDC(window);//屏幕DC
		HDC dc = CreateCompatibleDC(0);//内存DC
		if (_dc == NULL || dc == NULL)
		{
			if (_dc != NULL) { ReleaseDC(window, _dc); }
			if (dc != NULL) { DeleteDC(dc); }
			return Buffer;
		}

		RECT re;
		GetWindowRect(window, &re);
		Variable::windows_Width = re.right;
		Variable::windows_Heigth = re.bottom;
		//OCR、纹理上传、显示都按这一对尺寸走，它们必须和下面这块缓冲区严格配套
		Variable::ScreenShot_Width = Variable::windows_Width;
		Variable::ScreenShot_Heigth = Variable::windows_Heigth;

		const size_t NeedBytes = (size_t)Variable::windows_Width * (size_t)Variable::windows_Heigth * 4;
		if (Buffer == nullptr || BufferBytes < NeedBytes)
		{
			delete[] Buffer;//尺寸变了就重新分配，别让旧的小缓冲区继续用
			Buffer = new (std::nothrow) char[NeedBytes]();//失败返回 nullptr，不让异常穿到主循环
			BufferBytes = NeedBytes;
		}
		if (Buffer == nullptr)//分配失败（尺寸过大/内存不足）就别往下走了
		{
			ReleaseDC(window, _dc);
			DeleteDC(dc);
			return nullptr;
		}

		HBITMAP bm = CreateCompatibleBitmap(_dc, Variable::windows_Width, Variable::windows_Heigth);//建立和屏幕兼容的bitmap
		if (bm == NULL)
		{
			ReleaseDC(window, _dc);
			DeleteDC(dc);
			return Buffer;
		}
		SelectObject(dc, bm);//将memBitmap选入内存DC
		StretchBlt(dc, 0, 0, Variable::windows_Width, Variable::windows_Heigth, _dc, 0, 0, Variable::windows_Width, Variable::windows_Heigth, SRCCOPY);//复制屏幕图像到内存DC

		BITMAP bmInfo;
		memset(&bmInfo, 0, sizeof(bmInfo));
		//旧代码是 GetObject(bm, 84, buff)：把 84 字节的 BITMAP 结构写进了截图缓冲区，而且结果没人用
		GetObject(bm, sizeof(bmInfo), &bmInfo);

		void* buff = nullptr;
		tagBITMAPINFO bi;
		memset(&bi, 0, sizeof(bi));
		bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
		bi.bmiHeader.biWidth = Variable::windows_Width;
		bi.bmiHeader.biHeight = Variable::windows_Heigth;
		bi.bmiHeader.biPlanes = 1;
		bi.bmiHeader.biBitCount = 32;
		bi.bmiHeader.biCompression = 0;
		bi.bmiHeader.biSizeImage = 0;

		void* dcf = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &buff, NULL, NULL);
		if (dcf == NULL || buff == nullptr)
		{
			if (dcf != NULL) { DeleteObject(dcf); }
			DeleteObject(bm);
			ReleaseDC(window, _dc);
			DeleteDC(dc);
			return Buffer;
		}
		GetDIBits(dc, bm, 0, Variable::windows_Heigth, buff, &bi, DIB_RGB_COLORS);

		for (int yyy = 0; yyy < Variable::windows_Heigth; yyy++)
		{
			memcpy(&Buffer[(yyy * Variable::windows_Width * 4)], &((char*)buff)[((Variable::windows_Heigth - yyy - 1) * Variable::windows_Width) * 4], (4 * Variable::windows_Width));
		}


		DeleteObject(dcf);
		DeleteObject(bm);
		ReleaseDC(window, _dc);//旧代码漏了这一个（GetWindowDC 拿到的一定要 ReleaseDC）
		DeleteDC(dc);
		return Buffer;
	}
}