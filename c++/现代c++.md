# 现代c++（c++11/14/17/20）

[toc]

## 第 1 章 迈向现代 C++

### 1.1 被弃用的特性


1. 不再允许字符串字面值常量赋值给一个 char *。如果需要用字符串字面值常量赋值和初始化一个 char *，应该使用 const char * 或者 auto。 cpp char *str = "hello world!"; // 将出现弃用警告  
- 如果一个类有析构函数，为其生成拷贝构造函数和拷贝赋值运算符的特性被弃用了。 
- C 语言风格的类型转换被弃用(即在变量前使用 (convert_type)) ，应该使用 static_cast、 reinterpret_cast、const_cast 来进行类型转换。

todo：拷贝构造函数


## 第 2 章语言可用性的强化
### 2.1 常量

nullptr

C++11 引入了 nullptr 关键字，专门用来区分空指针、0。而 nullptr 的类型为 nullptr_t，能够隐式的转换为任何指针或成员指针的类型，也能和他们进行相等或者不等的比较。

constexpr

编译器能够在编译时就把这些表达式直接优化并植入到程序运行时
C++11 提供了 constexpr 让用户显式的声明函数或对象构造函数在编译期会成为常量表达式
从 C++14 开始，constexpr 函数可以在内部使用局部变量、循环和分支等简单语句


### 2.2 变量及其初始化

if/switch

C++17 可以在 if 和 switch 语句中声明一个临时的变量，通过分号(;)与后续的语句分割。


初始化列表

C++11 首先把初始化列表的概念绑定到了类型上，并将其称之为 std:: initializer_list，允许构造函数或其他函数像参数一样使用初始化列表

函数参数可以通过std::initializer_list类型的参数可以接收。类似数组初始值的参数(例如，{1,2,3})。但是std::initializer_list只能用于函数参数

```c++
#include <initializer_list>
#include <vector>
class MagicFoo {
public:
    std::vector<int> vec;
    MagicFoo(std::initializer_list<int> list) {
        for (std::initializer_list<int>::iterator it = list.begin(); 
             it != list.end(); ++it)
            vec.push_back(*it);
    }
};
int main() {
    // after C++11
    MagicFoo magicFoo = {1, 2, 3, 4, 5};

    std::cout << "magicFoo: ";
    for (std::vector<int>::iterator it = magicFoo.vec.begin(); it != magicFoo.vec.end(); ++it) std::cout << *it << std::endl;
}
```


结构化绑定

结构化绑定提供了类似其他语言中提供的多返回值的功能，C++11 新 增了 std::tuple 容器用于构造一个元组，进而囊括多个返回值

```c++
#include <iostream>
#include <tuple>

std::tuple<int, double, std::string> f() {
    return std::make_tuple(1, 2.3, "456");
}

int main() {
    auto [x, y, z] = f();
    std::cout << x << ", " << y << ", " << z << std::endl;
    return 0;
}
```


### 2.3 类型推导

C++11 引入了 auto 和 decltype 这两个关键字实现了类型推导，让编译器来操心变量的类型。

auto 不能用于函数传参，因此下面的做法是无法通过编译的

```c++
#include <initializer_list>
#include <vector>
#include <iostream>

class MagicFoo {
public:
    std::vector<int> vec;
    MagicFoo(std::initializer_list<int> list) {
        // 从 C++11 起, 使用 auto 关键字进行类型推导
        for (auto it = list.begin(); it != list.end(); ++it) {
            vec.push_back(*it);
        }
    }
};
int main() {
    MagicFoo magicFoo = {1, 2, 3, 4, 5};
    std::cout << "magicFoo: ";
    for (auto it = magicFoo.vec.begin(); it != magicFoo.vec.end(); ++it) {
        std::cout << *it << ", ";
    }
    std::cout << std::endl;
    return 0;
}
```


decltype 关键字是为了解决 auto 关键字只能对变量进行类型推导的缺陷而出现的

