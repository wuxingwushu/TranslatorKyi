#include "tesseract.h"

char* Tesseract_OCR(l_int32 x, l_int32 y, l_int32 w, l_int32 h)
{

    //printf("%d - %d - %d - %d", x, y, w, h);

    char* outText;
    tesseract::TessBaseAPI* api = new tesseract::TessBaseAPI();
    // Initialize tesseract-ocr with English, without specifying tessdata path
    if (api->Init("tessdata", "eng")) {
        fprintf(stderr, "Could not initialize tesseract.\n");
        exit(1);
    }

    // Open input image with leptonica library
    Pix* image = pixRead("1.png");
    BOX* region = boxCreate(x, y, w, h);
    PIX* imgCrop = pixClipRectangle(image, region, NULL);
    boxDestroy(&region);
    api->SetImage(imgCrop);
    // Get OCR result
    outText = api->GetUTF8Text();
    printf("OCR output:\n%s", outText);

    // Destroy used object and release memory
    api->End();
    delete api;
    pixDestroy(&image);
    pixDestroy(&imgCrop);

    return outText;
}
