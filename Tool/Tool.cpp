#include "Tool.h"
#include <tchar.h>
#include <new>//std::nothrow：截图的缓冲区分配失败时返回 nullptr，而不是抛异常
#include <cstdio>//fopen/fread/fwrite：给日志文件补 UTF-8 BOM
#include <filesystem>//判断日志文件是否已存在、是否为空


namespace TOOL {

	spdlog::logger* logger;

	//日志文件（logs/Error.txt）的内容本来就是 UTF-8，但文件没有 BOM 时，
	//不少查看方式会按系统代码页(936)去解释它，中文日志在屏幕上就成了乱码：
	//  记事本/旧版 VS 的自动识别、Windows PowerShell 的 Get-Content（默认按 ANSI 解码）等等。
	//这里给日志文件补一个 UTF-8 BOM（EF BB BF），它们就会按 UTF-8 打开，中文显示正常。
	//BOM 只能出现在文件最前面：已有内容且开头没有 BOM 时，把 BOM 补在最前面（只做一次）；
	//文件是新建/空的时候，直接写 BOM，后面的日志接在它后面。
	static void EnsureLogFileUtf8Bom(const std::string& Path)
	{
		static const unsigned char Bom[3] = { 0xEF, 0xBB, 0xBF };

		std::error_code DirEc;
		const size_t Slash = Path.find_last_of("/\\");
		if (Slash != std::string::npos)
		{
			std::filesystem::create_directories(Path.substr(0, Slash), DirEc);//没有 logs 目录时先建出来
		}

		std::error_code Ec;
		const bool Exists = std::filesystem::exists(Path, Ec);
		const std::uintmax_t Size = Exists ? std::filesystem::file_size(Path, Ec) : 0;

		if (Exists && Size >= 3)
		{
			FILE* f = fopen(Path.c_str(), "rb");
			if (f == nullptr)
			{
				return;
			}
			unsigned char Head[3] = { 0, 0, 0 };
			const size_t Got = fread(Head, 1, sizeof(Head), f);
			fclose(f);
			if (Got == sizeof(Head) && Head[0] == Bom[0] && Head[1] == Bom[1] && Head[2] == Bom[2])
			{
				return;//已经有 BOM 了
			}

			//读出原内容，再整体重写成「BOM + 原内容」（日志文件很小，代价可忽略）
			FILE* in = fopen(Path.c_str(), "rb");
			if (in == nullptr)
			{
				return;
			}
			std::string Body;
			char Buffer[4096];
			size_t Read = 0;
			while ((Read = fread(Buffer, 1, sizeof(Buffer), in)) > 0)
			{
				Body.append(Buffer, Read);
			}
			fclose(in);

			FILE* out = fopen(Path.c_str(), "wb");
			if (out == nullptr)
			{
				return;//文件被别的程序占着（比如日志正开着），这次就先算了
			}
			fwrite(Bom, 1, sizeof(Bom), out);
			if (!Body.empty())
			{
				fwrite(Body.data(), 1, Body.size(), out);
			}
			fclose(out);
			return;
		}

		FILE* f = fopen(Path.c_str(), "ab");
		if (f == nullptr)
		{
			return;
		}
		fwrite(Bom, 1, sizeof(Bom), f);
		fclose(f);
	}

	void SpdLogInit() {
		auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
		console_sink->set_level(spdlog::level::warn);//设置警报等级
		console_sink->set_pattern("[multi_sink_example] [%^%l%$] %v");//打印显示

		//日志文件先补上 UTF-8 BOM，再让 spdlog 接管（spdlog 只会往后追加，不影响最前面的 BOM）
		EnsureLogFileUtf8Bom("logs/Error.txt");

		auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("logs/Error.txt", false);//日志文件路径，是否覆写
		file_sink->set_level(spdlog::level::trace);//设置警报等级

		logger = new spdlog::logger("multi_sink", { console_sink, file_sink }); // 日志保存
		logger->set_level(spdlog::level::debug);//设置日志警报保存等级
	}

	template <typename T>
	T Converter(const std::string& s){
		try {
			T v{};
			std::istringstream _{ s };
			_.exceptions(std::ios::failbit);
			_ >> v;
			return v;
		}
		catch (std::exception& e) {
			throw std::runtime_error("cannot parse value '" + s + "' to type<T>.");
		};
	}