```c++
if (std::is_same<decltype(x), int>::value)
    std::cout << "type x == int" << std::endl;
if (std::is_same<decltype(x), float>::value)
    std::cout << "type x == float" << std::endl;
if (std::is_same<decltype(x), decltype(z)>::value)
    std::cout << "type z == type x" << std::endl;
```





### 2.4 控制流

if constexpr

C++17 将 constexpr 这个关键字引入到 if 语句中，允许在代码中声明常量 表达式的判断条件，让代码在编译时就完成分支判断

```c++
#include <iostream>

template<typename T>
auto print_type_info(const T& t) {
    if constexpr (std::is_integral<T>::value) {
        return t + 1;
    } else {
        return t + 0.001;
    }
}
int main() {
    std::cout << print_type_info(5) << std::endl;
    std::cout << print_type_info(3.14) << std::endl;
}

// 在编译时，实际代码就会表现为如下:

int print_type_info(const int& t) {
    return t + 1;
}
double print_type_info(const double& t) {
    return t + 0.001;
}
int main() {
    std::cout << print_type_info(5) << std::endl;
    std::cout << print_type_info(3.14) << std::endl;
}
```

区间(范围) for 迭代

C++11 引入了基于范围的迭代写法

```c++
#include <iostream>
#include <vector>
#include <algorithm>

int main() {
    std::vector<int> vec = {1, 2, 3, 4};
    if (auto itr = std::find(vec.begin(), vec.end(), 3); itr != vec.end()) 
    	*itr = 4;

    for (auto element : vec)
        std::cout << element << std::endl; // read only

    for (auto &element : vec) {
        element += 1;                      // writeable
    }

    for (auto element : vec)
        std::cout << element << std::endl; // read only
}
```


### 2.5 模板

外部模板

传统 C++ 中，模板只有在使用时才会被编译器实例化，换句话说，只要在每个编译单元(文件)中 编译的代码中遇到了被完整定义的模板，都会实例化

C++11 引入了外部模板，扩充了原来的强制编译器在特定位置实例化模板的语法


```c++
template class std::vector<bool>;          // 强行实例化
extern template class std::vector<double>; // 不在该当前编译文件中实例化模板
```

尖括号 “>”

在传统 C++ 的编译器中，>>一律被当做右移运算符来进行处理

而 C++11 开始，连续的右尖括号将变得合法

```c++
template<bool T>
class MagicType {
    bool magic = T;
};

// in main function:
std::vector<MagicType<(1>2)>> magic; // 合法, 但不建议写出这样的代码
```


类型别名模板

传统c++没有办法为模板定义一个 新的名称

C++11 使用 using 引入了下面这种形式的写法，并且同时支持对传统 typedef 相同的功效

```c++
template<typename T, typename U>
class MagicType {
public:
    T dark;
    U magic;
};

// 不合法
template<typename T>
typedef MagicType<std::vector<T>, std::string> FakeDarkMagic;

typedef int (*process)(void *);
using NewProcess = int(*)(void *);
template<typename T>
using TrueDarkMagic = MagicType<std::vector<T>, std::string>;

int main() {
    TrueDarkMagic<bool> you;
}

```


默认模板参数

```c++
template<typename T = int, typename U = int>
auto add(T x, U y) {
    return x+y;
}

int main() {
    std::cout << add(1.1,2.2) << std::endl;
    return 0;
}
```


变长参数模板

C++11 加入了新的表示方 法，允许任意个数、任意类别的模板参数，同时也不需要在定义时将参数的个数固定。

```c++
template<typename... Ts> class Magic;

class Magic<int,
            std::vector<int>,
            std::map<std::string,
            std::vector<int>>> darkMagic;


// 可以使用 sizeof... 来计算参数的个数
template<typename... Ts>
void magic(Ts... args) {
    std::cout << sizeof...(args) << std::endl;
}

```

折叠表达式

C++ 17 中将变长参数这种特性进一步带给了表达式

```c++
#include <iostream>
template<typename ... T>
auto sum(T ... t) {
    return (t + ...);
}
int main() {
    std::cout << sum(1, 2, 3, 4, 5, 6, 7, 8, 9, 10) << std::endl;
}
```

