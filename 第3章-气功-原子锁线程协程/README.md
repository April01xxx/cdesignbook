## 第3章-气功-原子锁线程协程

    不妨恶作剧的为编程行业引入修真体系 [炼气 -> 筑基 -> 金丹 -> 元婴 -> 化神] . 
    那时候在学校中或者野路子锻炼感受天地间元气, 在练气期幸福不知睡眠. 感受编程行业斑驳交割
    的元气最终选择几门开始自己的练气生涯. 期间勤奋些的或者时间够了, 一舒心中豪情啪一声进入
    筑基期, 心随意动. 修炼生涯正式展开, 蹭蹭的进入了门派中磨炼. 随着门派体系或者一些心有不
    甘的选手日夜操戈, 自我驱动跃升成为人妖大战的前线主力, 吾为金丹期. 此时的战场才刚刚开始.
    同样以前修炼暴露的问题也一并展开. 无数人在此厮杀, 对抗域外天魔, 或者在远古战场中获得奇
    遇. 又或者占有一个门派的全力支持, 通过大毅力破吾金丹, 晋升元婴大佬. 隐射一方, 出手之间
    自带领域此时也是白骨功成, 为门派马首是瞻. 同样于生时天资聪慧, 道心自成的元婴大佬, 不被
    红尘迷恋. 占一代之大气运, 耐一世之大孤独. 甩手间风云变幻, 天雷滚滚, 超脱物外, 万中无一
    化神巨擘独立无为之境, 位于东方. 无一丝情感遥望着心的远方, 立于缥缈峰~
    各位看官化神难道就是编程的极限吗, 一切刚刚开始, 这里先不表.
    本章讲的气功, 等同于金丹期修炼的法术. 打通和操作系统联系的基本关节. 专业程序或多或少依
	赖于平台, 不同平台的修炼总会有大不同. 本章就是在不同平台间练就一门气功, 剑气贯双江
    
### 3.1 原子锁

    一个古老的话题, 主要为了解决资源竞争问题. 在说原子锁之前需要科普一些基本原子操作.

![原子自旋](./img/永恒万花筒.jpg)

#### 3.1.1 常用的几种原子操作

    首先来举个简单例子:

```C
static int _a = 0;

++_a; 
/*
	++_a 大致可以拆分为下面三步

	1' 把 _a 的值放入寄存器中
	2' 把 寄存器中值加1
	3' 返回寄存器中值并且设置给a

 */
```

    以上执行会导致一个问题, 如果两个线程同时执行到 1' 那么造成一个现象 _a最终没有预期的
	大. 如何避免上面问题呢. 常见思路是互斥. 当然这里有更好路子, 利用编译器提供的原子操作
	. 用 CPU原子指令的封装. 说白了用编译器提供这方面基础功能, 让我们实现原子相加. 
	例如 GCC就提供不少像下面这种指令.  

```C
type __sync_add_and_fetch (type * ptr, type value, ...);
type __sync_lock_test_and_set (type * ptr, type value, ...);
bool __sync_bool_compare_and_swap (type * ptr, type oldval, type newval, ...);
```

    这类原子操作命令直接查编译手册, 写个小例子, 就知道窍门了. 我们简单解释下, 
	__sync_add_and_fetch 等同于将 ptr指向的内存加上 value值, 并且返回最终加好的值. 
	__sync_lock_test_and_set 的意思是把 value的值给ptr指向的内存, 并且返回 ptr原先指向
	的内存值. __sync_bool_compare_and_swap 的意思是 ptr指向的值和原先的 oldval相等吗, 
	相等将其设置为 newval. 并且返回 ptr指向值和oldval相等与否的 bool值. 
	为了让大家更好认知, 不妨封装一层, 请收看注释:

```C
// v += a ; return v;
#define ATOM_ADD(v, a)		__sync_add_and_fetch(&(v), (a))
// type tmp = v ; v = a; return tmp;
#define ATOM_SET(v, a)		__sync_lock_test_and_set(&(v), (a))
// v &= a; return v;
#define ATOM_AND(v, a)		__sync_and_and_fetch(&(v), (a))
// return ++v;
#define ATOM_INC(v) 		__sync_add_and_fetch(&(v), 1)
// return --v;
#define ATOM_DEC(v) 		__sync_sub_and_fetch(&(v), 1)
// bool b = v == c; b ? v=a : ; return b;
#define ATOM_CAS(v, c, a)	__sync_bool_compare_and_swap(&(v), (c), (a))

 // 保证代码不乱序
#define ATOM_SYNC() 		__sync_synchronize()

// 对ATOM_LOCK 解锁, 当然 直接调用相当于 v = 0;
#define ATOM_UNLOCK(v)		__sync_lock_release(&(v))
```

    以上定义了 ADD SET AND INC DEC 等原子操作. 基础库中封装最常用的就这些了. 下面展示哈全
	貌. 更多细节可以细查 man 手册, 一切无所遁形.

#### 3.1.2 原子锁的跨平台实现

    代码已经表述了一切好的坏的没得又得, 没有什么比代码更好明白了, 否则那就抄几遍 ~

scatom.h

```C
#ifndef _H_SIMPLEC_SCATOM
#define _H_SIMPLEC_SCATOM

/*
 * 作者 : wz
 * 描述 : 简单的原子操作,目前只考虑 VS(CL) 和 gcc
 */

// 如果 是 VS 编译器
#if defined(_MSC_VER)

#include <windows.h>

#pragma warning(push)
//忽略 warning C4047: “==”:“void *”与“LONG”的间接级别不同
#pragma warning(disable:4047)

// v 和 a 都是 long 这样数据
#define ATOM_ADD(v, a)		InterlockedAdd((LONG volatile *)&(v), (LONG)(a))
#define ATOM_SET(v, a)		InterlockedExchange((LONG volatile *)&(v), (LONG)(a))
#define ATOM_AND(v, a)		InterlockedAnd((LONG volatile *)&(v), (LONG)(a))
#define ATOM_INC(v) 		InterlockedIncrement((LONG volatile *)&(v))
#define ATOM_DEC(v) 		InterlockedDecrement((LONG volatile *)&(v))
//
// 对于 InterlockedCompareExchange(v, c, a) 等价于下面
// long tmp = v ; v == a ? v = c : ; return tmp;
//
// 咱们的 ATOM_CAS(v, c, a) 等价于下面
// long tmp = v ; v == c ? v = a : ; return tmp;
//
#define ATOM_CAS(v, c, a)	((LONG)(c) == InterlockedCompareExchange((LONG volatile *)&(v), (LONG)(a), (LONG)(c)))

#pragma warning(pop)

// 保证代码不乱序优化后执行
#define ATOM_SYNC() 		MemoryBarrier()

#define ATOM_UNLOCK(v)		ATOM_SET(v, 0)

// 否则 如果是 gcc 编译器
#elif defined(__GNUC__)

// v += a ; return v;
#define ATOM_ADD(v, a)		__sync_add_and_fetch(&(v), (a))
// type tmp = v ; v = a; return tmp;
#define ATOM_SET(v, a)		__sync_lock_test_and_set(&(v), (a))
// v &= a; return v;
#define ATOM_AND(v, a)		__sync_and_and_fetch(&(v), (a))
// return ++v;
#define ATOM_INC(v) 		__sync_add_and_fetch(&(v), 1)
// return --v;
#define ATOM_DEC(v) 		__sync_sub_and_fetch(&(v), 1)
// bool b = v == c; b ? v=a : ; return b;
#define ATOM_CAS(v, c, a)	__sync_bool_compare_and_swap(&(v), (c), (a))

 // 保证代码不乱序
#define ATOM_SYNC() 		__sync_synchronize()

// 对ATOM_LOCK 解锁, 当然 直接调用相当于 v = 0;
#define ATOM_UNLOCK(v)		__sync_lock_release(&(v))

#endif // !_MSC_VER && !__GNUC__

/*
 * 试图加锁, 用法举例
 
	 if(ATOM_TRYLOCK(v)) {
		 // 已经有人加锁了, 处理返回事件
		...
	 }
 
	 // 得到锁资源, 开始处理
	 ...
 
	 ATOM_UNLOCK(v);
 
 * 返回1表示已经有人加锁了, 竞争锁失败.
 * 返回0表示得到锁资源, 竞争锁成功
 */
#define ATOM_TRYLOCK(v)		ATOM_SET(v, 1)

//
// 使用方式:
//  int lock = 0;
//  ATOM_LOCK(lock);
//  ...
//  ATOM_UNLOCK(lock);
//
#define ATOM_LOCK(v)		while(ATOM_SET(v, 1))

#endif // !_H_SIMPLEC_SCATOM
```

    这些代码很短, scatom.h 希望抄写几遍, 保证有效果. 当然我们的原子锁主打是 linux平台.
	也是当前开发届主旋律, winds辅助开发, linux在外实战. 使用起来就更简单了. 例如在上一
	章写了个 tstr字符串. 它不是线程安全的. 可以利用上面原子锁, 简单帮它改成线程安全: 

