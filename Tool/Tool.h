#pragma once
#include <time.h>
#include <string>
#include <cstring>//memcpy：下面 CopyToBuffer 用
#include <windows.h>
#include "../Variable.h"
#include <filesystem>

#include "spdlog/spdlog.h"
#include "spdlog/cfg/env.h"  // support for loading levels from the environment variable
#include "spdlog/fmt/ostr.h" // support for user defined types
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/basic_file_sink.h"

namespace TOOL {

	extern spdlog::logger* logger;

	void SpdLogInit();

	
	template <typename T>
	T Converter(const std::string& s);

	bool BoolConverter(std::string s);

	std::string StrTolower(std::string Str);
	std::string StrName(std::string Str);
	void FilePath(const char* path, std::vector<std::string>* strS, const char* Suffix, const char* Name, int* Index);


	bool SetModifyRegedit(const char* Name, bool Bool);

	
	void CtrlAndC();
	void CtrlAndV();

	

	template<typename T>std::string toString(const T& t); //数据转字符串

	std::string ws2s(const std::wstring& ws);//宽字符 → 多字节：按系统 ANSI 代码页(中文系统是 GBK)，给 CF_TEXT 剪贴板/老式 ANSI 接口用

	std::wstring s2ws(const std::string& s);//多字节(系统 ANSI/GBK) → 宽字符，与 ws2s 互为反向

	std::string UnicodeToUtf8(const std::string& str);//GBK(系统 ANSI) 字节 → UTF-8：剪贴板/老接口进来的文本用它转成内部统一的 UTF-8

	std::string Utf8ToUnicode(const std::string& utf8_str);//UTF-8 → GBK(系统 ANSI) 字节：给只认 ANSI 的接口用（例如 CF_TEXT）

	std::string ClipboardTochar(); //获得剪贴板的内容

	void CopyToClipboard(std::string str); //将内容复制到剪贴板

	//把 src 安全地拷进定长缓冲区：最多写 destBytes-1 字节，并且一定会补 '\0'。
	//项目里原本到处是 memcpy(dest, str.c_str(), str.size()) 这种写法，没有任何长度上限，
	//剪贴板、翻译结果、配置文件里的长文本一进来就会把栈/堆上的定长数组写爆。
	inline void CopyToBuffer(char* dest, size_t destBytes, const std::string& src) {
		if (dest == nullptr || destBytes == 0) { return; }
		const size_t CopyBytes = (src.size() < destBytes - 1) ? src.size() : (destBytes - 1);
		if (CopyBytes > 0) { memcpy(dest, src.data(), CopyBytes); }
		dest[CopyBytes] = '\0';
	}

	char* screen(char* buf);//截图&保存（全屏）。返回的缓冲区由本函数自己持有，调用方不要 delete[]
	


/************************************************- FPS -************************************************/

	extern clock_t kaishi_time, jieshu_time;//储存时间戳
	extern int number_time;//当前第几帧
	extern const int number;//多少帧刷新一次
	extern const double miao_time;//用来计算FPS的数
	extern double FPStime;//帧数

	const int values_number = 60;//记录多少FPS数据
	extern float values[values_number];//储存FPS数据
	extern float Max_values; //FPS数据 最大值
	extern float Min_values;//FPS数据 最小值
	extern double Mean_values;//平均帧数

	extern void FPS();





/*********************************************- 耗时检测 -*********************************************/
	
/*
功能暂时不太完善，但是可以使用，按照规定使用就可以了，

1. 他们三，RefreshTiming()，EndTiming()，StartTiming()，必须在一次循环都得运行一边
2. 一个  StartTiming()  就必须有一个  StartEnd()  。
3. RefreshTiming() 只可以有一个。
4. StartTiming()， StartEnd() 嵌套最多10层。
5. StartTiming()，设置的名字必须是唯一的，不可以重复。

6. MomentTiming(), MomentEnd()这是检测间接运行的代码片段的时耗
7. MomentTiming() 名字可以不唯一，需要一个 int 来储存索引。

例子：
#include "Timing.h"//引用头文件就可以了

int main(){
	while(1){
	TOOL::StartTiming("唯一的名字", true);
	-- 被检测耗时的区域 --
	TOOL::StartEnd();

	RefreshTiming();
	}

	return 0;
}
*/
	
	extern clock_t TemporaryCycleTime;//周期总耗时开始时间戳
	extern clock_t CycleTime;//周期总耗时
	extern int Gap;//间隔
	extern int CurrentCount;//现在是第次轮回
	extern bool DetectionSwitch;//更新开关
	extern bool DetectionQuantityName;//第一次检测开关
	extern int Quantity;//总检测数量
	extern int DetectionCount;//现在是检测第几个

	//嵌套堆载
	extern clock_t TemporaryConsumetime[10];//临时时间堆载
	extern clock_t TemporaryConsumeName[10];//临时嵌套索引堆载
	extern int TemporaryTimeQuantity; //堆载指针

	//结果数据
	const int DetectionNumber = 1000;//最多检测数量
	extern int ConsumeNumber;//最多检测数量
	extern char* Consume_name[DetectionNumber];//储存检测的名字
	extern clock_t TemporaryConsume_time[DetectionNumber];//储存检测的周期总累加耗时
	extern double Consume_time[DetectionNumber];//储存检测的百分比
	extern double Consume_Second[DetectionNumber];//储存检测的秒

	//时间记录
	const int SecondVectorNumber = 60;//最多记录检测数量
	extern bool SecondVectorBool[DetectionNumber]; //秒数据 最大值
	extern float* Consume_SecondVector[DetectionNumber];//储存检测的秒数组
	extern int SecondVectorIndex[DetectionNumber];//秒数组索引
	extern float Max_Secondvalues[SecondVectorNumber]; //秒数据 最大值
	extern float Min_Secondvalues[SecondVectorNumber];//秒数据 最小值

	//                       唯一名字       是否记录时间数据
	extern void StartTiming(char* name, bool RecordBool = false);
	extern void StartEnd();
	//                       唯一名字     储存独立索引
	extern void MomentTiming(char* name, int* Index);
	extern void MomentEnd();
	extern void RefreshTiming();
}