非类型模板参数

一种常见模板参数形式可以让不同字面量成为模板参数


```c++
template <typename T, int BufSize>
class buffer_t {
public:
    T& alloc();
    void free(T& item);
private:
    T data[BufSize];
}

buffer_t<int, 100> buf; // 100 作为模板参数
```


模版参数推导

C++17 引入了这一特性，我们的确可以 auto 关键字，让编译器辅助完成具体类型的推导，

```c++
template <auto value> void foo() {
    std::cout << value << std::endl;
    return;
}

int main() {
    foo<10>();  // value 被推导为 int 类型
}
```


### 2.6 面向对象

委托构造

C++11 引入了委托构造的概念，这使得构造函数可以在同一个类中一个构造函数调用另一个构造函 数，从而达到简化代码的目的

继承构造

C++11 利 用关键字 using 引入了继承构造函数的概念:

```c++
#include <iostream>
class Base {
public:
    int value1;
    int value2;
    Base() {
        value1 = 1;
    }
    Base(int value) : Base() { // 委托 Base() 构造函数
        value2 = value;
    }
};
class Subclass : public Base {
public:
    using Base::Base; // 继承构造
};
int main() {
    Subclass s(3);
    std::cout << s.value1 << std::endl;
    std::cout << s.value2 << std::endl;
}

```


显式虚函数重载

在传统 C++ 中，经常容易发生意外重载虚函数的事情

C++11 引入了 override 和 final 这两个关键字来防止上述情形的发生。

override 当重载虚函数时，引入 override 关键字将显式的告知编译器进行重载，编译器将检查基函数
是否存在这样的虚函数，否则将无法通过编译

```c++
struct Base {
    virtual void foo(int);
};
struct SubClass: Base {
    virtual void foo(int) override; // 合法
    virtual void foo(float) override; // 非法, 父类没有此虚函数
};
```


final 则是为了防止类被继续继承以及终止虚函数继续重载引入的。

修饰类时：表示类不可以被继承

修饰函数时：表示函数不可以被重载

```c++
struct Base {
    virtual void foo() final;
};
struct SubClass1 final: Base {
}; // 合法

struct SubClass2 : SubClass1 {
}; // 非法, SubClass1 已 final

struct SubClass3: Base {
    void foo(); // 非法, foo 已 final
};
```


显式禁用默认函数

[todo]


强类型枚举

C++11 引入了枚举类(enumeration class)，并使用 enum class 的语法进行声明

他不能够被隐式的转换为整数，同时也不能够将其与整数数 字进行比较，更不可能对不同的枚举类型的枚举值进行比较。

```c++
enum class new_enum : unsigned int { 
	value1 ,
	value2 , 
	value3 = 100, 
	value4 = 100
};
```


## 第 3 章语言运行期的强化


### 3.1 Lambda 表达式

Lambda 表达式的基本语法如下：

```c++
[捕获列表](参数列表) mutable(可选) 异常属性 -> 返回类型 {
// 函数体
}
```

[todo]mutable(可选) 异常属性

1. 值捕获 与参数传值类似，值捕获的前提是变量可以拷贝，不同之处则在于，被捕获的变量在 lambda 表达式被创建时拷贝，而非调用时才拷贝
2. 引用捕获 与引用传参类似，引用捕获保存的是引用，值会发生变化。
3. 隐式捕获
	- [] 空捕获列表
	- [name1, name2, . . . ] 捕获一系列变量
	- [&] 引用捕获, 让编译器自行推导捕获列表 • [=] 值捕获, 让编译器执行推导引用列表
4. 表达式捕获


```c++
void lambda_value_capture() {
    int value = 1;
    auto copy_value = [value] {
        return value;
    };
    value = 100;
    auto stored_value = copy_value();
    std::cout << "stored_value = " << stored_value << std::endl;
    // 这时, stored_value == 1, 而 value == 100.
    // 因为 copy_value 在创建时就保存了一份 value 的拷贝
}


void lambda_reference_capture() {
    int value = 1;
    auto copy_value = [&value] {
        return value;
    };
    value = 100;
    auto stored_value = copy_value();
    std::cout << "stored_value = " << stored_value << std::endl;
    // 这时, stored_value == 100, value == 100.
    // 因为 copy_value 保存的是引用
}

```