```C
struct astr {
    int lock;
    struct tstr str;
}

// 初始化
struct astr as = { 0 };

// 使用
ATOM_LOCK(as.lock);

// 各种对于 astr.str 操作都是线程安全的
...

ATOM_UNLOCK(as.lock);

// 销毁
free(as.str.str);
```

    以上就是原子锁使用的核心步骤. 当然了, 装波的事情远远还没有结束. 很久以前别人问什么是
	自旋锁, 当时羞愧难当. 后面才知道就是写了无数遍的原子锁. 更多的是想说少炒作一些概念, 
	多一些真诚. 编程本身就那些东西, 说明白了大家都懂了. 切记编程道上多真多善否则基本无望
	元婴. 当然高阶金丹期也都能够胜任主程了, 多数定型了. 上面原子锁仍然可以优化, 例如采用
	阻塞替代忙等待, 降低 CPU空转. 等等优化. 总而言之在解决资源竞争问题上, 消耗最小是真无
	锁编程. 通过业务优化避免锁的产生. C 开发用系统互斥锁偏重, 这也是原子锁遗留的原因.

### 3.2 POSIX 线程库

    对于 POSIX 标准线程库, 也就是我们常在 Linux使用 pthread线程库. 首先为其举个常用
    API 的提纲. 常用的说明手册:

```C
/*
 * PThread Attribute Functions
 */

//
// pthread_attr_init - 初始化一个线程环境变量
// pthread_attr_destroy - 销毁一个线程环境变量
// attr     : pthread_attr_t 线程环境变量
// return   : 0 表示成功
//
extern int __cdecl pthread_attr_init (pthread_attr_t * attr);
extern int __cdecl pthread_attr_destroy (pthread_attr_t * attr);

//
// pthread_attr_setdetachstate - 设置线程的运行结束的分离属性
// attr         : pthread_attr_t 线程环境变量
// detachstate  : 默认是 PTHREAD_CREATE_JOINABLE, 需要执行 pthread_join 销毁遗留的线程空间
//              : PTHREAD_CREATE_DETACHED 属性等同于 pthread_detach, 结束即销毁
// return       : 0 表示成功
//
extern int __cdecl pthread_attr_setdetachstate (pthread_attr_t * attr, int detachstate);
```

    线程构建初始化如下:

```C
/*
 * PThread Functions
 */

//
// pthread_create - 创建一个线程
// tid      : 返回创建线程的句柄 pthread_t 类型变量
// attr     : 线程创建初始化的量, 主要看 pthread_attr_xxxx 一系列设置
// start    : 线程创建成功运行的实体
// arg      : start 启动后传入的额外参数
// return   : 返回 0 表示成功, 非零表示失败例如 EAGAIN
//
extern int __cdecl pthread_create (pthread_t * tid,
                            const pthread_attr_t * attr,
                            void * (__cdecl * start) (void *),
                            void * arg);

//
// pthread_equal - 两个线程id比较
// t1       : 线程id
// t2       : 线程id
// return   : 1 表示二者相同, 0 表示二者不同
//
extern int __cdecl pthread_equal (pthread_t t1, pthread_t t2);

//
// pthread_exit - 退出当前线程
// value_ptr    : 会保存在线程的私有变量中, 留给 pthread_join 得到
// return       : void
//
extern void __cdecl pthread_exit (void * value_ptr);

//
// pthread_join - 等待线程 pthread_create -> start 函数运行结束
// thread       : 线程id
// value_ptr    : 返回 start 返回值, 或 pthread_exit 设置的值
// return       : 0表示成功, 其它查源码吧
//
extern int __cdecl pthread_join (pthread_t thread, void ** value_ptr);
```

    线程互斥量, 基本和 pthread_create 使用频率差不多. 加上手工注释希望大家
    能够感性认知哈

```C
/*
 * Mutex Attribute Functions
 */
static pthread_mutex_t _mtx = PTHREAD_MUTEX_INITIALIZER;

//
// pthread_mutex_init - 初始化一个线程互斥量类型
// pthread_mutex_destroy - 清理一个线程互斥量类型, 必须和 pthread_mutex_init成对
//
extern int __cdecl pthread_mutex_init (pthread_mutex_t * mutex, const pthread_mutexattr_t * attr);
extern int __cdecl pthread_mutex_destroy (pthread_mutex_t * mutex);

//
// pthread_mutex_lock - 加锁
// pthread_mutex_unlock - 解锁
//
extern int __cdecl pthread_mutex_lock (pthread_mutex_t * mutex);
extern int __cdecl pthread_mutex_unlock (pthread_mutex_t * mutex);
```

    擦, 才说了一点点. 翻译 API有点累, 不想继续扯了. 下次用到额外的 api 继续深入翻译. 
	上面 PTHREAD_MUTEX_INITIALIZER 初始化的互斥量, 不需要调用 pthread_mutex_destroy 
	默认跟随系统生命周期. 对于 pthread 线程, 假如你用了 xxx_init 那么最终最好都需要调用 
	xxx_destroy . 不妨通过上面代码简单露一手了哈哈

```C
//
// async_run - 开启一个自销毁的线程 运行 run
// run		: 运行的主体
// arg		: run的参数
// return	: >= SufBase 表示成功
//
inline int 
async_run_(node_f run, void * arg) {
	pthread_t tid;
	pthread_attr_t attr;

	// 构建pthread 线程奔跑起来
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	if (pthread_create(&tid, &attr, (start_f)run, arg) < 0) {
		pthread_attr_destroy(&attr);
		RETURN(ErrBase, "pthread_create error run, arg = %p | %p.", run, arg);
	}

	pthread_attr_destroy(&attr);
	return SufBase;
}
```

    使用起来就非常轻松了, async_run_((node_f) run, xxx) 异步分离线程就跑起来哈哈

#### 3.2.1 winds 搭建 pthread 线程库

    winds 上使用 POSIX 的 pthread 线程特别 cool, 很爽. 毕竟 winds自带的线程库用起来要人命, 
	丑的无语. 采用的方案是 pthread for win32 open code project. 自行到 github 上找 
	GerHobbelt 大神的 pthread-win32 项目. 源码结构特别清晰好懂, 异常服. 
    随后下载我为上面大神构建一个简易的发布的 pthread for winds 32位静态库发布版项目 
	pthread.winds.lib.2.10.0.1. 可以自行搜索使用 ~ 那开始爆料了~

> 使用说明:  

    sched.h
    pthread.h
    semaphore.h
    pthread_lib.lib
    
    添加到用到的 Visual Studio  sln 项目中.( pthread.h 中已经包含了 pthread_lib.lib )
	对原版提供的头文件进行过大面积修改, 部分摘录如下:

```C
#if !defined(_H_PTHREAD) && defined(_MSC_VER)
#define _H_PTHREAD

/*
 * See the README file for an explanation of the pthreads-win32 version
 * numbering scheme and how the DLL is named etc.
 */
#define PTW32_VERSION 2, 10, 0, 1
#define PTW32_VERSION_STRING "2, 10, 0, 1"

#pragma comment(lib, "pthread_lib.lib")

#endif
```

    为了达到和 linux上使用 #include <pthread.h> 一样效果. 还需要为项目添加一个包含的文件
	目录. 就和下面这样, 自行下一步下一步~

![库目录](./img/库目录.png)

> 说明完毕

    从此以后, 你要的一切 pthread 都会给你! 为所爱的人去战斗 <*-*> 

#### 3.2.2 pthread 练手

    利用构建好的 pthread 模块, 写个 Demo 练练手. 用的 api 是系统中关于读写锁相关操作. 为了
    解决大量消费者少量生产者的问题, 解决方案模型:

