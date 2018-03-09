## 程序员的枷"锁"

大学学习计算机开始就接触到了各种各样的锁，有unix内核级别的，有c++开发编程里的锁，还有mysql中各种锁，工作后又接触了分布式锁。太多锁了，很多都是囫囵吞枣，很容易被这些名词搞晕，实际上锁的类型远远不止这些，这里系统梳理一下其中常见的一部分。

### 一、锁

无论何种锁，都是多任务场景下的并发处理方式，目的都是解决资源冲突。锁又可分为很多类型。单机环境下有，unix系统级别的各种锁，语言层面的锁，mysql层面的锁。而分布式环境下又有基于redis实现的锁，和基于zk实现的分布式锁。在介绍各种锁之前，先思考两个核心问题,：

```js
Q1.拿到锁后，其他竞争者该怎么办？是阻塞还是空转，还是返回错误重新尝试？

Q2.拿到锁时任务如果挂掉了，通过什么机制来保证这个锁一定会被释放？还是不释放，陷入死锁状态？还是设置超时机制?
```

下面逐一介绍。

### 二、单机并发锁

#### 2.1 unix内核级别锁

锁的逻辑基本按照下面的方式来进行的，拿到锁后处理资源，处理完成后释放锁。

```js
lock. 
    dosomething；//进入临界区处理资源 
unlock.
```
多任务的情况下，去竞争处理资源的优先权限。程序抢到锁，是在获得cpu的执行的前提下，去获取资源处理优先权。这里注意区分cpu、和资源，这里的资源可以指寄存器或者内存中的某个数据，也可以指文件或者外设。锁的目的是竞争资源，而不是竞争cpu。

谁去竞争资源呢，cpu代表线程去竞争，所以抢锁的前提是先抢到cpu的执行权限，而多核情况下cpu的处理权限是根据cpu0来调度分配的。

unix系统下的锁有：
a.互斥锁。mutex。遇到锁后会休眠，释放cpu。
b. 自旋锁。spinlock。遇到锁时，占用cpu空等。
c.读写锁。rwlock。
d.顺序锁。seqlock。 本质上是一个自旋锁+一个计数器。顺序锁，写操作每次获取锁都都要对计数器加１．解锁后也要加１．

##### 2.1.1 互斥锁和自旋锁。

假设一个两核的机器（cpu0,cpu1），两个线程A，B并发处理资源M。

cpu获得资源处理。线程和cpu对应的关系。

场景1：
线程A：cpu0抢到M
线程B：线程B休眠，cpu1处理其他任务

场景2：
线程A：cpu0抢到M
线程B：cpu1空转

在场景1，线程A抢占到了锁，处理资源M。此时线程B如果休眠的话，释放出cpu1让其处理其他的任务，那么这是互斥锁；

在场景2，线程B始终hold着cpu1的处理权限，空转着。故名思议这就是自旋锁。这里，如果线程A始终没释放锁，那么自旋锁的场景下，cpu1会一直被占用着，对资源是一种很大的浪费。

下面是基于posix库实现的互斥锁：

###### a.互斥锁:

示例文件：thread.cpp

```js
#include <stdlib.h>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/syscall.h>

using namespace std;
pthread_mutex_t mymutest;
void* mutex_test(void* arg)
{
  pid_t ptd = syscall(__NR_gettid);
  cout<<ptd << " create succcess!" <<endl;
  sleep(1);
  int ret = pthread_mutex_lock(&mymutest);
  if(ret != 0)
  {
      cout<<"cant get lock .num=" << arg << endl;
      return NULL;
  }
  cout<<"theadid:"<<ptd << " processing.." <<endl;

  for(int i = 0; i < 50000000 ;i++)
  {
      for (int j = 0 ;j <10000000;j++)
      {
          time_t t1 = time(NULL);
      }
  }

  pthread_mutex_unlock(&mymutest);
  return NULL;

}
int main()
{
    // 定义线程的 id 变量，多个变量使用数组。
    pthread_mutex_init(&mymutest,NULL);//互斥阻塞住，同时让出cpu。
    pthread_t tids[2];
    for(int i = 0; i < 2; ++i)
    {
        //参数依次是：创建的线程id，线程参数，调用的函数，传入的函数参数
        int ret = pthread_create(&tids[i], NULL, mutex_test, (void*)i);
        if (ret != 0)
        {
           cout << "pthread_create error: error_code=" << ret << endl;
        }
    }
    sleep(30);
    pthread_mutex_destroy(&mymutest);//销毁操作一定要再sleep之后，否则锁就会被提前销毁。
    //等各个线程退出后，进程才结束，否则进程强制结束了，线程可能还没反应过来；
    pthread_exit(NULL);
}
```