### 3.2 函数对象包装器

std::function

Lambda 表达式的本质是一个和函数对象类型相似的类类型(称为闭包类型)的对象(称为闭包对 象)，当 Lambda 表达式的捕获列表为空时，闭包对象还能够转换为函数指针值进行传递

C++11 std::function 是一种通用、多态的函数封装，它的实例可以对任何可以调用的目标实体进 行存储、复制和调用操作，

```c++
#include <functional>
#include <iostream>

int foo(int para) {
    return para;
}

int main() {
    // std::function 包装了一个返回值为 int, 参数为 int 的函数
    std::function<int(int)> func = foo;

    int important = 10;
    std::function<int(int)> func2 = [&](int value) -> int {
        return 1+value+important;
    };
    std::cout << func(10) << std::endl;
    std::cout << func2(10) << std::endl;
}
```

### 3.3 右值引用

右值引用和左值引用

需要拿到一个将亡值，就需要用到右值引用的申明:T &&，其中 T 是类型。右值引用的声明让这个 临时值的生命周期得以延长、只要变量还活着，那么将亡值将继续存活。


C++11 提供了 std::move 这个方法将左值参数无条件的转换为右值，有了它我们就能够方便的获得 一个右值临时对象


```c++
#include <iostream>
#include <string>

void reference(std::string& str) {
    std::cout << "左值" << std::endl;
}
void reference(std::string&& str) {
    std::cout << "右值" << std::endl;
}

int main()
{
    std::string lv1 = "string,"; // lv1 是一个左值
    // std::string&& r1 = lv1; // 非法, 右值引用不能引用左值
    std::string&& rv1 = std::move(lv1); // 合法, std::move可以将左值转移为右值
    std::cout << rv1 << std::endl; // string,

    const std::string& lv2 = lv1 + lv1; // 合法, 常量左值引用能够延长临时变量的生命周期
    // lv2 += "Test"; // 非法, 常量引用无法被修改
    std::cout << lv2 << std::endl; // string,string

    std::string&& rv2 = lv1 + lv2; // 合法, 右值引用延长临时对象生命周期
    rv2 += "Test"; // 合法, 非常量引用能够修改临时变量
    std::cout << rv2 << std::endl; // string,string,string,Test

    reference(rv2); // 输出左值

    return 0;
}
```


[todo]


移动语义

传统的 C++ 没有区分『移动』和『拷贝』的概念，造成了大量的数据拷贝，浪费时间和空间。右值 引用的出现恰好就解决了这两个概念的混淆问题，

右值类型的复制函数可以实现，对象的移动

```c++
#include <iostream>
class A {
public:
    int *pointer;
    A():pointer(new int(1)) { 
        std::cout << "构造" << pointer << std::endl; 
    }
    A(A& a):pointer(new int(*a.pointer)) { 
        std::cout << "拷贝" << pointer << std::endl; 
    } // 无意义的对象拷贝
    A(A&& a):pointer(a.pointer) { 
        a.pointer = nullptr;
        std::cout << "移动" << pointer << std::endl; 
    }
    ~A(){ 
        std::cout << "析构" << pointer << std::endl; 
        delete pointer; 
    }
};
// 防止编译器优化
A return_rvalue(bool test) {
    A a,b;
    if(test) return a; // 等价于 static_cast<A&&>(a);
    else return b;     // 等价于 static_cast<A&&>(b);
}
int main() {
    A obj = return_rvalue(false);
    std::cout << "obj:" << std::endl;
    std::cout << obj.pointer << std::endl;
    std::cout << *obj.pointer << std::endl;
    return 0;
}
```



完美转发

[todo]



## 第 4 章容器


### 4.1 线性容器 

std::array

声明： std::array<type, size>