```C
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#define _INT_BZ     (13)
#define _INT_WTH    (2)
#define _INT_RTH    (10)

struct rwarg {
    pthread_t id;
    pthread_rwlock_t rwlock;    // 加锁用的
    int idx;                    // 指示buf中写到那了
    char buf[BUFSIZ];           // 存储临时数据
};

// 写线程, 主要随机写字符进去
void twrite(struct rwarg * arg);
// 读线程
void treads(struct rwarg * arg);

/*
 * 主函数测试线程读写逻辑
 * 少量写线程, 大量读线程测试
 */
int main(int argc, char * argv[]) {
    // 初始化定义需要使用的量. C99以上写法, 避免跨平台不同实现的警告问题, 感谢好人随性徜徉
    struct rwarg arg = { 0, .rwlock = PTHREAD_RWLOCK_INITIALIZER, 0, "" };    
    int i;

    // 读线程跑起来
    for(i = 0; i < _INT_RTH; ++i) 
        pthread_create((pthread_t *)&arg, NULL, (void * (*)(void *))treads, &arg);

    // 写线程再跑起来
    for(i = 0; i < _INT_WTH; ++i)
        pthread_create((pthread_t *)&arg, NULL, (void * (*)(void *))twrite, &arg);

    // 简单等待一下
    printf("sleep input enter:");
    getchar();

    return EXIT_SUCCESS;
}

// 写线程, 主要随机写字符进去
void 
twrite(struct rwarg * arg) {
    pthread_detach(pthread_self());

    pthread_rwlock_wrlock(&arg->rwlock);
    while(arg->idx < _INT_BZ) {
        arg->buf[arg->idx] = 'a' + arg->idx;
        ++arg->idx;
    }
    pthread_rwlock_unlock(&arg->rwlock);
}

// 读线程
void 
treads(struct rwarg * arg) {
    pthread_detach(pthread_self());
    
    while(arg->idx < _INT_BZ) {
        pthread_rwlock_rdlock(&arg->rwlock);
        puts(arg->buf);
        pthread_rwlock_unlock(&arg->rwlock);
    }
}
```

    因为手握 pthread 神器不知道写个啥, 随便写了上面点. 大量读者读加锁频繁, 少量写线程模型.
    可以临摹一遍, 感受一下远古时期那些妖魔大能之间, 天地昏暗, 万仞无边的气息~ 
    关于 POSIX 线程库 pthread 就到这里了. 看看头文件, 查查手册, 再不济看看源码一切都是那
	么自然.

### 3.3 读写锁

    pthread 已经提供了读写锁, 为什么还要没事瞎搞呢. 其实这个问题好理解. 1' 要剖析一下基
	本原理; 2' 它有点重, 不如用原子锁构造一个. 3' pthread 读写锁存在写竞争不过读的隐患.
    特别是3', 不妨把上面代码刷到演武场上演示演示. 会发现打印了大量空白, 说白了就是写锁被
	大量读锁阻塞了. (问题很严重) 

    下面对读写锁进行详细分析. 首先看下面有用的套话

	读写锁 是为了 解决, 大量 ''读'' 和 少量 ''写'' 的业务而设计的.  

	读写锁有3个特征:

	1'. 当读写锁是写加锁状态时，
	    在这个锁被解锁之前，所有试图对这个锁加锁的线程都会被阻塞

	2'. 当读写锁在读加锁状态时，
	    再以读模式对它加锁的线程都能得到访问权，但以写模式加锁的线程将会被阻塞

	3'. 当读写锁在读加锁状态时，
	    如果有线程试图以写模式加锁，读写锁通常会阻塞随后的读模式加锁

    从上面表述可以看出, pthread的线程库对于第三个特征没有完成. 默认还是平等竞争. 
	3' 默认写锁优先级高于读锁, 对其有遏制效果. 

#### 3.3.1 读写锁设计 interface

    通过上面3大特征和已经构建好的 scatom.h原子操作接口, 不妨设计如下读写锁接口 
	
scrwlock.h

```C
#ifndef _H_SIMPLEC_SCRWLOCK
#define _H_SIMPLEC_SCRWLOCK

#include "scatom.h"

/*
 * create simple write and read lock
 * struct rwlock need zero.
 * is scatom ext
 */

// init need all is 0
struct rwlock {
	int rlock;
	int wlock;
};

// add read lock
extern void rwlock_rlock(struct rwlock * lock);
// add write lock
extern void rwlock_wlock(struct rwlock * lock);

// add write lock
extern void rwlock_unrlock(struct rwlock * lock);
// unlock write
extern void rwlock_unwlock(struct rwlock * lock);

#endif // !_H_SIMPLEC_SCRWLOCK
```

    通过 scrwlock.h可以看出来这里读写锁是分别加锁和解锁的. pthread 线程库只走一个
    pthread_rwlock_unlock 这就是为啥读锁压制写锁的原因了, 因为它不做区分. 同等对待.
	当大量读锁出现的时候自然遏制写锁 (其实是策略问题, 没有高低)
    上面接口使用方法也灰常简单, 举例如下:

```C
struct rwarg {
    pthread_t id;
    struct rwlock rwlock;       // 加锁用的
    int idx;                    // 指示buf中写到那了
    char buf[BUFSIZ];           // 存储临时数据
};

// 初始化
struct rwarg arg = { 0 };

// 写线程, 主要随机写字符进去
...
    rwlock_wlock(&arg->rwlock);
    while(arg->idx < _INT_BZ) {
        arg->buf[arg->idx] = 'a' + arg->idx;
        ++arg->idx;
    }
    rwlock_unwlock(&arg->rwlock);
...

// 读线程
...
    while(arg->idx < _INT_BZ) {
        rwlock_rlock(&arg->rwlock);
        puts(arg->buf);
        rwlock_unrlock(&arg->rwlock);
    }
...
```

    本质就是两把交叉的锁模拟出一把读写锁. 来来回回, 虚虚实实, 随意潇洒~

#### 3.3.2 读写锁实现 implement

    这里展示的是详细的设计部分. 按照上面3个基准特征开始 write code, 首先看读加锁

```C
// add read lock
void 
rwlock_rlock(struct rwlock * lock) {
	for (;;) {
		// 看是否有人在试图读, 得到并防止代码位置优化
		while (lock->wlock)
			ATOM_SYNC();

		ATOM_INC(lock->rlock);
		// 没有写占用, 开始读了
		if (!lock->wlock)
			break;

		// 还是有写, 删掉添加的读
		ATOM_DEC(lock->rlock);
	}
}
```

    while (lock->wlock) ... 表示读锁为写锁让道. 随后得到资源后读锁获取资源开始引用加1. 
    再看看读解锁的实现:

```C
// unlock read lock
inline void 
rwlock_unrlock(struct rwlock * lock) {
	ATOM_DEC(lock->rlock);
}
```

    读解锁只是将读的锁值引用减1. 方便写加锁的时候判断是否有有人读. 再看看写加锁和解锁

```C
// add write lock
void 
rwlock_wlock(struct rwlock * lock) {
	ATOM_LOCK(lock->wlock);
	// 等待读占用锁
	while (lock->rlock)
		ATOM_SYNC();
}

// unlock write lock
inline void 
rwlock_unwlock(struct rwlock * lock) {
	ATOM_UNLOCK(lock->wlock);
}
```

    到这~关于读写锁的炫迈已经嚼完了. 读写锁使用常见, 能够想到就是网络IO中读写分离.
	很酷炫, 但不推荐, 因为(恐龙强大吗, 强大, 但是灭绝了) 它不是必须的~
    
### 3.4 设计协程库

    以上我们搞定了原子锁, 读写锁, POSIX 线程. 忘记说了有本很古老的 POSIX线程程序设计
    一本书很不错. 如果做专业的多线程开发那本书是必须的. 服务器开发行业最难的无外乎就是
	多线程和网络这两个方面了. 继续聊回来协程火起来的缘由(主要是我入行慢) 还是被 Lua 
	的 coroutine.create (f) 带起来. 这里将从系统层面分析协程库的实现细节. 

#### 3.4.1 协程库引言

    上层语言中协程比较常见. 例如C# 中 yield retrun, lua 中 coroutine.yield 等构建同步
	并发的程序. 本文是探讨如何从底层实现开发级别的协程库. 在说协程之前, 顺带温故一下进程和
	线程关系. 进程拥有一个完整的虚拟地址空间，不依赖于线程而独立存在. 线程是进程的一部分，
	没有自己的地址空间，与进程内的其他线程一起共享分配给该进程的所有资源. 进程和线程是1对
	多关系, 协程同线程关系也是类似. 一个线程中可以有多个协程. 协程同线程相比区别再于, 线程
	是操作系统控制调度(异步并发), 而线程是程序自身控制调度(同步串行). 
	简单总结协程特性如下:

　　  1. 相比线程具有更优的性能(假定, 程序写的没有明显失误) , 省略了操作系统的切换操作

　　  2. 相比线程占用更少的内存空间, 线程是操作系统对象很耗资源, 协程是用户态资源.

　　  3. 对比线程开发, 逻辑结构更复杂, 需要开发人员了解程序运行走向.

    举个例子 数码宝贝例子 : 滚球兽 ->  亚古兽－>  暴龙兽－>  机械暴龙兽 －> 战斗暴龙兽

![进化](./img/进化.jpg)

    '类比协程进化史' if .. else / switch -> goto -> setjmp / logjump -> coroutine -<
    协程开发是串行程序开发中构建异步效果的开发模型.

#### 3.4.2 协程库储备, winds 部分

    在 winds 有一种另一个东西叫做纤程 fiber.  官方说明是"Microsoft公司给Windows添加了一
	种纤程，以便能够非常容易地将现有的UNIX服务器应用程序移植到Windows中". 这就是纤程概念的
	由来. 在这里会详细解释其中关于 winds fiber常用 api. 
	先浏览关于当前线程开启纤程相关接口说明.

```C
//
// Fiber creation flags
//
#define FIBER_FLAG_FLOAT_SWITCH 0x1     // context switch floating point

/*
 * VS编译器特性约定
 * 1. 其参数都是从右向左通过堆栈传递的
 * 2. 函数调用在返回前要由被调用者清理堆栈(被调用函数弹出的时候销毁堆栈)
 */
#define WINAPI      __stdcall

/*
 * 将当前线程转成纤程, 返回转换成功的主纤程对象域
 * lpParameter    : 转换的时候传入到主线程中用户数据
 * dwFlags        : 附加参数, 默认填写 FIBER_FLAG_FLOAT_SWITCH
 *                : 返回转换成功后的主纤程对象域
 */
WINBASEAPI __out_opt LPVOID WINAPI ConvertThreadToFiberEx(
    __in_opt LPVOID lpParameter,
    __in DWORD dwFlags
);

// 得到当前纤程中用户传入的数据, 就是上面 lpParameter
__inline PVOID GetFiberData(void)    { return *(PVOID *) (ULONG_PTR) __readfsdword (0x10); }

// 得到当前运行纤程对象
__inline PVOID GetCurrentFiber(void) { return (PVOID) (ULONG_PTR) __readfsdword (0x10); }
                                                          
/*
 * 将当前纤程转换成线程, 对映ConvertThreadToFiberEx操作系列函数. 返回原始环境
 *                : 返回成功状态, TRUE标识成功
 */
WINBASEAPI BOOL WINAPI ConvertFiberToThread(VOID);
```

    下面是关于如何创建纤程并切换(启动)官方接口说明.

```C
// 标识纤程执行体的注册函数声明, lpFiberParameter 可以通过 GetFiberData 得到
typedef VOID (WINAPI * PFIBER_START_ROUTINE)(LPVOID lpFiberParameter);
typedef PFIBER_START_ROUTINE LPFIBER_START_ROUTINE;

/*
 * 创建一个没有启动纤程对象并返回
 * dwStackCommitSize    : 当前纤程栈大小, 0标识默认大小
 * dwStackReserveSize   : 当前纤程初始化化保留大小, 0标识默认大小
 * dwFlags              : 纤程创建状态, 默认FIBER_FLAG_FLOAT_SWITCH, 支持浮点数操作
 * lpStartAddress       : 指定纤程运行的载体.等同于纤程执行需要指明执行函数
 * lpParameter          : 纤程执行的时候, 传入的用户数据, 在纤程中GetFiberData可以得到
 *                      : 返回创建好的纤程对象 
 */                                              
WINBASEAPI __out_opt LPVOID WINAPI CreateFiberEx(
    __in     SIZE_T dwStackCommitSize,
    __in     SIZE_T dwStackReserveSize,
    __in     DWORD dwFlags,
    __in     LPFIBER_START_ROUTINE lpStartAddress,
    __in_opt LPVOID lpParameter
);

// 销毁一个申请的纤程资源和CreateFiberEx成对出现
WINBASEAPI VOID WINAPI DeleteFiber(__in LPVOID lpFiber);

// 纤程跳转, 跳转到lpFiber指定的纤程
WINBASEAPI VOID WINAPI SwitchToFiber(__in LPVOID lpFiber);
```

    通过上面解释过的 api 写一个基础的演示 demo , fiber.c. 实践能补充猜想:

```C
#include <stdio.h>
#include <windows.h>

static void WINAPI _fiber_run(LPVOID fiber) {
	puts("_fiber_run begin");
	// 切换到主纤程中
	SwitchToFiber(fiber);
	puts("_fiber_run e n d");

	// 主动切换到主纤程中, 子纤程不会主动切换到主纤程
	SwitchToFiber(fiber);
}

//
// winds fiber hello world
//
int main(int argc, char * argv[]) {
	PVOID fiber, fiberc;
	// A pointer to a variable that is passed to the fiber. 
	// The fiber can retrieve this data by using the GetFiberData macro.
    fiber = ConvertThreadToFiberEx(NULL, FIBER_FLAG_FLOAT_SWITCH);
	// 创建普通纤程, 当前还是在主纤程中
	fiberc = CreateFiberEx(0, 0, FIBER_FLAG_FLOAT_SWITCH, _fiber_run, fiber);
	puts("main ConvertThreadToFiberEx begin");

	SwitchToFiber(fiberc);
	puts("main ConvertThreadToFiberEx SwitchToFiber begin");
	
	SwitchToFiber(fiberc);
	puts("main ConvertThreadToFiberEx SwitchToFiber again begin");

	DeleteFiber(fiberc);
	ConvertFiberToThread();
	puts("main ConvertThreadToFiberEx e n d");
	return EXIT_SUCCESS;
}
```

    总结起来运用纤程的步骤无外乎如下, 以两个纤程举例:
        1、使用ConverThreadToFiber(Ex)将当前线程转换到纤程，这是纤程F1
        2、定义一个纤程函数，用于创建一个新纤程
        3、纤程F1中调用CreateFiber(Ex)函数创建一个新的纤程F2
        4、SwitchToFiber函数进行纤程切换，让新创建的纤程F2执行
        5、F2纤程函数执行完毕的时候，使用SwitchToFiber转换到F1
        6、在纤程F1中调用DeleteFiber来删除纤程F2
        7、纤程F1中调用ConverFiberToThread，转换为线程
        8、线程结束

    上面的测试代码执行最终结果如下, 更加详细的, 呵呵只能靠自己, winds 深入资料不多

![fiber](./img/fiber.png)

    winds fiber 储备部分画上句号了. 现在市场上 winds高级工程师很少了, 因为功法少, 
	太邪乎了. 吃亏不讨好~ (从买的书籍上看抛开老美, 韩国棒子对 winds研究的比较深入)

#### 3.4.3 协程储备, linux 部分

    winds 纤程出现的本源自于 unix. 而一脉而下的 linux也有这类机制. 自己称之为上下
	文 ucp 对象, 上下文记录跳转机制. 翻译了些高频率用的 api 手册如下:

```C
#include <ucontext.h>

/*
 * 得到当前程序运行此处上下文信息
 * ucp        : 返回当前程序上下文并保存在ucp指向的内存中
 *            : -1标识失败, 0标识成功
 */
int getcontext(ucontext_t * ucp);

/*
 * 设置到执行程序上下文对象中. 
 * ucp        : 准备跳转的上下文对象
 *            : 失败返回-1. 成功不返回
 */
int setcontext(const ucontext_t * ucp);

/*
 * 重新设置ucp上下文. 
 * ucp      : 待设置的上下文对象
 * func     : 新上下文执行函数体, 其实gcc认为声明是void * func(void)
 * argc     : func 函数参数个数
 * ...      : 传入func中的可变参数, 默认都是 int类型
 */
void makecontext(ucontext_t * ucp, void (* func)(), int argc, ...);

/*
 * 保存当前上下文对象 oucp, 并且跳转到执行上下文件对象 ucp 中
 * oucp       : 保存当前上下文对象
 * ucp        : 执行的上下文对象
 *            : 失败返回-1, 成功不返回
 */
int swapcontext (ucontext_t * ucp, ucontext_t * ucp);
```

    相比 winds fiber确实很清爽. 扩充一下 ucontext_t 一种实现结构