编译文件：g++ thread.cpp -o thread -lpthread

执行结果：

```js
[root@VM_136_39_centos /data/release/test]# ./thread
2577 create succcess!

2578 create succcess!

theadid:2577 processing..
```

创建了两个线程2577和2578，其中2578拿到了锁执行了一个非常耗时的循环。2577在等待锁。这个时候查看每个线程的cpu消耗情况：

```
[root@VM_136_39_centos /data/home/user]# ps -eo ruser,pid,ppid,lwp,psr,args,stat -L | grep thread
root         2     0     2   1 [kthreadd]                  S
root      2576 25426  2576   0 [thread] <defunct>          Zl+
root      2576 25426  2577   1 [thread] <defunct>          Rl+
root      2576 25426  2578   2 [thread] <defunct>          Sl+
```

注意第四列是线程id，第五列是执行的cpu核，第七列是线程的状态。

其中2578分配了cpu2,执行状态为Sl+ 即休眠状态。

2577分配了cpu1，目前的状态也是Rl+，即2577拿到锁后，处理资源过程中，2578线程在等待锁，这个时候虽然是阻塞住了，但是其实已经释放了CPU2.因为状态是休眠状态。

###### b.自旋锁：

示例文件：spinlock.cpp

```js
#include <stdlib.h>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/syscall.h>

using namespace std;
//自旋锁
pthread_spinlock_t mymutest;
void* mutex_test(void* arg)
{
  pid_t ptd = syscall(__NR_gettid);
  cout<<ptd << " create succcess!\n" <<endl;
  sleep(1);
  int ret = pthread_spin_lock(&mymutest);
  if(ret != 0)
  {
      cout<<"cant get lock .num=" << arg << endl;
      return NULL;
  }
  cout<<"theadid:"<<ptd << " processing.." <<endl;

  for(int i = 0; i < 50000000 ;i++)
  {
      for (int j = 0 ;j <10000000;j++)
      {
          time_t t1 = time(NULL);
      }
  }

  pthread_spin_unlock(&mymutest);
  return NULL;

}
int main()
{
    // 定义线程的 id 变量，多个变量使用数组。
    pthread_spin_init(&mymutest,NULL);//自旋锁 自旋锁在多核下，会占用cpu，导致其他的也用不了。
    pthread_t tids[2];
    for(int i = 0; i < 2; ++i)
    {
        //参数依次是：创建的线程id，线程参数，调用的函数，传入的函数参数
        int ret = pthread_create(&tids[i], NULL, mutex_test, (void*)i);
        if (ret != 0)
        {
           cout << "pthread_create error: error_code=" << ret << endl;
        }
    }
    sleep(30);
    pthread_spin_destroy(&mymutest);//销毁操作一定要再sleep之后，否则锁就会被提前销毁。
    //等各个线程退出后，进程才结束，否则进程强制结束了，线程可能还没反应过来；
    pthread_exit(NULL);
}
```

自旋锁的执行结果：

编译文件：g++ spinlock.cpp -o spinlock -lpthread

```js
[root@VM_136_39_centos /data/release/test]# ./spinlock
3197 create succcess!

3198 create succcess!

theadid:3197 processing..
```

创建了两个线程32559和32560、其中32559拿到了锁执行了一个非常耗时的循环。这个时候查看每个线程的cpu消耗情况：

```js
[root@VM_136_39_centos /data/home/user]# ps -eo ruser,pid,ppid,lwp,psr,args,stat -L | grep spinlock
root      3196 25426  3196   3 [spinlock] <defunct>        Zl+
root      3196 25426  3197   1 [spinlock] <defunct>        Rl+
root      3196 25426  3198   3 [spinlock] <defunct>        Rl+
```

其中3197分配了cpu1,执行状态为Rl+ 即运营中的状态。
3198分配了cpu3，目前的状态也是Rl+，即3198拿到锁后，处理资源过程中，3197线程在等待锁，但是并没有释放cpu3，线程状态始终处于运行中的状态，即空转。

##### 2.1.2 读写锁和顺序锁。

###### a.读写锁：

同一时刻只有一个线程可以获得写锁，同一时刻可以有多线程获得读锁。