- std::array 对象的大小是固定的，如果容器大小是固定 的，那么可以优先考虑使用 std::array 容器
- 使用 std::array 能够让代码变得更加 ‘‘现代化’’，而且封装了一些操作函 数，比如获取数组大小以及检查是否非空，同时还能够友好的使用标准库中的容器算法，比如 std::sort。

todo: 另外由于 std::vector 是自动扩容的，当存入大量的数据 后，并且对容器进行了删除操作，容器并不会自动归还被删除元素相应的内存，这时候就需要手动运行 shrink_to_fit() 释放这部分内存。


```c++
std::array<int, 4> arr = {1, 2, 3, 4};

arr.empty(); // 检查容器是否为空
arr.size();  // 返回容纳的元素数

// 迭代器支持
for (auto &i : arr)
{
    // ...
}

// 用 lambda 表达式排序
std::sort(arr.begin(), arr.end(), [](int a, int b) {
    return b < a;
});

// 数组大小参数必须是常量表达式
constexpr int len = 4;
std::array<int, len> arr = {1, 2, 3, 4};

// 非法,不同于 C 风格数组，std::array 不会自动退化成 T*
// int *arr_p = arr;

void foo(int *p, int len) {
    return;
}

std::array<int, 4> arr = {1,2,3,4};

// C 风格接口传参
// foo(arr, arr.size()); // 非法, 无法隐式转换
foo(&arr[0], arr.size());
foo(arr.data(), arr.size());

// 使用 `std::sort`
std::sort(arr.begin(), arr.end());

```


std::forward_list

std::forward_list 使用单向链表进行实现，当不需要双向迭代时，具有比 std::list 更高的空间利用率。



### 4.2 无序容器

传统 C++ 中的有序容器 std::map/std::set，这些元素内部通过红黑树进行实现， 插入和搜索的平均复杂度均为 O(log(size))。


C++11 引入了两组无序容器:std::unordered_map/std::unordered_multimap 和 std::unordered_set /std::unordered_multiset。



todo: multimap


```c++
#include <iostream>
#include <string>
#include <unordered_map>
#include <map>

int main() {
    // 两组结构按同样的顺序初始化
    std::unordered_map<int, std::string> u = {
        {1, "1"},
        {3, "3"},
        {2, "2"}
    };
    std::map<int, std::string> v = {
        {1, "1"},
        {3, "3"},
        {2, "2"}
    };

    // 分别对两组结构进行遍历
    std::cout << "std::unordered_map" << std::endl;
    for( const auto & n : u)
        std::cout << "Key:[" << n.first << "] Value:[" << n.second << "]\n";

    std::cout << std::endl;
    std::cout << "std::map" << std::endl;
    for( const auto & n : v)
        std::cout << "Key:[" << n.first << "] Value:[" << n.second << "]\n";
}

output:
std::unordered_map
Key:[2] Value:[2]
Key:[3] Value:[3]
Key:[1] Value:[1]

std::map
Key:[1] Value:[1]
Key:[2] Value:[2]
Key:[3] Value:[3]
```


### 4.3 元组

声明：std::tuple<typename... Ts>
例如：std::tuple<double, char, std::string>

1. std::make_tuple: 构造元组
2. std::get: 获得元组某个位置的值 
3. std::tie: 元组拆包

```c++
#include <tuple>
#include <iostream>

auto get_student(int id)
{
// 返回类型被推断为 std::tuple<double, char, std::string>

if (id == 0)
    return std::make_tuple(3.8, 'A', "张三");
if (id == 1)
    return std::make_tuple(2.9, 'C', "李四");
if (id == 2)
    return std::make_tuple(1.7, 'D', "王五");
    return std::make_tuple(0.0, 'D', "null");
    // 如果只写 0 会出现推断错误, 编译失败
}

int main()
{
    auto student = get_student(0);
    std::cout << "ID: 0, "
    << "GPA: " << std::get<0>(student) << ", "
    << "成绩: " << std::get<1>(student) << ", "
    << "姓名: " << std::get<2>(student) << '\n';

    double gpa;
    char grade;
    std::string name;

    // 元组进行拆包
    std::tie(gpa, grade, name) = get_student(1);
    std::cout << "ID: 1, "
    << "GPA: " << gpa << ", "
    << "成绩: " << grade << ", "
    << "姓名: " << name << '\n';
}

std::tuple<std::string, double, double, int> t("123", 4.5, 6.7, 8);
std::cout << std::get<std::string>(t) << std::endl;
std::cout << std::get<double>(t) << std::endl; // 非法, 引发编译期错误
std::cout << std::get<3>(t) << std::endl;

```


