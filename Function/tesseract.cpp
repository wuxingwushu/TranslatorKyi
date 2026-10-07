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
    //尺寸必须用"上一次截图"的那一对：data 就是 TOOL::screen 交出来的缓冲区，
    //如果按 windows_* 算，截图之后用户换了分辨率/显示器，这里就会越界读那块堆内存。
    const int DataW = Variable::ScreenShot_Width;
    const int DataH = Variable::ScreenShot_Heigth;
    if (data == nullptr || DataW <= 0 || DataH <= 0) { return std::string(); }
    Pix* image = pixCreate(DataW, DataH, 32);
    if (image == nullptr) { return std::string(); }
    memcpy((char*)pixGetData(image), data, (size_t)DataH * (size_t)DataW * 4);
    //选区是"当前屏幕上"的鼠标坐标，而截图可能是更早那一刻取的（中途换过分辨率/多屏），
    //先按图像边界把矩形夹一下，别把越界的矩形丢给 tesseract（会得到一张空图）。
    l_int32 CropX = x, CropY = y, CropW = w, CropH = h;
    if (CropX < 0) { CropW += CropX; CropX = 0; }
    if (CropY < 0) { CropH += CropY; CropY = 0; }
    if (CropX > DataW) { CropX = DataW; }
    if (CropY > DataH) { CropY = DataH; }
    if (CropX + CropW > DataW) { CropW = DataW - CropX; }
    if (CropY + CropH > DataH) { CropH = DataH - CropY; }
    if (CropW <= 0 || CropH <= 0) { pixDestroy(&image); return std::string(); }
    BOX* region = boxCreate(CropX, CropY, CropW, CropH);
    PIX* imgCrop = pixClipRectangle(image, region, NULL);
    if (imgCrop == nullptr)//裁剪失败就别把空图塞给 tesseract
    {
        boxDestroy(&region);
        pixDestroy(&image);
        return std::string();
    }
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
