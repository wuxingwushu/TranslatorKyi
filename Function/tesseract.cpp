#include "tesseract.h"

std::string Tesseract_OCR(l_int32 x, l_int32 y, l_int32 w, l_int32 h, const char* Model)
{
    //printf("%d - %d - %d - %d", x, y, w, h);

    char* outText;
    tesseract::TessBaseAPI* api = new tesseract::TessBaseAPI();
    // Initialize tesseract-ocr with English, without specifying tessdata path
    if (api->Init("tessdata", Model)) {
        fprintf(stderr, "Could not initialize tesseract.\n");
        exit(1);
    }

    // Open input image with leptonica library : ��������ͼ����leptonica��
    Pix* image = pixRead("TemporaryData");
    BOX* region = boxCreate(x, y, w, h);
    PIX* imgCrop = pixClipRectangle(image, region, NULL);
    boxDestroy(&region);
    api->SetImage(imgCrop);
    // Get OCR result
    outText = api->GetUTF8Text();
    std::string Text = outText;
    //printf("OCR output:\n%s", outText);

    // Destroy used object and release memory
    api->End();
    delete api;
    delete[] outText;
    pixDestroy(&image);
    pixDestroy(&imgCrop);

    return Text;
}