读写锁处于写锁状态时，所有试图对读写锁加锁的线程，不管是读者试图加读锁，还是写者试图加写锁，都会被阻塞。

读写锁处于读锁状态时，有写者试图加写锁时，之后的其他线程的读锁请求会被阻塞，以避免写者长时间的不写锁。

如果读操作非常频繁，写锁会有获取不到锁的可能。由于读写获取锁的优先级一样，当读多写少的时候，写操作可能会一直获取不到锁。

读写锁实现：

示例文件：rwlock.cpp

```js
#include <stdlib.h>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/syscall.h>

using namespace std;
//////////////////读写锁。 两读一写。
pthread_rwlock_t rwlock;             //声明读写锁
int data  =1 ;
void* read1(void* arg)
{
    while(1)
    {
        pthread_rwlock_rdlock(&rwlock);
        cout<<"A读取共享资源:"<< data << endl;
        sleep(2);
        pthread_rwlock_unlock(&rwlock);    //读者释放读锁
        sleep(1);
    }
    return NULL;
}
void* read2(void* arg)
{

    while(1)
    {
        pthread_rwlock_rdlock(&rwlock);
        cout<<"B读取共享资源:"<< data << endl;
        sleep(2);
        pthread_rwlock_unlock(&rwlock);    //读者释放读锁
        sleep(1);
    }
    return NULL;
}

void* write1(void* arg)
{
    while(1)
    {
        pthread_rwlock_wrlock(&rwlock);      //写者加写锁
        data++;                              //对共享资源写数据
        cout<<"C写者写入:"<< data << endl;
        sleep(2);
        pthread_rwlock_unlock(&rwlock);      //释放写锁
        sleep(1);
    }
    return NULL;
}
int main()
{
    // 定义线程的 id 变量，多个变量使用数组。
    pthread_rwlock_init(&rwlock, NULL);//读写锁初始化
    //pthread_spin_mutex_init();自旋锁 自旋锁在多核下，会占用cpu，导致其他的也用不了。
    pthread_t ptidr1,ptidr2,ptidw1,ptidw2 ;
    pthread_create(&ptidw1, NULL, write1, NULL);
    pthread_create(&ptidr1, NULL, read1, NULL);
    pthread_create(&ptidr2, NULL, read2, NULL);
    sleep(100);
    pthread_rwlock_destroy(&rwlock);
}

```

读写锁执行结果：

编译文件：g++ rwlock.cpp -o rwlock -lpthread

```js
[root@VM_136_39_centos /data/release/test]# ./rwlock
C写者写入:2
B读取共享资源:2
A读取共享资源:2

C写者写入:3
B读取共享资源:3
A读取共享资源:3

C写者写入:4
B读取共享资源:4
A读取共享资源:4
```

###### b.顺序锁：

顺序锁。seqlock。 本质上是一个自旋锁+一个计数器。顺序锁，写操作每次获取锁都都要对计数器加１．解锁后也要加１．顺序锁是对读写锁的补充，解决了读写锁中，写任务获取不到锁的情况。顺序锁的结构体：

```js
typedef struct {
    struct seqcount seqcount;
    spinlock_t lock;
} seqlock_t;
```

读者读前和读后都要获取一次序列号值，如果两次序列相等，则拿到数据，否则，表示已经执行了写操作，重新读取数据，直至成功。写操作永远不会等待，可以随时拿到写权限。读操作可能会需要重试几次。

#### 2.2 mysql中的锁

从锁的粒度上分为：
表级锁。 table-level locking。
页级锁。 page-level locking。
行级锁。 row-level locking。Innodb中没有页锁
从锁的性格上来说分为：悲观锁、乐观锁。

乐观锁从本质上来说不算锁。因为没有加锁和释放锁的动作。但是从另一个角度来看，乐观锁和悲观锁都一样，都是多个节点抢夺资源的优先处理权限，都是处理并发的方式。乐观锁使用版本号，可以在数据初始化时指定一个版本号，每次对数据的更新操作都对版本号执行+1操作。并判断当前版本号是不是该数据的最新的版本号。乐观锁比较简单，这里不做详解。

从锁的特性上来说分为：共享锁，排他锁
下面以mysql Innodb引擎为例来测试这几种锁。
测试表t_resource结构如下：

