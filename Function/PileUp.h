#include <string>
#include <map>

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
        delete mPileUp;
    };

    void add(T Parameter) {
        if (Index >= Max)
        {
            std::cout << "超出" << std::endl;
            return;
        }
        mPileUp[Index] = Parameter;
        Index++;
    };

    T pop() {
        if (Index == 0)
        {
            std::cout << "空" << std::endl;
            return 0;
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

    void* Set(std::string name, U V) {
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


