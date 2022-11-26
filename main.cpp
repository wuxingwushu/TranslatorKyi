#include <tesseract/baseapi.h>
#include <leptonica/allheaders.h>
#include "FilePath.h"
#include <iostream>

#include "qcoreapplication.h"
#include "qapplication.h"
#include "qmainwindow.h"

int main(int argc, char* argv[])
{

    char* outText;

    tesseract::TessBaseAPI* api = new tesseract::TessBaseAPI();
    // Initialize tesseract-ocr with English, without specifying tessdata path
    if (api->Init(tessdata, "eng")) {
        fprintf(stderr, "Could not initialize tesseract.\n");
        exit(1);
    }

    // Open input image with leptonica library
    Pix* image = pixRead(TP);
    api->SetImage(image);
    // Get OCR result
    outText = api->GetUTF8Text();
    printf("OCR output:\n%s", outText);

    // Destroy used object and release memory
    api->End();
    delete api;
    delete[]outText;
    pixDestroy(&image);




    QApplication a(argc, argv);
    std::cout << qVersion() << std::endl;
    QMainWindow win;
    win.show();

    a.exec();

    
    return 0;
}