```C
/* Userlevel context.  */
typedef struct ucontext {
     unsigned long int uc_flags;
     struct ucontext * uc_link;                // 下一个执行的序列, NULL不继续执行了
     stack_t uc_stack;                         // 当前上下文, 堆栈信息
     mcontext_t uc_mcontext;
     __sigset_t uc_sigmask;
    struct _libc_fpstate __fpregs_mem;
} ucontext_t;

/* Alternate, preferred interface.  */
typedef struct sigaltstack {
    void * ss_sp;                             // 指向当前堆栈信息首地址
    int ss_flags;
    size_t ss_size;                           // 当前堆栈大小
} stack_t;
```

    上面加了中文注释的部分, 就是我们开发中需要用到的几个字段. 设置执行顺序, 指定当前上下文
    堆栈信息. 有了这些知识, 我们在 linux上练练手, 演示一下结果:

```C
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <ucontext.h>

#define PERROR_EXIT(msg) \
    do { perror(msg); exit(EXIT_FAILURE); } while (0)

static void _func1(uint32_t low32, uint32_t hig32) {
    // 得到所传入的指针类型
    uintptr_t ptr = (uintptr_t)low32 | ((uintptr_t)hig32 << 32);
    ucontext_t * ucts = (ucontext_t *)ptr;

    // 开始操作
    puts("func1: started");
    puts("func1: swapcontext(ucts + 1, ucts + 2)");
    if (swapcontext(ucts + 1, ucts + 2) < 0)
        PERROR_EXIT("swapcontext");
    puts("func1: returning");
}

static void _func2(uint32_t low32, uint32_t hig32) {
    uintptr_t ptr = (uintptr_t)low32 | ((uintptr_t)hig32 << 32);
    ucontext_t * ucts = (ucontext_t *)ptr;

    puts("func2: started");
    puts("func2: swapcontext(ucts + 2, ucts + 1)");
    if (swapcontext(ucts + 2, ucts + 1) < 0)
        PERROR_EXIT("swapcontext");
    puts("func2: returning");
}

//
// use ucontext hello world
//
int main(int argc, char * argv[]) {
    ucontext_t ucts[3];
    char stack1[16384];
    char stack2[16384];
    uintptr_t ptr = (uintptr_t)ucts;
    uint32_t low32 = (uint32_t)ptr;
    uint32_t hig32 = (uint32_t)(ptr >> 32);

    if (getcontext(ucts + 1) < 0)
        PERROR_EXIT("getcontext");
    ucts[1].uc_stack.ss_sp = stack1;
    ucts[1].uc_stack.ss_size = sizeof stack1;
    // ucts[1] -> ucts[0]
    ucts[1].uc_link = ucts;
    makecontext(ucts + 1, (void (*)())_func1, 2, low32, hig32);

    // 开始第二个搞
    if (getcontext(ucts + 2) < 0)
        PERROR_EXIT("getcontext");
    ucts[2].uc_stack.ss_sp = stack2;
    ucts[2].uc_stack.ss_size = sizeof stack2;
    // ucts[2] -> ucts[1]
    ucts[2].uc_link = ucts + 1;
    makecontext(ucts + 2, (void (*)())_func2, 2, low32, hig32);

    puts("main: swapcontext(ucts, ucts + 2)");
    if (swapcontext(ucts, ucts + 2) < 0)
        PERROR_EXIT("swapcontext");

    puts("main: exiting");
    return EXIT_SUCCESS;
}
```

    linux 接口很自然, 执行流程很清晰. (上面也可以深入封装, 去掉重复过程) 最终结果如下

![ucontext](./img/ucontext.png)

    很多时候我们写代码, 或者说在模仿代码的时候. 花点心思 也许 就是突破.

#### 3.4.4 一切就绪, 那就开始协程设计吧

    关于协程的设计主要围绕打开关闭创建切换阻塞几个操作

```C
#ifndef _H_SIMPLEC_SCOROUTINE
#define _H_SIMPLEC_SCOROUTINE

#define SCO_DEAD		(0)		// 协程死亡状态
#define SCO_READY		(1)		// 协程已经就绪
#define SCO_RUNNING		(2)		// 协程正在运行
#define SCO_SUSPEND		(3)		// 协程暂停等待

// 协程管理器
typedef struct scomng * scomng_t;

//
// 注册的协程体
// sco		: 创建开启的协程总对象
// arg		: 用户创建协程的时候传入的参数
//
typedef void (* sco_f)(scomng_t sco, void * arg);

//
// sco_open - 开启协程系统函数, 并返回创建的协程管理器
// return	: 返回创建的协程对象
//
extern scomng_t sco_open(void);

//
// sco_close - 关闭已经开启的协程系统函数
// sco		: sco_oepn 返回的当前协程中协程管理器
//
extern void sco_close(scomng_t sco);

//
// sco_create - 创建一个协程, 此刻是就绪态
// sco		: 协程管理器
// func		: 协程体执行的函数体
// arg		: 协程体中传入的参数
// return	: 返回创建好的协程id
//
extern int sco_create(scomng_t sco, sco_f func, void * arg);

//
// sco_resume - 通过协程id激活协程
// sco		: 协程系统管理器
// id		: 具体协程id, sco_create 返回的协程id
//
extern void sco_resume(scomng_t sco, int id);

//
// sco_yield - 关闭当前正在运行的协程, 让协程处理暂停状态
// sco		: 协程系统管理器
//
extern void sco_yield(scomng_t sco);

//
// sco_status - 得到当前协程状态
// sco		: 协程系统管理器
// id		: 协程id
// return	: 返回 _SCO_* 相关的协程状态信息
//
extern int sco_status(scomng_t sco, int id);

//
// sco_running - 当前协程系统中运行的协程id
// sco		: 协程系统管理器
// return	: 返回 < 0 表示没有协程在运行
//
extern int sco_running(scomng_t sco);

#endif // !_H_SIMPLEC_SCOROUTINE
```

    通过上面接口设计不妨给出一段测试代码. 感受接口的用法. 测试真是个好东西

```C
#include <stdio.h>
#include <stdlib.h>
#include "scoroutine.h"

#define _INT_TEST	(5)

struct args {
	int n;
};

static void _foo(scomng_t sco, struct args * as) {
	int start = as->n;
	int i = -1;

	while (++i < _INT_TEST) {
		printf("coroutine %d : %d.\n", sco_running(sco), start + i);
		sco_yield(sco);
	}
}

static void _test(void * sco) {
	struct args argo = { 000 };
	struct args argt = { 100 };

	int coo = sco_create(sco, (sco_f)_foo, &argo);
	int cot = sco_create(sco, (sco_f)_foo, &argt);

	puts("********************_test start********************");
	while (sco_status(sco, coo) && sco_status(sco, cot)) {
		sco_resume(sco, coo);
		sco_resume(sco, cot);
	}
	puts("********************_test e n d********************");
}

//
// 测试主函数, 主要测试协程使用
//
int main(void) {
	scomng_t sco = sco_open();

	puts("--------------------突然想起了什么,--------------------\n");
	_test(sco);

	// 再来测试一下, 纤程切换问题
	struct args arg = { 222 };
	int co = sco_create(sco, (sco_f)_foo, &arg);
	for (int i = -1; i < _INT_TEST; ++i)
		sco_resume(sco, co);

	puts("\n--------------------笑了笑, 我自己.--------------------");
	sco_close(sco);
}
```

    不妨提前剧透结果, 也能通过执行流程分析出来主要就是 resume 和 yield 来回切:

![协程测试结果](./img/协程测试结果.png)

    扯一点, 这里用了个 (sco_f)_foo 编译时替换运行时 struct args * as = arg 更快些.
	当然也可以通过宏伪造函数

#### 3.4.5 协程库的初步实现

    讲的有点琐碎, 主要还是需要通过代码布局感受作者意图. 这里协程库实现总思路是 winds
	实现一份, liunx 实现一份. 如何蹂在一起, 请看下面布局

scoroutine.c