```js
Table: t_resource
Create Table: 

CREATE TABLE `t_resource` (
  `resource_id` int(11) NOT NULL AUTO_INCREMENT,
  `region_id` varchar(36) NOT NULL DEFAULT '',
  `receive_uin` bigint(20) NOT NULL DEFAULT 0,
  PRIMARY KEY (`resource_id`),
  KEY `receive_uin` (`receive_uin`)
) ENGINE=InnoDB AUTO_INCREMENT=0 DEFAULT CHARSET=UTF8;

MySQL [guess]> set autocommit=0;   //设置事务为手动提交
查询阻塞的session“”
show status like 'innodb_row_lock_current_waits%';
```

其中receive_uin是索引。
对第一章提出的问题Q2.在mysql的处理方式是：如果某个任务拿到锁后挂了没有释放锁，会有超时机制，如果超过一定时间没有等到锁，其他任务会返回等待超时。

##### 2.2.1 共享锁

也叫读锁，简称S锁，原理：一个事务获取了一个数据行的共享锁，其他事务能获得该行对应的共享锁，但不能获得排他锁，即一个事务在读取一个数据行的时候，其他事务也可以读，但不能对该数据行进行增删改。

设置共享锁：SELECT … LOCK IN SHARE MODE;

session1（加共享锁读）:

```js
mysql> select * from t_resource where receive_uin = 123 lock in share mode;
+-------------+-----------+-------------+
| resource_id | region_id | receive_uin |
+-------------+-----------+-------------+
|           1 | aa        |         123 |
+-------------+-----------+-------------+
1 row in set (0.00 sec)
```

session2（共享锁读）：

```js
mysql> select * from t_resource where receive_uin = 123 lock in share mode;
+-------------+-----------+-------------+
| resource_id | region_id | receive_uin |
+-------------+-----------+-------------+
|           1 | aa        |         123 |
+-------------+-----------+-------------+
1 row in set (0.00 sec)
```

查看此时阻塞的session：

```js
mysql> show status like 'innodb_row_lock_current_waits%';
+-------------------------------+-------+
| Variable_name                 | Value |
+-------------------------------+-------+
| Innodb_row_lock_current_waits | 0     |
+-------------------------------+-------+
1 row in set (0.01 sec)
```

session3（尝试修改共享锁）：

```js
mysql> update t_resource set region_id="xx" where receive_uin=123;
ERROR 1205 (HY000): Lock wait timeout exceeded; try restarting transaction
```

查看此时阻塞的session：

```js
mysql> show status like 'innodb_row_lock_current_waits%';
+-------------------------------+-------+
| Variable_name                 | Value |
+-------------------------------+-------+
| Innodb_row_lock_current_waits | 1     |
+-------------------------------+-------+
1 row in set (0.00 sec)
```

session1加共享锁后，session2依然可与加共享锁去读。但是如果去修改就不行了,如session3就阻塞住了。

##### 2.2.2 排他锁

排他锁也叫写锁，简称x锁，原理：一个事务获取了一个数据行的排他锁，其他事务就不能再获取该行的其他锁（排他锁或者共享锁），即一个事务在读取一个数据行的时候，其他事务不能对该数据行进行增删改查。

设置排他锁：SELECT … FOR UPDATE;

__表锁__：

session1（排他锁非索引）:

```js
mysql> select * from t_resource where region_id = "aa" for update;
+-------------+-----------+-------------+
| resource_id | region_id | receive_uin |
+-------------+-----------+-------------+
|           1 | aa        |         123 |
+-------------+-----------+-------------+
1 row in set (0.00 sec)
```

session2（排他锁非索引查询）：

```js
mysql> select * from t_resource where region_id = "yy" for update;
ERROR 1205 (HY000): Lock wait timeout exceeded; try restarting transaction
```

阻塞的session：

```js
mysql> show status like 'innodb_row_lock_current_waits%';
+-------------------------------+-------+
| Variable_name                 | Value |
+-------------------------------+-------+
| Innodb_row_lock_current_waits | 1     |
+-------------------------------+-------+
1 row in set (0.00 sec)
```

可以看出虽然session1和session2的查询的记录不是一行，session1拿到锁后，session2依然阻塞住了。说明此时mysql并没有锁住行，而是锁住了表。这是因为在没有索引的查询下，mysql会优先使用表锁。

__行锁__：

receive_uin才是索引，我们测试用索引的查询条件。

session1（排他锁加上索引的查询）:

```js
mysql> select * from t_resource where receive_uin = 123 for update;
+-------------+-----------+-------------+
| resource_id | region_id | receive_uin |
+-------------+-----------+-------------+
|           1 | aa        |         123 |
+-------------+-----------+-------------+
1 row in set (0.00 sec)
```

