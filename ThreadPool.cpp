#include "ThreadPool.h"
#include <unistd.h>
#include <iostream>
#include <string.h>
#include <string>
#include <stdlib.h>

//任务的构造函数
template <typename T>
Task<T>:: Task(callback f ,void* arg):func(f)
{
    this->arg = (T*)arg;
}
template <typename T>
Task<T>::~Task()
{
}

template <typename T>
ThreadPool<T>::ThreadPool(int min , int max):minNum(min) , maxNum(max)
{
    busyNum = 0;
    liveNum = 0;
    exitNum = 0;

    isShutDown = false;

    threadIDs.resize(maxNum,0);

    //创建管理者线程
    memset(&managerID,0,sizeof(managerID));
    if(pthread_create(&managerID,NULL,manager,this)!=0)
    {
        cout<<"create manager thread failed."<<endl;
        exit(-1);
    }

    //创建工作者线程
    for(int i =0 ;i < minNum; i++)
    {
        if(pthread_create(&threadIDs[i],NULL ,worker,this)!=0)
        {
            cout<<"ctreate workers failed."<<endl;
            exit(-1);
        }
    }

    //初始化互斥锁 和 条件变量
    if(pthread_mutex_init(&poolMutex,NULL)!=0 || \
        pthread_mutex_init(&m_taskQ,NULL) !=0)
    {
        cout<<"pthread_mutex_init failed"<<endl;
        exit(-1);
    }

    if(pthread_cond_init(&notEmpty,NULL)!=0)
    {
        cout<<"pthread_cond_init failed"<<endl;
        exit(-1);
    }

}

//析构函数 释放资源
template <typename T>
ThreadPool<T>::~ThreadPool()
{
    
    //确认关闭线程池
    isShutDown = true;

    //销毁所有的线程
    //销毁管理者线程
    pthread_join(managerID,NULL);

    //唤醒所有线程
    for(int i =0 ;i < liveNum;i++)
    {
        pthread_cond_signal(&notEmpty);
    }

    //销毁工作者线程
    //销毁锁 条件变量
    pthread_mutex_destroy(&poolMutex);
    pthread_mutex_destroy(&m_taskQ);
    pthread_cond_destroy(&notEmpty);

}

//返回工作中的线程数量
template <typename T>
int ThreadPool<T>:: getBusyNum()
{
    //在操作线程池的变量 先加锁
    pthread_mutex_lock(&poolMutex);
    int ret = busyNum;
    pthread_mutex_unlock(&poolMutex);

    return ret;
}
//返回存活的线程数量
template <typename T>
int ThreadPool<T>:: getLiveNum()
{
    pthread_mutex_lock(&poolMutex);
    int ret = liveNum;
    pthread_mutex_unlock(&poolMutex);

    return ret;
}

//添加任务到任务队列
template <typename T>
void ThreadPool<T>:: addTask(Task<T>& task)
{
    Taskptr<T>  ptr(task);
    //如果线程池关闭了 就绝不进行处理
    if(isShutDown){return ;}
    //给任务队列上锁
    pthread_mutex_lock(&m_taskQ);
    taskQ.push(ptr);
    pthread_mutex_unlock(&m_taskQ);
    //唤醒等待处理任务的线程
    pthread_cond_signal(&notEmpty);
}

// 重载 添加任务到任务队列
// f: 函数指针
// arg : 参数
template <typename T>
void ThreadPool<T>:: addTask(callback f ,void* arg)
{
    if(isShutDown){return ;}
    //给队列上锁
    pthread_mutex_lock(&m_taskQ);
    //生成智能指针
    Taskptr<T> tsk(new Task<T>(f , arg));
    taskQ.push(tsk);
    pthread_mutex_unlock(&m_taskQ);
   
    //唤醒等待工作的线程
    pthread_cond_signal(&notEmpty);

}