```C
// Compiler Foreplay
#if !defined(_MSC_VER) && !defined(__GNUC__)
#	error "error : Currently only supports the Best New CL and GCC!"
#endif

#include <string.h>
#include <stdlib.h>
#include <assert.h>

// 默认协程栈大小 和 初始化协程数量
#define _INT_STACK		(256 * 1024)
#define _INT_COROUTINE	(16)

#include "scoroutine$winds.h"
#include "scoroutine$linux.h"

//
// sco_status - 得到当前协程状态
// sco		: 协程系统管理器
// id		: 协程id
// return	: 返回 SCO_* 相关的协程状态信息
//
inline int
sco_status(scomng_t sco, int id) {
	assert(sco && id >= 0 && id < sco->cap);
	return sco->cos[id] ? sco->cos[id]->status : SCO_DEAD;
}

//
// sco_running - 当前协程系统中运行的协程id
// sco		: 协程系统管理器
// return	: 返回 < 0 表示没有协程在运行
//
inline int
sco_running(scomng_t sco) {
	return sco->running;
}
```

    多说无益. 上面数值宏统一了每个协程栈大小, 协程数量等基础共性. 不同平台不同局部实现文件
	对映. 使用了 $ 符号表示当前头文件是私有头文件, 局部的. 以前流传用 - 符号. 缺陷是 - 符
	号不可以在 .h 和 .c 文件中识别出来. 为了和谐统一巧妙(or Sha bi)用 $ 替代了 - .
    开始了, 下面逐个分析协程库的不同平台的实现部分~

#### 3.4.5 scoroutine$winds.h

    对于这种框架基础库方面设计, 懂了好懂, 不懂有点难受. 难受就是进步的契机. 功法修炼还是循序
	渐进, 先从设计结构入手

```C
#if !defined(_H_SIMPLEC_SCOROUTINE$WINDS) && defined(_MSC_VER)
#define _H_SIMPLEC_SCOROUTINE$WINDS

#include <windows.h>
#include "scoroutine.h"

// 声明协程结构 和 协程管理器结构
struct sco {
	PVOID ctx;			// 当前协程运行的环境
	sco_f func;			// 协程体执行
	void * arg;			// 用户输入的参数
	int status;			// 当前协程运行状态 SCO_*
};

// 构建 struct sco 协程对象
static inline struct sco * _sco_new(sco_f func, void * arg) {
	struct sco * co = malloc(sizeof(struct sco));
	assert(co && func);
	co->func = func;
	co->arg = arg;
	co->status = SCO_READY;
	return co;
}

struct scomng {
	PVOID main;			// 当前主协程记录运行环境
	int running;		// 当前协程中运行的协程id

	struct sco ** cos;	// 协程对象集, 循环队列
	int cap;			// 协程对象集容量
	int idx;			// 当前协程集中轮询到的索引
	int cnt;			// 当前存在的协程个数
};

// 销毁一个协程对象
static inline void _sco_delete(struct sco * co) {
	DeleteFiber(co->ctx);
	free(co);
}

#endif // !_H_SIMPLEC_SCOROUTINE$WINDS
```

    SCO_READY 表示准备状态, 协程管理器内部维护了一个简易状态机. 方便记录当前协程是啥状况.
	大体可以总结为这样
		co_create   -> CS_Ready
		co_resume   -> CS_Running
		co_yield    -> CS_Suspend
    协程运行完毕后就是 CS_Dead. 主协程默认一直运行不参与状态切换中. 协调控制所有子协程.
    最后就是线程中开启协程和关闭协程操作:

```C
inline scomng_t
sco_open(void) {
	struct scomng * comng = malloc(sizeof(struct scomng));
	assert(NULL != comng);
	comng->running = -1;
	comng->cos = calloc(_INT_COROUTINE, sizeof(struct sco *));
	comng->cap = _INT_COROUTINE;
	comng->idx = 0;
	comng->cnt = 0;
	assert(NULL != comng->cos);
	// 在当前线程环境中开启Window协程
	comng->main = ConvertThreadToFiberEx(NULL, FIBER_FLAG_FLOAT_SWITCH);
	return comng;
}

void
sco_close(scomng_t sco) {
	int i = -1;
	while (++i < sco->cap) {
		struct sco * co = sco->cos[i];
		if (co) {
			_sco_delete(co);
			sco->cos[i] = NULL;
		}
	}

	free(sco->cos);
	sco->cos = NULL;
	free(sco);
	// 切换当前协程系统变回默认的主线程, 关闭协程系统
	ConvertFiberToThread();
}
```

    大头戏逐渐来了, 创建, 启动, 阻塞

```C
int
sco_create(scomng_t sco, sco_f func, void * arg) {
	struct sco * co = _sco_new(func, arg);
	struct sco ** cos = sco->cos;
	int cap = sco->cap;
	// 下面开始寻找, 如果数据足够的话
	if (sco->cnt < sco->cap) {
		// 当循环队列去查找
		int idx = sco->idx;
		do {
			if (NULL == cos[idx]) {
				cos[idx] = co;
				++sco->cnt;
				++sco->idx;
				return idx;
			}
			idx = (idx + 1) % cap;
		} while (idx != sco->idx);

		assert(idx == sco->idx);
		return -1;
	}

	// 这里需要重新构建空间
	cos = realloc(cos, sizeof(struct sco *) * cap * 2);
	assert(NULL != cos);
	memset(cos + cap, 0, sizeof(struct sco *) * cap);
	sco->cos = cos;
	sco->cap = cap << 1;
	++sco->cnt;
	cos[sco->idx] = co;
	return sco->idx++;
}

void
sco_yield(scomng_t sco) {
	struct sco * co;
	int id = sco->running;
	if ((id < 0 || id >= sco->cap) || !(co = sco->cos[id]))
		return;
	co->status = SCO_SUSPEND;
	sco->running = -1;
	co->ctx = GetCurrentFiber();
	SwitchToFiber(sco->main);
}
```

    以上是创建一个协程和挂起协程将操作顺序交给主协程. 随后构建最重要的一环激活协程.
    comng::cos 中保存所有的协程对象, 不够就 realloc, 够直接返回. 其中查询用的协程
	对象循环查找. 协程之间的跳转采用先记录当前环境, 后跳转思路.

```C
static inline VOID WINAPI _sco_main(struct scomng * comng) {
	int id = comng->running;
	struct sco * co = comng->cos[id];
	// 执行协程体
	co->func(comng, co->arg);
	co = comng->cos[id];
	co->status = SCO_DEAD;
	// 跳转到主纤程体中销毁
	SwitchToFiber(comng->main);
}

void
sco_resume(scomng_t sco, int id) {
	struct sco * co;
	int running;

	assert(sco && id >= 0 && id < sco->cap);

	// SCO_DEAD 状态协程, 完全销毁其它协程操作
	running = sco->running;
	if (running != -1) {
		co = sco->cos[running];
		assert(co && co->status == SCO_DEAD);
		sco->cos[running] = NULL;
		--sco->cnt;
		sco->idx = running;
		sco->running = -1;
		_sco_delete(co);
		if (running == id)
			return;
	}

	// 下面是协程 SCO_READY 和 SCO_SUSPEND 处理
	co = sco->cos[id];
	if ((!co) || (co->status != SCO_READY && co->status != SCO_SUSPEND))
		return;

	// Window特性创建纤程, 并保存当前上下文环境, 切换到创建的纤程环境中
	if (co->status == SCO_READY)
		co->ctx = CreateFiberEx(_INT_STACK, 0, 
								FIBER_FLAG_FLOAT_SWITCH, 
								(LPFIBER_START_ROUTINE)_sco_main, sco);

	co->status = SCO_RUNNING;
	sco->running = id;
	sco->main = GetCurrentFiber();
	// 正常逻辑切换到创建的子纤程中
	SwitchToFiber(co->ctx);
}
```

    关于 winds部分实现协程的功能基本都稿完了. 就是数据结构和系统接口的一套杂糅.
	重点看围绕状态切换那些部分~

#### 3.4.6 scoroutine$linux.h

    关于 linux部分封装, 相比 winds只是多了写操作细节. 主要理解状态切换的那块

