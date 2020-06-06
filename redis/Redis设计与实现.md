# Redis的设计与实现

## 第2章 简单动态字符串

```c
struct sdshdr {

    // 记录 buf 数组中已使用字节的数量
    // 等于 SDS 所保存字符串的长度
    int len;

    uint64_t alloc; /* excluding the header and null terminator */    

    // 记录 buf 数组中未使用字节的数量
    int free;

    // 字节数组，用于保存字符串
    char buf[];

};

// free 属性的值为 0 ， 表示这个 SDS 没有分配任何未使用空间。
// len 属性的值为 5 ， 表示这个 SDS 保存了一个五字节长的字符串。
// buf 属性是一个 char 类型的数组， 数组的前五个字节分别保存了 'R' 、 'e' 、 'd' 、 'i' 、 's' 五个字符， 而最后一个字节则保存了空字符 '\0' 。

```

特点：

1. 常数复杂度获取字符串长度
2. 杜绝缓冲区溢出
3. 减少修改字符串时带来的内存重分配次数¶
    - 空间预分配
    - 惰性空间释放
4. 二进制安全, 还可以存放非字符数据
5. 兼容部分 C 字符串函数¶



## 第3章 链表
多个 listNode 可以通过 prev 和 next 指针组成双端链表

```c
typedef struct listNode {

    // 前置节点
    struct listNode *prev;

    // 后置节点
    struct listNode *next;

    // 节点的值
    void *value;

} listNode;

typedef struct list {

    // 表头节点
    listNode *head;

    // 表尾节点
    listNode *tail;

    // 链表所包含的节点数量
    unsigned long len;

    // 节点值复制函数
    void *(*dup)(void *ptr);

    // 节点值释放函数
    void (*free)(void *ptr);

    // 节点值对比函数
    int (*match)(void *ptr, void *key);

} list;

```



## 第4章 字典的实现

哈希表作为底层实现，而每个哈希表节点就保存了字典中的一个键值对。

**哈希表**

```c
typedef struct dictht {

    // 哈希表数组
    dictEntry **table;

    // 哈希表大小
    unsigned long size;

    // 哈希表大小掩码，用于计算索引值
    // 总是等于 size - 1
    unsigned long sizemask;

    // 该哈希表已有节点的数量
    unsigned long used;

} dictht;
```

* 每个dictEntry结构保存着一个键值对。
* size属性记录了哈希表的大小，也即是table数组的大小，
* 而used属性则记录了哈希表目前已有节点（键值对）的数量

**哈希表节点**

```c
typedef struct dictEntry {

    // 键
    void *key;

    // 值
    union {
        void *val;
        uint64_t u64;
        int64_t s64;
    } v;

    // 指向下个哈希表节点，形成链表
    struct dictEntry *next;

} dictEntry;
```

* next属性是指向另一个哈希表节点的指针，这个指针可以将多个哈希值相同的键值对连接在一次，以此来解决键冲突（collision）的问题。


**字典**

```c
typedef struct dict {

    // 类型特定函数
    dictType *type;

    // 私有数据
    void *privdata;

    // 哈希表
    dictht ht[2];

    // rehash 索引
    // 当 rehash 不在进行时，值为 -1
    int rehashidx; /* rehashing not in progress if rehashidx == -1 */

} dict;
```

* 数组中的每个项都是一个dictht哈希表，一般情况下，字典只使用ht[0]哈希表，ht[1]哈希表只会在对ht[0]哈希表进行rehash时使用。
* 除了ht[1]之外，另一个和rehash有关的属性就是rehashidx，它记录了rehash目前的进度，如果目前没有在进行rehash，那么它的值为-1。



**4.3 解决键冲突**

Redis的哈希表使用链地址法（separate chaining）来解决键冲突，

因为dictEntry节点组成的链表没有指向链表表尾的指针，所以为了速度考虑，程序总是将新节点添加到链表的表头位置（复杂度为O（1）），排在其他已有节点的前面。

**4.4 rehash**

当哈希表保存的键值对数量太多或者太少时，程序需要对哈希表的大小进行相应的扩展或者收缩。扩展和收缩哈希表的工作可以通过执行rehash（重新散列）操作来完成，


扩容: x >= ht[0].used\*2 && x == pow(2, n) && min(x)  (第一个大于等于ht[0].used\*2的2 n（2的n次方幂）；)

收缩: x >= ht[0].used && x == 2n && min(x)	(第一个大于等于ht[0].used的2 n。)


负载因子 = 哈希表已保存节点数量 / 哈希表大小

load_factor = ht[0].used / ht[0].size

load_factor<0.1
	收缩
load_factor>=1
	扩展




**4.5 渐进式rehash**


以下是哈希表渐进式rehash的详细步骤：

1. 为ht[1]分配空间，让字典同时持有ht[0]和ht[1]两个哈希表。
2. 在字典中维持一个索引计数器变量rehashidx，并将它的值设置为0，表示rehash工作正式开始。
3. 在rehash进行期间，每次对字典执行添加、删除、查找或者更新操作时，程序除了执行指定的操作以外，还会顺带将ht[0]哈希表在rehashidx索引上的所有键值对rehash到ht[1]，当rehash工作完成之后，程序将rehashidx属性的值增一。
4. 随着字典操作的不断执行，最终在某个时间点上，ht[0]的所有键值对都会被rehash至ht[1]，这时程序将rehashidx属性的值设为-1，表示rehash操作已完成。

