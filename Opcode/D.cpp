#include <iostream>
#include <string>
#define DLLEXPORT extern "C" __declspec(dllexport)
//由于C++具有函数重载的特性 C语言的方式进行导出 避免命名冲突

DLLEXPORT void print(__int64 P)
{
    std::cout << "[Error]: " << std::endl;
    std::cout << (void*)P << std::endl;
    std::cout << (*((std::string*)P)).c_str() << std::endl;
    return;
}


DLLEXPORT void ErrorLog()
{
    std::cout << "[Error]: " << std::endl;
    return;
}
