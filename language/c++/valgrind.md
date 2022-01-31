# valgrind


## Memcheck

### 越界访问，内存泄漏

```c
#include <stdlib.h>

void fun() {

    int* p = (int*)malloc(10*sizeof(int));

    p[10] = 0;


}


int main() {

    fun();

}
```

```shell

[root@centos-linux-7 valgrind]# gcc -g v.c &&  valgrind ./a.out
==27012== Memcheck, a memory error detector
==27012== Copyright (C) 2002-2017, and GNU GPL'd, by Julian Seward et al.
==27012== Using Valgrind-3.15.0 and LibVEX; rerun with -h for copyright info
==27012== Command: ./a.out
==27012==
==27012== Invalid write of size 4
==27012==    at 0x40054E: fun (v.c:7)
==27012==    by 0x400563: main (v.c:15)
==27012==  Address 0x51f9068 is 0 bytes after a block of size 40 alloc'd
==27012==    at 0x4C29F73: malloc (vg_replace_malloc.c:309)
==27012==    by 0x400541: fun (v.c:5)
==27012==    by 0x400563: main (v.c:15)
==27012==
==27012==
==27012== HEAP SUMMARY:
==27012==     in use at exit: 40 bytes in 1 blocks
==27012==   total heap usage: 1 allocs, 0 frees, 40 bytes allocated
==27012==
==27012== LEAK SUMMARY:
==27012==    definitely lost: 40 bytes in 1 blocks
==27012==    indirectly lost: 0 bytes in 0 blocks
==27012==      possibly lost: 0 bytes in 0 blocks
==27012==    still reachable: 0 bytes in 0 blocks
==27012==         suppressed: 0 bytes in 0 blocks
==27012== Rerun with --leak-check=full to see details of leaked memory
==27012==
==27012== For lists of detected and suppressed errors, rerun with: -s
==27012== ERROR SUMMARY: 1 errors from 1 contexts (suppressed: 0 from 0)
[root@centos-linux-7 valgrind]#

```


### 使用未初始化变量

```c
#include <stdlib.h>
#include <stdio.h>

void fun() {

    int* p = (int*)malloc(10*sizeof(int));

    p[10] = 0;


}


int main() {
	int a[5];
    int i, s;
    a[0] = a[1] = a[3] = a[4] = 0;
    s = 0;
    for (i = 0; i < 5; i++ ) {
        s += a[i];
    }

    if (s == 377) {
        printf("sum is %d", s);
    }



}
``

```shell

[root@centos-linux-7 valgrind]#
[root@centos-linux-7 valgrind]# gcc -g v.c &&  valgrind ./a.out
==28075== Memcheck, a memory error detector
==28075== Copyright (C) 2002-2017, and GNU GPL'd, by Julian Seward et al.
==28075== Using Valgrind-3.15.0 and LibVEX; rerun with -h for copyright info
==28075== Command: ./a.out
==28075==
==28075== Conditional jump or move depends on uninitialised value(s)
==28075==    at 0x4005F4: main (v.c:23)
==28075==
==28075==
==28075== HEAP SUMMARY:
==28075==     in use at exit: 0 bytes in 0 blocks
==28075==   total heap usage: 0 allocs, 0 frees, 0 bytes allocated
==28075==
==28075== All heap blocks were freed -- no leaks are possible
==28075==
==28075== Use --track-origins=yes to see where uninitialised values come from
==28075== For lists of detected and suppressed errors, rerun with: -s
==28075== ERROR SUMMARY: 1 errors from 1 contexts (suppressed: 0 from 0)
[root@centos-linux-7 valgrind]#
[root@centos-linux-7 valgrind]#

```



### 非法访问

```c
[root@centos-linux-7 valgrind]# cat v.c
#include <stdlib.h>
#include <stdio.h>


int main() {
    int len = 4;
    int* pt = (int*)malloc(len*sizeof(int));
    int* p = pt;

    for (int i = 0; i < len; i++) {
        p++;
    }

    *p = 5;

    printf("the value of p equal:%d", *p);


    return 0;
}

