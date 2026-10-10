#include "Log.h"
#include <cstdio>//fopen/fread/fwrite：给日志文件补 UTF-8 BOM
#include <cstdlib>//std::atexit / abort
#include <stdlib.h>//_set_invalid_parameter_handler（MSVC 只在 <stdlib.h> 里声明）
#include <exception>//std::set_terminate / std::current_exception
#include <string>
#include <filesystem>//判断日志文件是否已存在、是否为空
#include "Charset.h"//WideToUtf8：非法参数处理器里的宽字符文本转 UTF-8

#include "spdlog/cfg/env.h"  // support for loading levels from the environment variable
#include "spdlog/fmt/ostr.h" // support for user defined types
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/basic_file_sink.h"

namespace TOOL {

	spdlog::logger* logger;

	//日志路径只有这一份，崩溃兜底写盘时也要用同一个文件
	static const char* LogFilePath = "logs/Error.txt";

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

	//崩溃/退出时的兜底写盘：直接用 C 的 FILE*，不经过 spdlog。
	//原因：std::terminate 的现场可能还有别的线程正握着日志锁，走 spdlog 有死锁风险，
	//那样连最后一行线索都留不下来。
	static void AppendFatalLine(const char* Tag, const std::string& Text)
	{
		FILE* f = fopen(LogFilePath, "ab");
		if (f == nullptr) { return; }
		fprintf(f, "\n[%s] %s\n", Tag, Text.c_str());
		fclose(f);
	}

	//退出路径的「路标」：跟 AppendFatalLine 一样直接写盘、不经过 spdlog。
	//只在退出/清理这类只跑一遍的地方调用，开销可以忽略；
	//崩溃后看日志里最后一条 [STEP] 就知道崩在哪一步。
	void LogStep(const char* Tag)
	{
		FILE* f = fopen(LogFilePath, "ab");
		if (f == nullptr) { return; }
		fprintf(f, "[STEP] %s\n", Tag);
		fclose(f);
	}

	static std::terminate_handler gPreviousTerminate = nullptr;
	//std::terminate 的兜底处理器。
	//「出错模块 ucrtbase.dll、异常代码 0xc0000409」在 Windows 上就是 abort()/fast-fail 的表现，
	//本工程里能走到它的路径主要有三条：
	//  1) 某个线程里逃出了异常 → std::terminate（llama.cpp 的 ggml.cpp 在静态初始化时
	//     就把自己注册成了 terminate handler：打印调用栈后 abort；Release 是 GUI 子系统，
	//     它打印的 stderr 根本没人能看到，于是表现为「毫无征兆的静默崩溃」）；
	//  2) CRT 的非法参数处理器（默认 _invoke_watson，同样是 fast-fail）；
	//  3) 代码里直接调用 abort() 的地方（application.cpp 的 check_vk_result、GGML_ABORT 等）。
	//这里给 1)、2) 各挂一个处理器：先把原因写进日志，再交回原来的处理器（保留原有行为）。
	static void TkTerminateHandler()
	{
		std::string Why = "没有活动异常";
		try {
			if (std::current_exception() != nullptr) {
				std::rethrow_exception(std::current_exception());
			}
		}
		catch (const std::exception& e) { Why = e.what(); }
		catch (...) { Why = "非 std::exception 类型的异常"; }

		AppendFatalLine("FATAL", std::string("线程里有异常逃出来，std::terminate 被调用：") + Why);

		if (gPreviousTerminate != nullptr) { gPreviousTerminate(); }//链回原来的处理器（llama.cpp 会打印调用栈）
		abort();
	}

	//CRT 非法参数（_s 系列函数收到空指针/长度不合法等）默认走 _invoke_watson → fast-fail。
	//这里同样先落盘再终止，至少能知道是哪个表达式、哪个文件哪一行。
	static void TkInvalidParameterHandler(const wchar_t* Expression, const wchar_t* Function,
		const wchar_t* File, unsigned int Line, uintptr_t)
	{
		std::string Text = "CRT 非法参数：表达式=";
		Text += (Expression != nullptr) ? WideToUtf8(Expression) : "(null)";
		Text += " 函数=";
		Text += (Function != nullptr) ? WideToUtf8(Function) : "(null)";
		Text += " 位置=";
		Text += (File != nullptr) ? WideToUtf8(File) : "(null)";
		Text += ":" + std::to_string(Line);
		AppendFatalLine("FATAL", Text);
		abort();
	}

	//程序退出时刷日志。所有退出路径（ImGui 菜单「退出」、托盘菜单、自更新重启、ESC）都是 exit(0)，
	//atexit 注册的函数一定会被调用；spdlog 默认不自动 flush（logger.h 里 flush_level_ 是 off），
	//不刷的话最后几行还留在缓冲区里就随进程一起消失了——排查崩溃时最需要的恰恰是最后几行。
	static void TkLogShutdown()
	{
		if (logger == nullptr) { return; }
		try {
			logger->info("程序退出");
			logger->flush();
		}
		catch (...) {}
	}

	void SpdLogInit() {
		auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
		console_sink->set_level(spdlog::level::warn);//设置警报等级
		console_sink->set_pattern("[multi_sink_example] [%^%l%$] %v");//打印显示

		//日志文件先补上 UTF-8 BOM，再让 spdlog 接管（spdlog 只会往后追加，不影响最前面的 BOM）
		EnsureLogFileUtf8Bom(LogFilePath);

		auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(LogFilePath, false);//日志文件路径，是否覆写
		file_sink->set_level(spdlog::level::trace);//设置警报等级

		logger = new spdlog::logger("multi_sink", { console_sink, file_sink }); // 日志保存
		logger->set_level(spdlog::level::debug);//设置日志警报保存等级

		//错误级日志立刻落盘：Vulkan 初始化和崩溃前的线索都走 error 级，
		//默认的缓冲策略会让它们在强杀/崩溃时一起消失。
		logger->flush_on(spdlog::level::err);

		//崩溃兜底：链住现有的 terminate handler（llama.cpp 静态初始化时注册过一个），
		//再挂上 CRT 非法参数处理器；两者都只负责「把原因写进日志」。
		gPreviousTerminate = std::get_terminate();
		std::set_terminate(TkTerminateHandler);
		_set_invalid_parameter_handler(TkInvalidParameterHandler);

		//所有退出路径（包括各处的 exit(0)）都会走到 atexit：在这里刷一次日志，
		//日志里就有了「程序退出」这一行，也能把「正常退出」和「崩溃」区分开。
		std::atexit(TkLogShutdown);
	}
}