session2（排他锁命中索引的查询）：

```js
mysql> select * from t_resource where receive_uin = 456 for update;
+-------------+-----------+-------------+
| resource_id | region_id | receive_uin |
+-------------+-----------+-------------+
|           2 | yy        |         456 |
+-------------+-----------+-------------+
1 row in set (0.00 sec)
```

阻塞中的session数：

```js
mysql> show status like 'innodb_row_lock_current_waits%';
+-------------------------------+-------+
| Variable_name                 | Value |
+-------------------------------+-------+
| Innodb_row_lock_current_waits | 0     |
+-------------------------------+-------+
1 row in set (0.00 sec)
```

session1获取锁后，session2并没有阻塞住。因为receive_uin是索引。mysql会精确到行，从而进行了行锁。session1获取锁后，session2不能获取读锁，这也是排他锁。

### 三、分布式锁

分布式锁用来满足分布式环境下多任务竞争资源的。一个好的分布式锁要满足以下特点：

    a.保证应用同一时间只能被一台机器上的一个线程执行。

    b.能够避免死锁。

    c.阻塞锁。

    d.高可用的获取锁和释放锁。

    e.具有较高的获取锁和释放锁性能

常见的分布式锁是基于Redis的分布式锁和基于ZK的分布式锁。下面主要介绍这两个：

#### 3.1 基于Redis的分布式锁

通过setnx 命令来设置锁，第一个设置成功的返回1，即抢到了锁。之后的都只是设置为0.

把锁的值设置为获取锁的时间戳，其他进程获取时，先比较时间戳与当前的时间戳，判断锁是否超时，如果已经超时就执行del操作把锁删除，把当前进程的时间戳设置进去。引入时间戳的超时机制可以防止一个某个任务抢到锁后挂了导致的锁不被释放的问题。解决了第一章中的Q2.问题。

```js
bool Lock(){
   time now;
   iRet  = Setnx(mylock,now );
   if(iRet == 1)
   {
        return true;//获取锁成功
   }  else{
   time  lastLockTime= Get(mylock);
   if(now - lastLockTime > expireTime) //超时了就重新设置。
    {
          iRet = getset(mylock,now);//重新获取锁并把当前任务的时间戳设置进去
          if(iRet == 0)
             return true;//获取锁成功
          else 
             return false;
     }
    return false;//获取锁失败
}
```

#### 3.2 基于Zk的分布式锁

zookeeper实现锁的方式是客户端一起竞争写某条数据，比如/path/lock，只有第一个客户端能写入成功，其他的客户端都会写入失败。写入成功的客户端就获得了锁，写入失败的客户端，注册watch事件，等待锁的释放，从而继续竞争该锁。

原理：

客户端请求创建节点，创建临时节点成功的拿到锁，拿到锁后释放锁。如果某个获取锁后挂了，该临时子节点会被watch删掉，从而实现释放锁的目的，很好的解决了第一章中的Q2.问题。

假设zk的锁根节点，A，B 进程请求获取对db的某个表t_resource操作的锁。
/distubute/lock/

    1. A请求锁，即创建A进程的临时节点 /distubute/lock/t_resource_pidA_001。临时节点的命名规则： 表名_进程id_seqno。seqno是zk分配的自增的id；

    2. B请求锁。创建B进程的临时节点：/distubute/lock/t_resource_pidB_002；

    3. A B 两个客户端通过GeTChild获取/distubute/lock/下的子节点。并找出seqno最小的那个节点，如果最小的临时节点是自己，就获取到了锁。

这个例子中A获取了锁。假设A挂了或者A与zk的连接断开了，那么ZK的watch机制会把该临时节点删除，从而实现释放临时节点，这种能够天然避免某个客户端长期拿到锁不释放。A释放节点也是执行一次删除临时节点的操作。

### 四、总结

以上只是简述了以下到目前为止遇到的各种锁，并没有深入的介绍下。但无论哪种锁，其目的都是为了解决多任务环境下的并发竞争资源的问题。各种锁都要遇到两个核心问题。

    1.拿到锁后，其他竞争者该怎么办？是阻塞还是空转，还是返回错误重新尝试？

    2.拿到锁时任务如果挂掉了，通过什么机制来保证这个锁一定会被释放？ 从这两个问题的角度去理解各种锁，或许会好一些。
