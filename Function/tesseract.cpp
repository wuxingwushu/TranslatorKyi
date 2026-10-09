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
    OcrWait();//后台识别还在用 api，析构前先等它结束
    api->End();
    delete api;
}

//截图翻译专用：把识别丢到后台线程，截图界面就不用等 OCR 做完才切换。
//data/x/y/w/h 在这里按值拷进线程；调用方要保证这次识别没结束前不会再截图，
//否则 TOOL::screen() 会改写那块缓冲区，后台线程就读到别的画面了。
bool Tesseract::OcrBegin(l_int32 x, l_int32 y, l_int32 w, l_int32 h, char* data)
{
    if (mOcrRunning.load()) { return false; }//上一次还没识别完，这次不等它
    OcrWait();                               //回收上一次已经结束的线程
    mOcrResult.clear();
    mOcrDone = false;
    mOcrRunning = true;
    mOcrThread = std::thread([this, x, y, w, h, data]() {
        std::string Result;
        try { Result = IdentifyPictures(x, y, w, h, data); }
        catch (...) { Result.clear(); }
        mOcrResult = Result;
        mOcrRunning = false;
        mOcrDone = true;
    });
    return true;
}

bool Tesseract::OcrTakeResult(std::string& Result)
{
    if (!mOcrDone.load()) { return false; }//还没识别完，调用方下一帧再来
    OcrWait();//等线程真的退出再读 mOcrResult
    Result = mOcrResult;
    mOcrResult.clear();
    mOcrDone = false;
    return true;
}

void Tesseract::OcrWait()
{
    if (mOcrThread.joinable()) { mOcrThread.join(); }
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
