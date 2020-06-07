#include <iostream>
#include "1.h"


#define F(x) ( x + 1 )


class Foo {
 public:
  typedef int bar_type;
};

template<typename T>
class Bar {
 /* typename */
 T::bar_type bar;
};

int Test();

int main() {
    Bar<Foo> b;

    int n = F(1);
    std::cout << n << std::endl;
    std::cout << Test() << std::endl;
    return 0;
}