//工作线程 传入实例对象
template <typename T>
void* ThreadPool<T>:: worker(void* arg)
{
   
    //把arg转为ThreadPool*
    //思考：这里的转换可以是其他方式的吗？
    ThreadPool* pool = static_cast<ThreadPool*>(arg);

    //等待任务队列不为空  且线程池没有关闭
    //就从任务队列中取出一个任务执行
    while(true)
    {       
        //操作任务队列
        pthread_mutex_lock(&pool->m_taskQ);
        while(pool->taskQ.size()==0 && pool->isShutDown ==false)
        {
            pthread_cond_wait(&pool->notEmpty,&pool->m_taskQ);

            //如果是要销毁一个线程
            if(pool->exitNum > 0 )
            {
                pthread_mutex_lock(&pool->poolMutex);
                pool->exitNum--;
                if(pool->liveNum > pool->minNum)
                {
                    pool->liveNum--;
                    pthread_mutex_unlock(&pool->poolMutex);
                    //cout<<"销毁线程"<<endl;
                    pool->threadExit();
                }
                pthread_mutex_unlock(&pool->poolMutex);
            }
        }

        //如果线程池关闭了 就结束线程
        if(pool->isShutDown)
        {
            pthread_mutex_unlock(&pool->m_taskQ);
            pool->threadExit();
        }
        
        //取出一个任务进行执行
        Taskptr<T> task = pool->taskQ.front();
        pool->taskQ.pop();
        pthread_mutex_unlock(&pool->m_taskQ);

        task->func(task->arg);

        pthread_mutex_lock(&pool->poolMutex);
        pool->busyNum++;
        pthread_mutex_unlock(&pool->poolMutex);

        //执行任务
        task->func(task->arg);

        //任务结束
        pthread_mutex_lock(&pool->poolMutex);
        pool->busyNum--;
        pthread_mutex_unlock(&pool->poolMutex);
    }
    return nullptr;
}

// 管理者线程
template <typename T>
void* ThreadPool<T>:: manager(void* arg)
{
    //转换
    ThreadPool* pool = static_cast<ThreadPool*>(arg);

    //如果线程池没有关闭就一直检测
    while(!pool->isShutDown )
    {
        
        sleep(2);
        //给线程池加锁
        //取出一些参数
        int busy_thread_cnt = pool->getBusyNum();
        int live_thread_cnt = pool->getLiveNum();
        int thread_max_cnt = pool->maxNum;
        int thread_min_cnt = pool->minNum;

        //任务队列中任务数量
        pthread_mutex_unlock(&pool->m_taskQ);
        int task_in_Q = pool->taskQ.size();
        pthread_mutex_unlock(&pool->m_taskQ);

        //创建线程的规则：当前任务个数多余存货线程数量 
        //且 存活的线程数 小于 最大线程数

        const int create_num = 2;//一次创建线程的个数
        if( live_thread_cnt < task_in_Q  &&  live_thread_cnt < thread_max_cnt )
        {
           
            //准备创建线程 上锁
            pthread_mutex_lock(&pool->poolMutex);

            int cur_create = 0;
            for(int i =0;i<thread_max_cnt &&\
            cur_create<create_num&&\
            pool->liveNum<pool->maxNum;i++)
            {
                if(pool->threadIDs[i]==0 )
                {
                    pthread_create(&pool->threadIDs[i],NULL,worker,pool);
                    pool->liveNum++;
                    cur_create++;
                }
            }

            pthread_mutex_unlock(&pool->poolMutex);
        }


        //销毁线程的规则
        //忙的线程没有超过现有线程的一半 
        //且 现有线程数量大于最小线程数量
        const int destroy_num = 2; //一次销毁的个数
        if(busy_thread_cnt*2 <live_thread_cnt && live_thread_cnt > pool->minNum)
        {
            //准备销毁线程
            pthread_mutex_lock(&pool->poolMutex);
            pool->exitNum = destroy_num;
            pthread_mutex_unlock(&pool->poolMutex);
            for(int i = 0;i < destroy_num;i++)
            {
            
                pthread_cond_signal(&pool->notEmpty); 
            }
        }
    }
    return nullptr;
} 

template <typename T>
void ThreadPool<T>::  threadExit()
{
    pthread_t threadId = pthread_self();
    for(int i =0 ;i < maxNum ;i++)
    {
        if(threadIDs[i] == threadId )
        {
                threadIDs[i]  = 0 ;
                break;
        }
    }
    pthread_exit(NULL);
}