	bool BoolConverter(std::string s){
		std::transform(s.begin(), s.end(), s.begin(), ::tolower);
		static const std::unordered_map<std::string, bool> s2b{
			{"1", true},  {"true", true},   {"yes", true}, {"on", true},
			{"0", false}, {"false", false}, {"no", false}, {"off", false},
		};
		auto const value = s2b.find(s);
		if (value == s2b.end()) {
			throw std::runtime_error("'" + s + "' is not a valid boolean value.");
		}
		return value->second;
	}

	std::string StrTolower(std::string Str) {
		std::string str;
		for (size_t i = 0; i < Str.size(); i++)
		{
			str += tolower(Str[i]);
		}
		return str;
	}

	std::string StrName(std::string Str) {
		size_t dianI = Str.size();
		size_t xieI = 0;
		for (size_t i = Str.size() - 1; i > 0; i--) {
			if (Str[i] == '.') {
				dianI = i - 1;
			}
			if (Str[i] == '\\') {
				dianI -= i;
				xieI = i + 1;
				break;
			}
		}
		return Str.substr(xieI, dianI);
	}

	void FilePath(const char* path, std::vector<std::string>* strS, const char* Suffix, const char* Name, int* Index) {
		std::string ModelFileName;
		for (const auto& entry : std::filesystem::directory_iterator(path)) {
			ModelFileName = entry.path().filename().string();//获取文件名字
			for (size_t i = 0; i < ModelFileName.size(); i++)
			{
				if ((ModelFileName[i] == '.') && (StrTolower(ModelFileName.substr(i + 1, ModelFileName.size() - i - 1)) == StrTolower(Suffix))) {
					ModelFileName = ModelFileName.substr(0, i);
					strS->push_back(ModelFileName);
					if (ModelFileName == Name) {
						Index[0] = strS->size() - 1;
					}
				}
			}
		}
	}

	bool SetModifyRegedit(const char* Name, bool Bool) {
		TCHAR pFileName[MAX_PATH] = { 0 };
		DWORD dwRet = GetModuleFileName(NULL, pFileName, MAX_PATH);

		HKEY hKey;
		LPCTSTR lpRun = _T("Software\\Microsoft\\Windows\\CurrentVersion\\Run");
		long lRet = RegOpenKeyEx(HKEY_CURRENT_USER, lpRun, 0, KEY_WRITE, &hKey);
		if (lRet != ERROR_SUCCESS)
			return false;

		if (Bool) {
			lRet = RegSetValueEx(hKey, Name, 0, REG_SZ, (const BYTE*)pFileName, (_tcslen(pFileName) + 1) * sizeof(TCHAR));
			if (lRet != ERROR_SUCCESS) {
				RegCloseKey(hKey);
				return false;
			}
		}
		else {
			lRet = RegDeleteValue(hKey, Name);
			if (lRet != ERROR_SUCCESS && lRet != ERROR_FILE_NOT_FOUND) {
				RegCloseKey(hKey);
				return false;
			}
		}
		RegCloseKey(hKey);
		return true;
	}


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

	//ostringstream对象用来进行格式化的输出，常用于将各种类型转换为string类型
	//ostringstream只支持<<操作符
	template<typename T>std::string toString(const T& t)
	{
		std::ostringstream oss;  //创建一个格式化输出流
		oss << t;             //把值传递如流中
		return oss.str();
	}

	//宽字符 → 多字节（按系统 ANSI 代码页，中文系统上就是 GBK/936）。
	//函数名沿用老代码（Tool.h 里原来的注释还写反了），实际语义以调用点为准：
	//本项目里它专门把宽字符转成「给 CF_TEXT 剪贴板、老式 ANSI 接口」用的字节。
	//不再用 setlocale + wcstombs_s：setlocale 改的是进程全局状态，AI 翻译在后台线程、
	//界面在主线程，两个线程同时进来会互相把 locale 改回去，偶发转出错乱的文本。
	std::string ws2s(const std::wstring& ws)
	{
		if (ws.empty())
		{
			return std::string();
		}
		const int Len = WideCharToMultiByte(CP_ACP, 0, ws.c_str(), (int)ws.size(), nullptr, 0, nullptr, nullptr);
		if (Len <= 0)
		{
			return std::string();
		}
		std::string Result((size_t)Len, '\0');
		WideCharToMultiByte(CP_ACP, 0, ws.c_str(), (int)ws.size(), &Result[0], Len, nullptr, nullptr);
		return Result;
	}

