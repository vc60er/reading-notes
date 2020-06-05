# linux

[toc]


## 浮点

### [float和double的精度](https://www.cnblogs.com/c-primer/p/5992696.html)
浮点的表为：二进制的科学计数法
```
数值=尾巴数\*底数^指数
注：
底数=2
底数：省略了开头第一位的1
指数：通过移位法表示（float +127， double+1024）
小数部分的表示法：乘2，取整
```
|类型|位数|符号位|指数位|尾数位|指数范围|有效位|最大底数|
|---|----|------|-----|------|-------|-----|------|
|float|32|1bit|8bits|23bits|-127~128|[6,7)|2^23 = 8388608|
|double|64|1bit|11bits|52bits|-1023~1024|[15,16)|2^52 = 4503599627370496|



### 内存

#### [解读VMSTAT中的ACTIVE/INACTIVE MEMORY](http://linuxperf.com/?p=97)

vmstat -a 命令能看到active memory 和 inactive memory：
```shell
$ vmstat -a 
procs -----------memory---------- ---swap-- -----io---- -system-- ------cpu-----
 r  b   swpd   free  inact active   si   so    bi    bo   in   cs us sy id wa st
 1  0 138096 319560 1372408 1757848    0    0     2     3    2    3  1  0 99  0  0
```

Active Memory = ACTIVE_ANON + ACTIVE_FILE，
Inactive Memory = INACTIVE_ANON + INACTIVE_FILE。


ACTIVE_ANON: active anonymous pages, 进程的堆栈内存页面
ACTIVE_FILE: active file-backed pages, 程序文件、数据文件所对应的内存页


它采用了LRU list列表，把刚访问过的页面放在列首，越接近列尾的就是越长时间未访问过的页面

Linux kernel设计了两种LRU list: active list 和 inactive list, 
刚访问过的页面放进active list，长时间未访问过的页面放进inactive list，
这样从inactive list回收页面就变得简单了。
内核线程kswapd会周期性地把active list中符合条件的页面移到inactive list中




#### [FREE命令显示的BUFFERS与CACHED的区别](http://linuxperf.com/?p=32)

free 命令是Linux系统上查看内存使用状况最常用的工具，然而很少有人能说清楚 “buffers” 与 “cached” 之间的区别：
```shell
# free
             total       used       free     shared    buffers     cached
Mem:       3848656    2983016     865640       5312     324432    2024904
-/+ buffers/cache:     633680    3214976
Swap:      2031612          0    2031612
```
*buffers*

表示块设备(block device)所占用的缓存页

直接对块设备进行读写操作；
文件系统的metadata（元数据），比如 SuperBlock。


*cached*

表示普通文件数据所占用的缓存页。

*Swap*

栈堆内存的anonymous pages（匿名页），page-out时候是要写到交换区中，swap cache 是 swap ares 的 page cache。
所以，将要page-out的匿名页即将被swap-out时会先被放进swap cache，但通常只存在很短暂的时间，因为紧接着在pageout完成之后它就会从swap cache中删除，




https://www.cnblogs.com/kevingrace/p/5991604.html
https://www.cnblogs.com/logo-fox/p/5820910.html
https://blog.csdn.net/bingqingsuimeng/article/details/51858858



### 文件

#### [第八章、Linux 磁盘与文件系统管理](http://cn.linux.vbird.org/linux_basic/0230filesystem_1.php)
inode,metadata,Superblock
块设备

文件系统中将文件内容和文件属性（权限，所有者， 群组，时间）分别存储在不同的区块，
1. superblock：记录此 filesystem 的整体信息，包括inode/block的总量、使用量、剩余量， 以及文件系统的格式与相关信息等；
2. inode：记录文件的属性，一个文件占用一个inode，同时记录此文件的数据所在的 block 号码；
3. block：实际记录文件的内容和文件名，若文件太大时，会占用多个 block 


每个 inode 与 block 都有编号，而每个文件都会占用一个 inode ，inode 内则有文件数据放置的 block 号码（一般是多个）

* inode 大小均固定为 128 bytes；
* block的大小有1K，2K，4K
* block的编号记录在inode table，每个block占用4字节

inode table 包含