```
```shell
[root@centos-linux-7 valgrind]#
[root@centos-linux-7 valgrind]# g++ -g v.c &&  valgrind ./a.out
==30807== Memcheck, a memory error detector
==30807== Copyright (C) 2002-2017, and GNU GPL'd, by Julian Seward et al.
==30807== Using Valgrind-3.15.0 and LibVEX; rerun with -h for copyright info
==30807== Command: ./a.out
==30807==
==30807== Invalid write of size 4
==30807==    at 0x40068A: main (v.c:14)
==30807==  Address 0x5a18050 is 0 bytes after a block of size 16 alloc'd
==30807==    at 0x4C29F73: malloc (vg_replace_malloc.c:309)
==30807==    by 0x40065F: main (v.c:7)
==30807==
==30807== Invalid read of size 4
==30807==    at 0x400694: main (v.c:16)
==30807==  Address 0x5a18050 is 0 bytes after a block of size 16 alloc'd
==30807==    at 0x4C29F73: malloc (vg_replace_malloc.c:309)
==30807==    by 0x40065F: main (v.c:7)
==30807==
the value of p equal:5==30807==
==30807== HEAP SUMMARY:
==30807==     in use at exit: 16 bytes in 1 blocks
==30807==   total heap usage: 1 allocs, 0 frees, 16 bytes allocated
==30807==
==30807== LEAK SUMMARY:
==30807==    definitely lost: 16 bytes in 1 blocks
==30807==    indirectly lost: 0 bytes in 0 blocks
==30807==      possibly lost: 0 bytes in 0 blocks
==30807==    still reachable: 0 bytes in 0 blocks
==30807==         suppressed: 0 bytes in 0 blocks
==30807== Rerun with --leak-check=full to see details of leaked memory
==30807==
==30807== For lists of detected and suppressed errors, rerun with: -s
==30807== ERROR SUMMARY: 2 errors from 2 contexts (suppressed: 0 from 0)
[root@centos-linux-7 valgrind]#

```


### 内存覆盖

```c
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int main() {
    char x[50];
    int i;
    for (i = 0; i < 50; i++) {
        x[i]=i+1;
    }

    strncpy(x+20, x, 20); // ok
    strncpy(x+20, x, 21); // overlap
    strncpy(x, x+20, 20); // ok
    strncpy(x, x+20, 21); // overlap

    x[39]='\0';
    strcpy(x, x+20); // ok

    x[39]=39;
    x[40]='\0';
    strcpy(x, x+20); // overlap

    return 0;
}
```
```shell

[root@centos-linux-7 valgrind]# g++ -g v.c &&  valgrind ./a.out
==32674== Memcheck, a memory error detector
==32674== Copyright (C) 2002-2017, and GNU GPL'd, by Julian Seward et al.
==32674== Using Valgrind-3.15.0 and LibVEX; rerun with -h for copyright info
==32674== Command: ./a.out
==32674==
==32674== Source and destination overlap in strncpy(0x1fff0003e9, 0x1fff0003d5, 21)
==32674==    at 0x4C2D843: __strncpy_sse2_unaligned (vg_replace_strmem.c:555)
==32674==    by 0x4006A3: main (v.c:13)
==32674==
==32674== Source and destination overlap in strncpy(0x1fff0003d5, 0x1fff0003e9, 21)
==32674==    at 0x4C2D843: __strncpy_sse2_unaligned (vg_replace_strmem.c:555)
==32674==    by 0x4006DB: main (v.c:15)
==32674==
==32674== Source and destination overlap in strcpy(0x1fff0003c0, 0x1fff0003d4)
==32674==    at 0x4C2D282: strcpy (vg_replace_strmem.c:513)
==32674==    by 0x400715: main (v.c:22)
==32674==
==32674==
==32674== HEAP SUMMARY:
==32674==     in use at exit: 0 bytes in 0 blocks
==32674==   total heap usage: 0 allocs, 0 frees, 0 bytes allocated
==32674==
==32674== All heap blocks were freed -- no leaks are possible
==32674==
==32674== For lists of detected and suppressed errors, rerun with: -s
==32674== ERROR SUMMARY: 3 errors from 3 contexts (suppressed: 0 from 0)
[root@centos-linux-7 valgrind]#
[root@centos-linux-7 valgrind]#
[root@centos-linux-7 valgrind]#
[root@centos-linux-7 valgrind]#

