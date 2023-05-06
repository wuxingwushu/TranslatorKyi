#pragma once
#include "tesseract.h"


Tesseract::Tesseract(const char* Model)
{
    api = new tesseract::TessBaseAPI();
    if (api->Init("TessData", Model)) {//初始化， Model 是识别的模型
        fprintf(stderr, "Could not initialize tesseract.\n");
        exit(1);
    }
}

Tesseract::~Tesseract()
{
    api->End();
    delete api;
}

std::string Tesseract::IdentifyPictures(l_int32 x, l_int32 y, l_int32 w, l_int32 h, char* data) {
    Pix* image = pixCreate(Variable::windows_Width, Variable::windows_Heigth, 32);
    memcpy((char*)pixGetData(image), data, (Variable::windows_Heigth * Variable::windows_Width * 4));
    BOX* region = boxCreate(x, y, w, h);
    PIX* imgCrop = pixClipRectangle(image, region, NULL);
    api->SetImage(imgCrop);
    char* outText = api->GetUTF8Text();
    //intf("OCR output:\n%s", outText);
    Text = outText;
    boxDestroy(&region);
    pixDestroy(&imgCrop);
    pixDestroy(&image);
    delete[] outText;
    return Text;
}
