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