```

### 动态内存管理错误

```cpp
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int main() {
    int i;
    char* p = (char*)malloc(10);
    char* pt = p;

    for (i = 0; i < 10; i++) {
        p[i]= 'z';
    }
    delete p;

    pt[1] = 'x';
    free(pt);

    return 0;
}
```

```shell
[root@centos-linux-7 valgrind]# g++ -g v.cpp &&  valgrind ./a.out
==1789== Memcheck, a memory error detector
==1789== Copyright (C) 2002-2017, and GNU GPL'd, by Julian Seward et al.
==1789== Using Valgrind-3.15.0 and LibVEX; rerun with -h for copyright info
==1789== Command: ./a.out
==1789==
==1789== Mismatched free() / delete / delete []
==1789==    at 0x4C2B51D: operator delete(void*) (vg_replace_malloc.c:586)
==1789==    by 0x4006FC: main (v.cpp:13)
==1789==  Address 0x5a18040 is 0 bytes inside a block of size 10 alloc'd
==1789==    at 0x4C29F73: malloc (vg_replace_malloc.c:309)
==1789==    by 0x4006C1: main (v.cpp:7)
==1789==
==1789== Invalid write of size 1
==1789==    at 0x400705: main (v.cpp:15)
==1789==  Address 0x5a18041 is 1 bytes inside a block of size 10 free'd
==1789==    at 0x4C2B51D: operator delete(void*) (vg_replace_malloc.c:586)
==1789==    by 0x4006FC: main (v.cpp:13)
==1789==  Block was alloc'd at
==1789==    at 0x4C29F73: malloc (vg_replace_malloc.c:309)
==1789==    by 0x4006C1: main (v.cpp:7)
==1789==
==1789== Invalid free() / delete / delete[] / realloc()
==1789==    at 0x4C2B06D: free (vg_replace_malloc.c:540)
==1789==    by 0x400713: main (v.cpp:16)
==1789==  Address 0x5a18040 is 0 bytes inside a block of size 10 free'd
==1789==    at 0x4C2B51D: operator delete(void*) (vg_replace_malloc.c:586)
==1789==    by 0x4006FC: main (v.cpp:13)
==1789==  Block was alloc'd at
==1789==    at 0x4C29F73: malloc (vg_replace_malloc.c:309)
==1789==    by 0x4006C1: main (v.cpp:7)
==1789==
==1789==
==1789== HEAP SUMMARY:
==1789==     in use at exit: 0 bytes in 0 blocks
==1789==   total heap usage: 1 allocs, 2 frees, 10 bytes allocated
==1789==
==1789== All heap blocks were freed -- no leaks are possible
==1789==
==1789== For lists of detected and suppressed errors, rerun with: -s
==1789== ERROR SUMMARY: 3 errors from 3 contexts (suppressed: 0 from 0)
[root@centos-linux-7 valgrind]#
```


### 内存泄露

```c++
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

typedef struct _node {
    struct _node *l;
    struct _node *r;
    char v;
} node;

node* mk(node *l, node* r, char val) {
    node* f = (node*)malloc(sizeof(*f));
    f->l = l;
    f->r = r;
    f->v = val;
    return f;
}


void nodefr(node* n) {
    if (n != NULL) {
        nodefr(n->l);
        nodefr(n->r);
        free(n);
    }
}



int main() {
   node* tree1, *tree2, *tree3;
   tree1 = mk(mk(mk(0,0,'3'), 0, '2'), 0, '1');
   tree2 = mk(0, mk(0,mk(0,0,'6'), '5'),'4');
   tree3 = mk(mk(tree1, tree2,'8'), 0, '7');

   return 0;
}
```

```shell
#
[root@centos-linux-7 valgrind]#
[root@centos-linux-7 valgrind]# g++ -g v.cpp &&  valgrind --leak-check=full  ./a.out
==7226== Memcheck, a memory error detector
==7226== Copyright (C) 2002-2017, and GNU GPL'd, by Julian Seward et al.
==7226== Using Valgrind-3.15.0 and LibVEX; rerun with -h for copyright info
==7226== Command: ./a.out
==7226==
==7226==
==7226== HEAP SUMMARY:
==7226==     in use at exit: 192 bytes in 8 blocks
==7226==   total heap usage: 8 allocs, 0 frees, 192 bytes allocated
==7226==
==7226== 192 (24 direct, 168 indirect) bytes in 1 blocks are definitely lost in loss record 8 of 8
==7226==    at 0x4C29F73: malloc (vg_replace_malloc.c:309)
==7226==    by 0x40065E: mk(_node*, _node*, char) (v.cpp:12)
==7226==    by 0x400774: main (v.cpp:34)
==7226==
==7226== LEAK SUMMARY:
==7226==    definitely lost: 24 bytes in 1 blocks
==7226==    indirectly lost: 168 bytes in 7 blocks
==7226==      possibly lost: 0 bytes in 0 blocks
==7226==    still reachable: 0 bytes in 0 blocks
==7226==         suppressed: 0 bytes in 0 blocks
==7226==
==7226== For lists of detected and suppressed errors, rerun with: -s
==7226== ERROR SUMMARY: 1 errors from 1 contexts (suppressed: 0 from 0)
[root@centos-linux-7 valgrind]#
```




## 参考资料

<https://www.ibm.com/developerworks/cn/linux/l-cn-valgrind/>

<https://www.valgrind.org/>

<http://www.it.uc3m.es/pbasanta/asng/course_notes/memory_profiler_en.html>

<https://senlinzhan.github.io/2017/12/31/valgrind/>

