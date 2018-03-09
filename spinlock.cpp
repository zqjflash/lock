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