## 第 5 章智能指针与内存管理



而 C++11 引入了智能指针的概念，使用了引用计数的想法，让程序 员不再需要关心手动释放内存。这些智能指针就包括 std::shared_ptr/std::unique_ptr/std::weak_ptr， 使用它们需要包含头文件 <memory>。



### 5.2 std::shared_ptr

std::shared_ptr 是一种智能指针，它能够记录多少个 shared_ptr 共同指向一个对象

std::make_shared 就能够用来消除显式的使用 new，所以std::make_shared 会分配创建传入参数中的
对象，并返回这个对象类型的std::shared_ptr指针


std::shared_ptr 可以通过 get() 方法来获取原始指针，通过 reset() 来减少一个引用计数，并通 过use_count()来查看一个对象的引用计数




### 5.3 std::unique_ptr

std::unique_ptr 是一种独占的智能指针，它禁止其他智能指针与其共享同一个对象

### 5.4 std::weak_ptr



## 第 6 章正则表达式

todo:



## 第 7 章并行与并发

### 7.1 并行基础

std::thread

std::thread 用于创建一个执行的线程实例

```c++
#include <iostream>
#include <thread>

int main() {
    std::thread t([](){
        std::cout << "hello world." << std::endl;
    });
    t.join();
    return 0;
}
```

std::mutex

std::mutex 是 C++11 中最基本的 mutex 类，通过实例化 std::mutex 可以创建互斥量，而通过其成
员函数 lock() 可以进行上锁，unlock() 可以进行解锁，还为互斥量提供了一个 RAII 语法的模板类 std::lock_guard

```c++
#include <iostream>
#include <thread>

int v = 1;

void critical_section(int change_v) {
    static std::mutex mtx;
    std::lock_guard<std::mutex> lock(mtx);

    // 执行竞争操作
    v = change_v;

    // 离开此作用域后 mtx 会被释放
}

int main() {
    std::thread t1(critical_section, 2), t2(critical_section, 3);
    t1.join();
    t2.join();

    std::cout << v << std::endl;
    return 0;
}
```


std::unique_lock

std::lock_guard 不能显式的调用 lock 和 unlock，而 std::unique_lock 可以在声明后的任意位置调 用，可以缩小锁的作用范围，提供更高的并发度。
```c++
#include <iostream>
#include <thread>

int v = 1;

void critical_section(int change_v) {
    static std::mutex mtx;
    std::unique_lock<std::mutex> lock(mtx);
    // 执行竞争操作
    v = change_v;
    std::cout << v << std::endl;
    // 将锁进行释放
    lock.unlock();

    // 在此期间，任何人都可以抢夺 v 的持有权

    // 开始另一组竞争操作，再次加锁
    lock.lock();
    v += 1;
    std::cout << v << std::endl;
}

int main() {
    std::thread t1(critical_section, 2), t2(critical_section, 3);
    t1.join();
    t2.join();
    return 0;
}
```

7.3 期物
todo:



7.4 条件变量

std::condition_variable
condition_variable 实例被创建出现主要就是用于唤醒等待 线程从而避免死锁，std::condition_variable的 notify_one() 用于唤醒一个线程;notify_all() 则是通 知所有线程。

