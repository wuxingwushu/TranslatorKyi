#include "Log.h"
#include <cstdio>//fopen/fread/fwrite：给日志文件补 UTF-8 BOM
#include <filesystem>//判断日志文件是否已存在、是否为空

#include "spdlog/cfg/env.h"  // support for loading levels from the environment variable
#include "spdlog/fmt/ostr.h" // support for user defined types
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/basic_file_sink.h"

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
}