```C
#if !defined(_H_SIMPLEC_SCOROUTINE$LINUX) && defined(__GNUC__)
#define _H_SIMPLEC_SCOROUTINE$LINUX

#include <scoroutine.h>
#include <ucontext.h>
#include <stddef.h>
#include <stdint.h>

// 声明协程结构 和 协程管理器结构
struct sco {
	char * stack;			// 当前协程栈指针
	ucontext_t ctx;			// 当前协程运行的上下文环境
	ptrdiff_t cap;			// 当前栈的容量
	ptrdiff_t cnt;			// 当前栈的大小

	sco_f func;				// 协程体执行
	void * arg;				// 用户输入的参数
	int status;				// 当前协程运行状态 SCO_*
};

// 构建 struct sco 协程对象
static inline struct sco * _sco_new(sco_f func, void * arg) {
	struct sco * co = malloc(sizeof(struct sco));
	assert(co && func);
	co->func = func;
	co->arg = arg;
	co->status = SCO_READY;

	co->stack = NULL;
	co->cap = 0;
	co->cnt = 0;

	return co;
}

// 销毁一个协程对象
static inline void _sco_delete(struct sco * co) {
	free(co->stack);
	free(co);
}

struct scomng {
	char stack[_INT_STACK];	// 当前协程中开辟的栈对象
	ucontext_t main;		// 当前协程上下文对象

	int running;			// 当前协程中运行的协程id

	struct sco ** cos;		// 协程对象集, 循环队列
	int cap;				// 协程对象集容量
	int idx;				// 当前协程集中轮询到的索引
	int cnt;				// 当前存在的协程个数
};

inline scomng_t
sco_open(void) {
	struct scomng * comng = malloc(sizeof(struct scomng));
	assert(NULL != comng);
	comng->running = -1;
	comng->cos = calloc(_INT_COROUTINE, sizeof(struct sco *));
	comng->cap = _INT_COROUTINE;
	comng->idx = 0;
	comng->cnt = 0;
	assert(NULL != comng->cos);
	return comng;
}

void
sco_close(scomng_t sco) {
	int i = -1;
	while (++i < sco->cap) {
		struct sco * co = sco->cos[i];
		if (co) {
			_sco_delete(co);
			sco->cos[i] = NULL;
		}
	}

	free(sco->cos);
	sco->cos = NULL;
	free(sco);
}

int
sco_create(scomng_t sco, sco_f func, void * arg) {
	struct sco * co = _sco_new(func, arg);
	struct sco ** cos = sco->cos;
	int cap = sco->cap;
	// 下面开始寻找, 如果数据足够的话
	if (sco->cnt < sco->cap) {
		// 当循环队列去查找
		int idx = sco->idx;
		do {
			if (NULL == cos[idx]) {
				cos[idx] = co;
				++sco->cnt;
				++sco->idx;
				return idx;
			}
			idx = (idx + 1) % cap;
		} while (idx != sco->idx);

		assert(idx == sco->idx);
		return -1;
	}

	// 这里需要重新构建空间
	cos = realloc(cos, sizeof(struct sco *) * cap * 2);
	assert(NULL != cos);
	memset(cos + cap, 0, sizeof(struct sco *) * cap);
	sco->cos = cos;
	sco->cap = cap << 1;
	++sco->cnt;
	cos[sco->idx] = co;
	return sco->idx++;
}

// 协程运行的主体
static inline void _sco_main(uint32_t low32, uint32_t hig32) {
	uintptr_t ptr = (uintptr_t)low32 | ((uintptr_t)hig32 << 32);
	struct scomng * comng = (struct scomng *)ptr;
	int id = comng->running;
	struct sco * co = comng->cos[id];
	// 执行协程体
	co->func(comng, co->arg);
	co = comng->cos[id];
	co->status = SCO_DEAD;
	_sco_delete(co);
	comng->cos[id] = NULL;
	--comng->cnt;
	comng->idx = id;
	comng->running = -1;
}

void
sco_resume(scomng_t sco, int id) {
	uintptr_t ptr;
	struct sco * co;
	int status;
	int running = sco->running;
	assert(running == -1 && id >= 0 && id < sco->cap);

	// 下面是协程 SCO_READY 和 SCO_SUSPEND 处理
	co = sco->cos[id];
	if ((!co) || (status = co->status) == SCO_DEAD)
		return;

	sco->running = id;
	co->status = SCO_RUNNING;
	switch (status) {
	case SCO_READY:
		// 兼容x64指针通过makecontext传入
		ptr = (uintptr_t)sco;
		// 构建栈和运行链
		getcontext(&co->ctx);
		co->ctx.uc_stack.ss_sp = sco->stack;
		co->ctx.uc_stack.ss_size = _INT_STACK;
		co->ctx.uc_link = &sco->main;
		makecontext(&co->ctx, (void(*)())_sco_main, 2, (uint32_t)ptr, (uint32_t)(ptr >> 32));
		// 保存当前运行状态到sco->main, 然后跳转到 co->ctx运行环境中
		swapcontext(&sco->main, &co->ctx);
		break;
	case SCO_SUSPEND:
		// stack add is high -> low
		memcpy(sco->stack + _INT_STACK - co->cnt, co->stack, co->cnt);
		swapcontext(&sco->main, &co->ctx);
		break;
	default:
		assert(co->status && 0);
	}
}

// 保存当前运行的堆栈信息
static void _sco_savestack(struct sco * co, char * top) {
	char dummy = 0;
	ptrdiff_t size = top - &dummy;
	assert(size <= _INT_STACK);
	if (co->cap < size) {
		free(co->stack);
		co->cap = size;
		co->stack = malloc(size);
	}
	co->cnt = size;
	memcpy(co->stack, &dummy, size);
}

void
sco_yield(scomng_t sco) {
	struct sco * co;
	int id = sco->running;
	if ((id < 0 || id >= sco->cap) || !(co = sco->cos[id]))
		return;
	assert((char *)&co > sco->stack);
	_sco_savestack(co, sco->stack + _INT_STACK);
	co->status = SCO_SUSPEND;
	sco->running = -1;
	swapcontext(&co->ctx, &sco->main);
}

#endif // !_H_SIMPLEC_SCOROUTINE$LINUX
```

    需要注意的几点就是 makecontext 默认可变参数全是 int 类型, 所以注意 x64指针拆分.
    当我们要 sco_yield 的时候需要保存当前阻塞的栈状态 _sco_savestack, 方便恢复.
    目前疯狂贴代码, 但愿下次不会了, 毕竟跨平台代码很多时候都是粗暴通过宏分支解决.
    后面赠送个 time 时间模块的代码, 做为这个金丹功法的额外赠送. 重复一下下: 
	程序员世界看数据结构和操作系统, 同样自然世界看得是

*All knowledge is, in final analysis, history.*

*All sciences are, in the abstract, mathematics.*

*All judgements are, in their rationale, statistics.*

### 3.5 高效的时间业务库

    底层库一定会有它, 时间业务帮助库. 简单基础必须. 例如业务常见字符串和时间戳来回转.
    是否同一天, 同一周, 时间开始点什么鬼. 那么阅读理解开始

