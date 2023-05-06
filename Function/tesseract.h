#pragma once
#include <tesseract/baseapi.h>
#include <leptonica/allheaders.h>
#include "../Tool/Tool.h"

class Tesseract
{
public:
	Tesseract(const char* Model);
	
	~Tesseract();

	std::string IdentifyPictures(l_int32 x, l_int32 y, l_int32 w, l_int32 h, char* data);

	std::string GetText() { return Text; }//获得识别内容

private:
	std::string Text;//识别的内容
	tesseract::TessBaseAPI* api; //Tesseract 引用
};

