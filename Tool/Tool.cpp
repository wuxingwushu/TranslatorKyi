#include "Tool.h"
#include <iostream>

namespace TOOL {

	spdlog::logger* logger;

	void SpdLogInit() {
		auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
		console_sink->set_level(spdlog::level::warn);//设置警报等级
		console_sink->set_pattern("[multi_sink_example] [%^%l%$] %v");//打印显示

		auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("logs/Error.txt", false);//日志文件路径，是否覆写
		file_sink->set_level(spdlog::level::trace);//设置警报等级

		logger = new spdlog::logger("multi_sink", { console_sink, file_sink }); // 日志保存
		logger->set_level(spdlog::level::debug);//设置日志警报保存等级
	}

	template <typename T>
	inline T Converter(const std::string& s) {
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
		char pFileName[MAX_PATH] = { 0 };
		DWORD dwRet = GetModuleFileName(NULL, (LPSTR)pFileName, MAX_PATH);

		HKEY hKey;
		LPCTSTR lpRun = "Software\\Microsoft\\Windows\\CurrentVersion\\Run";
		long lRet = RegOpenKeyEx(HKEY_LOCAL_MACHINE, lpRun, 0, KEY_WRITE, &hKey);
		if (lRet != ERROR_SUCCESS)
			return false;

		if (Bool)
			RegSetValueEx(hKey, Name, 0, REG_SZ, (const BYTE*)(LPCSTR)pFileName, MAX_PATH);//添加一个Key
		else
			RegDeleteValueA(hKey, Name);//删除一个Key
		RegCloseKey(hKey);//关闭注册表
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

	//string 转 wstring
	std::string ws2s(const std::wstring& ws)
	{
		size_t i;
		std::string curLocale = setlocale(LC_ALL, NULL);
		setlocale(LC_ALL, "chs");
		const wchar_t* _source = ws.c_str();
		size_t _dsize = 2 * ws.size() + 1;
		char* _dest = new char[_dsize];
		memset(_dest, 0x0, _dsize);
		wcstombs_s(&i, _dest, _dsize, _source, _dsize);
		std::string result = _dest;
		delete[] _dest;
		setlocale(LC_ALL, curLocale.c_str());
		return result;
	}

	//wstring 转 string
	std::wstring s2ws(const std::string& s)
	{
		size_t i;
		std::string curLocale = setlocale(LC_ALL, NULL);
		setlocale(LC_ALL, "chs");
		const char* _source = s.c_str();
		size_t _dsize = s.size() + 1;
		wchar_t* _dest = new wchar_t[_dsize];
		wmemset(_dest, 0x0, _dsize);
		mbstowcs_s(&i, _dest, _dsize, _source, _dsize);
		std::wstring result = _dest;
		delete[] _dest;
		setlocale(LC_ALL, curLocale.c_str());
		return result;
	}

	//Unicode 转到 utf8
	std::string UnicodeToUtf8(const std::string& str) {
		const std::wstring wstr = s2ws(str);
		std::vector<char> utf8buf;
		utf8buf.reserve(wstr.length() * 4);

		for (const auto& wc : wstr) {
			if (wc < 0x80) {
				utf8buf.push_back(static_cast<char>(wc));
			}
			else if (wc < 0x800) {
				utf8buf.push_back(static_cast<char>((wc >> 6) | 0xC0));
				utf8buf.push_back(static_cast<char>((wc & 0x3F) | 0x80));
			}
			else if (wc < 0x10000) {
				utf8buf.push_back(static_cast<char>((wc >> 12) | 0xE0));
				utf8buf.push_back(static_cast<char>(((wc >> 6) & 0x3F) | 0x80));
				utf8buf.push_back(static_cast<char>((wc & 0x3F) | 0x80));
			}
			else {
				utf8buf.push_back(static_cast<char>((wc >> 18) | 0xF0));
				utf8buf.push_back(static_cast<char>(((wc >> 12) & 0x3F) | 0x80));
				utf8buf.push_back(static_cast<char>(((wc >> 6) & 0x3F) | 0x80));
				utf8buf.push_back(static_cast<char>((wc & 0x3F) | 0x80));
			}
		}

		return std::string(utf8buf.data(), utf8buf.size());
	}

	//utf8 转到 Unicode
	std::string Utf8ToUnicode(const std::string& utf8_str) {
		std::wstring result;
		int i = 0;

		while (i < utf8_str.size()) {
			wchar_t ch = 0;
			unsigned char byte = utf8_str[i];

			if (byte <= 0x7F) {
				// Single-byte character
				ch = byte;
				i += 1;
			}
			else if ((byte & 0xE0) == 0xC0) {
				// Two-byte character
				ch |= (byte & 0x1F) << 6;
				ch |= (utf8_str[i + 1] & 0x3F);
				i += 2;
			}
			else if ((byte & 0xF0) == 0xE0) {
				// Three-byte character
				ch |= (byte & 0x0F) << 12;
				ch |= (utf8_str[i + 1] & 0x3F) << 6;
				ch |= (utf8_str[i + 2] & 0x3F);
				i += 3;
			}
			else if ((byte & 0xF8) == 0xF0) {
				// Four-byte character
				ch |= (byte & 0x07) << 18;
				ch |= (utf8_str[i + 1] & 0x3F) << 12;
				ch |= (utf8_str[i + 2] & 0x3F) << 6;
				ch |= (utf8_str[i + 3] & 0x3F);
				i += 4;
			}
			else {
				// Invalid byte
				i += 1;
			}

			result += ch;
		}

		return TOOL::ws2s(result);
	}

	std::string ClipboardTochar() {
		int ClipboardBoll = 5;
		while (ClipboardBoll > 0) {
			ClipboardBoll--;
			if (!OpenClipboard(NULL))//打开剪贴板
			{
				printf("打开剪贴板失败\n");
				CloseClipboard();//关闭剪贴板
				continue;
			}

			HGLOBAL hmem = GetClipboardData(CF_TEXT);//获取剪切板内容块
			if (hmem == NULL)    // 对剪切板分配内存
			{
				printf("获取剪切板内容块错误!!!\n");
				CloseClipboard();//关闭剪贴板
				continue;
			}
			else {
				std::string CharS = (char*)GlobalLock(hmem);//获取内容块的地址
				CloseClipboard();//关闭剪贴板
				return CharS;
			}
		}
	}

	void CopyToClipboard(std::string str) {
		int ClipboardBoll = 5;
		while (ClipboardBoll > 0) {
			ClipboardBoll--;
			if (!OpenClipboard(NULL))//打开剪贴板
			{
				puts("打开剪贴板失败\n");
				CloseClipboard();
				continue;
			}

			if (!EmptyClipboard())       // 清空剪切板，写入之前，必须先清空剪切板
			{
				puts("清空剪切板失败\n");
				CloseClipboard();
				continue;
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
				CloseClipboard();
				continue;
			}

			memcpy_s(lpMemory, strlen(str.c_str()) + 1, str.c_str(), strlen(str.c_str()) + 1);   // 将数据复制进入内存区域

			GlobalUnlock(hMemory);                   // 解除内存锁定

			if (SetClipboardData(CF_TEXT, hMemory) == NULL)
			{
				puts("设置剪切板数据失败!!!\n");
				CloseClipboard();
				continue;
			}

			CloseClipboard();//关闭剪贴板
			return;
		}
	}

	char* screen(char* buf) {
		HWND window = GetDesktopWindow();
		HDC _dc = GetWindowDC(window);//屏幕DC
		HDC dc = CreateCompatibleDC(0);//内存DC

		RECT re;
		GetWindowRect(window, &re);
		Variable::windows_Width = re.right;
		Variable::windows_Heigth = re.bottom;

		if (buf == nullptr) {
			buf = new char[Variable::windows_Heigth * Variable::windows_Width * 4];
		}
			
		void* buff = buf;
		HBITMAP bm = CreateCompatibleBitmap(_dc, Variable::windows_Width, Variable::windows_Heigth);//建立和屏幕兼容的bitmap
		SelectObject(dc, bm);//将memBitmap选入内存DC
		StretchBlt(dc, 0, 0, Variable::windows_Width, Variable::windows_Heigth, _dc, 0, 0, Variable::windows_Width, Variable::windows_Heigth, SRCCOPY);//复制屏幕图像到内存DC

		GetObject(bm, 84, buff);

		

		tagBITMAPINFO bi;
		bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
		bi.bmiHeader.biWidth = Variable::windows_Width;
		bi.bmiHeader.biHeight = Variable::windows_Heigth;
		bi.bmiHeader.biPlanes = 1;
		bi.bmiHeader.biBitCount = 32;
		bi.bmiHeader.biCompression = 0;
		bi.bmiHeader.biSizeImage = 0;

		void* dcf = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &buff, NULL, NULL);
		GetDIBits(dc, bm, 0, Variable::windows_Heigth, buff, &bi, DIB_RGB_COLORS);

		for (int yyy = 0; yyy < Variable::windows_Heigth; yyy++)
		{
			memcpy(&buf[(yyy * Variable::windows_Width * 4)], &((char*)buff)[((Variable::windows_Heigth - yyy - 1) * Variable::windows_Width) * 4], (4 * Variable::windows_Width));
		}


		DeleteObject(dcf);
		DeleteObject(bm);
		DeleteDC(_dc);
		DeleteDC(dc);
		return buf;
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