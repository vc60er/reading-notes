#include <iostream>
#include <memory>


using namespace std;

#pragma pack(4)
struct aa{
    char a;
    short b;
    int c;
    char d;
};
#pragma pop()

struct A{

int a; // +4
short b; // +4
int c; // +4
char d; // +4

};

struct B{

int a ; // +4
int c; // +4
short b; // +2
char d; // +2

};


# pragma pack(16)
struct C{
      char a;
      double b;

};
# pragma pop()



# pragma pack(16)
struct D{
      char a;
      short c;
      double b;
};
# pragma pop()

# pragma pack(16)
struct E{
      char a;
      double b;
      short c;
};
# pragma pop()

# pragma pack(4)
struct F {
      char a;
      double b;
      short c;
};
# pragma pop()

int main() {
   // int *sp = new int(10);
   // shared_ptr<int> sp1(sp);
   // shared_ptr<int> sp2(sp);
    std::cout << sizeof(aa) << std::endl;


    auto pa = (A*)0;
    printf("a=%d\nb=%d\nc=%d\nd=%d\n", &(pa->a), &(pa->b), &(pa->c), &(pa->d));
    std::cout << "A:" << sizeof(A) << std::endl; // 16

    auto pb = (B*)0;
    printf("a=%d\nc=%d\nb=%d\nd=%d\n", &(pb->a), &(pb->c), &(pb->b), &(pb->d));
    std::cout << "B:" << sizeof(B) << std::endl; // 12


    auto pc = (C*)0;
    printf("a=%d\nb=%d\n", &(pc->a), &(pc->b));
    std::cout << "C:" << sizeof(C) << std::endl;
    std::cout << "D:" << sizeof(D) << std::endl;
    std::cout << "E:" << sizeof(E) << std::endl;
    std::cout << "F:" << sizeof(F) << std::endl;

    return 0;
}



