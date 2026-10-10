#pragma once
#include <string>
#include <map>
#include <iostream>//std::cout：下面 add()/pop() 的越界提示要用

template <typename T>
class PileUp
{
private:
    unsigned int Index = 0;
    unsigned int Max;
    T* mPileUp;
public:
    PileUp(unsigned int size) {
        Max = size;
        mPileUp = new T[size];
    };

    ~PileUp() {
        delete[] mPileUp;   //new T[size] 必须配 delete[]，用 delete 属未定义行为
    };

    void add(T Parameter) {
        if (Index >= Max)
        {
            std::cout << "add: GoBeyond" << std::endl;
            return;
        }
        mPileUp[Index] = Parameter;
        Index++;
    };

    T pop() {
        if (Index == 0)
        {
            std::cout << "pop: Empty" << std::endl;
            return T{};   //原来写 return 0：T 为 std::string 时会构造空指针，属未定义行为
        }
        Index--;
        return mPileUp[Index];
    }
};
struct StructVariable {
    std::string Type;
    void* Pointer;
};

struct Stu {
    PileUp<bool>* boolS;
    PileUp<char>* charS;
    PileUp<int>* intS;
    PileUp<float>* floatS;
    PileUp<double>* doubleS;
    PileUp<std::string>* stringS;
};



template <typename U>
class MapVariable
{
public:
    MapVariable() {
        
    }
    ~MapVariable() {
        
    }

    void New(std::string name, U V) {
        mVariable.insert(std::make_pair(name, V));
    }

    void Set(std::string name, U V) {
        mVariable[name] = V;
    }

    U Get(std::string name) {
        return mVariable[name];
    }

    void Delete(std::string name) {
        mVariable.erase(name);
    }
        
private:
    std::map<std::string, U> mVariable;
};