* 12个直接指向：12\*1K=12K inode->block
* 间接： 256\*1K=256K inode->inode(block)->block
* 双间接： 256\*256\*1K=2562K inode->inode(block)->inode(block)->block
* 三间接： 256\*256\*256\*1K=2563K, inode->inode(block)->inode(block)->inode(block)->block


superblock 的大小为 1024bytes、他记录的信息主要有：

* block 与 inode 的总量；
* 未使用与已使用的 inode / block 数量；
* block 与 inode 的大小 (block 为 1, 2, 4K，inode 为 128 bytes)；
* filesystem 的挂载时间、最近一次写入数据的时间、最近一次检验磁盘 (fsck) 的时间等文件系统的相关信息；
* 一个 valid bit 数值，若此文件系统已被挂载，则 valid bit 为 0 ，若未被挂载，则 valid bit 为 1 。

*目录*
目录也占用一个inode，和至少一个block，block记录在这个目录下的文件名与该文件名占用的 inode 号码数据



*dentry*

是directory entry的缩写，是目录与其下文件的缓存，主要信息包括文件名和其parent目录名。一个dentry链表常常是可以从一个文件向上追踪其每一个parent directory，直到root。


### 进程


### [理解LINUX LOAD AVERAGE的误区](http://linuxperf.com/?p=176])
*unix*

load表示runnable processes的数量，包括正在使用CPU的进程数量，和正在等待CPU的进程数量。

*linux*

除了以上两种状态的进程以外，还增加了uninterruptible sleep的进程数量。通常等待IO设备、等待网络的时候，进程会处于uninterruptible sleep状态



#### [Linux进程状态码总结](https://www.huliujia.com/blog/f70756be368904eb2e2c463671d67fd5d0dd0b72/)

http://linuxperf.com/?cat=4


#### [借助PERF工具分析CPU使用率](http://linuxperf.com/?p=36)


然后我们看到 CPU 1 的 %system 飙升到95%：
```shell
# sar -P ALL -u 2 2

08:21:16 PM     CPU     %user     %nice   %system   %iowait    %steal     %idle
08:21:18 PM     all      2.25      0.00     48.25      0.00      0.00     49.50
08:21:18 PM       0      0.50      0.00      1.00      0.00      0.00     98.51
08:21:18 PM       1      4.02      0.00     95.98      0.00      0.00      0.00
```

现在我们用 perf 工具采样：
```shell
# perf record -a -e cycles -o cycle.perf -g sleep 10
[ perf record: Woken up 18 times to write data ]
[ perf record: Captured and wrote 4.953 MB cycle.perf (~216405 samples) ]
```
注：”-a”表示对所有CPU采样，如果只需针对特定的CPU，可以使用”-C”选项。

把采样的数据生成报告：

# perf report -i cycle.perf | more

```shell
# Samples: 40K of event 'cycles'
# Event count (approx.): 18491174032
#
# Overhead          Command                   Shared Object              Symbol
# ........  ...............  ..............................    ................
#
    75.65%              cat  [kernel.kallsyms]                 [k] __clear_user                     
                        |
                        --- __clear_user
                           |          
                           |--99.56%-- read_zero
                           |          vfs_read
                           |          sys_read
                           |          system_call_fastpath
                           |          __GI___libc_read
                            --0.44%-- [...]

     2.34%              cat  [kernel.kallsyms]                 [k] system_call                      
                        |
                        --- system_call
                           |          
                           |--56.72%-- __write_nocancel
                           |          
                            --43.28%-- __GI___libc_read
```

我们很清楚地看到，CPU利用率有75%来自 cat 进程 的 sys_read 系统调用，perf 甚至精确地告诉了我们是消耗在 read_zero 这个 kernel routine 上。


### I/O

#### [理解 %IOWAIT (%WIO)](http://linuxperf.com/?p=33)
%iowait表示在一个采样周期内有百分之几的时间属于以下情况：CPU空闲、并且有仍未完成的I/O请求。

* %iowait、%idle、%user、%system 等这些指标都是全局性的，并不是特指某个进程。
* 进程都处于休眠状态、在等待某个特定事件：比如等待定时器、或者来自网络的数据、或者键盘输入、或者等待I/O操作完成
* I/O并没有变化，%iowait 却升高了，原因仅仅是CPU的空闲时间增加了
* CPU的繁忙状况保持不变，I/O的并发程度会影响总的I/O时间，从而影响iowait