渐进式rehash的好处在于它采取分而治之的方式，将rehash键值对所需的计算工作均摊到对字典的每个添加、删除、查找和更新操作上，从而避免了集中式rehash而带来的庞大计算量。






## 第5章 跳跃表

跳跃表（skiplist）是一种有序链表结构，它通过在每个节点中维持多个指向其他节点的指针，从而达到快速访问

在大部分情况下，跳跃表的效率可以和平衡树相媲美，并且因为跳跃表的实现比平衡树要来得更为简单，所以有不少程序都使用跳跃表来代替平衡树。

1. 层跳

跃表节点的level数组可以包含多个元素，每个元素都包含一个指向其他节点的指针，程序可以通过这些层来加快访问其他节点的速度，一般来说，层的数量越多，访问其他节点的速度就越快。每次创建一个新跳跃表节点的时候，程序都根据幂次定律（power law，越大的数出现的概率越小）随机生成一个介于1和32之间的值作为level数组的大小，这个大小就是层的“高度”。

2. 前进指针

每个层都有一个指向表尾方向的前进指针（level[i].forward属性），用于从表头向表尾方向访问节点

3. 跨度

层的跨度（level[i].span属性）用于记录两个节点之间的距离：

4. 后退指针

节点的后退指针（backward属性）用于从表尾向表头方向访问节点：跟可以一次跳过多个节点的前进指针不同，因为每个节点只有一个后退指针，所以每次只能后退至前一个节点。




## 第6章 整数集合

保存整数值的集合抽象数据结构，它可以保存类型为int16_t、int32_t或者int64_t的整数值，并且保证集合中不会出现重复元素。

整数集合（intset）是集合键的底层实现之一： 当一个集合只包含整数值元素， 并且这个集合的元素数量不多时， Redis 就会使用整数集合作为集合键的底层实现。


```shell
redis> SADD numbers 1 3 5 7 9
(integer) 5

redis> OBJECT ENCODING numbers
"intset"
```

```c
typedef struct intset {

    // 编码方式,INTSET_ENC_INT16,INTSET_ENC_INT32,INTSET_ENC_INT64, 表示contents中的元素的数据类型
    uint32_t encoding;

    // 集合包含的元素数量，表示contents中元素的个数
    uint32_t length;

    // 保存元素的数组
    int8_t contents[];

} intset;
```





## 第7章 压缩列表

压缩列表是Redis为了节约内存而开发的，是由一系列特殊编码的连续内存块组成的顺序型（sequential）数据结构。一个压缩列表可以包含任意多个节点（entry），每个节点可以保存一个字节数组或者一个整数值。


**表头**

1. zlbytes uint32_t ：压缩列表的总长
2. zltail uint32_t：结尾节点的偏移量
3. zllen uint16_t：节点数


**元素**

1. previous_entry_length：压缩列表中前一个节点的长度
2. encoding： content 属性所保存数据的类型以及长度
3. content：保存节点的值， 节点值可以是一个字节数组或者整数， 值的类型和长度由节点的 encoding 属性决定

 
00、01、10 的是字节数组编码

编码 |  编码长度 |   content属性保存的值
---|---------|--------------------
00bbbbbb                                     | 1 字节| 长度小于等于 63 字节的字节数组。
01bbbbbb xxxxxxxx                            | 2 字节| 长度小于等于 16383 字节的字节数组。
10______ aaaaaaaa bbbbbbbb cccccccc dddddddd | 5 字节| 长度小于等于 4294967295 的字节数组。


11 开头的是整数编码

编码      | 编码长度  |  content 属性保存的值
---------|-----------|-------------------
11000000 |   1 字节 |   int16_t 类型的整数。
11010000 |   1 字节 |   int32_t 类型的整数。
11100000 |   1 字节 |   int64_t 类型的整数。
11110000 |   1 字节 |   24 位有符号整数。
11111110 |   1 字节 |   8 位有符号整数。
1111xxxx |   1 字节 |  使用这一编码的节点没有相应的 content 属性，<br> 因为编码本身的 xxxx 四个位已经保存了一个介于 0 和 12 之间的值， <br>所以它无须 content 属性。





## 第9章 数据库

**9.4.2 保存过期时间**

redisDb结构的expires字典保存了数据库中所有键的过期时间，我们称这个字典为过期字典：

- 过期字典的键是一个指针，这个指针指向键空间中的某个键对象（也即是某个数据库键）。
- 过期字典的值是一个long long类型的整数，这个整数保存了键所指向的数据库键的过期时间——一个毫秒精度的UNIX时间戳。


**9.4.5 过期键的判定**

通过过期字典，程序可以用以下步骤检查一个给定键是否过期：

1. 检查给定键是否存在于过期字典：如果存在，那么取得键的过期时间。
2. 检查当前UNIX时间戳是否大于键的过期时间：如果是的话，那么键已经过期；否则的话，键未过期。

**9.5 过期键删除策略**


❑定时删除：在设置键的过期时间的同时，创建一个定时器（timer），让定时器在键的过期时间来临时，立即执行对键的删除操作。
❑惰性删除：放任键过期不管，但是每次从键空间中获取键时，都检查取得的键是否过期，如果过期的话，就删除该键；如果没有过期，就返回该键。
❑定期删除：每隔一段时间，程序就对数据库进行一次检查，删除里面的过期键。至于要删除多少过期键，以及要检查多少个数据库，则由算法决定。




