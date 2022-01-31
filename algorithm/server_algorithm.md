[TOC]

## 1. 调度算法
### 1.1. 轮询
### 1.2. 加权轮询
### 1.3. 平滑权重轮询

定义:

- init_weight(初始化权重)：预定义的权重
- cur_weight(当前权重)：计算获得权重，在调度过程中实际使用的权重


步骤:

1. cur_weight=init_weight
2. 分配max(cur_weight)对应的节点
3. cur_weigth=max(cur_weight)-sum(cur_weight)+init_weight
4. 重复2 ~ 4


>中文描述.   
>调度节点记录所有服务节点的当前权重值，初始化为配置对应值。
>当有请求需要调度时，每次会先把各节点的当前权重值加上自己的配置权重值，然后选择分配当前权重值最高的节点，同时被选择的节点权重值减去所有节点的原始权重值总和。
>若所有节点权重值都为零，则重置为初始化时配置的权重值。

举例:

服务节点为node={a,b,c}，init_weight={5,1,1}，
![img](https://mmbiz.qpic.cn/mmbiz_jpg/j3gficicyOvasuwkLUmCKAuKF3urWh30SaXlZ5icgpctxWCicDtDAwj90rkwnK6Pur3ehhoObHmlfJvJAqofRkwONA/640?wx_fmt=jpeg&tp=webp&wxfrom=5&wx_lazy=1&wx_co=1)


### 1.4. 随机

通过随机数取映射到对应的节点

定义:

待调度的节点为node={a,b,c}

步骤:

1. random()%len(node)=x
2. 分配node[x]
3. 重复1 ~ 3


### 1.5. 加权随机

原理:

将随机数范围区间，按照预定义权重分成多个子区间，随机数sharding到那个子区间，则分配该区间对应的节点

定义:

- node: 待分配的节点
- weight：分配权重
- sum_weight：　对weight累积求和的结果

步骤:

1. sum_weight={sum(weight)}
2. x=random%sum_weight[len(sum_weight)-1]
3. for i=0; x > sum_weight[i]; i++ {}
4. 分配node[x]
5. 重复2 ~ 5


实现:

```go
```


### 1.6. 最小负载

最小负载算法是根据各服务节点当前的真实负载能力进行请求分配的，当前负载最小的节点会被优先选择

最小负载算法可以描述为：

1. 服务节点定时向调度节点上报各自的负载情况，调度节点更新并记录所有服务节点的当前负载值。
2. 当有请求需要调度时，每次分配选择当前负载最小（负载盈余最大）的服务节点。


### 1.7. 两次随机选择策略

最小负载算法可以在请求异构情况下做到更好的均衡性。然而一般情况下服务节点的负载数据都是定时同步到调度节点，存在一定的滞后性，而使用滞后的负载数据进行调度会导致产生“群居”行为，在这种行为中，请求将批量地发送到当前某个低负载的节点，而当下一次同步更新负载数据时，该节点又有可能处于较高位置，然后不会被分配任何请求。再下一次又变成低负载节点被分配了更多的请求，一直处于这种很忙和很闲的循环状态，不利于服务器的稳定。

为应对这种情况，两次随机选择策略算法做了一些改进，该算法可以描述为：

1. 服务节点定时向调度节点上报各自的负载情况，调度节点更新并记录所有服务节点的当前负载值。
2. 从所有可用节点列表中做两次随机选择操作，得到两个节点。
3. 比较这两个节点负载情况，选择负载更低的节点作为被调度的节点。



### 1.8. 一致性哈希
#### 1.8.1. 划段
#### 1.8.2. 割环法

原理:

将节点和key哈希取模之后映射到一个较大的环上（2^32元素），分配时，选择在环上从key出发的遇到的第一个节点进行分配。

定义：

- 虚拟节点：又叫影子节点，其作用相当于真实节点的影子，或者引用，或者软连接之类。引入虚拟节点目的是解决环上的节点不均匀的问题，因为在真实节点数量非常有限（一般是几个，几十个）的情况下，对其哈希求余数，再映射到环上，就有可能出现非常不均匀的情况，解决办法就是通过虚拟节点补充真实节点的量
- node：真实节点
- vid：真实节点的虚拟节点id

步骤:

1. 构造环 ring = sort(hash(node[...].ip_port.vid[...])%2^32)
2. 分配x，x = find(ring, hash(key) < ring[x])


分析:
- 时间复杂度：因为调度过程是在有序的集合中查找节点，所以，可以采用二分查找，查找的节点数为n，每个节点的虚拟节点数为v，所以时间复杂度为O(log(nv))

实现:


参考资料：

<https://writings.sh/post/consistent-hashing-algorithms-part-2-consistent-hash-ring>

#### 1.8.3. 二次取模

原理:

使用所有节点对请求key的哈希取模，获得调度节点，当遇到被调度的节点不可用时候，再对使用所有**可用**节点对其key的哈希取模，获得被调度的节点

定义：

- 松散表（loose tabel）：所有节点表
- 紧实表（compact tabel）：所有可用节点表


步骤:

1. x=hash(key)%len(loose_tabel)
2. when x is available then return x
3. x=hash(key)%len(compacct_tabel)
4. 分配x

分析:


#### 1.8.4. 最高随机权重

原理:

计算key与所有node的hash，选择能获得最大hash值的node作为分配节点

定义：

步骤:

1. x=max(hash(key, node1), hash(key, node2), hash(key, node3))
2. 分配x

分析:

- 分配的一致性, 由hash(key,node)是一个确定值来保证
- 当某个节点不可用时: 其上的key会被分散到其他节点
- 当新增节点时: 会从其他节点转一个一部分key过来,
- 时间复杂度: O(n)


#### 1.8.5. Jump consistent hash


原理:

随机函数在种子相同的情况, 能够产生的相同的随机数序列, 利用这一性质, 以key做为种子, 产生r:(0,1)的随机数字序列, 再对随机数序的倒数列求积, 直到积不在小于所有节点数为止, 获得积即为要分配的节点


定义:


步骤:

1. x=(x_last+1)/random
2. when x < len(node) 重复 1 ~ 2
3. 分配x


分析:

- 某个key分配的一致性, 由随机数种子来保证
- 当某个节点不可用时, 其上的key, 将被分配到下一个node上; 又因为, len(node)会减1, 所以, 之前分配给最后一个node的key将被转移到其他不确定节点
- 当新增节点时, 如果新增在末尾, 由于len(node)会加1, 所以, 会从其他节点转移key过来, 如果新增节点不在末尾, 则从此节点开始的位置起, 他们上的key都被分配给了上一个节点
- 时间复杂度:

实现:

```
int ch(int key, int num_buckets) {
    random.seed(key) ;
    int b = -1; //  bucket number before the previous jump
    int j = 0; // bucket number before the current jump
    while(j < num_buckets){
        b = j;
        double r = random.next(); // 0<r<1.0, r must not equle 0 
        j = floor( (b+1) / r); // add 1 is because b may be equle 0     
    }
    return b;
}

// 上述算法的一个变种，只是将随机函数通过线性同余的方式改造而来的。
int JumpConsistentHash(unsigned long long key, int num_buckets)
{
    long long  b = -1, j = 0;
    while (j < num_buckets) {
        b = j;
        key = key * 2862933555777941757ULL + 1;
        j = (b + 1) * (double(1LL << 31) / double((key >> 33) + 1));
    }
    return b;
}
```

## 2. 不放回随机抽样算法

不放回随机抽样即从 n 个数据中抽取 m 个不重复的数据

### 2.1. Knuth 洗牌抽样

原理:

1. 从后往前遍历数组，交换当前位置与之之前的随机位置

步骤:

1. 从后往前遍历数组，交换当前位置与之之前的随机位置


分析:

- 会修改原始数组，或者需要copy原始数组
- 时间复杂度：O(n)


实现:

```go
func Perm(arr []int) []int {
	n := len(arr)
	for i := n - 1; i >= 0; i-- {
		r := 0
		if i > 0 {
			r = rand.Intn(i)
		}
		arr[i], arr[r] = arr[r], arr[i]
	}
	return arr
}

```



### 2.2. 占位洗牌随机抽样

运用占位洗牌算法实现的随机抽样的方式称为占位洗牌随机抽样

原理:

挑选随机位置插牌，在 Knuth 洗牌算法的基础上，不对原数组进行交换操作，而是通过一个额外的 map 来记录元素间的交换关系，我们称为占位洗牌算法。


步骤:

分析:

实现:

```go
func Perm2(arr []int) []int {
	mp := make(map[int]int)
	ret := []int{}
	for i := len(arr) - 1; i >= 0; i-- {
		r := 0
		if i > 0 {
			r = rand.Intn(i)
		}

		x := 0
		if _, ok := mp[r]; ok {
			x = mp[r]
		} else {
			x = arr[r]
		}

		ret = append(ret, x)

		if _, ok := mp[i]; ok {
			mp[r] = mp[i]
		} else {
			mp[r] = arr[i]
		}
	}

	return ret
}
```


### 未知算法

步骤:

1. 维护一个随机不重复元素的组数
2. 元素按照顺序添加到数组末尾
3. 先从已初始化的数组中挑选一个位置x，将ｘ与数组末尾数字交换


分析:

- 不需要修改原始数组，或者copy原始数组
- 时间复杂度：O(m)



实现:

```go
func Perm(n int) []int {
        m := make([]int, n)
        for i := 0; i < n; i++ {
                x := int(fastrand.Uint32n(uint32(i + 1)))
                m[i] = m[x] // m[x]不重复，x随机
                m[x] = i    // i不重复, x随机
        }
        return m
}
```

### 2.3. 选择抽样技术抽样

### 2.4. 蓄水池抽样

### 2.5. 随机分值排序抽样

原理:
给候n个元素的选集合随机打分，返回排名前m的元素

步骤:
实现:

### 2.6. 朴素的带权重抽样
原理:

类似于[1.5. 加权随机](), 因为是不放回实验，所以需要将每次选取的元素，从原始集合中剔除，然后进行下一次实验

步骤:

实现:

分析:

- 时间复杂度: O(m*n)
- 会修原始数组


### 2.7. 带权重的 A-Res 算法蓄水池抽样

原理:

蓄水池抽样算法解决的核心问题时，在不确定大小的数据集中抽样，并保证公平性。比如，数据集的大小是N（抽样结束前不确定），抽样m，每个元素的被抽到的概率都是m/N

A-Res（Algorithm A With a Reservoir） 是蓄水池抽样算法的带权重版本，算法主体思想与经典蓄水池算法一样都是维护含有 m 个元素的结果集，对每个新元素尝试去替换结果集中的元素。同时它巧妙的利用了随机分值排序算法抽样的思想，在对数据做随机分值的时候结合数据的权重大小生成排名分数，以满足分值与权重之间的正相关性，而这个 A-Res 算法生成随机分值的公式就是：


$$k_i=u_i^{{1 \over w^i}}$$


其中 $w_i$  为第 i 个数据的权重值， $u_i$是从（0,1]之间的一个随机值。

