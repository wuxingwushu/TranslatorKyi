#include <iostream>
#define DLLEXPORT extern "C" __declspec(dllexport)
//由于C++具有函数重载的特性 C语言的方式进行导出 避免命名冲突

template <typename T>
class PileUp
{
private:
    unsigned int Index = 0;
    unsigned int Max;
    T* mPileUp;
public:
    PileUp(unsigned int size){
        Max = size;
        mPileUp = new T[size];
    };

    ~PileUp(){
        delete mPileUp;
    };

    void add(T Parameter){
        if (Index >= Max)
        {
            std::cout << "超出" << std::endl;
            return;
        }
        mPileUp[Index] = Parameter;
        Index++;
    };

    T pop(){
        if (Index == 0)
        {
            std::cout << "空" << std::endl;
            return 0;
        }
        Index--;
        return mPileUp[Index];
    }
};

struct Stu {
    PileUp<bool>* boolS;
    PileUp<char>* charS;
    PileUp<int>* intS;
    PileUp<float>* floatS;
    PileUp<double>* doubleS;
    PileUp<std::string>* stringS;
};

DLLEXPORT void print(Stu* T)
{
    std::cout << T->stringS->pop() << std::endl;
}

DLLEXPORT void printADD(Stu* T)
{
    std::cout << T->stringS->pop() + " - " + T->stringS->pop() << std::endl;
}


DLLEXPORT void add(Stu* T)
{
    int shu1 = T->intS->pop();
    int shu2 = T->intS->pop();
    T->intS->add(shu1 + shu2);
}