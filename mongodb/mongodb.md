# mongodb

MongoDB是为快速开发互联网Web应用而设计的数据库系统。


## MongoDB 的特点
可以横向扩展
文档型数据库支持嵌套

## MongoDB 适用场景

## MongoDB vs Mysql

## 真的快吗

## WiredTiger



[MongoDB is web scale](http://www.mongodb-is-web-scale.com/)
[MySQL vs MongoDB 1000 reads](https://stackoverflow.com/questions/9702643/mysql-vs-mongodb-1000-reads)
[性能 - MySQL与MongoDB 1000读取](https://www.itranslater.com/qa/details/2110807474235245568)




# 为什么 MongoDB 使用 B 树
https://draveness.me/whys-the-design-mongodb-b-tree/

## 概述
MongoDB 使用了 WiredTiger 作为默认的存储引擎


## 设计
* 作为非关系型的数据库，MongoDB 对于遍历数据的需求没有关系型数据库那么强，它追求的是读写单个记录的性能；
* 大多数的数据库面对的都是读多写少的场景，B 树与 LSM 树在该场景下有更大的优势；

## 非关系型

而在 MongoDB 假设的场景中遍历数据并不是常见的需求。


MySQL 认为遍历数据的查询是常见的，所以它选择 B+ 树作为底层数据结构，而舍弃了通过非叶节点存储数据这一特性
MongoDB 认为查询单个数据记录远比遍历数据更加常见，由于 B 树的非叶结点也可以存储数据，所以查询一条数据所需要的平均随机 IO 次数会比 B+ 树少，


MongoDB 中推荐的设计方法，其实是使用嵌入文档，将 post 和属于它的所有 comments 都存储到一起：


MySQL 使用 B+ 树是因为数据的遍历在关系型数据库中非常常见，它经常需要处理各个表之间的关系并通过范围查询一些数据；
但是 MongoDB 作为面向文档的数据库，与数据之间的关系相比，它更看重以文档为中心的组织方式，所以选择了查询单个文档性能较好的 B 树，这个选择对遍历数据的查询也可以保证可以接受的时延；

LSM 树是一种专门用来优化写入的数据结构，它将随机写变成了顺序写显著地提高了写入性能，但是却牺牲了读的效率，这与大多数场景需要的特点是不匹配的，所以 MongoDB 最终还是选择读取性能更好的 B 树作为默认的数据结构；



b+的叶子结点不存放数据，b存放数据，需要再看看