```c++
#include <queue>
#include <chrono>
#include <mutex>
#include <thread>
#include <iostream>
#include <condition_variable>


int main() {
    std::queue<int> produced_nums;
    std::mutex mtx;
    std::condition_variable cv;
    bool notified = false;  // 通知信号

    // 生产者
    auto producer = [&]() {
        for (int i = 0; ; i++) {
            std::this_thread::sleep_for(std::chrono::milliseconds(900));
            std::unique_lock<std::mutex> lock(mtx);
            std::cout << "producing " << i << std::endl;
            produced_nums.push(i);
            notified = true;
            cv.notify_all(); // 此处也可以使用 notify_one
        }
    };
    // 消费者
    auto consumer = [&]() {
        while (true) {
            std::unique_lock<std::mutex> lock(mtx);
            while (!notified) {  // 避免虚假唤醒
                cv.wait(lock);
            }
            // 短暂取消锁，使得生产者有机会在消费者消费空前继续生产
            lock.unlock();
            std::this_thread::sleep_for(std::chrono::milliseconds(1000)); // 消费者慢于生产者
            lock.lock();
            while (!produced_nums.empty()) {
                std::cout << "consuming " << produced_nums.front() << std::endl;
                produced_nums.pop();
            }
            notified = false;
        }
    };

    // 分别在不同的线程中运行
    std::thread p(producer);
    std::thread cs[2];
    for (int i = 0; i < 2; ++i) {
        cs[i] = std::thread(consumer);
    }
    p.join();
    for (int i = 0; i < 2; ++i) {
        cs[i].join();
    }
    return 0;
}
```



7.5 原子操作与内存模型

std::atomic<int> counter;

并非所有的类型都能提供原子操作，这是因为原子操作的可行性取决于 CPU 的架构以及 所实例化的类型结构是否满足该架构对内存对齐条件的要求

成员函数：fetch_add, fetch_sub 等， 同时通过重载方便的提供了对应的 +，- 版本

```c++
#include <atomic>
#include <thread>
#include <iostream>

std::atomic<int> count = {0};

int main() {
    std::thread t1([](){
        count.fetch_add(1);
    });
    std::thread t2([](){
        count++;        // 等价于 fetch_add
        count += 1;     // 等价于 fetch_add
    });
    t1.join();
    t2.join();
    std::cout << count << std::endl;
    return 0;
}
```

一致性模型	

todo:




## 第 9 章其他杂项

### 9.1 新类型

long long int

C++11 的工作则是正式把它纳入标准库，规定了一个 long long int 类 型至少具备 64 位的比特数。


### 9.2 noexcept 的修饰和操作


使用noexcept修饰的函数，有可能会异常，否则不可能
noexcept 还能够做操作符，用于操作一个表达式，当表达式无异常时，返回 true，否则返回 false。



C++11 将异常的声明简化为以下两种情况

1. 函数可能抛出任何异常
2. 函数不能抛出任何异常


```c++
void may_throw(); // 可 能 抛 出 异 常
void no_throw() noexcept; // 不 可 能 抛 出 异 常

#include <iostream>
void may_throw() {
    throw true;
}
auto non_block_throw = []{
    may_throw();
};
void no_throw() noexcept {
    return;
}

auto block_throw = []() noexcept {
    no_throw();
};
int main()
{
    std::cout << std::boolalpha
        << "may_throw() noexcept? " << noexcept(may_throw()) << std::endl
        << "no_throw() noexcept? " << noexcept(no_throw()) << std::endl
        << "lmay_throw() noexcept? " << noexcept(non_block_throw()) << std::endl
        << "lno_throw() noexcept? " << noexcept(block_throw()) << std::endl;
    return 0;
}


try {
    may_throw();
} catch (...) {
    std::cout << "捕获异常, 来自 my_throw()" << std::endl;
}
try {
    non_block_throw();
} catch (...) {
    std::cout << "捕获异常, 来自 non_block_throw()" << std::endl;
}
try {
    block_throw();
} catch (...) {
    std::cout << "捕获异常, 来自 block_throw()" << std::endl;
}


```


### 9.3 字面量

原始字符串字面量

禁止字符转意

C++11 提供了原始字符串字面量的写法，可以在一个字符串前方使用 R 来修饰这个字符串，同时， 将原始字符串使用括号包裹









### 虚函数
<https://coolshell.cn/articles/12165.html>

https://coolshell.cn/articles/12176.html