	//多字节（系统 ANSI/GBK）→ 宽字符，与上面的 ws2s 互为反向
	std::wstring s2ws(const std::string& s)
	{
		if (s.empty())
		{
			return std::wstring();
		}
		const int Len = MultiByteToWideChar(CP_ACP, 0, s.c_str(), (int)s.size(), nullptr, 0);
		if (Len <= 0)
		{
			return std::wstring();
		}
		std::wstring Result((size_t)Len, L'\0');
		MultiByteToWideChar(CP_ACP, 0, s.c_str(), (int)s.size(), &Result[0], Len);
		return Result;
	}

	//GBK（系统 ANSI）字节 → UTF-8。函数名沿用老代码，方向以本注释为准：
	//输入是剪贴板/老接口给的 ANSI 字节，输出是可以直接交给 ImGui、curl、jsoncpp 的 UTF-8。
	std::string UnicodeToUtf8(const std::string& str) {
		if (str.empty())
		{
			return std::string();
		}
		const std::wstring wstr = s2ws(str);
		if (wstr.empty())
		{
			return std::string();
		}
		const int Len = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), nullptr, 0, nullptr, nullptr);
		if (Len <= 0)
		{
			return std::string();
		}
		std::string Result((size_t)Len, '\0');
		WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), &Result[0], Len, nullptr, nullptr);
		return Result;
	}

	//UTF-8 → GBK（系统 ANSI）字节，给只认 ANSI 的地方用（例如 CF_TEXT 剪贴板）。
	//老实现是手写 UTF-8 解码：它把四字节序列硬塞进单个 wchar_t，超出 BMP 的字符
	//（emoji 等）会解错，这里换成 Win32 的转换，代理对也交给系统处理。
	std::string Utf8ToUnicode(const std::string& utf8_str) {
		if (utf8_str.empty())
		{
			return std::string();
		}
		const int WideLen = MultiByteToWideChar(CP_UTF8, 0, utf8_str.c_str(), (int)utf8_str.size(), nullptr, 0);
		if (WideLen <= 0)
		{
			return std::string();
		}
		std::wstring Wide((size_t)WideLen, L'\0');
		MultiByteToWideChar(CP_UTF8, 0, utf8_str.c_str(), (int)utf8_str.size(), &Wide[0], WideLen);
		return ws2s(Wide);
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

	/*********************************************- FPS -*********************************************/
	clock_t kaishi_time, jieshu_time;//储存时间戳
	int number_time = 0;//当前第几帧
	const int number = 60;//多少帧刷新一次
	const double miao_time = (number + 1) * 1000;//用来计算FPS的数
	double FPStime = 0.0f;//帧数

	float values[values_number] = {};//储存FPS数据
	float Max_values; //FPS数据 最大值
	float Min_values;//FPS数据 最小值
	double Mean_values;//平均帧数

	void FPS()
	{
		if (number_time >= number) {
			number_time = 0;
			jieshu_time = clock();
			FPStime = miao_time / double(jieshu_time - kaishi_time);
			Max_values = 0.0f;
			Min_values = 10000.0f;
			Mean_values = 0.0f;
			for (int i = 0; i < values_number; i++) {
				Mean_values += values[i];

				if (values_number == i + 1) {
					values[i] = FPStime;
				}
				else {
					values[i] = values[i + 1];
				}

				if (values[i] > Max_values) {
					Max_values = values[i];
				}
				if (values[i] < Min_values) {
					Min_values = values[i];
				}
			}
			Mean_values = Mean_values / values_number;
			kaishi_time = clock();
		}
		else {
			number_time++;
		}
	}





	/*********************************************- 耗时检测 -*********************************************/

	clock_t TemporaryCycleTime;//周期总耗时开始时间戳
	clock_t CycleTime = 100;//周期总耗时
	int Gap = 100;//间隔
	int CurrentCount = 0;//现在是第次轮回
	bool DetectionSwitch = false;//更新开关
	bool DetectionQuantityName = true;//第一次检测开关
	int Quantity = 0;//总检测数量
	int DetectionCount = 0;//现在是检测第几个

	//嵌套堆载
	clock_t TemporaryConsumetime[10]{};//临时时间堆载
	clock_t TemporaryConsumeName[10]{};//临时嵌套索引堆载
	int TemporaryTimeQuantity = -1; //堆载指针

	//结果数据
	int ConsumeNumber;//最多检测数量
	char* Consume_name[DetectionNumber]{};//储存检测的名字
	clock_t TemporaryConsume_time[DetectionNumber]{};//储存检测的周期总累加耗时
	double Consume_time[DetectionNumber]{};//储存检测的百分比
	double Consume_Second[DetectionNumber]{};//储存检测的秒

	//时间记录
	bool SecondVectorBool[DetectionNumber]{}; //秒数据 最大值
	float* Consume_SecondVector[DetectionNumber]{};//储存检测的秒数组
	int SecondVectorIndex[DetectionNumber]{};//秒数组索引
	float Max_Secondvalues[SecondVectorNumber]{}; //秒数据 最大值
	float Min_Secondvalues[SecondVectorNumber]{};//秒数据 最小值

	void StartTiming(char* name, bool RecordBool)
	{
		if (DetectionQuantityName) {
			SecondVectorBool[Quantity] = RecordBool;//这个检测对象是否开启了记录
			if ((Quantity != 0) && (name == Consume_name[0])) {
				DetectionQuantityName = false;//所有检测录入一边（注意，要是一次循环没有录入就是没有录入，会有BUG，所以应用时要让他第一次全部录入进去）
				DetectionCount = 0;//新的一轮记得设为 0 ，要不然会出问题
				ConsumeNumber = Quantity;
			}
			else {
				Consume_name[Quantity] = name;
				if (SecondVectorBool[Quantity]) {
					Consume_SecondVector[Quantity] = new float[SecondVectorNumber] {};//申请记录时间数据用的空间
				}
				Quantity++;
			}
		}
		else {
			if (name == Consume_name[0]) {//判断是否是新的一轮
				DetectionCount = 0;
			}
		}
		TemporaryTimeQuantity++;//堆载指针压载
		TemporaryConsumetime[TemporaryTimeQuantity] = clock();
		TemporaryConsumeName[TemporaryTimeQuantity] = DetectionCount + TemporaryTimeQuantity;//压入索引
	}

	void StartEnd()
	{
		TemporaryConsume_time[TemporaryConsumeName[TemporaryTimeQuantity]] += (clock() - TemporaryConsumetime[TemporaryTimeQuantity]);
		TemporaryTimeQuantity--;//堆载指针出载
		DetectionCount++;
	}

	void MomentTiming(char* name, int* Index)
	{
		if (Index[0] == NULL) {
			Index[0] = Quantity;
			Consume_name[Quantity] = name;
			Consume_Second[Quantity] = 0.0f;
			SecondVectorBool[Quantity] = false;
			Quantity++;
		}
		TemporaryTimeQuantity++;//堆载指针压载
		TemporaryConsumetime[TemporaryTimeQuantity] = clock();
		TemporaryConsumeName[TemporaryTimeQuantity] = Index[0];//压入索引
	}

	void MomentEnd()
	{
		Consume_Second[TemporaryConsumeName[TemporaryTimeQuantity]] = double(clock() - TemporaryConsumetime[TemporaryTimeQuantity]) / 1000;
		TemporaryTimeQuantity--;//堆载指针出载
	}

	void RefreshTiming()
	{
		if (DetectionSwitch) {
			CycleTime = clock() - TemporaryCycleTime;//Interval 个轮回，结束计时，得出时间
			for (int i = 0; i < ConsumeNumber; i++) {
				Consume_time[i] = (double(TemporaryConsume_time[i] * 100) / CycleTime);//求出他在一个帧周期的耗时占比
				Consume_Second[i] = (double(TemporaryConsume_time[i]) / (1000 * Gap));//他所花时间
				TemporaryConsume_time[i] = 0;//清零，累计下 Interval 个轮回的时间

				if (SecondVectorBool[i]) {//时间是否记录
					Max_Secondvalues[i] = -10000.0f;
					Min_Secondvalues[i] = 10000.0f;
					for (int j = 0; j < SecondVectorNumber; j++) {
						if (SecondVectorNumber == j + 1) {
							Consume_SecondVector[i][j] = float(Consume_Second[i]);
						}
						else {
							Consume_SecondVector[i][j] = Consume_SecondVector[i][j + 1];
						}

						if (Consume_SecondVector[i][j] > Max_Secondvalues[i]) {
							Max_Secondvalues[i] = Consume_SecondVector[i][j];
						}
						if (Consume_SecondVector[i][j] < Min_Secondvalues[i]) {
							Min_Secondvalues[i] = Consume_SecondVector[i][j];
						}
					}
				}
			}
			DetectionSwitch = false;
			TemporaryCycleTime = clock();//Interval 个轮回，开始计时
		}
		else {
			CurrentCount++;//一轮结束
			if (CurrentCount >= Gap) {//判断第 Interval 就更新显示
				CurrentCount = 0;
				DetectionSwitch = true;
			}
		}
	}
}