#pragma once
#include <tesseract/baseapi.h>
#include <leptonica/allheaders.h>
#include "../Tool/Tool.h"
#include <atomic>
#include <string>
#include <thread>

class Tesseract
{
public:
	Tesseract(const char* Model);
	
	~Tesseract();

	std::string IdentifyPictures(l_int32 x, l_int32 y, l_int32 w, l_int32 h, char* data);

	//截图翻译用：识别放到后台线程，界面不用等 OCR 做完（结果由 OcrTakeResult 每帧取）
	bool OcrBegin(l_int32 x, l_int32 y, l_int32 w, l_int32 h, char* data);
	bool OcrTakeResult(std::string& Result);//识别完成返回 true，否则 false
	bool OcrRunning() const { return mOcrRunning.load(); }
	void OcrWait();//脚本接口要同步拿文本时才用：不管跑没跑完都等它结束

	std::string GetText() { return Text; }//获得识别内容

private:
	std::string Text;//识别的内容
	tesseract::TessBaseAPI* api; //Tesseract 引用
	std::thread mOcrThread;
	std::atomic<bool> mOcrRunning{ false };
	std::atomic<bool> mOcrDone{ false };
	std::string mOcrResult;
};

