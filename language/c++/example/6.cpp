#include <iostream>
#include <memory>

using namespace std;

int main() {
    int *sp = new int(10);
    shared_ptr<int> sp1(sp);
    shared_ptr<int> sp2(sp);

    std::cout << "sp1=" << sp1.use_count() << std::endl;
    std::cout << "sp2=" << sp2.use_count() << std::endl;

    return 0;
}
