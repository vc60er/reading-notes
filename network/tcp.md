# tcp

<!-- MarkdownTOC autolink="true" -->

- [TCP头格式](#tcp%E5%A4%B4%E6%A0%BC%E5%BC%8F)
- [TCP的状态机](#tcp%E7%9A%84%E7%8A%B6%E6%80%81%E6%9C%BA)
- [数据传输中的Sequence Number](#%E6%95%B0%E6%8D%AE%E4%BC%A0%E8%BE%93%E4%B8%AD%E7%9A%84sequence-number)
- [TCP重传机制](#tcp%E9%87%8D%E4%BC%A0%E6%9C%BA%E5%88%B6)
	- [超时重传机制](#%E8%B6%85%E6%97%B6%E9%87%8D%E4%BC%A0%E6%9C%BA%E5%88%B6)
	- [快速重传机制](#%E5%BF%AB%E9%80%9F%E9%87%8D%E4%BC%A0%E6%9C%BA%E5%88%B6)
	- [SACK 方法](#sack-%E6%96%B9%E6%B3%95)
	- [Duplicate SACK – 重复收到数据的问题](#duplicate-sack-%E2%80%93-%E9%87%8D%E5%A4%8D%E6%94%B6%E5%88%B0%E6%95%B0%E6%8D%AE%E7%9A%84%E9%97%AE%E9%A2%98)
- [拥塞控制](#%E6%8B%A5%E5%A1%9E%E6%8E%A7%E5%88%B6)
	- [TCP的RTT算法](#tcp%E7%9A%84rtt%E7%AE%97%E6%B3%95)
		- [经典算法](#%E7%BB%8F%E5%85%B8%E7%AE%97%E6%B3%95)
		- [Karn/Partridge 算法](#karnpartridge-%E7%AE%97%E6%B3%95)
		- [Jacobson / Karels 算法](#jacobson--karels-%E7%AE%97%E6%B3%95)
	- [TCP滑动窗口-流控](#tcp%E6%BB%91%E5%8A%A8%E7%AA%97%E5%8F%A3-%E6%B5%81%E6%8E%A7)
		- [Zero Window](#zero-window)
		- [Silly Window Syndrome](#silly-window-syndrome)
	- [TCP的拥塞处理 – Congestion Handling](#tcp%E7%9A%84%E6%8B%A5%E5%A1%9E%E5%A4%84%E7%90%86-%E2%80%93-congestion-handling)
		- [慢热启动算法 – Slow Start](#%E6%85%A2%E7%83%AD%E5%90%AF%E5%8A%A8%E7%AE%97%E6%B3%95-%E2%80%93-slow-start)
		- [拥塞避免算法 – Congestion Avoidance](#%E6%8B%A5%E5%A1%9E%E9%81%BF%E5%85%8D%E7%AE%97%E6%B3%95-%E2%80%93-congestion-avoidance)
		- [拥塞状态时的算法](#%E6%8B%A5%E5%A1%9E%E7%8A%B6%E6%80%81%E6%97%B6%E7%9A%84%E7%AE%97%E6%B3%95)
		- [快速恢复算法 – Fast Recovery](#%E5%BF%AB%E9%80%9F%E6%81%A2%E5%A4%8D%E7%AE%97%E6%B3%95-%E2%80%93-fast-recovery)
			- [TCP Reno](#tcp-reno)
			- [TCP New Reno](#tcp-new-reno)
		- [FACK算法](#fack%E7%AE%97%E6%B3%95)
	- [其它拥塞控制算法简介](#%E5%85%B6%E5%AE%83%E6%8B%A5%E5%A1%9E%E6%8E%A7%E5%88%B6%E7%AE%97%E6%B3%95%E7%AE%80%E4%BB%8B)
		- [TCP Vegas 拥塞控制算法](#tcp-vegas-%E6%8B%A5%E5%A1%9E%E6%8E%A7%E5%88%B6%E7%AE%97%E6%B3%95)
		- [HSTCP\(High Speed TCP\) 算法](#hstcphigh-speed-tcp-%E7%AE%97%E6%B3%95)
		- [TCP BIC 算法](#tcp-bic-%E7%AE%97%E6%B3%95)
		- [TCP WestWood算法](#tcp-westwood%E7%AE%97%E6%B3%95)
- [参考文章](#%E5%8F%82%E8%80%83%E6%96%87%E7%AB%A0)

<!-- /MarkdownTOC -->


## TCP头格式
![pic](https://coolshell.cn/wp-content/uploads/2014/05/TCP-Header-01.jpg)

* SeqNum：数据包的发送序号，连接建立之初SeqNum不一定是0，SeqNum的增加是和传输的字节数相关的
* AckNum：接收到数据，最大的连续包的SeqNum+1
* Window：滑动窗口大小，接收端告诉发送端自己还有多少缓冲区可以接收数据


## TCP的状态机

![pic](https://coolshell.cn/wp-content/uploads/2014/05/tcp_open_close.jpg)


1. tcp是建立在不可靠网络之上的，这个网络上的传输也不存在连接，所谓连接其实是指通信双方维护的“连接状态”
2. tcp的三次握手完成了通信双方的数据序号的初始化（Inital Sequence Number），初始化过程为：
> 1. 发起方发送syn、seq给接收方
> 2. 接收方发送syn、seq，ack给发起方，确认收到对方的seq
> 3. 发起方ack, 确认收到对方的seq

> 注意：
> 
> * 前两个步骤，分别告诉对方己方的初始seq，第三步骤，发起方确认发收到接受方的初始化序号，因为tcp规定数据传输需要确认，所以第三步必不可缺。因为，第三步结束之后，连接双方都已经确认知道了对方的seq，也不会存在第四步骤

3. 四次挥手是为了连接的双方都能正确修改本地状态，释放资源（连接状态），步骤如下
> 1. 发起方发送fin, 接收方收之后进入close_wait状态
> 2. 接收方发送ack
> 3. 接收方发送fin, 发起方收到后进入time_wait状态
> 4. 发起方发送ack，

> 注意:
> 
> * 在步骤2完成之后，如果接收方的数据还没有发送结束，会等待发送结束后执行步骤3，这也是为什么步骤2和3不能合并在一起的原因
> * 步骤4当然不能缺少，因为需要确认已收到接收方的fin
> * 因为步骤4之后，不再有对方的ack，因此步骤4之后会等待两个MSL（RFC793定义了MSL为2分钟，Linux设置成了30s）。主要是为了：1）TIME_WAIT确保有足够的时间让对端收到了ACK，如果被动关闭的那方没有收到Ack，就会触发被动端重发Fin，一来一去正好2个MSL，2）有足够的时间让这个连接不会跟后面的连接混在一起。
> * 连接关闭、资源释放之后，意味着连接的4元组将会被用于新的连接


## 数据传输中的Sequence Number


## TCP重传机制
### 超时重传机制

一定时间内没收到包的ack后，则认为此包丢失，会重传丢失包及其后续的所有包

这种实现方式需要等待一个timeout时间，如果这个时间较长，会对效率造成很大影响，如果较短，则在网络缓慢的啥时候误报率较高。超时时间的计算见“TCP的RTT算法”


### 快速重传机制

丢包事件通过三次相同的ack来判断。实现方式是，如果接收方收到不连续的包，则会其及对后续收到的包都ack最后一个连续的包，发送方收到3个相同的ack之后，认为此包丢失。会重传丢失包及其后续的所有包

![pic](https://coolshell.cn/wp-content/uploads/2014/05/FASTIncast021.png)


### SACK 方法

Selective Acknowledgment (SACK)（参看RFC 2018），这种方式需要在TCP头里加一个SACK的东西，ACK还是Fast Retransmit的ACK，SACK则是汇报收到的数据碎版。这样，在发送端就可以根据回传的SACK来知道哪些数据到了，哪些没有到。


![pic](https://coolshell.cn/wp-content/uploads/2014/05/tcp_sack_example-1024x577.jpg)


### Duplicate SACK – 重复收到数据的问题

Duplicate SACK又称D-SACK，其主要使用了SACK来告诉发送方有哪些数据被重复接收了。

D-SACK使用了SACK的第一个段来做标志:

* 如果SACK的第一个段的范围被ACK所覆盖，那么就是D-SACK
* 如果SACK的第一个段的范围被SACK的第二个段覆盖，那么就是D-SACK


todo


## 拥塞控制
### TCP的RTT算法

RTT——Round Trip Time，也就是一个数据包从发出去到回来的时间，tcp通过计算RTT来获得重传的超时时间RTO（Retransmission TimeOut）


#### 经典算法

计算最近几次RTT的加权移动平均计算，然后放大一定倍数

步骤:

1. 先采样RTT，记下最近好几次的RTT值。
2. 通过加权移动平均计算SRTT（ Smoothed RTT）。公式为：
>SRTT = ( α * SRTT ) + ((1- α) * RTT)
>其中的 α 取值在0.8 到 0.9之间
3. 开始计算RTO。公式如下：
>RTO = min [ UBOUND,  max [ LBOUND,   (β * SRTT) ]  ]
>
>其中：
>
>UBOUND是最大的timeout时间，上限值
>LBOUND是最小的timeout时间，下限值
>β 值一般在1.3到2.0之间。


#### Karn/Partridge 算法

经典算法中，对发生重传的两种情况，即，真丢包和假丢包，算计的rtt不正确。真丢包计算大了，假丢包计算小了。

Karn / Partridge算法的解决办法是：

- 忽略重传，不把重传的RTT做采样。
- 当发生重传，就对现有的RTO值翻倍


![pic](https://coolshell.cn/wp-content/uploads/2014/05/Karn-Partridge-Algorithm.jpg)


#### Jacobson / Karels 算法

前面两种算法用的都是“加权移动平均”，这种方法最大的毛病就是如果RTT有一个大的波动的话，很难被发现，因为被平滑掉了。

Jacobson/Karels算法的解决办法是：引入了最新的RTT的采样和平滑过的SRTT的差距做因子来计算。 公式如下：（其中的DevRTT是Deviation RTT的意思）

```
SRTT = SRTT + α (RTT – SRTT)  —— 计算平滑RTT

DevRTT = (1-β)*DevRTT + β*(|RTT-SRTT|) ——计算平滑RTT和真实的差距（加权移动平均）

RTO= µ * SRTT + ∂ *DevRTT —— 神一样的公式

（其中：在Linux下，α = 0.125，β = 0.25， μ = 1，∂ = 4 ——这就是算法中的“调得一手好参数”，nobody knows why, it just works…）
```


### TCP滑动窗口-流控

为了实现可靠的传输，tcp需要估算出网络大的实际带宽，预防传输数据过载的时候导致拥塞及丢包。

首先介绍一下tcp的发送队列和接收队列:

```
发送队列：

network <---- |...LastByteAcked|...LastByteSent|...LastByteWritten|...| <--- application


LastByteAcked: 被接收端Ack过的位置
LastByteSent: 表示发出去了，但还没有收到成功确认的Ack
LastByteWritten: 应用程序写入的位置


接收队列：

application <--- |LastByteRead...|...NextByteExpected|...LastByteRcved|...| <--- network

LastByteRead: 应用程序读到的位置
NextByteExpected: 收到的连续包的最后一个位置
LastByteRcved: 收到的包的最后一个位置
```

现在我们知道了：

- 接收端在给发送端回ACK中会汇报自己的AdvertisedWindow = MaxRcvBuffer – LastByteRcved – 1, 
换句话说AdvertisedWindow是接收方tcp列队上可用来写入数据的空间大小。



再来看一下发送方是如何使用AdvertisedWindow的:

![pic](https://coolshell.cn/wp-content/uploads/2014/05/tcpswwindows.png)

- 已收到ack确认的数据。
- 发送未ack的数据
- 未发送数据
- 窗口以外的数据（接收方没空间）


滑动窗口包括：发送未ack的数据，未发送数据

![pic](https://coolshell.cn/wp-content/uploads/2014/05/tcpswslide.png)

所以：

发送端，在LastByteAcked移动端的时候，移动最大可写入位置，使得AdvertisedWindow=发送未ack的数据+未发送数据


#### Zero Window

如果越到接收缓慢的情况，window可能会变成0，如果window=0，那么数据将不再发送，如下图所示：


![pic](https://coolshell.cn/wp-content/uploads/2014/05/tcpswflow.png)

解决这个问题，TCP使用了Zero Window Probe技术，缩写为ZWP:

1. 发送端在窗口变成0后
2. 会发ZWP的包给接收方，让接收方来ack他的Window尺寸，
3. 一般这个值会设置成3次，第次大约30-60秒（不同的实现可能会不一样）。
4. 如果3次过后还是0的话，有的TCP实现就会发RST把链接断了。


#### Silly Window Syndrome

如果，窗口由于某种原因，一直维持在一较小的大小，那么发送端也就只能每次发送较少的数据出去，导致数据的传输效率不高。可以通过两种办法解决这个问题：

- 如果这个问题是由Receiver端引起的，那么就会使用 David D Clark’s 方案。在receiver端，如果收到的数据导致window size小于某个值，可以直接ack(0)回sender，这样就把window给关闭了，也阻止了sender再发数据过来，等到receiver端处理了一些数据后windows size 大于等于了MSS，或者，receiver buffer有一半为空，就可以把window打开让send 发送数据过来。
- 如果这个问题是由Sender端引起的，那么就会使用著名的 Nagle’s algorithm。这个算法的思路也是延时处理，他有两个主要的条件：1）要等到 Window Size>=MSS 或是 Data Size >=MSS，2）收到之前发送数据的ack回包，他才会发数据，否则就是在攒数据。

注意:

- MTU=1500byte
- MSS(Max Segment Size)=MTU-tcp.head-ip.head=1460


### TCP的拥塞处理 – Congestion Handling

如果网络上的延时突然增加，那么，TCP对这个事做出的应对只有重传数据，但是，重传会导致网络的负担更重，于是会导致更大的延迟以及更多的丢包，于是，这个情况就会进入恶性循环被不断地放大。试想一下，如果一个网络内有成千上万的TCP连接都这么行事，那么马上就会形成“网络风暴”，TCP这个协议就会拖垮整个网络。

所以，TCP不能忽略网络上发生的事情，而无脑地一个劲地重发数据，对网络造成更大的伤害。对此TCP的设计理念是：TCP不是一个自私的协议，当拥塞发生的时候，要做自我牺牲。就像交通阻塞一样，每个车都应该把路让出来，而不要再去抢路了。

拥塞控制主要是四个算法：1）慢启动，2）拥塞避免，3）拥塞发生，4）快速恢复。

#### 慢热启动算法 – Slow Start

慢启动的意思是，刚刚加入网络的连接，一点一点地提速

慢启动的算法如下(cwnd全称Congestion Window)：

1. 连接建好的开始先初始化cwnd = 1\*MSS，表明可以传一个MSS大小的数据。
2. 每当收到一个ACK，cwnd++; 呈线性上升
3. 每当过了一个RTT，cwnd = cwnd\*2; 呈指数让升
4. 还有一个ssthresh（slow start threshold），是一个上限，当cwnd >= ssthresh时，就会进入“拥塞避免算法”


![pic](https://coolshell.cn/wp-content/uploads/2014/05/tcp.slow_.start_.jpg)



疑问：

1. 收到ack和过了RTT是同一件事情？

todo

2. cwnd和wnd是的区别是什么?
	- 滑动窗口（wnd）是接收端接收数据的能力
	- 拥塞窗口（cwnd）是传输过程中的传输能力
	- LastByteSent-LastByteAcked<=min(wnd,cwnd)
	- https://www.zhihu.com/question/264518499/answer/315348958




#### 拥塞避免算法 – Congestion Avoidance

当cwnd >= ssthresh时，就会进入“拥塞避免算法”。一般来说ssthresh的值是65535，单位是字节，当cwnd达到这个值时后，算法如下：

1. 收到一个ACK时，cwnd = cwnd + 1/cwnd
2. 当每过一个RTT时，cwnd = cwnd + 1


#### 拥塞状态时的算法

当丢包的时候，会有两种情况：

1. 等到RTO超时，重传数据包。TCP认为这种情况太糟糕，反应也很强烈。
	1. sshthresh = cwnd /2
	2. cwnd 重置为 1
	3. 进入慢启动过程
2. Fast Retransmit算法，也就是在收到3个duplicate ACK时就开启重传，而不用等到RTO超时。
	1. TCP Tahoe的实现和RTO超时一样。
	2. TCP Reno的实现是：
		1. cwnd = cwnd /2
		2. sshthresh = cwnd
		3. 进入快速恢复算法——Fast Recovery

#### 快速恢复算法 – Fast Recovery

快速重传和快速恢复算法一般同时使用。快速恢复算法是认为，你还有3个Duplicated Acks说明网络也不那么糟糕，所以没有必要像RTO超时那么强烈。 注意，正如前面所说，进入Fast Recovery之前，cwnd 和 sshthresh已被更新：

##### TCP Reno

1. cwnd = cwnd /2
2. sshthresh = cwnd

然后，真正的Fast Recovery算法如下：

1. cwnd = sshthresh  + 3 * MSS （3的意思是确认有3个数据包被收到了）
2. 重传Duplicated ACKs指定的数据包
3. 如果再收到 duplicated Acks，那么cwnd = cwnd +1
4. 如果收到了新的Ack，那么，cwnd = sshthresh ，然后就进入了拥塞避免的算法了。


##### TCP New Reno

主要是对没有SACK的支持的情况下对Fast Recovery算法的改进：

- 当sender这边收到了3个Duplicated Acks，进入Fast Retransimit模式，开发重传重复Acks指示的那个包。如果只有这一个包丢了，那么，重传这个包后回来的Ack会把整个已经被sender传输出去的数据ack回来。如果没有的话，说明有多个包丢了。我们叫这个ACK为Partial ACK。
- 一旦Sender这边发现了Partial ACK出现，那么，sender就可以推理出来有多个包被丢了，于是乎继续重传sliding window里未被ack的第一个包。直到再也收不到了Partial Ack，才真正结束Fast Recovery这个过程


下面我们来看一个简单的图示以同时看一下上面的各种算法的样子：

![pic](https://coolshell.cn/wp-content/uploads/2014/05/tcp.fr_.jpg)


#### FACK算法

FACK全称Forward Acknowledgment,FACK用来做重传过程中的拥塞流控。

- 这个算法会把SACK中最大的Sequence Number 保存在snd.fack这个变量中，snd.fack的更新由ack带秋，如果网络一切安好则和snd.una一样（snd.una就是还没有收到ack的地方，也就是前面sliding window里的category #2的第一个地方）
- 然后定义一个awnd = snd.nxt – snd.fack（snd.nxt指向发送端sliding window中正在要被发送的地方——前面sliding windows图示的category#3第一个位置），这样awnd的意思就是在网络上的数据。（所谓awnd意为：actual quantity of data outstanding in the network）
- 如果需要重传数据，那么，awnd = snd.nxt – snd.fack + retran_data，也就是说，awnd是传出去的数据 + 重传的数据。
- 然后触发Fast Recovery 的条件是： ( ( snd.fack – snd.una ) > (3\*MSS) ) || (dupacks == 3) ) 。这样一来，就不需要等到3个duplicated acks才重传，而是只要sack中的最大的一个数据和ack的数据比较长了（3个MSS），那就触发重传。在整个重传过程中cwnd不变。直到当第一次丢包的snd.nxt<=snd.una（也就是重传的数据都被确认了），然后进来拥塞避免机制——cwnd线性上涨。



### 其它拥塞控制算法简介
#### TCP Vegas 拥塞控制算法

它主要对TCP Reno 做了些修改。这个算法通过对RTT的非常重的监控来计算一个基准RTT。然后通过这个基准RTT来估计当前的网络实际带宽，如果实际带宽比我们的期望的带宽要小或是要多的活，那么就开始线性地减少或增加cwnd的大小。如果这个计算出来的RTT大于了Timeout后，那么，不等ack超时就直接重传


#### HSTCP(High Speed TCP) 算法

其对最基础的算法进行了更改，他使得Congestion Window涨得快，减得慢。其中：

- 拥塞避免时的窗口增长方式： cwnd = cwnd + α(cwnd) / cwnd
- 丢包后窗口下降方式：cwnd = (1- β(cwnd))\*cwnd


注：α(cwnd)和β(cwnd)都是函数，如果你要让他们和标准的TCP一样，那么让α(cwnd)=1，β(cwnd)=0.5就可以了。 对于α(cwnd)和β(cwnd)的值是个动态的变换的东西。 关于这个算法的实现，你可以参看Linux源码：/net/ipv4/tcp_highspeed.c


#### TCP BIC 算法

BIC这个算法主要用的是Binary Search——二分查找来干这个事。 关于这个算法实现，你可以参看Linux源码：/net/ipv4/tcp_bic.c

#### TCP WestWood算法

westwood采用和Reno相同的慢启动算法、拥塞避免算法。westwood的主要改进方面：在发送端做带宽估计，当探测到丢包时，根据带宽值来设置拥塞窗口、慢启动阈值。

带宽估计的方法是：每个RTT时间，会测量一次带宽，测量带宽的公式很简单，就是这段RTT内成功被ack了多少字节。因为，这个带宽和用RTT计算RTO一样，也是需要从每个样本来平滑到一个值的——也是用一个加权移平均的公式。


## 参考文章
- <https://coolshell.cn/articles/11564.html>
- <https://coolshell.cn/articles/11609.html>
- <https://zh.wikipedia.org/wiki/TCP%E6%8B%A5%E5%A1%9E%E6%8E%A7%E5%88%B6>
- <https://datatracker.ietf.org/doc/html/rfc5681>