%iowait 和io瓶颈没有必然关系。

如果看到 %iowait 升高，还需检查I/O量有没有明显增加，avserv/avwait/avque等指标有没有明显增大，应用有没有感觉变慢，如果都没有，就没什么好担心的。


#### [DISK 100% BUSY，谁造成的？](http://linuxperf.com/?p=40)
iostat等命令看到的是系统级的统计，比如下例中我们看到/dev/sdb很忙，如果要追查是哪个进程导致的I/O繁忙，应该怎么办
```shell
# iostat -xd
...
Device:         rrqm/s   wrqm/s     r/s     w/s    rkB/s    wkB/s avgrq-sz avgqu-sz   await r_await w_await  svctm  %util
sda               0.00     0.00    0.00    0.00     0.00     0.00     0.00     0.00    0.00    0.00    0.00   0.00   0.00
sdb               0.00     0.00 6781.67    0.00  3390.83     0.00     1.00     0.85    0.13    0.13    0.00   0.13  85.03
dm-0              0.00     0.00    0.00    0.00     0.00     0.00     0.00     0.00    0.00    0.00    0.00   0.00   0.00
dm-1              0.00     0.00    0.00    0.00     0.00     0.00     0.00     0.00    0.00    0.00    0.00   0.00   0.00
dm-2              0.00     0.00    0.00    0.00     0.00     0.00     0.00     0.00    0.00    0.00    0.00   0.00   0.00
...
```


可以用上万能工具SystemTap。比如：我们希望找出访问/dev/sdb的进程，可以用下列脚本，它的原理是对submit_bio下探针：
```shell
#! /usr/bin/env stap

global device_of_interest

probe begin {
  device_of_interest = $1
  printf ("device of interest: 0x%x\n", device_of_interest)
}

probe kernel.function("submit_bio")
{
  dev = $bio->bi_bdev->bd_dev
  if (dev == device_of_interest)
    printf ("[%s](%d) dev:0x%x rw:%d size:%d\n",
            execname(), pid(), dev, $rw, $bio->bi_size)
}
```


这个脚本需要在命令行参数中指定需要监控的硬盘设备号，得到这个设备号的方法如下：
```shell
# ll /dev/sdb
brw-rw----. 1 root disk 8, 16 Oct 24 15:52 /dev/sdb

Major number(12-bit):  8 i.e. 0x8
Minor number(20-bit): 16 i.e. 0x00010
合在一起得到设备号： 0x800010
```

执行脚本，我们看到：

```shell
# ./dev_task_io.stp 0x800010
device of interest: 0x800010
[dd](31202) dev:0x800010 rw:0 size:512
[dd](31202) dev:0x800010 rw:0 size:512
[dd](31202) dev:0x800010 rw:0 size:512
[dd](31202) dev:0x800010 rw:0 size:512
[dd](31202) dev:0x800010 rw:0 size:512
```

结果很令人满意，我们看到是进程号为31202的dd命令在对/dev/sdb进行读操作。





https://www.cnblogs.com/anker/p/3271773.html
https://www.cnblogs.com/lxmhhy/p/6212405.html
https://www.cnblogs.com/lxmhhy/p/6214113.html
https://liubigbin.github.io/2016/03/11/Linux-%E4%B9%8B%E5%AE%88%E6%8A%A4%E8%BF%9B%E7%A8%8B%E3%80%81%E5%83%B5%E6%AD%BB%E8%BF%9B%E7%A8%8B%E4%B8%8E%E5%AD%A4%E5%84%BF%E8%BF%9B%E7%A8%8B/
https://michaelyou.github.io/2015/03/12/%E5%AD%A4%E5%84%BF%E8%BF%9B%E7%A8%8B%E4%B8%8E%E5%83%B5%E5%B0%B8%E8%BF%9B%E7%A8%8B/
https://www.jianshu.com/p/7390f73ad668
https://www.jianshu.com/p/7265be04cb98
https://zhuanlan.zhihu.com/p/96098130


