# mysql

<!-- MarkdownTOC autolink=true -->

- [索引](#%E7%B4%A2%E5%BC%95)
	- [底层算法](#%E5%BA%95%E5%B1%82%E7%AE%97%E6%B3%95)
	- [数据存储](#%E6%95%B0%E6%8D%AE%E5%AD%98%E5%82%A8)
	- [类型](#%E7%B1%BB%E5%9E%8B)
- [锁](#%E9%94%81)
- [事务](#%E4%BA%8B%E5%8A%A1)
- [引擎](#%E5%BC%95%E6%93%8E)
- [索引](#%E7%B4%A2%E5%BC%95-1)
	- [b tree & b+ tree](#b-tree--b-tree)
- [笔记](#%E7%AC%94%E8%AE%B0)
- [资源](#%E8%B5%84%E6%BA%90)
- [笔记-『浅入浅出』MySQL 和 InnoDB](#%E7%AC%94%E8%AE%B0-%E3%80%8E%E6%B5%85%E5%85%A5%E6%B5%85%E5%87%BA%E3%80%8Fmysql-%E5%92%8C-innodb)
	- [数据页结构](#%E6%95%B0%E6%8D%AE%E9%A1%B5%E7%BB%93%E6%9E%84)
- [索引](#%E7%B4%A2%E5%BC%95-2)
	- [索引的数据结构](#%E7%B4%A2%E5%BC%95%E7%9A%84%E6%95%B0%E6%8D%AE%E7%BB%93%E6%9E%84)
	- [数据页结构](#%E6%95%B0%E6%8D%AE%E9%A1%B5%E7%BB%93%E6%9E%84-1)
	- [聚集索引和辅助索引](#%E8%81%9A%E9%9B%86%E7%B4%A2%E5%BC%95%E5%92%8C%E8%BE%85%E5%8A%A9%E7%B4%A2%E5%BC%95)
		- [聚集索引](#%E8%81%9A%E9%9B%86%E7%B4%A2%E5%BC%95)
		- [辅助索引\(非聚集索引\)](#%E8%BE%85%E5%8A%A9%E7%B4%A2%E5%BC%95%E9%9D%9E%E8%81%9A%E9%9B%86%E7%B4%A2%E5%BC%95)
		- [索引的设计](#%E7%B4%A2%E5%BC%95%E7%9A%84%E8%AE%BE%E8%AE%A1)
- [锁](#%E9%94%81-1)
	- [并发控制机制](#%E5%B9%B6%E5%8F%91%E6%8E%A7%E5%88%B6%E6%9C%BA%E5%88%B6)
	- [锁的种类](#%E9%94%81%E7%9A%84%E7%A7%8D%E7%B1%BB)
	- [锁的粒度](#%E9%94%81%E7%9A%84%E7%B2%92%E5%BA%A6)
	- [锁的算法](#%E9%94%81%E7%9A%84%E7%AE%97%E6%B3%95)
	- [死锁的发生](#%E6%AD%BB%E9%94%81%E7%9A%84%E5%8F%91%E7%94%9F)
- [事务与隔离级别](#%E4%BA%8B%E5%8A%A1%E4%B8%8E%E9%9A%94%E7%A6%BB%E7%BA%A7%E5%88%AB)
- [笔记-MySQL 索引设计概要](#%E7%AC%94%E8%AE%B0-mysql-%E7%B4%A2%E5%BC%95%E8%AE%BE%E8%AE%A1%E6%A6%82%E8%A6%81)
- [磁盘 IO](#%E7%A3%81%E7%9B%98-io)
	- [随机读取](#%E9%9A%8F%E6%9C%BA%E8%AF%BB%E5%8F%96)
	- [查询过程](#%E6%9F%A5%E8%AF%A2%E8%BF%87%E7%A8%8B)
		- [索引片](#%E7%B4%A2%E5%BC%95%E7%89%87)
		- [过滤因子](#%E8%BF%87%E6%BB%A4%E5%9B%A0%E5%AD%90)
		- [匹配列与过滤列](#%E5%8C%B9%E9%85%8D%E5%88%97%E4%B8%8E%E8%BF%87%E6%BB%A4%E5%88%97)
- [索引的设计](#%E7%B4%A2%E5%BC%95%E7%9A%84%E8%AE%BE%E8%AE%A1-1)
	- [三星索引，创建步骤](#%E4%B8%89%E6%98%9F%E7%B4%A2%E5%BC%95%EF%BC%8C%E5%88%9B%E5%BB%BA%E6%AD%A5%E9%AA%A4)
- [MySQL 索引性能分析概要](#mysql-%E7%B4%A2%E5%BC%95%E6%80%A7%E8%83%BD%E5%88%86%E6%9E%90%E6%A6%82%E8%A6%81)
- [为什么 MySQL 使用 B+ 树](#%E4%B8%BA%E4%BB%80%E4%B9%88-mysql-%E4%BD%BF%E7%94%A8-b-%E6%A0%91)
- [概述](#%E6%A6%82%E8%BF%B0)
- [设计](#%E8%AE%BE%E8%AE%A1)
	- [读写性能](#%E8%AF%BB%E5%86%99%E6%80%A7%E8%83%BD)
	- [数据加载](#%E6%95%B0%E6%8D%AE%E5%8A%A0%E8%BD%BD)
- [索引的正确使用](#%E7%B4%A2%E5%BC%95%E7%9A%84%E6%AD%A3%E7%A1%AE%E4%BD%BF%E7%94%A8)

<!-- /MarkdownTOC -->


## 索引
### 底层算法
1. btree
2. hash
3. rtree

### 数据存储
1. 聚合索引
2. 非聚合索引

### 类型
- 主键索引
- 唯一索引
- 普通索引
- 联合缩影
- 全文索引
- 前缀索引


## 锁

## 事务

## 引擎





## 索引
### b tree & b+ tree
b:
树内的每个节点都存储数据
叶子节点之间无指针相邻

b+:
数据只出现在叶子节点
所有叶子节点增加了一个链指针

针对上面的B+树和B树的特点，我们做一个总结
(1)B树的树内存储数据，因此查询单条数据的时候，B树的查询效率不固定，最好的情况是O(1)。我们可以认为在做单一数据查询的时候，使用B树平均性能更好。但是，由于B树中各节点之间没有指针相邻，因此B树不适合做一些数据遍历操作。

(2)B+树的数据只出现在叶子节点上，因此在查询单条数据的时候，查询速度非常稳定。因此，在做单一数据的查询上，其平均性能并不如B树。但是，B+树的叶子节点上有指针进行相连，因此在做数据遍历的时候，只需要对叶子节点进行遍历即可，这个特性使得B+树非常适合做范围查询。




## 笔记


- [面试官:谈谈你对mysql联合索引的认识？](https://www.cnblogs.com/rjzheng/p/12557314.html)

*联合索引*

1. 前缀匹配（最左匹配原则）
2. 遇到范围查询（>、<、between、like）之后停止匹配（范围查询后的条件索引失效），
3. 条件中列的顺序，会被优化器按照索引的顺序来优化

*索引的列*

索引的列，如果是全部的select列，则不需要再次回表查询
索引的列，可用于优化排序





- [面试官:讲讲mysql表设计要注意啥](https://www.cnblogs.com/rjzheng/p/11174714.html)

**问题1:为什么一定要设一个主键？**
	主键会被用于聚合索引，没有定义的情况下，系统会定义一个隐藏的主键，如果显示定义自己的主键，再次再此键上的查询能到达最优

**问题2:主键是用自增还是UUID?**
	自增id能保证新插入的id是最大值，所以会被保存在聚合缩影的末尾
	uuid不能保证新插入的id是最大值，会造成在聚合索引上的随机插入，从而引发页的分裂

**问题3:主键为什么不推荐有业务含义?**
	有业务含义，意味着
	有可能被修改，修改主键上的值成本比较高
	有可能不是自增的

**问题4:表示枚举的字段为什么不用enum类型？**
	回答:在工作中表示枚举的字段，一般用tinyint类型。
	那为什么不用enum类型呢？下面两个原因
	(1)ENUM类型的ORDER BY操作效率低，需要额外操作
	(2)如果枚举值是数值，有陷阱	

**问题5:货币字段用什么类型?**
	回答:如果货币单位是分，可以用Int类型。如果坚持用元，用Decimal。
	千万不要答float和double，因为float和double是以二进制存储的，所以有一定的误差。
	打个比方，你建一个列如下

	CREATE TABLE `t` (
	  `price` float(10,2) DEFAULT NULL,
	) ENGINE=InnoDB DEFAULT CHARSET=utf8
	然后insert给price列一个数据为1234567.23，你会发现显示出来的数据变为1234567.25，精度失准！

**问题6:时间字段用什么类型?**
	varchar：做时间比较运算，你需要用STR_TO_DATE等函数将其转化为时间类型，你会发现这么写是无法命中索引的
	timestamp：四个字节的整数，2038年以后的时间，是无法用timestamp类型存储的。但是它有一个优势，timestamp类型是带有时区信息的
	datetime：占用8个字节，他存储的是时间绝对值，不带有时区信息

**问题7:为什么不直接存储图片、音频、视频等大容量内容?**
	读取数据是按照行读取的

**问题8:字段为什么要定义为NOT NULL?**
	(1)索引性能不好：Mysql难以优化引用可空列查询，它会使索引、索引统计和值更加复杂。	
	(2)查询会出现一些不可预料的结果



- [【原创】杂谈自增主键用完了怎么办)](https://www.cnblogs.com/rjzheng/p/10669043.html)

把自增主键的类型改为BigInt类型就好了！

1. 直接修改：对于修改数据类型这种操作，是不支持并发的DML操作！也就是说，如果你直接使用ALTER这样的语句在线修改表数据结构，会导致这张表无法进行更新类操作(DELETE、UPDATE、DELETE)。

2. 使用第三方工具：gh-ost，pt-osc

3. 主存切换：改从库表结构，然后主从切换，切换过程中可能会有数据丢失



- [【原创】分库分表后如何部署上线](https://www.cnblogs.com/rjzheng/p/9597810.html)









## 资源
http://blog.codinglabs.org/articles/theory-of-mysql-index.html






# 笔记-『浅入浅出』MySQL 和 InnoDB
https://draveness.me/mysql-innodb/

*存储*
tablespace->segment->extent->page->row
innodb_page_size = 16KB
表的定义和索引分开存储（表的定义：.idb；索引：.frm）



Compact 和 Redundant 格式最大的不同就是记录格式的第一个部分；在 Compact 中，行记录的第一部分倒序存放了一行数据中列的长度（Length），而 Redundant 中存的是每一列的偏移量（Offset），从总体上上看，Compact 行记录格式相比 Redundant 格式能够减少 20% 的存储空间。



行溢出数据


### 数据页结构

页是 InnoDB 存储引擎管理数据的最小磁盘单位，而 B-Tree 节点就是实际存放表中数据的页面

内部的 Page Header/Page Directory 关心的是页的状态信息，
而 Fil Header/Fil Trailer 关心的是记录页的头信息

Infimum 和 Supremum 这两个虚拟的记录（可以理解为占位符），Infimum 记录是比该页中任何主键值都要小的值，Supremum 是该页中的最大值：

User Records 就是整个页面中真正用于存放行记录的部分，而 Free Space 就是空余空间了，它是一个链表的数据结构，为了保证插入和删除的效率，整个页面并不会按照主键顺序对所有记录进行排序，它会自动从左侧向右寻找空白节点进行插入




## 索引

### 索引的数据结构
整个页面并不会按照主键顺序对所有记录进行排序，它会自动从左侧向右寻找空白节点进行插入，行记录在物理存储上并不是按照顺序的，它们之间的顺序是由 next_record 这一指针控制的。


### 数据页结构

InnoDB 存储引擎在绝大多数情况下使用 B+ 树建立索引，这是关系型数据库中查找最为常用和有效的索引，但是 B+ 树索引并不能找到一个给定键对应的具体值，它只能找到数据行对应的页,数据库把整个页读入到内存中，并在内存中查找具体的数据行。

行记录在物理存储上并不是按照顺序的，它们之间的顺序是由 next_record 这一指针控制的。

B+ 树是平衡树，它查找任意节点所耗费的时间都是完全相同的，比较的次数就是 B+ 树的高度；


### 聚集索引和辅助索引
聚集索引中存放着一条行记录的全部信息，
而辅助索引中只包含索引列和一个用于查找对应行记录的『书签』

#### 聚集索引

聚集索引就是按照表中主键的顺序构建一颗 B+ 树，并在叶节点中存放表中的行记录数据。

#### 辅助索引(非聚集索引)

辅助索引也是通过 B+ 树实现的，但是它的叶节点并不包含行记录的全部数据，仅包含索引中的所有键和一个用于查找对应行记录的『书签』，在 InnoDB 中这个书签就是当前记录的主键。


#### 索引的设计
参考《数据库索引设计与优化》




## 锁

### 并发控制机制
InnoDB 存储引擎中使用的就是悲观锁

乐观锁是一种思想，它其实并不是一种真正的『锁』，它会先尝试对资源进行修改，在写回时判断资源是否进行了改变，如果没有发生改变就会写回，否则就会进行重试，在整个的执行过程中其实都没有对数据库进行加锁；

悲观锁就是一种真正的锁了，它会在获取资源前对资源进行加锁，确保同一时刻只有有限的线程能够访问该资源，其他想要尝试获取资源的操作都会进入等待状态，直到该线程完成了对资源的操作并且释放了锁后，其他线程才能重新操作资源；


### 锁的种类
共享锁（读锁）：允许事务对一条行数据进行读取；
互斥锁（写锁）：允许事务对一条行数据进行删除或更新；

共享锁之间是兼容的，而互斥锁与其他任意锁都不兼容：

### 锁的粒度
InnoDB 支持多种粒度的锁，也就是行锁和表锁；意向锁就是一种表级锁。
行锁
表锁
意向锁：意向锁其实不会阻塞全表扫描之外的任何请求，它们的主要目的是为了表示是否有人请求锁定表中的某一行数据。是预见是否存在行锁的一种方法。

意向共享锁：事务想要在获得表中某些记录的共享锁，需要在表上先加意向共享锁；
意向互斥锁：事务想要在获得表中某些记录的互斥锁，需要在表上先加意向互斥锁；


### 锁的算法

Record Lock：
where条件中有字段，使得能通索引定位的recodrd，使用用记录锁，否则使用行锁

Gap Lock：
where条件中有字段，使得能通过索引定位一个连续区域，使用间隙锁；

Next-Key Lock：
它是记录锁和记录前的间隙锁的结合，Next-Key 锁锁定的是当前值和前面的范围，Next-Key 锁的作用其实是为了解决幻读的问题，

### 死锁的发生

两个事务（t1，t2），各需要两把锁（A，B），相互支持对方需要的锁，事务不能结束，造成死锁
持有：t1：A  t2:B
需要：t1：B	t2:A



## 事务与隔离级别

RAED UNCOMMITED：使用查询语句不会加锁，可能会读到未提交的行（Dirty Read）；

读取到了其他事务没有提交的数据

READ COMMITED：只对记录加记录锁，而不会在记录之间加间隙锁，所以允许新的记录插入到被锁定记录的附近，所以再多次使用查询语句时，可能得到不同的结果（Non-Repeatable Read）；

多次读取，读到了其他事务的提交的修改，造成多次读取到的数据不一致，可以通过加行锁来解决

REPEATABLE READ：多次读取同一范围的数据会返回第一次查询的快照，不会返回不同的数据行，但是可能发生幻读（Phantom Read）；

多次读取，读到了其他事务添加的新数据，造成多次读区到的数据不一致，可以通过加间隙锁来解决


SERIALIZABLE：InnoDB 隐式地将全部的查询语句加上共享锁，解决了幻读的问题；




原文：
RAED UNCOMMITED：使用查询语句不会加锁，可能会读到未提交的行（Dirty Read）；
READ COMMITED：只对记录加记录锁，而不会在记录之间加间隙锁，所以允许新的记录插入到被锁定记录的附近，所以再多次使用查询语句时，可能得到不同的结果（Non-Repeatable Read）；


我理解是：
脏读，和不可重复读的原因好像有问题，
RAED UNCOMMITED：事务没有commit就落地盘，可能造成其他事务会读到未提交的行（Dirty Read）；
READ COMMITED：事务中select没有行锁，可能会读取到其他事务commit的修改。造成不可重复读（Non-Repeatable Read）；

并且，与下文中描述的原因也不一致

原文：
脏读
在一个事务中，读取了其他事务未提交的数据。
不可重复读
在一个事务中，同一行记录被访问了两次却得到了不同的结果。




# 笔记-MySQL 索引设计概要
https://draveness.me/sql-index-intro/


## 磁盘 IO
都会将该行或者多行所在的页全部加载进来，

### 随机读取
内存池<=内存读取《-磁盘

### 查询过程
#### 索引片
索引片其实就是 SQL 查询在执行过程中扫描的一个索引片段

#### 过滤因子
满足查询条件的记录行数所占的比例：

#### 匹配列与过滤列
匹配列：等值谓词
过滤列：范围谓词


## 索引的设计

### 三星索引，创建步骤
- 所有等值谓词中的列，作为索引开头的最开始的列（任意顺序）；
	建立必要索引，还能减少索引片的大小以减少需要扫描的数据行；
- 将 ORDER BY 列加入索引中
	排序的时候可以利用索引来排序
- 将查询语句剩余的列全部加入到索引中
	查询到数据之后，直接返回，避免回表





# MySQL 索引性能分析概要
https://draveness.me/sql-index-performance/


# 为什么 MySQL 使用 B+ 树
https://draveness.me/whys-the-design-mysql-b-plus-tree/
## 概述
## 设计

### 读写性能
hash索引在飞主键查询，处理范围查询，排序性能会非常差
B+树能够保证数据按照键的顺序进行存储

### 数据加载
B 树与 B+ 树的最大区别就是，
B 树可以在非叶结点中存储数据
B+ 树的所有数据其实都存储在叶子节点中，而这些叶节点可以通过『指针』依次按顺序连接

hash：范围查询和排序的时候，利用不了索引
B树：在范围查询的时候，要进行多次随机io


哈希：哈希虽然能够提供 O(1) 的单数据行操作性能，但是对于范围查询和排序却无法很好地支持，最终导致全表扫描；
B 树： B 树能够在非叶节点中存储数据，但是这也导致在查询连续数据时可能会带来更多的随机 I/O，而 B+ 树的所有叶节点可以通过指针相互连接，能够减少顺序遍历时产生的额外随机 I/O；


B+ 树这种数据结构会增加树的高度从而增加整体的耗时，然而高度为 3 的 B+ 树就能够存储千万级别的数据，实践中 B+ 树的高度最多也就 4 或者 5






## 索引的正确使用

类型一致
不要有表达式
like 使用前缀
把or语句写成union all


