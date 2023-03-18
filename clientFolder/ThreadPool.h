
#pragma once
#ifndef _THREADPOO_H_
#define _THREADPOO_H_

#include <pthread.h>
#include <queue>
#include <vector>
#include <memory>

using namespace std;
//任务类

using callback = void(*)(void*);


struct args
{
    int fd;
    char* buf[100];
    int n ; 
    struct epoll_event* event; 
};

template <typename T>
class Task
{
public:
    //f 函数指针
    //arg函数传入的参数
    Task(callback f ,void* arg );
    ~Task();

public:
    callback func;
    T* arg;
};
template <typename T>
using Taskptr = shared_ptr<Task<T>>;

template <typename T>
class ThreadPool
{

public:
    
    //min:最小的工作线程数量
    //max:最大的工作线程数量
    ThreadPool(int min , int max);

    //添加任务到任务队列
    void addTask(Task<T>* task);

    // 重载 添加任务到任务队列
    // f: 函数指针
    // arg : 参数
    void addTask(callback f ,void* arg);

    int getBusyNum();//返回工作中的线程数量
    int getLiveNum();//返回存活的线程数量

    void threadExit();

    //析构函数
    //释放相关资源
    ~ThreadPool(); //结束线程

private:

    queue<Taskptr<T>> taskQ; // 任务队列
    pthread_t managerID ;//管理者线程ID
    vector<pthread_t> threadIDs; //工作线程的ID是否被占用

    int maxNum ; //线程池最大线程数量
    int minNum ; //线程池最小线程数量
    int busyNum ; // 正在工作的线程
    int liveNum ; // 存活的线程
    int exitNum ;//要销毁的线程

    bool isShutDown;//线程池是否关闭

    pthread_mutex_t poolMutex; //整个线程池的锁
    pthread_cond_t notEmpty; //条件变量 任务队列是否为空
    pthread_mutex_t m_taskQ; //操作任务队列时候需要一个锁

    static void* worker(void* arg); //工作线程 传入实例对象
    static void* manager(void* arg); // 管理者线程

};

#endif