步骤:

1. 对于前 m 个数, 计算特值$k_i$，直接放入蓄水池中
2. 对于从 m+1,m+2,...,n 的第 i 个数，通过公式 $k_i=u_i^{{1 \over w^i}}$计算特征值$k_i$，如若特征值超过蓄水池中最小值，则替换最小值


实现:

```python
import heapq
import random

def a_res(samples, m):
    """
    :samples: [(item, weight), ...]
    :k: number of selected items
    :returns: [(item, weight), ...]
    """

    heap = [] # [(new_weight, item), ...]
    for sample in samples:
        wi = sample[1]
        ui = random.uniform(0, 1)
        ki = ui ** (1/wi)

        if len(heap) < m:
            heapq.heappush(heap, (ki, sample))
        elif ki > heap[0][0]:
            heapq.heappush(heap, (ki, sample))

            if len(heap) > m:
                heapq.heappop(heap)

    return [item[1] for item in heap]
```

分析:

- 时间复杂度： O(m*log(n/m))
- 不会修改原始数组


### 2.8. 带权重的 A-ExpJ 算法蓄水池抽样

原理:

A-Res 需要对每个元素产生一个随机数，而生成高质量的随机数有可能会有较大的性能开销，《Weighted random sampling with a reservoir》论文中给出了一种更为优化的指数跳跃的算法 A-ExpJ 抽样（Algorithm A with exponential jumps），它能将随机数的生成量从O(n)减少到 O(m\*log(n/m))，原理类似于通过一次额外的随机来跳过一段元素的特征值$k_i$的计算。