```C
#ifndef _H_SIMPLEC_SCTIMEUTIL
#define _H_SIMPLEC_SCTIMEUTIL

#include <time.h>
#include <stdbool.h>

//
// 1s = 1000ms = 1000000us = 1000000000ns
// 1秒  1000毫秒  1000000微秒  1000000000纳秒
// ~ 力求最小时间业务单元 ~ 
//

#ifdef __GNUC__

#include <unistd.h>
#include <sys/time.h>

//
// sh_msleep - 睡眠函数, 时间颗粒度是毫秒.
// m		: 待睡眠的毫秒数
// return	: void
//
#define sh_msleep(m) \
		usleep(m * 1000)

#endif

// 为Visual Studio导入一些和linux上优质思路
#ifdef _MSC_VER

#include <windows.h>

#define sh_msleep(m) \
		Sleep(m)

//
// usleep - 微秒级别等待函数
// usec		: 等待的微秒
// return	: The usleep() function returns 0 on success.  On error, -1 is returned.
//
extern int usleep(unsigned usec);

/*
 * 返回当前得到的时间结构体, 高仿linux上调用
 * pt	: const time_t * , 输入的时间戳指针
 * ptm	: struct tm * , 输出的时间结构体
 *		: 返回 ptm 值
 */
#define localtime_r(pt, ptm) localtime_s(ptm, pt), ptm

#endif

// 定义时间串类型
#define _INT_STULEN (64)
typedef char stime_t[_INT_STULEN];

/*
 * 将 [2016-7-10 21:22:34] 格式字符串转成时间戳
 * tstr	: 时间串分隔符只能是单字节的.
 * pt	: 返回得到的时间戳
 * otm	: 返回得到的时间结构体
 *		: 返回false表示构造失败
 */
extern bool stu_gettime(stime_t tstr, time_t * pt, struct tm * otm);

/*
 * 判断当前时间戳是否是同一天的.
 * lt : 判断时间一
 * rt : 判断时间二
 *    : 返回true表示是同一天, 返回false表示不是
 */
extern bool stu_tisday(time_t lt, time_t rt);

/*
 * 判断当前时间戳是否是同一周的.
 * lt : 判断时间一
 * rt : 判断时间二
 *    : 返回true表示是同一周, 返回false表示不是
 */
extern bool stu_tisweek(time_t lt, time_t rt);

//
// stu_sisday - 判断当前时间串是否是同一天的.
// ls : 判断时间一
// rs : 判断时间二
//    : 返回true表示是同一天, 返回false表示不是
//
extern bool stu_sisday(stime_t ls, stime_t rs);

//
// 判断当前时间串是否是同一周的.
// ls : 判断时间一
// rs : 判断时间二
//    : 返回true表示是同一周, 返回false表示不是
//
extern bool stu_sisweek(stime_t ls, stime_t rs);

/*
 * 将时间戳转成时间串 [2016-07-10 22:38:34]
 * nt	: 当前待转的时间戳
 * tstr	: 保存的转后时间戳位置
 *		: 返回传入tstr的首地址
 */
extern char * stu_gettstr(time_t nt, stime_t tstr);

/*
 * 得到当前时间戳 [2016-7-10 22:38:34]
 * tstr	: 保存的转后时间戳位置
 *		: 返回传入tstr的首地址
 */
extern char * stu_getntstr(stime_t tstr);

//
// stu_getmstr - 得到加毫秒的串 [2016-07-10 22:38:34 500]
// tstr		: 保存最终结果的串
// return	: 返回当前串长度
//
#define _STR_MTIME			"%04d-%02d-%02d %02d:%02d:%02d %03ld"
extern size_t stu_getmstr(stime_t tstr);

//
// stu_getmstrn - 得到特定包含时间串, fmt 依赖 _STR_MTIME
// buf		: 保存最终结果的串
// len		: 当前buf串长度
// fmt		: 输出格式串例如 -> "simplec-%04d%02d%02d-%02d%02d%02d-%03ld.log"
// return	: 返回当前串长度
//
extern size_t stu_getmstrn(char buf[], size_t len, const char * const fmt);

#endif // !_H_SIMPLEC_SCTIMEUTIL
```

    下面开始剖析它了, 推荐随后的代码可以全部拔到你的项目中. 也算是久经考验的忠诚
	战斗士. 首先看一个飘逸的字符串解析为系统时间结构的函数

```C
// 从时间串中提取出来年月日时分秒
static bool _stu_gettm(stime_t tstr, struct tm * otm) {
	char c;
	int sum, * py, * es;

	if ((!tstr) || !(c = *tstr) || c < '0' || c > '9')
		return false;

	py = &otm->tm_year;
	es = &otm->tm_sec;
	sum = 0;
	while ((c = *tstr) && py >= es) {
		if (c >= '0' && c <= '9') {
			sum = 10 * sum + c - '0';
			++tstr;
			continue;
		}

		*py-- = sum;
		sum = 0;

		// 去掉特殊字符, 一直找到下一个数字
		while ((c = *++tstr) && (c < '0' || c > '9'))
			;
	}
	// 非法, 最后解析出错
	if (py != es)
		return false;

	*es = sum; // 保存最后秒数据
	return true;
}

bool
stu_gettime(stime_t tstr, time_t * pt, struct tm * otm) {
	time_t t;
	struct tm st;

	// 先高效解析出年月日时分秒
	if (!_stu_gettm(tstr, &st))
		return false;

	st.tm_year -= 1900;
	st.tm_mon -= 1;
	// 得到时间戳, 失败返回false
	if ((t = mktime(&st)) == -1)
		return false;

	// 返回最终结果
	if (pt)
		*pt = t;
	if (otm)
		*otm = st;

	return true;
}
```

    又好又快, 思路是围绕解析时间字符串, 分隔为一系列的数值. 再巧妙利用指针移位赋值.
    继续看两个时间戳是否是同一天的小学数学分析

```C
inline bool
stu_tisday(time_t lt, time_t rt) {
	// GMT [World] + 8 * 3600 = CST [China], 得到各自当前天数
	lt = (lt + 8UL * 3600) / (24 * 3600);
	rt = (rt + 8UL * 3600) / (24 * 3600);
	return lt == rt;
}
```

    8UL * 3600 科普一下, GMT(Greenwich Mean Time) 代表格林尼治标准时间, 也是咱们代码中
    time(NULL) 返回的时间戳. 而中国北京标准时间采用的 CST(China Standard Time UT+8:00). 
	因而需要在原先的标准时间戳基础上加上 8h, 就得到咱们中国帝都的时间戳. 
	说道时间业务上面, 推荐用新的标准函数 timespec_get 替代 gettimeofday! 精度更高, 更规范.
	对于 gettimeofday 还有 usleep linux 上常用函数, 我们在用到的时候回给出详细 winds 实现.

	扩展一点, 假如有个策划需求, 我们规定一天的开始时间是 5时0分0秒. 现实世界默认一天开始时
	间是 0时0分0秒. 那你会怎么做呢 ? 

	其实有很多方式, 只要计算好偏移量就可以. 例如我们假如在底层支持. 可以这么写

```C
#define _INT_DAYNEWSTART	( 0UL * 3600 + 0 * 60 + 0)
inline bool
stu_tisday(time_t lt, time_t rt) {
	// GMT [World] + 8 * 3600 = CST [China], 得到各自当前天数
	lt = (lt + 8UL * 3600 - _INT_DAYNEWSTART) / (24 * 3600);
	rt = (rt + 8UL * 3600 - _INT_DAYNEWSTART) / (24 * 3600);
	return lt == rt;
}
```

	可以用于游戏服务器的底层库中. 同样对于如果判断是否是同一周什么鬼, 也是减去上面
	偏移量. 后面代码不再扩展解释, 大家多写多用就会吸星大法了. 本书很多素材基本都是
	在游戏服务器中常用业务. 扯一点题外话, 游戏相比其它互联网项目而言, 开宝箱的几率
	很高. 技术上多数吃老本, 新技术落后. 业务上面增删改查少很多. 总体而言轻巧些. 

```C
bool
stu_tisweek(time_t lt, time_t rt) {
	time_t mt;
	struct tm st;

	if (lt < rt) { //得到最大时间, 保存在lt中
		mt = lt;
		lt = rt;
		rt = mt;
	}

	// 得到lt 表示的当前时间
	localtime_r(&lt, &st);

	// 得到当前时间到周一起点的时间差
	st.tm_wday = st.tm_wday ? st.tm_wday - 1 : 6;
	mt = st.tm_wday * 24 * 3600 + st.tm_hour * 3600 
        + st.tm_min * 60 + st.tm_sec;

	// [min, lt], lt = max(lt, rt) 就表示在同一周内
	return rt >= lt - mt;
}

size_t 
stu_getmstr(stime_t tstr) {
    time_t t;
    struct tm st;
    struct timespec tv;

    timespec_get(&tv, TIME_UTC);
    t = tv.tv_sec;
    localtime_r(&t, &st);
    return snprintf(tstr, sizeof(stime_t), _STR_MTIME,
                    st.tm_year + 1900, st.tm_mon + 1, st.tm_mday,
                    st.tm_hour, st.tm_min, st.tm_sec,
                    tv.tv_nsec / 1000000);
}
```

    对于比较的问题, 用草纸花花图图就明白了. 
    这里关于时间核心业务都搞定了. 还有什么搞不定, 下次如果用到, 再细说 ~

### 3.6 展望

    这章目的, 打通跨平台一些共性操作. 给大家抛砖引玉, 试了解开发中都需要的基础操作. 
    学会一种方法, 应对不同平台的封装的策略. 也是以后步入金丹期, 漫天空气炮的一个好的开始~
    最后希望, 多陪陪爱我们的人, 房子票子那种法宝有最好, 没有也不影响你求的道 <*-*>

![白龙](./img/黑龙.jpg)
    
    以梦为马
    海子 节选

    面对大河我无限惭愧

    我年华虚度 空有一身疲倦

    和所有以梦为马的诗人一样

    岁月易逝 一滴不剩