步骤:

实现:


```python
function aexpj_weight_sampling(data_array, weight_array, n, m)
    local result, rank = {}, {}
    for i=1, m do
        local rand_score = math.random() ^ (1 / weight_array[i])
        local idx = binary_search(rank, rand_score)
        table.insert(rank, idx, {score = rand_score, data = data_array[i]})
    end

    local weight_sum, xw = 0, math.log(math.random()) / math.log(rank[m].score)
    for i=m+1, n do
        weight_sum = weight_sum + weight_array[i]
        if weight_sum >= xw then
            local tw = rank[m].score ^ weight_array[i]
            local rand_score = (math.random()*(1-tw) + tw) ^ (1 / weight_array[i])
            local idx = binary_search(rank, rand_score)
            table.insert(rank, idx, {score = rand_score, data = data_array[i]})
            table.remove(rank)
            weight_sum = 0
            xw = math.log(math.random()) / math.log(rank[m].score)
        end
    end

    for i=1, m do
        result[i] = rank[i].data
    end

    return result
end

```


分析:



## 3. 排序算法
### 3.1. 基础排序
#### 3.1.1. 冒泡排序

原理:

相临两个元素比较，前者较大则交换。最终将最大者，移到数组末尾。

分析:

时间复杂度：O(n^2）

#### 3.1.2. 插入排序

原理:

从第二个元素开始，向开头位置冒泡，使得最左边为较小元素。然后依次再从第三个元素，向左冒泡

分析:

时间复杂度：O(n^2）


#### 3.1.3. 选择排序

原理:

从数组中挑选最小的元素，放在数组第一位置，再从生于数据挑选最小的元素，放在数组第二个位置，依次类推。

分析:

时间复杂度：O(n^2）

#### 3.1.4. 快速排序

原理:

通过左右指针，从数组两端开始，逐个与基准元素比较，如果大于基准元素，则放到基准之后，小于则放到基准之前

步骤:

1. 选择第一个元素为基准，定义左右指针，分别指向首尾
2. 从后往前找，找到小于基准的元素，移动到左边，左边指针右移
3. 从前往后找，找到大于基准的元素，移动到右边，右边指针左移
4. 运行以上两部，直到左右指针相遇，计为中点
5. 将基准元素，放到中点位置
6. 中点左，右两边分别做执行快速排序

实现:

```go
func QuickSort(v []int, low, high int) {
    fmt.Println(v, low, high)

    if low >= high {
        return
    }
    first := low
    last := high
    key := v[low]

    for first < last {
        for first < last && v[last] >= key {
            last--
        }

        if first < last {
            v[first] = v[last]
            first++
        }

        for first > last && v[first] <= key {
            first++
        }

        if first < last {
            v[last] = v[first]
            last--
        }

        fmt.Println(v)
    }

    v[first] = key

    fmt.Println(v)
    fmt.Println("")

    QuickSort(v, low, first-1)
    QuickSort(v, first+1, high)

}

```
#### 3.1.5. 归并排序

原理:

第一步分割：递归地把当前序列平均分割成两半，直到剩下一个元素
第二步集成：按照元素顺序，将上一步得到的子序列集成到一起（归并），得到较长的有序子序列，直到继承完所有元素

![picc](https://mmbiz.qpic.cn/mmbiz_jpg/j3gficicyOvasuwkLUmCKAuKF3urWh30Sad9ic3smFWA5SYloAfiakDuKqfcokh4ro3Wj32rePPbWDTQW5zS2icA8Qg/640?wx_fmt=jpeg&tp=webp&wxfrom=5&wx_lazy=1&wx_co=1)

#### 3.1.6. 堆排序

原理:

构建大端树：a[2i+1] >= a[i] and a[2i+2] >= a[i], 将最顶端元素和末尾未排序元素交换，增加一个已排序元素，构造未知排序元素的大端树，重复以上过程

![pic](https://mmbiz.qpic.cn/mmbiz_jpg/j3gficicyOvasuwkLUmCKAuKF3urWh30Sa46XtiapqHUNmq5ZFIAX5591TMSYFfMThWIYIqTyA0z3e7WKFwcWRzHQ/640?wx_fmt=jpeg&tp=webp&wxfrom=5&wx_lazy=1&wx_co=1)

步骤:

1. 构建大端二叉树
    1. 父节点i的左子节点在位置(2i+1);
    2. 父节点i的右子节点在位置(2i+2);
    3. 子节点i的父节点在位置 floor((i-1)/2);
    4. 保证父节点大于子节点
2. 交换第一个元素和末尾元素。的到最大值
3. 构建去除末尾元素的大端全二叉树
4. 交换第一个元素和次末尾元素。的到次大值


实现:

```go
func HeapSort(array []int) {
    m := len(array)
    s := m / 2
    for i := s; i > -1; i-- {
        heap(array, i, m-1)
    }
    for i := m - 1; i > 0; i-- {
        array[i], array[0] = array[0], array[i]
        heap(array, 0, i-1)
        fmt.Println(array)
    }
}

func heap(array []int, i, end int) {
    l := 2*i + 1
    if l > end {
        return
    }
    n := l
    r := 2*i + 2
    if r <= end && array[r] > array[l] {
        n = r
    }
    if array[i] > array[n] {
        return
    }
    array[n], array[i] = array[i], array[n]
    heap(array, n, end)
}
```

#### 3.1.7. 希尔排序
### 3.2. 分配排序
#### 3.2.1. 计数排序
#### 3.2.2. 桶排序
#### 3.2.3. 基数排序
### 3.3. 多路归并排序
### 3.4. 跳跃表排序
### 3.5. 百分比近似排序
#### 3.5.1. HdrHistogram 算法
#### 3.5.2. CKMS 算法
#### 3.5.3. TDigest 算法
原理:
步骤:
实现:
分析:



## 4. 限流与过载保护
### 4.1. 计数器

原理:

在时间窗口内对请求计数，达到阈值后限制，下一个时间窗口开始时计数清0，重新计数

步骤:

实现:

分析:

- 当请求集中在时间窗口切换附近时，会出现两倍于阈值的请流量
- 流量锯齿严重


### 4.2. 漏桶

原理:

所有水（请求）都会先丢进漏桶中，漏桶则以固定的速率出水（处理请求），当请求量速率过大，水桶中的水则会溢出（请求被丢弃）

![img](https://mmbiz.qpic.cn/mmbiz_jpg/j3gficicyOvasuwkLUmCKAuKF3urWh30Sa5F1a0NFYFA4LSRiaxCjxm9VB5nk3icfKV8Sj4ct6Tw8D5riaFgMz653BQ/640?wx_fmt=jpeg&tp=webp&wxfrom=5&wx_lazy=1&wx_co=1)

步骤:

实现:

分析:
- 能够保证固定的速率速率


### 4.3. 令牌桶


原理:

系统会以一个恒定的速度往桶里放入令牌，而如果请求需要被处理，则需要先从桶里获取一个令牌，当桶里没有令牌可取时，则拒绝服务。

![img](https://mmbiz.qpic.cn/mmbiz_jpg/j3gficicyOvasuwkLUmCKAuKF3urWh30SaTseml70libXYU3Y4TWlMvJ2huicBbKrgs4ibL6PggwR4Y5ASicicKM5GDmA/640?wx_fmt=jpeg&tp=webp&wxfrom=5&wx_lazy=1&wx_co=1)

步骤:

实现:

分析:
- 能够保证固定的速率速率，并且，允许某种程度的突发请求量，


### 4.4. 滑动窗口

原理:

滑动窗口限频类似于 TCP 的滑动窗口协议，设置一个窗口大小，这个大小即当前最大在处理中的请求量，同时记录滑动窗口的左右端点，每次发送一个请求时滑动窗口右端点往前移一格，每次收到请求处理完毕响应后窗口左端点往前移一格，当右端点与左端点的差值超过最大窗口大小时，等待发送或拒绝服务。

![img](https://mmbiz.qpic.cn/mmbiz_jpg/j3gficicyOvasuwkLUmCKAuKF3urWh30SaxDicOq4prAUDnVdxMTtXhuWLwYUBK4Cml1dLjwRPQYcUgYyib2icgB8cA/640?wx_fmt=jpeg&tp=webp&wxfrom=5&wx_lazy=1&wx_co=1)

步骤:

实现:

分析:
- 能投获得下游服务处理能力的反馈，所有，不需要设置固定流量阈值



### 4.5. SRE 自适应限流

原理:

SRE 自适应限流相当于是一个动态的窗口，它根据过往请求的成功率动态调整向后端发送请求的速率，当成功率越高请求被拒绝的概率就越小；反之，当成功率越低请求被拒绝的概率就相应越大.

SRE 自适应限流算法需要在应用层记录过去两分钟内的两个数据信息：

1. requests：请求总量，应用层尝试的请求数
2. accepts：成功被后端处理的请求数

请求被拒绝的概率 p 的计算公式如下：

p = max(0, (requests-k\*accepts)/(requests+1))

当p=0时，放行请求，否则拒绝

步骤:


实现:


分析:



### 4.6. 熔断
原理:

熔断算法原理是系统统计并定时检查过往请求的失败（超时）比率，当失败（超时）率达到一定阈值之后，熔断器开启，并休眠一段时间，当休眠期结束后，熔断器关闭，重新往后端节点发送请求，并重新统计失败率。如此周而复始。

如下图所示：

![img](https://mmbiz.qpic.cn/mmbiz_jpg/j3gficicyOvasuwkLUmCKAuKF3urWh30SajD1e0uCHzIO8UdxZO7y9956ibyHCkKdkPN4ESUyibftpwLX5EA4mP43Q/640?wx_fmt=jpeg&tp=webp&wxfrom=5&wx_lazy=1&wx_co=1)


步骤:

实现:

分析:


### 4.7. Hystrix 半开熔断器
原理:

Hystrix 中的半开熔断器相对于简单熔断增加了一种半开状态，Hystrix 在运行过程中会向每个请求对应的节点报告成功、失败、超时和拒绝的状态，熔断器维护计算统计的数据，根据这些统计的信息来确定熔断器是否打开。如果打开，后续的请求都会被截断。然后会隔一段时间，尝试半开状态，即放入一部分请求过去，相当于对服务进行一次健康检查，如果服务恢复，熔断器关闭，随后完全恢复调用，如果失败，则重新打开熔断器，继续进入熔断等待状态。

如下图所示：

![img](https://mmbiz.qpic.cn/mmbiz_jpg/j3gficicyOvasuwkLUmCKAuKF3urWh30SaQluT1Lawj0VvlzWfE9d87tn2yWhJ8oJwjA5TIcAQ0KPmkP4XEcyqew/640?wx_fmt=jpeg&tp=webp&wxfrom=5&wx_lazy=1&wx_co=1)


步骤:

实现:

分析:


## 5. 序列化与编码
### 5.1. 标记语言
### 5.2. TLV 二进制序列化
### 5.3. 压缩编码
### 5.4. 其他编码


## 6. 加密与校验
### 6.1. CRC
原理:

步骤:

实现:

分析:

### 6.2. 奇偶校验
原理:

步骤:

实现:

分析:

### 6.3. MD 系列
原理:

步骤:

实现:

分析:

### 6.4. SHA 系列
原理:

步骤:

实现:

分析:

### 6.5. 对称密钥算法
原理:

步骤:

实现:

分析:

### 6.6. 非对称加密算法
原理:

步骤:

实现:

分析:

### 6.7. 哈希链



## 7. 缓存淘汰策略
### 7.1. FIFO

FIFO（First In First Out）是一种先进先出的数据缓存器，先进先出队列很好理解，当访问的数据节点不在缓存中时，从后端拉取节点数据并插入在队列头，如果队列已满，则淘汰最先插入队列的数据。

假设缓存队列长度为 6，过程演示如下：

![pic](https://mmbiz.qpic.cn/mmbiz_jpg/j3gficicyOvasuwkLUmCKAuKF3urWh30Sa0dA3tu6K7fGVpmsPKBEvvpre1E2bSmsx2bGqQjV7sLJb0cia7UouGzg/640?wx_fmt=jpeg&tp=webp&wxfrom=5&wx_lazy=1&wx_co=1)
    

### 7.2. LRU


LRU（Least recently used）是最近最少使用缓存淘汰算法，可它根据数据的历史访问记录来进行淘汰数据，其核心思想认为最近使用的数据是热门数据，下一次很大概率将会再次被使用。而最近很少被使用的数据，很大概率下一次不再用到。因此当缓存容量的满时候，优先淘汰最近很少使用的数据。因此它与 FIFO 的区别是在访问数据节点时，会将被访问的数据移到头结点。

假设缓存队列长度为 6，过程演示如下：

![pic](https://mmbiz.qpic.cn/mmbiz_jpg/j3gficicyOvasuwkLUmCKAuKF3urWh30SaYMpY6oicY1UUoUPWayXwFYqewE6F3TspeIWBKFahEsgv1bmtIJGm7gQ/640?wx_fmt=jpeg&tp=webp&wxfrom=5&wx_lazy=1&wx_co=1)


LRU 算法有个缺陷在于对于偶发的访问操作，比如说批量查询某些数据，可能使缓存中热门数据被这些偶发使用的数据替代，造成缓存污染，导致缓存命中率下降。



### 7.3. LFU


LFU 是最不经常使用淘汰算法，其核心思想认为如果数据过去被访问多次，那么将来被访问的频率也更高。LRU 的淘汰规则是基于访问时间，而 LFU 是基于访问次数。LFU 缓存算法使用一个计数器来记录数据被访问的次数，最低访问数的条目首先被移除。

假设缓存队列长度为 4，过程演示如下：

![pic](https://mmbiz.qpic.cn/mmbiz_jpg/j3gficicyOvasuwkLUmCKAuKF3urWh30SaDQUGVWwlogeFLRreBbCM4S9Ml3dsLaGDHFlibBuXV4ndDNaD8VOibwvw/640?wx_fmt=jpeg&tp=webp&wxfrom=5&wx_lazy=1&wx_co=1)

LFU 能够避免偶发性的操作导致缓存命中率下降的问题，但它也有缺陷，比如对于一开始有高访问率而之后长时间没有被访问的数据，它会一直占用缓存空间，因此一旦数据访问模式改变，LFU 可能需要长时间来适用新的访问模式，即 LFU 存在历史数据影响将来数据的"缓存污染"问题。另外对于对于交替出现的数据，缓存命中不高。



### 7.4. LRU-K

原理:


无论是 LRU 还是 LFU 都有各自的缺陷，LRU-K 算法更像是结合了 LRU 基于访问时间和 LFU 基于访问次数的思想，它将 LRU 最近使用过 1 次的判断标准扩展为最近使用过 K 次，以提高缓存队列淘汰置换的门槛。LRU-K 算法需要维护两个队列：访问列表和缓存列表。LRU 可以认为是 LRU-K 中 K 等于 1 的特化版。

步骤:

1. 数据第一次被访问，加入到访问列表，访问列表按照一定规则（如 FIFO，LRU）淘汰。
2. 当访问列表中的数据访问次数达到 K 次后，将数据从访问列表删除，并将数据添加到缓存列表头节点，如果数据已经在缓存列表中，则移动到头结点。
3. 若缓存列表数据量超过上限，淘汰缓存列表中排在末尾的数据，即淘汰倒数第 K 次访问离现在最久的数据。

假设访问列表长度和缓存列表长度都为 4，K=2，过程演示如下：

![pic](https://mmbiz.qpic.cn/mmbiz_jpg/j3gficicyOvasuwkLUmCKAuKF3urWh30Sa2PrD4hQpNmU0BABrPziaJohicyUSLiaYqsIyCeP7Mlc2UuFuwdiatLrnhg/640?wx_fmt=jpeg&tp=webp&wxfrom=5&wx_lazy=1&wx_co=1)

实现:

分析:

LRU-K 具有 LRU 的优点，同时能够降低缓存数据被污染的程度，实际应用可根据业务场景选择不同的 K 值，K 值越大，缓存列表中数据置换的门槛越高。



### 7.5. Two queues

Two queues 算法可以看做是 LRU-K 算法中 K=2，同时访问列表使用 FIFO 淘汰算法的一个特例。如下图所示：

![pic](https://mmbiz.qpic.cn/mmbiz_jpg/j3gficicyOvasuwkLUmCKAuKF3urWh30SaTRvJv9dwBia4kkDjzB7NiawHWMJQfuAOnicrgU2bockKv3xWRAWvUeLUA/640?wx_fmt=jpeg&tp=webp&wxfrom=5&wx_lazy=1&wx_co=1)

### 7.6. LIRS

LIRS（Low Inter-reference Recency Set）算法将缓存分为两部分区域：热数据区与冷数据区。LIRS 算法利用冷数据区做了一层隔离，目的是即使在有偶发性的访问操作时，保护热数据区的数据不会被频繁地被置换，以提高缓存的命中。

LIRS 继承了 LRU 根据时间局部性对冷热数据进行预测的思想，并在此之上 LIRS 引入了两个衡量数据块的指标：

IRR（Inter-Reference Recency）：表示数据最近两次访问之间访问其它数据的非重复个数
R （Recency）：表示数据最近一次访问到当前时间内访问其它数据的非重复个数，也就是 LRU 的维护的数据。
如下图，从左往右经过以下 8 次访问后，A 节点此时的 IRR 值为 3，R 值为 1。

![pic](https://mmbiz.qpic.cn/mmbiz_jpg/j3gficicyOvasuwkLUmCKAuKF3urWh30SaINdcs4SVhQibiceIOCt1J2fib1NCLuYCfmBaI5UQWmbhOat8YNgqPmP3A/640?wx_fmt=jpeg&tp=webp&wxfrom=5&wx_lazy=1&wx_co=1)


IRR 可以由 R 值计算而来，具体公式为：IRR=上一时刻的 R-当前时刻的 R，如上图当前时刻访问的节点是 F，那么当前时刻 F 的 R 值为 0，而上一个 F 节点的 R 值为 2，因此 F 节点的 IRR 值为 2。

LIRS 动态维护两个集合：

LIR（low IRR block set）：具有较小 IRR 的数据块集合，可以将这部分数据块理解为热数据，因为 IRR 低说明访问的频次高。
HIR（high IRR block set）：具有较高 IRR 的数据块集合，可以将这部分数据块理解为冷数据。
LIR 集合所有数据都在缓存中，而 HIR 集合中有部分数据不在缓存中，但记录了它们的历史信息并标记为未驻留在缓存中，称这部分数据块为 nonresident-HIR，另外一部分驻留在缓存中的数据块称为 resident-HIR。

LIR 集合在缓存中，所以访问 LIR 集合的数据是百分百会命中缓存的。而 HIR 集合分为 resident-HIR 和 nonresident-HIR 两部分，所以会遇到未命中情况。当发生缓存未命中需要置换缓存块时，会选择优先淘汰置换 resident-HIR。如果 HIR 集合中数据的 IRR 经过更新比 LIR 集合中的小，那么 LIR 集合数据块就会被 HIR 集合中 IRR 小的数据块挤出并转换为 HIR。

LIRS 通过限制 LIR 集合的长度和 resident-HIR 集合长度来限制整体大小，假设设定 LIR 长度为 2，resident-HIR 长度为 1 的 LIRS 算法过程演示如下：

![pic](https://mmbiz.qpic.cn/mmbiz_jpg/j3gficicyOvasuwkLUmCKAuKF3urWh30SaMwsPicMUWGz3bDQeNJypyMeBvfwyibhsIsibdZdGibAAUn19Vzgll9jcIQ/640?wx_fmt=jpeg&tp=webp&wxfrom=5&wx_lazy=1&wx_co=1)

1. 所有最近访问的数据都放置在称为 LIRS 堆栈的 FIFO 队列中（图中的堆栈 S），所有常驻的 resident-HIR 数据放置在另一个 FIFO 队列中（图中的堆栈 Q）。
2. 当栈 S 中的一个 LIR 数据被访问时，被访问的数据会被移动到堆栈 S 的顶部，并且堆栈底部的任何 HIR 数据都被删除，因为这些 HIR 数据的 IRR 值不再有可能超过任何 LIR 数据了。例如，图（b）是在图（a）上访问数据 B 之后生成的。
3. 当栈 S 中的一个 resident-HIR 数据被访问时，它变成一个 LIR 数据，相应地，当前在栈 S 最底部的 LIR 数据变成一个 HIR 数据并移动到栈 Q 的顶部。例如，图（c）是在图（a）上访问数据 E 之后生成的。
4. 当栈 S 中的一个 nonresident-HIR 数据被访问时，它变成一个 LIR 数据，此时将选择位于栈 Q 底部的 resident-HIR 数据作为替换的牺牲品，降级为 nonresident-HIR，而栈 S 最底部的 LIR 数据变成一个 HIR 数据并移动到栈 Q 的顶部。例如，图（d）是在图（a）上访问数据 D 之后生成的。
5. 当访问一个不在栈 S 中的数据时，它会成为一个 resident-HIR 数据放入栈 Q 的顶部，同样的栈 Q 底部的 resident-HIR 数据会降级为 nonresident-HIR。例如，图（e）是在图（a）上访问数据 C 之后生成的。

解释一下当栈 S 中的一个 HIR 数据被访问时，它为什么一定会变成一个 LIR 数据：这个数据被访问时，需要更新 IRR 值（即为当前的 R 值），使用这个新的 IRR 与 LIR 集合数据中最大的 R 值进行比较（即栈 S 最底部的 LIR 数据），新的 IRR 一定会比栈 S 最底部的 LIR 数据的 IRR 小（因为栈 S 最底部的数据一定是 LIR 数据，步骤 2 已经保证了），所以它一定会变成一个 LIR 数据。


### 7.7. MySQL InnoDB LRU

原理:

MySQL InnoDB 中的 LRU 淘汰算法采用了类似的 LIRS 的分级思想，它的置换数据方式更加简单，通过判断冷数据在缓存中存在的时间是否足够长（即还没有被 LRU 淘汰）来实现。数据首先进入冷数据区，如果数据在较短的时间内被访问两次或者以上，则成为热点数据进入热数据区，冷数据和热数据部分区域内部各自还是采用 LRU 替换算法。

步骤:

1. 访问数据如果位于热数据区，与 LRU 算法一样，移动到热数据区的头结点。
2. 访问数据如果位于冷数据区，若该数据已在缓存中超过指定时间，比如说 1s，则移动到热数据区的头结点；若该数据存在时间小于指定的时间，则位置保持不变。
3. 访问数据如果不在热数据区也不在冷数据区，插入冷数据区的头结点，若冷数据缓存已满，淘汰尾结点的数据。



## 8. 基数集与基数统计

基数集即不重复元素的集合，基数统计即统计基数集中元素的个数

### 8.1. 哈希表

使用哈希表统计基数值即将所有元素存储在一个哈希表中，利用哈希表对元素进行去重，并统计元素的个数，这种方法可以精确的计算出不重复元素的数量。

但使用哈希表进行基数统计，需要存储实际的元素数据，在数据量较少时还算可行，但是当数据量达到百万、千万甚至上亿时，使用哈希表统计会占用大量的内存，同时它的查找过滤成本也很高。

>常见的散列函数算法有：
>
>- 直接寻址法，
>- 数字分析法，
>- 平方取中法，
>- 折叠法，
>- 随机数法，
>- 除留余数法
>
>常见的冲突解决算法有：
>
>- 开放定址法
>- 拉链法
>- 再哈希法
>- 建立公共溢出区
>
### 8.2. 位图（Bitmap）

位图就是用每一比特位来存放真和假状态的一种数据结构。位图上某个索引上的值，可以代表该索引对应的数据的真假状态

使用位图进行基数统计不需要去存储实际元素信息，只需要用相应位置的 1bit 来标识某个元素是否出现过，这样能够极大地节省内存。



### 8.3. 布隆过滤器

原理:

用多个哈希函数将数据映射到位图的多个点上，

步骤:

实现:

分析:

### 8.4. 布谷鸟过滤器

原理:

使用两个哈希函数计算出两个哈希

它的思想来源于布谷鸟“鸠占鹊巢”的生活习性。布谷鸟哈希算法会有两个散列函数将元素映射到哈希表的两个不同位置。如果两个位置中有一个位置为空，那么就可以将元素直接放进去。但是如果这两个位置都满了，它就随机踢走一个，然后自己霸占了这个位置。

被踢走的那个元素会去查看它的另外一个散列值的位置是否是空位，如果是空位就占领它，如果不是空位，那就把受害者的角色转移出去，挤走对方，让对方再去找安身之处，如此循环直到某个元素找到空位为止。

布谷鸟哈希算法需要存储数据的整个元素信息，而布谷鸟过滤器为了减少内存，将存储的元素信息映射为一个简单的指纹信息，例如微信的用户 id 大小需要 8 字节，我们可以将它映射为 1 个字节甚至几个 bit 的指纹信息来进行存储。

布谷鸟过滤器的bucket中，同一个位置存储4个指纹，这样的好处是：1. 增大bucket同一位置的容量，在hash冲突的时候减少已存元素被踢出的概率。2. 有利于cpu缓存的使用

定义:

1. 布谷鸟过滤器中存储的是key的指纹，目的是减少内存使用，计算指纹的方法为：通过对key计算哈希之后，取其中的固定几位，后面通过f(x)代表
2. key的索引位置通过下面函数计算：i1=hash(x); i2=i1^hash(f(x))
3. b为存储指纹的bucket
4. x为要插入的key



插入步骤:

1. 计算出两个插入位置i1=hash(x); i2=i1^hash(f(x))
2. 如果b[i1]=null or b[i2]=null, 选择等于null的一个位置存下x的指纹f(x)
3. 否则，踢出b[i1]中的f(y), 在b[i1]存下f(x)
4. 计算f(y)的两个位置，j1=i1^hash(f(y))，j2=j1^hash(f(y))
5. 重复2～4


查找步骤:

1. 计算key的指纹f(x)和位置i1=hash(x); i2=i1^hash(f(x))
2. 如果b[i1]=f(x) or b[i2]=f(x)，则返回true
3. 否则返回false

删除步骤:

略

实现:

<https://github.com/efficient/cuckoofilter>


参考文章:

- <http://www.linvon.cn/posts/cuckoo/>
- <https://www.cnblogs.com/zhaodongge/p/15067657.html>
- <https://www.qtmuniao.com/2021/12/07/cuckoo-hash-and-cuckoo-filter/>


### 8.5. HyperLogLog

说到基数统计，就不得不提 Redis 里面的 HyperLogLog 算法了，前文所说的哈希表，位图，布隆过滤器和布谷鸟过滤器都是基于记录元素的信息并通过过滤（或近似过滤）相同元素的思想来进行基数统计的。

而 HyperLogLog 算法的思想不太一样，它的基础是观察到可以通过计算集合中每个数字的二进制表示中的前导零的最大数目来估计均匀分布的随机数的多重集的基数。如果观察到的前导零的最大数目是 n，则集合中不同元素的数量的估计是n^2 。

怎么理解呢？其实就是运用了数学概率论的理论，以抛硬币的伯努利试验为例，假设一直尝试抛硬币，直到它出现正面为止，同时记录第一次出现正面时共尝试的抛掷次数 k，作为一次完整的伯努利试验。那么对于 n 次伯努利试验，假设其中最大的那次抛掷次数为 。结合极大似然估算的方法，n 和  中存在估算关联关系即：。

对应于基数统计的场景，HyperLogLog 算法通过散列函数，将数据转为二进制比特串，从低位往高位看，第一次出现 1 的时候认为是抛硬币的正面，因此比特串中前导零的数目即是抛硬币的抛掷次数。因此可以根据存入数据中，转化后的二进制串集中最大的首次出现 1 的位置  来估算存入了多少不同的数据。

这种估算方式存在一定的偶然性，比如当某次抛硬币特别不幸时，抛出了很大的值，数据会偏差的厉害，为了降低这种极端偶然性带来的误差影响，在 HyperLogLog 算法中，会将集合分成多个子集（分桶计算），分别计算这些子集中的数字中的前导零的最大数量，最后使用调和平均数的计算方式将所有子集的这些估计值计算为全集的基数。例如 redis 会分为 16384 个子集进行分桶求平均统计。




定义:

- e为集合中的元素
- hash(e)计算e的哈希值
- p(x)为x的二进制表示中前导连续0的数量


原理:

已知集合中已存在的某个元素，及其在此集合中出现的概率，可以估计出集合的大概大小。基于这个方法，将要集合中的元素计算哈希值hash(e), 找得到hesh(e)的二进制表示的前导连续零个数的最大值，计为M=max(p(hash(e))), 则估计集合的大小是$2^{max(p(hash(e)))}$。这种估计方法固然存在偶然性，解决办法是，将元素分成多个桶来统计，然后计算均值，在使用均值乘以同来的数量，最后估计出总元素数量，HyperLogLog使用的是调和平均

步骤:

1. 将元素分成m个桶
2. 每个桶中计算元素的哈希hash(e)
3. 每个桶中找到hash(e)的二进制表示的前导连续零个数的最大值，计为 $M_j=max(p(hash(e)))$
4. 估算每个桶中不重复元素数的调和平均值为：$\frac{m} {\sum_{j=1}^m2^{-M_j}}$
5. 估算集中不重复元素的个数为：$\frac{m^2} {\sum_{j=1}^m2^{-M_j}}$


实现:

<http://www.xiaocc.xyz/2019-12-16/HyperLogLog%E5%8E%9F%E7%90%86%E5%88%86%E6%9E%90/>

分析:


参考资料:

- <https://zhuanlan.zhihu.com/p/141344814>
- <http://algo.inria.fr/flajolet/Publications/FlFuGaMe07.pdf>


## 9. 其他常用算法
### 9.1. 时间轮定时器

原理:

时间轮定时器将按照到期时间分桶放入缓存队列中，系统只需按照每个桶到期顺序依次执行到期的时间桶节点中的所有定时任务。如下图所示：

![pic](https://mmbiz.qpic.cn/mmbiz_jpg/j3gficicyOvasuwkLUmCKAuKF3urWh30SagbnOIBNFe0R7WyUoxZ1h9w3xicaYEMT7srCRT42iaS7qSTNicLWibBVr4g/640?wx_fmt=jpeg&tp=webp&wxfrom=5&wx_lazy=1&wx_co=1)


而针对定时任务时间跨度大，且精度要求较高的场景，使用单层时间轮消耗的内存可能比较大，因此还可以进一步优化为采用层级时间轮来实现，层级时间轮就类似我们的时钟，秒针转一圈后，分针才进一格的原理，当内层时间轮运转完一轮后，外层时间轮进一格，接下来运行下一格的内层时间轮。


步骤:

实现:

分析:

时间轮定时器是一种插入，运行和删除都比较理想的定时器。


### 9.2. 红包分配

原理:

以剩余单个红包的平均金额的2倍为上限，随机本次分配的金额。

步骤:

1. pkg=people_remain > 1 ? random(0, money_remain / people_remain * 2) : money_remain
2. money_remain-=pkg
3. people_remain-\-


实现:

分析:




## 参考文章
- <https://mp.weixin.qq.com/s/fcaQJxlBaP6mgeHIM5gTog>